namespace Client
{
    public partial class ClientForm : Form
    {
        private ILogic _client;
        public ClientForm(ILogic client)
        {
            _client = client;
            InitializeComponent();
            lbl_ClientGUID.Text = _client.Init();
        }

        private async void btn_GenerateMessage_Click(object sender, EventArgs e)
        {
            (rtb_serverMsg.Text, btn_VerifyServerData.Enabled) = await _client.GenerateMessageClick();
        }

        private void btn_GetServerPublicKey_Click(object sender, EventArgs e)
        {
            _client.GetServerPublicKeyClick();
        }


        private void btn_SignData_Click(object sender, EventArgs e)
        {
            _client.SignDataClick(rtb_clientMsg.Text, btn_VerifyClientData);
        }

        private void btn_VerifyClientData_Click(object sender, EventArgs e)
        {
            _client.VerifyClientDataClick(rtb_clientMsg.Text);
        }

        private void btn_VerifyServerData_Click(object sender, EventArgs e)
        {
            _client.VerifyServerDataClick(rtb_serverMsg.Text);
        }

        private void ClientForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            _client.DeleteDirectory();
        }

        private void groupBoxClient_Enter(object sender, EventArgs e)
        {

        }

        private void ClientForm_Load(object sender, EventArgs e)
        {

        }
    }
}
