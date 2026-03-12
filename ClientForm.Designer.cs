using static System.Net.Mime.MediaTypeNames;
using System.Windows.Forms;
using System.Xml.Linq;

namespace Client
{
    partial class ClientForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        private void InitializeComponent()
        {
            rtb_clientMsg = new RichTextBox();
            btn_SignData = new Button();
            btn_GenerateMessage = new Button();
            btn_VerifyClientData = new Button();
            lbl_ClientGUID = new Label();
            groupBoxClient = new GroupBox();
            btn_GetServerPublicKey = new Button();
            btn_VerifyServerData = new Button();
            rtb_serverMsg = new RichTextBox();
            groupBoxServer = new GroupBox();
            groupBoxClient.SuspendLayout();
            groupBoxServer.SuspendLayout();
            SuspendLayout();
            // 
            // rtb_clientMsg
            // 
            rtb_clientMsg.Location = new Point(26, 34);
            rtb_clientMsg.Margin = new Padding(3, 4, 3, 4);
            rtb_clientMsg.Name = "rtb_clientMsg";
            rtb_clientMsg.Size = new Size(342, 42);
            rtb_clientMsg.TabIndex = 0;
            rtb_clientMsg.Text = "";
            // 
            // btn_SignData
            // 
            btn_SignData.Location = new Point(26, 84);
            btn_SignData.Margin = new Padding(3, 4, 3, 4);
            btn_SignData.Name = "btn_SignData";
            btn_SignData.Size = new Size(342, 37);
            btn_SignData.TabIndex = 2;
            btn_SignData.Text = "Подписать сообщение";
            btn_SignData.UseVisualStyleBackColor = true;
            btn_SignData.Click += btn_SignData_Click;
            // 
            // btn_GenerateMessage
            // 
            btn_GenerateMessage.Location = new Point(26, 129);
            btn_GenerateMessage.Margin = new Padding(3, 4, 3, 4);
            btn_GenerateMessage.Name = "btn_GenerateMessage";
            btn_GenerateMessage.Size = new Size(342, 36);
            btn_GenerateMessage.TabIndex = 4;
            btn_GenerateMessage.Text = "Сгенерировать сообщение";
            btn_GenerateMessage.UseVisualStyleBackColor = true;
            btn_GenerateMessage.Click += btn_GenerateMessage_Click;
            // 
            // btn_VerifyClientData
            // 
            btn_VerifyClientData.Enabled = false;
            btn_VerifyClientData.Location = new Point(26, 173);
            btn_VerifyClientData.Margin = new Padding(3, 4, 3, 4);
            btn_VerifyClientData.Name = "btn_VerifyClientData";
            btn_VerifyClientData.Size = new Size(342, 36);
            btn_VerifyClientData.TabIndex = 6;
            btn_VerifyClientData.Text = "Верификация клиента";
            btn_VerifyClientData.UseVisualStyleBackColor = true;
            btn_VerifyClientData.Click += btn_VerifyClientData_Click;
            // 
            // lbl_ClientGUID
            // 
            lbl_ClientGUID.AutoSize = true;
            lbl_ClientGUID.Dock = DockStyle.Bottom;
            lbl_ClientGUID.Location = new Point(0, 199);
            lbl_ClientGUID.Name = "lbl_ClientGUID";
            lbl_ClientGUID.Size = new Size(0, 20);
            lbl_ClientGUID.TabIndex = 9;
            // 
            // groupBoxClient
            // 
            groupBoxClient.Controls.Add(rtb_clientMsg);
            groupBoxClient.Controls.Add(btn_SignData);
            groupBoxClient.Controls.Add(btn_VerifyClientData);
            groupBoxClient.Controls.Add(btn_GenerateMessage);
            groupBoxClient.Location = new Point(0, 0);
            groupBoxClient.Margin = new Padding(3, 4, 3, 4);
            groupBoxClient.Name = "groupBoxClient";
            groupBoxClient.Padding = new Padding(3, 4, 3, 4);
            groupBoxClient.Size = new Size(389, 257);
            groupBoxClient.TabIndex = 10;
            groupBoxClient.TabStop = false;
            groupBoxClient.Text = "Клиент";
            groupBoxClient.Enter += groupBoxClient_Enter;
            // 
            // btn_GetServerPublicKey
            // 
            btn_GetServerPublicKey.Location = new Point(22, 130);
            btn_GetServerPublicKey.Margin = new Padding(3, 4, 3, 4);
            btn_GetServerPublicKey.Name = "btn_GetServerPublicKey";
            btn_GetServerPublicKey.Size = new Size(342, 35);
            btn_GetServerPublicKey.TabIndex = 5;
            btn_GetServerPublicKey.Text = "Получить ключ сервера";
            btn_GetServerPublicKey.UseVisualStyleBackColor = true;
            btn_GetServerPublicKey.Click += btn_GetServerPublicKey_Click;
            // 
            // btn_VerifyServerData
            // 
            btn_VerifyServerData.Enabled = false;
            btn_VerifyServerData.Location = new Point(22, 84);
            btn_VerifyServerData.Margin = new Padding(3, 4, 3, 4);
            btn_VerifyServerData.Name = "btn_VerifyServerData";
            btn_VerifyServerData.Size = new Size(342, 37);
            btn_VerifyServerData.TabIndex = 3;
            btn_VerifyServerData.Text = "Верификация сервера";
            btn_VerifyServerData.UseVisualStyleBackColor = true;
            btn_VerifyServerData.Click += btn_VerifyServerData_Click;
            // 
            // rtb_serverMsg
            // 
            rtb_serverMsg.Location = new Point(22, 34);
            rtb_serverMsg.Margin = new Padding(3, 4, 3, 4);
            rtb_serverMsg.Name = "rtb_serverMsg";
            rtb_serverMsg.Size = new Size(342, 42);
            rtb_serverMsg.TabIndex = 1;
            rtb_serverMsg.Text = "";
            // 
            // groupBoxServer
            // 
            groupBoxServer.Controls.Add(rtb_serverMsg);
            groupBoxServer.Controls.Add(btn_VerifyServerData);
            groupBoxServer.Controls.Add(btn_GetServerPublicKey);
            groupBoxServer.Location = new Point(406, 0);
            groupBoxServer.Margin = new Padding(3, 4, 3, 4);
            groupBoxServer.Name = "groupBoxServer";
            groupBoxServer.Padding = new Padding(3, 4, 3, 4);
            groupBoxServer.Size = new Size(413, 257);
            groupBoxServer.TabIndex = 11;
            groupBoxServer.TabStop = false;
            groupBoxServer.Text = "Сервер";
            // 
            // ClientForm
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(791, 219);
            Controls.Add(groupBoxServer);
            Controls.Add(groupBoxClient);
            Controls.Add(lbl_ClientGUID);
            Margin = new Padding(3, 4, 3, 4);
            MaximizeBox = false;
            Name = "ClientForm";
            StartPosition = FormStartPosition.CenterScreen;
            Text = "Электронно-цифровая подпись";
            FormClosing += ClientForm_FormClosing;
            Load += ClientForm_Load;
            groupBoxClient.ResumeLayout(false);
            groupBoxServer.ResumeLayout(false);
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private RichTextBox rtb_clientMsg;
        private Button btn_SignData;
        private Button btn_GenerateMessage;
        private Button btn_VerifyClientData;
        private Label lbl_ClientGUID;
        private GroupBox groupBoxClient;
        private Button btn_GetServerPublicKey;
        private Button btn_VerifyServerData;
        private RichTextBox rtb_serverMsg;
        private GroupBox groupBoxServer;
    }
}