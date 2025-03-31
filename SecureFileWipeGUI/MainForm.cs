using System;
using System.IO;
using System.Security.Cryptography;
using System.Windows.Forms;

namespace SecureFileWipeGUI
{
    public partial class MainForm : Form
    {
        public MainForm()
        {
            InitializeComponent();
        }

        private void btnSelectFile_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();
            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                txtFilePath.Text = openFileDialog.FileName;
            }
        }

        private void btnWipeFile_Click(object sender, EventArgs e)
        {
            string filePath = txtFilePath.Text;
            if (File.Exists(filePath))
            {
                try
                {
                    SecureDelete(filePath);
                    MessageBox.Show("File securely wiped!", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            else
            {
                MessageBox.Show("File not found!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void SecureDelete(string filePath)
        {
            FileInfo fileInfo = new FileInfo(filePath);
            int passes = 3; // Overwrite 3 times for security
            byte[] randomData = new byte[fileInfo.Length];
            RandomNumberGenerator rng = RandomNumberGenerator.Create();

            for (int pass = 0; pass < passes; pass++)
            {
                rng.GetBytes(randomData);
                File.WriteAllBytes(filePath, randomData);
            }

            string randomFileName = Path.Combine(fileInfo.DirectoryName, Path.GetRandomFileName());
            File.Move(filePath, randomFileName);
            File.Delete(randomFileName);
        }
    }
}
