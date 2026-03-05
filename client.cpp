#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <windows.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/x509.h>
#include <openssl/bn.h>
#include <ctime>

using namespace std;

// Глобальные переменные
HANDLE g_hPipe = INVALID_HANDLE_VALUE;
atomic<bool> g_dataReady(false);
string g_receivedData;
atomic<bool> g_running(true);
atomic<bool> g_waitingForData(false);
atomic<int> g_receiveState(0);  // 0=ничего, 1=ждем сертификат, 2=ждем сообщение с подписью, 3=ждем результат

class CryptoKeys {
private:
    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;

public:
    CryptoKeys(const string& commonName, const string& organization) {
        srand(time(NULL));

        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
        EVP_PKEY_keygen(ctx, &pkey);
        EVP_PKEY_CTX_free(ctx);

        cert = X509_new();
        X509_set_version(cert, 2);

        ASN1_INTEGER* serial = ASN1_INTEGER_new();
        BIGNUM* bn = BN_new();
        BN_rand(bn, 160, 0, 0);
        BN_to_ASN1_INTEGER(bn, serial);
        X509_set_serialNumber(cert, serial);
        ASN1_INTEGER_free(serial);
        BN_free(bn);

        X509_gmtime_adj(X509_get_notBefore(cert), 0);
        X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 60 * 60);
        X509_set_pubkey(cert, pkey);

        X509_NAME* name = X509_get_subject_name(cert);
        X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"RU", -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)organization.c_str(), -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)commonName.c_str(), -1, -1, 0);

        X509_set_issuer_name(cert, name);
        X509_sign(cert, pkey, EVP_sha256());
    }

    ~CryptoKeys() {
        if (pkey) EVP_PKEY_free(pkey);
        if (cert) X509_free(cert);
    }

    string getCertificatePem() {
        BIO* bio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(bio, cert);
        BUF_MEM* bptr;
        BIO_get_mem_ptr(bio, &bptr);
        string certStr(bptr->data, bptr->length);
        BIO_free(bio);
        return certStr;
    }
    string getCertificateInfo() {
        BIO* bio = BIO_new(BIO_s_mem());
        X509_print(bio, cert);
        BUF_MEM* bptr;
        BIO_get_mem_ptr(bio, &bptr);
        string info(bptr->data, bptr->length);
        BIO_free(bio);
        return info;
    }
    EVP_PKEY* getPrivateKey() { return pkey; }
};

vector<unsigned char> signMessage(const string& message, EVP_PKEY* privateKey) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    const EVP_MD* md = EVP_sha256();
    unsigned char* sig = nullptr;
    unsigned int sigLen = 0;

    EVP_SignInit(mdctx, md);
    EVP_SignUpdate(mdctx, message.c_str(), message.length());
    EVP_SignFinal(mdctx, NULL, &sigLen, privateKey);

    sig = (unsigned char*)OPENSSL_malloc(sigLen);
    EVP_SignFinal(mdctx, sig, &sigLen, privateKey);

    vector<unsigned char> sigVec(sig, sig + sigLen);
    OPENSSL_free(sig);
    EVP_MD_CTX_destroy(mdctx);
    return sigVec;
}

bool verifySignature(const string& message, const vector<unsigned char>& signature, EVP_PKEY* publicKey) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    const EVP_MD* md = EVP_sha256();
    EVP_VerifyInit(mdctx, md);
    EVP_VerifyUpdate(mdctx, message.c_str(), message.length());
    int result = EVP_VerifyFinal(mdctx, signature.data(), signature.size(), publicKey);
    EVP_MD_CTX_destroy(mdctx);
    return (result == 1);
}

X509* loadCertificateFromPem(const string& pemStr) {
    BIO* bio = BIO_new_mem_buf(pemStr.c_str(), -1);
    X509* cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return cert;
}

EVP_PKEY* getPublicKeyFromCertificate(X509* cert) {
    return X509_get_pubkey(cert);
}

// --- Сетевые функции ---

string receiveData(HANDLE hPipe) {
    DWORD bytesRead;
    DWORD dataSize;
    ReadFile(hPipe, &dataSize, sizeof(dataSize), &bytesRead, NULL);
    vector<char> buffer(dataSize);
    ReadFile(hPipe, buffer.data(), dataSize, &bytesRead, NULL);
    return string(buffer.data(), dataSize);
}

void sendData(HANDLE hPipe, const string& data) {
    DWORD bytesWritten;
    DWORD dataSize = data.size();
    WriteFile(hPipe, &dataSize, sizeof(dataSize), &bytesWritten, NULL);
    WriteFile(hPipe, data.c_str(), dataSize, &bytesWritten, NULL);
}

