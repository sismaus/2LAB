using Libs;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;

namespace Client
{
    public static class FilePath
    {
        public static Guid ClientGuid = Guid.NewGuid();
        public static string baseKeysPath = "../../../keys/";
        public static string basePath = baseKeysPath + "keys_" + ClientGuid.ToString() + "/";
        public static string publicKey_client = basePath + "publicKey_client.pem";
        public static string privateKey_client = basePath + "privateKey_client.pem";
        public static string publicKey_server = basePath + "publicKey_server.pem";
    }
    public class Logic : ILogic
    {
        private IDS _ds;
        private byte[] signatureClient;
        private byte[] signatureServer;
        private RSAParameters _serverPublicKey;
                

        public Logic(IDS ds)
        {
            _ds = ds;            
        }

        public async Task<string> SendRequestToServer(string request)
        {
            using (var client = new TcpClient("127.0.0.1", 5000))
            using (var stream = client.GetStream())
            {
                byte[] requestData = Encoding.UTF8.GetBytes(request);
                await stream.WriteAsync(requestData, 0, requestData.Length);

                var buffer = new byte[1024];
                int bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length);
                return Encoding.UTF8.GetString(buffer, 0, bytesRead);
            }
        }
        public string Init()
        {            
            Directory.CreateDirectory(FilePath.baseKeysPath);
            Directory.CreateDirectory(FilePath.basePath);
            GenerateKeys();
            return FilePath.ClientGuid.ToString();
        }
        public static RSAParameters DeserializeKeyPem(string key)
        {
            using (RSA rsa = RSA.Create())
            {
                rsa.ImportFromPem(key);
                return rsa.ExportParameters(false);
            }
        }
        public void GenerateKeys()
        {
            _ds.GenerateKeys(out string pbKey, out string prKey);
            File.WriteAllBytes(FilePath.publicKey_client, Encoding.ASCII.GetBytes(pbKey));
            File.WriteAllBytes(FilePath.privateKey_client, Encoding.ASCII.GetBytes(prKey));
            MessageBox.Show("Ключи созданы!", "Keys Status", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        public async Task<(string, bool)> GenerateMessageClick()
        {
            try
            {
                string response = await SendRequestToServer("GET_RANDOM_MESSAGE");
                var parts = response.Split(':');
                string randomMessage = parts[0];
                signatureServer = Convert.FromBase64String(parts[1]);
                
                return (randomMessage, true);                
            }
            catch (Exception ex)
            {                
                MessageBox.Show(ex.Message, "Информация о генерации сообщения", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return ("", true);
            }            
        }
        public async void GetServerPublicKeyClick()
        {
            try
            {
                string response = await SendRequestToServer("GET_PUBLIC_KEY");
                _serverPublicKey = DeserializeKeyPem(response);
                MessageBox.Show("Публичный ключ сервера получен!", "Получение публичного ключа сервера", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "Получение публичного ключа сервера", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        public void SignDataClick(string clientMsg, Button btn_VerifyClientData)
        {
            if (clientMsg != "")
            {
                try
                {
                    signatureClient = _ds.SignData(clientMsg);                    
                    MessageBox.Show("Сообщение подписано!", "Информация о создании ЭЦП", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    btn_VerifyClientData.Enabled = true;
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Информация о создании ЭЦП", MessageBoxButtons.OK, MessageBoxIcon.Warning);                    
                }
            }
            else
            {
                MessageBox.Show("Введите сообщение!", "Информация о сообщении клиента", MessageBoxButtons.OK, MessageBoxIcon.Warning);                
            }            
        }

        public async void VerifyClientDataClick(string clientMsg)
        {
            try
            {
                string message = clientMsg;
                string pbKey = _ds.getPublicKeyPem();
                string response = await SendRequestToServer($"VERIFY_SIGNATURE:{message}:{Convert.ToBase64String(signatureClient)}:{pbKey}");
                if (Convert.ToBoolean(response))
                {
                    MessageBox.Show("Подпись верифицирована!", "Информация о верификации", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    MessageBox.Show("Подпись не верифицирована!", "Информация о верификации", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "Информация о верификации", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        public void VerifyServerDataClick(string serverMsg)
        {
            if (_serverPublicKey.Exponent != null)
            {
                bool isVerified = _ds.VerifyData(serverMsg, signatureServer, _serverPublicKey);
                if (isVerified)
                {
                    MessageBox.Show("Сообщение верифицировано!", "Информация о верификации", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    MessageBox.Show("Сообщение не верифицировано!", "Информация о верификации", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }
            else
            {
                MessageBox.Show("Отсутствует публичный ключ сервера!", "Получение публичного ключа сервера", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        public void DeleteDirectory()
        {
            if (Directory.Exists(FilePath.basePath))
                Directory.Delete(FilePath.basePath, true);
        }        
    }
}
