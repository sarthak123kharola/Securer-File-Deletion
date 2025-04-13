#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <cstdio>
#include <windows.h>

using namespace std;

void SecureDelete(string filepath) {
    const size_t chunk = 1024 * 1024;

    ifstream infile(filepath, ios::binary | ios::ate);
    if (!infile) {
        cerr << "Error: File not found. Please try again!\n";
        return;
    }
    streamsize filesize = infile.tellg();
    infile.close();

    if (filesize == 0) {
        cerr << "Error: File is empty. Please try again!\n";
        return;
    }

    random_device rd;
    
    mt19937 gen(rd());
    uniform_int_distribution<unsigned char> dist(0, 255);

    fstream fileout(filepath, ios::binary | ios::in | ios::out);
    if (!fileout) {
        cerr << "Error: Unable to open file for writing!\n";
        return;
    }

    int passes = 7;
    vector<unsigned char> buffer(chunk);
    for (int pass = 0; pass < passes; ++pass) {
        fileout.seekp(0);
        streamsize remaining = filesize;
        while (remaining > 0) {
            size_t toWrite = min(static_cast<streamsize>(chunk), remaining);
            for (size_t i = 0; i < toWrite; ++i) {
                buffer[i] = dist(gen);
            }
            fileout.write(reinterpret_cast<const char*>(buffer.data()), toWrite);
            remaining -= toWrite;
        }
        fileout.flush();
    }

    fileout.close();

    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        FILETIME ft;
        SYSTEMTIME st = { 2000, 1, 0, 1, 0, 0, 0, 0 };
        SystemTimeToFileTime(&st, &ft);
        SetFileTime(hFile, &ft, &ft, &ft);
        CloseHandle(hFile);
    } else {
        cerr << "Warning: Could not clear timestamps.\n";
    }

    string newName = filepath + "_deleted_" + to_string(rd() % 1000000);
    if (rename(filepath.c_str(), newName.c_str()) != 0) {
        cerr << "Warning: Renaming failed. Proceeding with deletion.\n";
        newName = filepath;
    }

    if (remove(newName.c_str()) == 0)
        cout << "File securely deleted with metadata removed!\n";
    else
        cerr << "Error: Failed to delete file. Please try again!\n";
}

int main() {
    while (true) {
        string filepath;
        cout << "Enter file path: ";
        getline(cin, filepath);
        SecureDelete(filepath);
    }
    return 0;
}