// --- Поток для приема данных ---

void networkReceiverThread() {
    while (g_running) {
        DWORD bytesAvailable = 0;
        PeekNamedPipe(g_hPipe, NULL, 0, NULL, &bytesAvailable, NULL);

        if (bytesAvailable > 0) {
            try {
                g_receivedData = receiveData(g_hPipe);
                g_dataReady = true;
            }
            catch (...) {
                // Ошибка чтения
            }
        }

        Sleep(100);
    }
}

// --- Обработка полученных данных ---

void ProcessReceivedData(HANDLE hPipe, X509*& serverCertificate) {
    cout << "\n[КЛИЕНТ] <<< Получены данные (" << g_receivedData.length() << " байт)" << endl;


    // === СЦЕНАРИЙ 2: Получение подписанного сообщения от сервера ===
    if (g_receiveState == 2 && g_receivedData.find("SIGNED_MESSAGE:") == 0) {
        string message = g_receivedData.substr(15);
        cout << "[КЛИЕНТ] Шаг 4: Получено сообщение от сервера" << endl;
        cout << "         Текст: " << message << endl;

        // Ждем пока появятся данные для подписи
        cout << "[КЛИЕНТ] Ожидание подписи..." << endl;

        DWORD bytesAvailable = 0;
        int timeout = 0;

        // Ждем максимум 5 секунд
        while (timeout < 50) {
            PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, NULL);

            if (bytesAvailable >= 256) {
                cout << "[КЛИЕНТ] Данные доступны (" << bytesAvailable << " байт)" << endl;
                break;
            }

            Sleep(100);
            timeout++;

            if (timeout % 10 == 0) {
                cout << "[КЛИЕНТ] Ожидание... (" << timeout * 100 << " мс)" << endl;
            }
        }

        if (bytesAvailable < 256) {
            cout << "[КЛИЕНТ] ОШИБКА: Подпись не получена (таймаут)" << endl;
            cout << "[КЛИЕНТ] Доступно байт: " << bytesAvailable << endl;
            g_dataReady = false;
            g_receivedData = "";
            g_receiveState = 0;
            return;
        }


        vector<unsigned char> signature(256);
        DWORD bytesRead;
        BOOL result = ReadFile(hPipe, signature.data(), 256, &bytesRead, NULL);
        
        if (result) {
            cout << "[КЛИЕНТ] Шаг 4: Получена подпись (" << bytesRead << " байт)" << endl;

            cout << "\n[КЛИЕНТ] ============================================" << endl;
            cout << "[КЛИЕНТ] Шаг 5: ВЕРИФИКАЦИЯ ПОДПИСИ" << endl;
            cout << "[КЛИЕНТ] ============================================" << endl;

            if (serverCertificate) {
                cout << "[КЛИЕНТ]   - Извлечение публичного ключа из сертификата сервера..." << endl;
                EVP_PKEY* serverPubKey = getPublicKeyFromCertificate(serverCertificate);

                if (serverPubKey) {
                    cout << "[КЛИЕНТ]   - Вычисление хэша от полученного сообщения..." << endl;
                    cout << "[КЛИЕНТ]   - Расшифровка подписи публичным ключом сервера..." << endl;
                    cout << "[КЛИЕНТ]   - Сравнение хэшей..." << endl;

                    bool isValid = verifySignature(message, signature, serverPubKey);
                    EVP_PKEY_free(serverPubKey);

                    cout << "\n[КЛИЕНТ] ============================================" << endl;
                    cout << "[КЛИЕНТ] Шаг 6: РЕЗУЛЬТАТ ПРОВЕРКИ" << endl;
                    cout << "[КЛИЕНТ] ============================================" << endl;

                    if (isValid) {
                        cout << "[КЛИЕНТ] *** СООБЩЕНИЕ ПОДЛИННОЕ ***" << endl;
                        cout << "[КЛИЕНТ] *** Источник подтвержден сертификатом сервера ***" << endl;
                        cout << "[КЛИЕНТ] *** Подпись соответствует публичному ключу ***" << endl;
                    }
                    else {
                        cout << "[КЛИЕНТ] *** СООБЩЕНИЕ НЕ ПОДЛИННОЕ ***" << endl;
                        cout << "[КЛИЕНТ] *** Подпись не соответствует ключу ***" << endl;
                        cout << "[КЛИЕНТ] *** Возможна подмена данных ***" << endl;
                    }
                    cout << "[КЛИЕНТ] ============================================" << endl;
                }
                else {
                    cout << "[КЛИЕНТ] Ошибка: не удалось извлечь публичный ключ!" << endl;
                }
            }
            else {
                cout << "[КЛИЕНТ] Ошибка: нет сертификата сервера!" << endl;
                cout << "[КЛИЕНТ] Сначала выполните пункт 3 (получить сертификат)" << endl;
            }
        }
        else {
            cout << "[КЛИЕНТ] ОШИБКА чтения подписи: " << GetLastError() << endl;
            g_dataReady = false;
            g_receivedData = "";
            g_receiveState = 0;
            return;

        }
        

        g_dataReady = false;
        g_receivedData = "";
        g_receiveState = 0;
        return;
    }

    // === СЦЕНАРИЙ 1: Результат проверки подписи клиента ===
    if (g_receivedData.find("RESULT:") == 0) {
        cout << "\n[КЛИЕНТ] ============================================" << endl;
        cout << "[КЛИЕНТ] Шаг 6: Получен статус верификации от сервера" << endl;
        cout << "[КЛИЕНТ] ============================================" << endl;

        string result = g_receivedData.substr(7);
        if (result.find("ВЕРНА") != string::npos) {
            cout << "[КЛИЕНТ] *** СТАТУС: ПОДПИСЬ ВЕРНА ***" << endl;
            cout << "[КЛИЕНТ] *** Сервер подтвердил подлинность сообщения ***" << endl;
            cout << "[КЛИЕНТ] *** Закрытый ключ клиента не скомпрометирован ***" << endl;
        }
        else {
            cout << "[КЛИЕНТ] *** СТАТУС: ПОДПИСЬ НЕВЕРНА ***" << endl;
            cout << "[КЛИЕНТ] *** Сервер отклонил сообщение ***" << endl;
        }
        cout << "[КЛИЕНТ] ============================================" << endl;

        g_dataReady = false;
        g_receivedData = "";
        g_receiveState = 0;
        return;
    }



    // Проверяем что данные текстовые
    bool isTextData = true;
    if (g_receivedData.length() > 0) {
        for (char c : g_receivedData) {
            if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                isTextData = false;
                break;
            }
        }
    }
    else {
        isTextData = false;
    }

    // Если бинарные данные - пропускаем
    if (!isTextData) {
        cout << "[КЛИЕНТ] Бинарные данные (пропускаем)" << endl;
        g_dataReady = false;
        g_receivedData = "";
        return;
    }
    // === СЦЕНАРИЙ 3: Получение сертификата сервера ===
    if (g_receiveState == 1 && isTextData && g_receivedData.length() > 100) {
        cout << "[КЛИЕНТ] Шаг 1: Получен публичный ключ сервера (в сертификате)" << endl;
        cout << "[КЛИЕНТ] Размер сертификата: " << g_receivedData.length() << " байт" << endl;

        if (serverCertificate) {
            X509_free(serverCertificate);
        }

        serverCertificate = loadCertificateFromPem(g_receivedData);

        if (serverCertificate) {
            cout << "[КЛИЕНТ] Сертификат сохранен для верификации" << endl;
        }
        else {
            cout << "[КЛИЕНТ] Ошибка загрузки сертификата!" << endl;
        }

        g_dataReady = false;
        g_receivedData = "";
        g_receiveState = 0;
        return;
    }

    g_dataReady = false;
    g_receivedData = "";
}

