namespace SecureFileWipeGUI
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;
        private System.Windows.Forms.Button btnSelectFile;
        private System.Windows.Forms.Button btnWipeFile;
        private System.Windows.Forms.TextBox txtFilePath;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private void InitializeComponent()
        {
            this.btnSelectFile = new System.Windows.Forms.Button();
            this.btnWipeFile = new System.Windows.Forms.Button();
            this.txtFilePath = new System.Windows.Forms.TextBox();
            this.SuspendLayout();

            // txtFilePath
            this.txtFilePath.Location = new System.Drawing.Point(12, 12);
            this.txtFilePath.Size = new System.Drawing.Size(360, 22);
            this.txtFilePath.ReadOnly = true;

            // btnSelectFile
            this.btnSelectFile.Location = new System.Drawing.Point(380, 10);
            this.btnSelectFile.Size = new System.Drawing.Size(100, 25);
            this.btnSelectFile.Text = "Select File";
            this.btnSelectFile.Click += new System.EventHandler(this.btnSelectFile_Click);

            // btnWipeFile
            this.btnWipeFile.Location = new System.Drawing.Point(200, 50);
            this.btnWipeFile.Size = new System.Drawing.Size(100, 25);
            this.btnWipeFile.Text = "Wipe File";
            this.btnWipeFile.Click += new System.EventHandler(this.btnWipeFile_Click);

            // MainForm
            this.ClientSize = new System.Drawing.Size(500, 100);
            this.Controls.Add(this.txtFilePath);
            this.Controls.Add(this.btnSelectFile);
            this.Controls.Add(this.btnWipeFile);
            this.Text = "Secure File Wiper";
            this.ResumeLayout(false);
            this.PerformLayout();
        }
    }
}
