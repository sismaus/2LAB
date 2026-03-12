using System.Security.Cryptography;

namespace Client
{
    public interface ILogic
    {
        public Task<string> SendRequestToServer(string request);
        public string Init();
        public static RSAParameters DeserializeKeyPem(string key) => throw new NotImplementedException();
        public void GenerateKeys();
        public Task<(string, bool)> GenerateMessageClick();
        public void GetServerPublicKeyClick();
        public void SignDataClick(string clientMsg, Button btn_VerifyClientData);
        public void VerifyClientDataClick(string clientMsg);
        public void VerifyServerDataClick(string serverMsg);
        public void DeleteDirectory();
    }
}
