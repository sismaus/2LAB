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

// ============================================================================
// КЛАСС КЛЮЧЕЙ И СЕРТИФИКАТОВ
// ============================================================================
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
// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (ОБЯЗАТЕЛЬНО ОБЪЯВИТЬ!)
// ============================================================================
HANDLE g_hPipe = INVALID_HANDLE_VALUE;
atomic<bool> g_dataReady(false);
string g_receivedData;
atomic<bool> g_running(true);

// Флаги состояния для Сценария 1
atomic<bool> g_certReceived(false);        // Сертификат клиента получен
atomic<bool> g_messageReceived(false);     // Сообщение получено
string g_pendingMessage;                   // Временное хранение сообщения (НЕ atomic для string)
vector<unsigned char> g_pendingSignature;
X509* clientCertificate = nullptr;
CryptoKeys* g_serverKeys = nullptr;
// ============================================================================
// КРИПТОГРАФИЧЕСКИЕ ФУНКЦИИ
// ============================================================================
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

// ============================================================================
// СЕТЕВЫЕ ФУНКЦИИ
// ============================================================================
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

// ============================================================================
// ПОТОК ДЛЯ ПРИЕМА ДАННЫХ
// ============================================================================
void networkReceiverThread() {
    while (g_running) {
        DWORD bytesAvailable = 0;
        PeekNamedPipe(g_hPipe, NULL, 0, NULL, &bytesAvailable, NULL);

        if (bytesAvailable > 0) {
            try {
                // Читаем данные
                g_receivedData = receiveData(g_hPipe);

                cout << "\n[СЕРВЕР] <<< Получено: " << g_receivedData.length() << " байт" << endl;

                // === 1. ОБРАБОТКА СЕРТИФИКАТА КЛИЕНТА ===
                if (g_receivedData.find("CERT_CLIENT:") == 0) {
                    cout << "[СЕРВЕР] Тип: СЕРТИФИКАТ КЛИЕНТА" << endl;

                    string clientCertPem = g_receivedData.substr(12);

                    // Загружаем сертификат
                    if (clientCertificate) {
                        X509_free(clientCertificate);
                    }

                    clientCertificate = loadCertificateFromPem(clientCertPem);

                    if (clientCertificate) {
                        cout << "[СЕРВЕР] Сертификат загружен и сохранен!" << endl;
                        g_certReceived = true;  // ← ВАЖНО: Устанавливаем флаг!
                    }
                    else {
                        cout << "[СЕРВЕР] Ошибка загрузки сертификата!" << endl;
                        g_certReceived = false;
                    }
                }
                // === 2. ОБРАБОТКА СООБЩЕНИЯ ДЛЯ ПРОВЕРКИ ===
                else if (g_receivedData.find("GET_") != 0 &&
                    g_receivedData.find("CERT_") != 0 &&
                    g_receivedData.find("RESULT") != 0 &&
                    g_receivedData.find("SIGNED_") != 0 &&
                    g_receivedData.length() > 0 &&
                    g_receivedData.length() < 100) {

                    cout << "[СЕРВЕР] Тип: СООБЩЕНИЕ" << endl;
                    cout << "[СЕРВЕР] Текст: " << g_receivedData << endl;

                    // Сохраняем сообщение
                    g_pendingMessage = g_receivedData;
                    g_messageReceived = true;  // ← ВАЖНО: Устанавливаем флаг!
                }
                // === 3. ОБРАБОТКА ЗАПРОСОВ ===
                else if (g_receivedData.find("GET_SERVER_CERT") == 0) {
                    cout << "[СЕРВЕР] Тип: ЗАПРОС СЕРТИФИКАТА СЕРВЕРА" << endl;
                    string certPem = g_serverKeys->getCertificatePem();
                    sendData(g_hPipe, certPem);
                    cout << "[СЕРВЕР] Сертификат отправлен" << endl;
                }
                else if (g_receivedData.find("GET_SIGNED_MESSAGE") == 0) {
                    cout << "[СЕРВЕР] Тип: ЗАПРОС ПОДПИСАННОГО СООБЩЕНИЯ" << endl;
                    // Обработка в пункте меню 3
                }
                else if (g_receivedData.length() == 256 || g_receivedData.length() == 512)
                {
                    cout << "[СЕРВЕР] Тип: Подпись(" << g_receivedData.length() << " байт)" << endl;
                    g_pendingSignature.assign(g_receivedData.begin(), g_receivedData.end());
                    cout << "[СЕРВЕР] Подпись сохранена" << endl;
                }
                else {
                    cout << "[СЕРВЕР] Тип: НЕИЗВЕСТНЫЙ" << endl;
                }

                g_dataReady = false;
                g_receivedData = "";
            }
            catch (...) {
                cout << "[СЕРВЕР] Ошибка чтения данных" << endl;
            }
        }

        Sleep(100);
    }


}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ============================================================================
int main() {
    SetConsoleOutputCP(1251);
    setlocale(LC_ALL, "Russian");

    cout << "========================================" << endl;
    cout << "   СЕРВЕР ЭЦП (с сертификатами X.509)" << endl;
    cout << "========================================" << endl;

    g_serverKeys = new CryptoKeys("Server", "ECPServer Organization");
    clientCertificate = nullptr;

    cout << "\n[СЕРВЕР] Сертификат сервера создан" << endl;
    cout << "  - Закрытый ключ: в защищенной памяти сервера" << endl;
    cout << "  - Открытый ключ: в составе сертификата X.509" << endl;

    HANDLE hPipe = CreateNamedPipe(
        L"\\\\.\\pipe\\ECSCertPipe",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 0, 0, 0, NULL
    );
    g_hPipe = hPipe;

    if (hPipe == INVALID_HANDLE_VALUE) {
        cout << "Ошибка создания канала!" << endl;
        return 1;
    }

    cout << "\n[СЕРВЕР] Ожидание подключения клиента..." << endl;
    ConnectNamedPipe(hPipe, NULL);
    cout << "[СЕРВЕР] Клиент подключен!" << endl;

    g_hPipe = hPipe;

    thread receiver(networkReceiverThread);

    cout << "[СЕРВЕР] Фон. прием данных запущен" << endl;

    while (g_running) {
        // Проверяем, пришли ли данные от клиента
        if (g_dataReady) {
            cout << "\n[СЕРВЕР] <<< Получены данные (" << g_receivedData.length() << " байт)" << endl;

            // === 1. ПРОВЕРКА НА СЕРТИФИКАТ КЛИЕНТА ===
            if (g_receivedData.find("CERT_CLIENT:") == 0) {
                cout << "[СЕРВЕР] Тип: СЕРТИФИКАТ КЛИЕНТА" << endl;

                string clientCertPem = g_receivedData.substr(12);
                cout << "[СЕРВЕР] Размер сертификата: " << clientCertPem.length() << " байт" << endl;

                if (clientCertificate) {
                    X509_free(clientCertificate);
                }

                clientCertificate = loadCertificateFromPem(clientCertPem);

                if (clientCertificate) {
                    cout << "[СЕРВЕР] Сертификат успешно загружен!" << endl;
                    g_certReceived = true;
                }
                else {
                    cout << "[СЕРВЕР] ОШИБКА загрузки сертификата!" << endl;
                    g_certReceived = false;
                }

                g_dataReady = false;
                g_receivedData = "";
                continue;
            }

            // === 2. ПРОВЕРКА НА СООБЩЕНИЕ (Сценарий 1) ===
            if (g_receivedData.find("GET_") != 0 &&
                g_receivedData.find("CERT_") != 0 &&
                g_receivedData.find("RESULT") != 0 &&
                g_receivedData.find("SIGNED_") != 0 &&
                g_receivedData.length() > 0 &&
                g_receivedData.length() < 1000) {

                cout << "[СЕРВЕР] Тип: СООБЩЕНИЕ ДЛЯ ПРОВЕРКИ" << endl;
                cout << "[СЕРВЕР] Текст: " << g_receivedData << endl;

                g_pendingMessage = g_receivedData;
                g_messageReceived = true;

                g_dataReady = false;
                g_receivedData = "";
                continue;
            }

            // === 3. ЗАПРОСЫ ===
            if (g_receivedData.find("GET_SERVER_CERT") == 0) {
                cout << "[СЕРВЕР] Тип: ЗАПРОС СЕРТИФИКАТА СЕРВЕРА" << endl;
                string certPem = g_serverKeys->getCertificatePem();
                sendData(hPipe, certPem);
                cout << "[СЕРВЕР] Сертификат отправлен" << endl;
                g_dataReady = false;
                g_receivedData = "";
                continue;
            }

            if (g_receivedData.find("GET_SIGNED_MESSAGE") == 0) {
                cout << "[СЕРВЕР] Тип: ЗАПРОС ПОДПИСАННОГО СООБЩЕНИЯ" << endl;
                g_dataReady = false;
                g_receivedData = "";
                continue;
            }

            cout << "[СЕРВЕР] Тип: НЕИЗВЕСТНЫЙ (очищено)" << endl;
            g_dataReady = false;
            g_receivedData = "";
        }

        // === МЕНЮ СЕРВЕРА ===
        cout << "\n=== МЕНЮ СЕРВЕРА ===" << endl;
        cout << "1. СЦЕНАРИЙ 1: Проверить подпись клиента" << endl;
        cout << "2. СЦЕНАРИЙ 2: Подписать сообщение для клиента" << endl;
        cout << "3. Отправить сертификат сервера клиенту" << endl;
        cout << "4. Показать статус" << endl;
        cout << "5. Выход" << endl;
        cout << "Выбор: ";

        int choice;
        cin >> choice;
        cin.ignore();

        if (choice == 1) {
            cout << "\n=== СЦЕНАРИЙ 1: Проверка подписи клиента ===" << endl;
            cout << "==============================================" << endl;

            if (!g_certReceived || !clientCertificate) {
                cout << "[СЕРВЕР] Шаг 1: Ожидание сертификата клиента..." << endl;
                cout << "[СЕРВЕР] ОШИБКА: Сертификат еще НЕ получен!" << endl;
                cout << "[СЕРВЕР] Попросите клиента отправить сообщение (пункт 1)" << endl;
                continue;
            }

            cout << "[СЕРВЕР] Шаг 1: Сертификат клиента получен" << endl;

            if (!g_messageReceived) {
                cout << "[СЕРВЕР] Шаг 2: Ожидание сообщения от клиента..." << endl;
                cout << "[СЕРВЕР] ОШИБКА: Сообщение еще НЕ получено!" << endl;
                cout << "[СЕРВЕР] Попросите клиента отправить подписанное сообщение" << endl;
                continue;
            }

            cout << "[СЕРВЕР] Шаг 2: Сообщение получено " << endl;
            cout << "[СЕРВЕР]         Текст: " << g_pendingMessage << endl;

            if (g_pendingSignature.empty()) {
                cout << "[СЕРВЕР] Шаг 3: Ожидание подписи от клиента..." << endl;
                cout << "[СЕРВЕР] ОШИБКА: Подпись еще НЕ получено!" << endl;
                cout << "[СЕРВЕР] Попросите клиента отправить подписанное сообщение" << endl;
                g_messageReceived = false;
                g_pendingMessage = "";
                continue;
            }

            cout << "\n[СЕРВЕР] Шаг 3:  Подпись получена " << endl;
            cout << "[СЕРВЕР] Размер подписи: " << g_pendingSignature.size() << " байт" << endl;

            Sleep(500);

            cout << "\n[СЕРВЕР] ============================================" << endl;
            cout << "[СЕРВЕР] Шаг 4: ВЕРИФИКАЦИЯ ПОДПИСИ" << endl;
            cout << "[СЕРВЕР] ============================================" << endl;
            cout << "[СЕРВЕР]   - Извлечение публичного ключа из сертификата..." << endl;
            cout << "[СЕРВЕР]   - Вычисление хэша от сообщения..." << endl;
            cout << "[СЕРВЕР]   - Расшифровка подписи публичным ключом..." << endl;
            cout << "[СЕРВЕР]   - Сравнение хэшей..." << endl;

            EVP_PKEY* clientPubKey = getPublicKeyFromCertificate(clientCertificate);
            bool isValid = verifySignature(g_pendingMessage, g_pendingSignature, clientPubKey);
            EVP_PKEY_free(clientPubKey);

            string result = isValid ? "ПОДПИСЬ ВЕРНА" : "ПОДПИСЬ НЕВЕРНА";

            cout << "\n[СЕРВЕР] РЕЗУЛЬТАТ: " << result << endl;
            cout << "[СЕРВЕР] ============================================" << endl;

            cout << "\n[СЕРВЕР] Шаг 5: Отправка результата клиенту..." << endl;
            sendData(hPipe, "RESULT:" + result);
            cout << "[СЕРВЕР] Результат отправлен " << endl;

            g_messageReceived = false;
            g_pendingMessage = "";
        }
        else if (choice == 2) {
            cout << "\n=== СЦЕНАРИЙ 2: Подпись на стороне сервера ===" << endl;
            cout << "==============================================" << endl;

            cout << "[СЕРВЕР] Шаг 3: Генерация и подписание сообщения" << endl;
            cout << "         Введите сообщение для подписи: ";
            string message;
            getline(cin, message);

            cout << "         Вычисление хэша от сообщения..." << endl;
            cout << "         Шифрование хэша закрытым ключом сервера..." << endl;
            vector<unsigned char> signature = signMessage(message, g_serverKeys->getPrivateKey());
            cout << "         Подпись создана (" << signature.size() << " байт)" << endl;

            cout << "\n[СЕРВЕР] Шаг 4: Отправка сообщения и подписи клиенту" << endl;

            sendData(hPipe, "SIGNED_MESSAGE:" + message);
            cout << "         Сообщение отправлено" << endl;

            Sleep(500);

            /*DWORD sigSize = signature.size();
            WriteFile(hPipe, &sigSize, sizeof(sigSize), NULL, NULL);
            cout << "         Размер подписи отправлен" << endl;
            Sleep(300);
            WriteFile(hPipe, signature.data(), sigSize, NULL, NULL);*/

            WriteFile(hPipe, signature.data(), signature.size(), NULL, NULL);
            cout << "         Подпись отправлена" << endl;

            cout << "[СЕРВЕР] ============================================" << endl;
        }
        else if (choice == 3) {
            cout << "\n[СЕРВЕР] Отправка сертификата сервера..." << endl;
            string certPem = g_serverKeys->getCertificatePem();
            sendData(hPipe, certPem);
            cout << "[СЕРВЕР] Сертификат отправлен клиенту" << endl;
        }
        else if (choice == 4) {
            cout << "\n=== СТАТУС СЕРВЕРА ===" << endl;
            cout << "Сертификат клиента: " << (g_certReceived ? "ПОЛУЧЕН " : "НЕ ПОЛУЧЕН") << endl;
            cout << "Сообщение от клиента: " << (g_messageReceived ? "ПОЛУЧЕНО " : "НЕ ПОЛУЧЕНО") << endl;
            cout << "Сертификат сервера: СОЗДАН " << endl;

            cout << "\n=== ИНФОРМАЦИЯ О СЕРТИФИКАТЕ СЕРВЕРА ===" << endl;
            string info = g_serverKeys->getCertificateInfo();
            cout << info << endl;
        }
        else if (choice == 5) {
            g_running = false;
            break;
        }
    }

    receiver.join();

    if (clientCertificate) {
        X509_free(clientCertificate);
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);


    if (g_serverKeys) {
        delete g_serverKeys;
        g_serverKeys = nullptr;
    }

    cout << "\n[СЕРВЕР] Завершен" << endl;
    system("pause");
    return 0;
}