// --- ГЛАВНАЯ ФУНКЦИЯ ---

int main() {
    SetConsoleOutputCP(1251);
    setlocale(LC_ALL, "Russian");

    cout << "========================================" << endl;
    cout << "   КЛИЕНТ ЭЦП (с сертификатами X.509)" << endl;
    cout << "========================================" << endl;

    CryptoKeys clientKeys("Client", "ECPClient Organization");
    X509* serverCertificate = nullptr;

    cout << "\n[КЛИЕНТ] Сертификат клиента создан" << endl;
    cout << "  - Закрытый ключ: в защищенной памяти клиента" << endl;
    cout << "  - Открытый ключ: в составе сертификата X.509" << endl;

    HANDLE hPipe = CreateFile(
        L"\\\\.\\pipe\\ECSCertPipe",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        cout << "Ошибка подключения к серверу! Убедитесь, что сервер запущен." << endl;
        system("pause");
        return 1;
    }

    cout << "\n[КЛИЕНТ] Подключено к серверу!" << endl;

    g_hPipe = hPipe;

    thread receiver(networkReceiverThread);

    cout << "[КЛИЕНТ] Фон. прием данных запущен" << endl;

    while (g_running) {
        if (g_waitingForData) {
            if (g_dataReady) {
                ProcessReceivedData(hPipe, serverCertificate);
                if (g_receiveState == 0) {
                    g_waitingForData = false;
                }
            }
            else {
                Sleep(100);
            }
            continue;
        }

        if (g_dataReady) {
            ProcessReceivedData(hPipe, serverCertificate);
            continue;
        }

        cout << "\n=== МЕНЮ КЛИЕНТА ===" << endl;
        cout << "1. СЦЕНАРИЙ 1: Подписать и отправить сообщение" << endl;
        cout << "2. СЦЕНАРИЙ 2: Получить подписанное сообщение от сервера" << endl;
        cout << "3. Получить сертификат сервера" << endl;
        cout << "4. Показать информацию о сертификате клиента" << endl;
        cout << "5. Выход" << endl;
        cout << "Выбор: ";

        int choice;
        cin >> choice;
        cin.ignore();

        if (choice == 1) {
            cout << "\n=== СЦЕНАРИЙ 1: Подпись на стороне клиента ===" << endl;
            cout << "==============================================" << endl;

            cout << "[КЛИЕНТ] Шаг 1: Формирование произвольного сообщения" << endl;
            cout << "         Введите текст сообщения: ";
            string message;
            getline(cin, message);

            cout << "[КЛИЕНТ] Шаг 2: Подписание сообщения закрытым ключом клиента" << endl;
            cout << "         Вычисление хэша от сообщения..." << endl;
            cout << "         Шифрование хэша закрытым ключом..." << endl;
            vector<unsigned char> signature = signMessage(message, clientKeys.getPrivateKey());
            cout << "         Подпись создана (" << signature.size() << " байт)" << endl;

            cout << "[КЛИЕНТ] Шаг 3: Отправка подписанного сообщения на сервер" << endl;

            // ОТПРАВЛЯЕМ ПОСЛЕДОВАТЕЛЬНО С БОЛЬШИМИ ЗАДЕРЖКАМИ
            cout << "         [3.1] Отправка сертификата клиента..." << endl;
            string certData = "CERT_CLIENT:" + clientKeys.getCertificatePem();
            sendData(hPipe, certData);
            cout << "         [3.1] Сертификат отправлен (" << certData.length() << " байт)" << endl;

            // ВАЖНО: Ждем подтверждения что сервер получил
            Sleep(1000);  // Увеличенная задержка!

            cout << "         [3.2] Отправка сообщения..." << endl;
            sendData(hPipe, message);
            cout << "         [3.2] Сообщение отправлено" << endl;

            Sleep(500);

            cout << "         [3.3] Отправка размера подписи..." << endl;
            DWORD sigSize = signature.size();
            WriteFile(hPipe, &sigSize, sizeof(sigSize), NULL, NULL);

            Sleep(500);

            cout << "         [3.4] Отправка подписи..." << endl;
            WriteFile(hPipe, signature.data(), sigSize, NULL, NULL);
            cout << "         [3.4] Подпись отправлена (" << sigSize << " байт)" << endl;

            cout << "\n[КЛИЕНТ] Все данные отправлены. Ожидание результата..." << endl;

            g_receiveState = 3;
            g_waitingForData = true;
        }
        else if (choice == 2) {
            cout << "\n=== СЦЕНАРИЙ 2: Подпись на стороне сервера ===" << endl;
            cout << "==============================================" << endl;

            if (!serverCertificate) {
                cout << "[КЛИЕНТ] Сначала получите сертификат сервера (пункт 3)" << endl;
                continue;
            }

            cout << "[КЛИЕНТ] Шаг 2: Запрос на генерацию случайного сообщения" << endl;
            cout << "         Отправка запроса серверу..." << endl;
            sendData(hPipe, "GET_SIGNED_MESSAGE");

            g_receiveState = 2;
            g_waitingForData = true;
            cout << "[КЛИЕНТ] Ожидание ответа от сервера..." << endl;
        }
        else if (choice == 3) {
            cout << "\n=== Запрос сертификата сервера ===" << endl;
            cout << "[КЛИЕНТ] Шаг 1: Запрос публичного ключа сервера" << endl;
            cout << "         Отправка запроса..." << endl;
            sendData(hPipe, "GET_SERVER_CERT");

            g_receiveState = 1;
            g_waitingForData = true;
            cout << "[КЛИЕНТ] Ожидание сертификата..." << endl;
        }
        else if (choice == 4) {
            cout << "\n=== ИНФОРМАЦИЯ О СЕРТИФИКАТЕ КЛИЕНТА ===" << endl;
            string info = clientKeys.getCertificateInfo();
            cout << info << endl;
        }
        else if (choice == 5) {
            g_running = false;
            break;
        }
    }

    receiver.join();

    if (serverCertificate) {
        X509_free(serverCertificate);
    }

    CloseHandle(hPipe);

    cout << "\n[КЛИЕНТ] Завершен" << endl;
    system("pause");
    return 0;
}
