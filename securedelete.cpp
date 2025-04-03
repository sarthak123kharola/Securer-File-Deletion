#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <cstdio>

using namespace std;

void SecureDelete(const string &filePath)
{
    ifstream file(filePath, ios::binary | ios::ate);
    if (!file)
    {
        cerr << "Error: File not found!\n";
        return;
    }
    streamsize fileSize = file.tellg();
    file.close();
    if (fileSize == 0)
    {
        cerr << "Error: File is empty!\n";
        return;
    }
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<unsigned char> dist(0, 255);
    ofstream fileOut(filePath, ios::binary);
    if (!fileOut)
    {
        cerr << "Error: Unable to open file for writing!\n";
        return;
    }
    int passes = 7;
    vector<unsigned char> buffer(fileSize);
    for (int pass = 0; pass < passes; ++pass)
    {
        for (auto &byte : buffer)
        {
            byte = dist(gen);
        }
        fileOut.seekp(0);
        fileOut.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        fileOut.flush();
    }
    fileOut.close();
    if (remove(filePath.c_str()) == 0)
        cout << "File securely deleted!\n";
    else
        cerr << "Error: Failed to delete file!\n";
}

int main()
{
    string filePath;
    cout << "Enter file path: ";
    getline(cin, filePath);
    SecureDelete(filePath);
    return 0;
}
