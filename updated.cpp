#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <limits>

using namespace std;

const DWORD SECTOR_SIZE = 512;
const DWORD MFT_ENTRY_SIZE = 1024;
const int MAX_MFT_ENTRIES = 1000000;

string wideToAscii(const wchar_t* wstr, int len) {
    string result;
    for (int i = 0; i < len && wstr[i] != L'\0'; ++i) {
        if (wstr[i] <= 127) {
            result += static_cast<char>(wstr[i]);
        }
    }
    return result;
}

struct DataRun {
    ULONGLONG startCluster;
    ULONGLONG length;
};

struct DeletedFile {
    string name;
    DWORD recordNumber;
    DWORD flags;
    bool isDeleted;
    bool nonResident;
    vector<DataRun> dataRuns;
    ULONGLONG fileSize;
    string parentPath;
};

ULONGLONG parseDataRuns(BYTE* runList, vector<DataRun>& runs) {
    ULONGLONG lastLCN = 0;
    while (*runList != 0x00) {
        BYTE header = *runList++;
        BYTE lenSize = header & 0x0F;
        BYTE offsetSize = (header >> 4) & 0x0F;

        ULONGLONG length = 0;
        for (int i = 0; i < lenSize; ++i)
            length |= ((ULONGLONG)(*runList++) << (i * 8));

        LONGLONG offset = 0;
        for (int i = 0; i < offsetSize; ++i)
            offset |= ((ULONGLONG)(*runList++) << (i * 8));

        if (offsetSize > 0 && (runList[-1] & 0x80))
            offset |= -((ULONGLONG)1 << (offsetSize * 8));

        lastLCN += offset;
        runs.push_back({ lastLCN, length });
    }
    return 0;
}

string parseResidentData(BYTE* attribute, DWORD attrLength) {
    if (attrLength < 24) return "";
    DWORD contentSize = *(DWORD*)(attribute + 16);
    DWORD contentOffset = *(WORD*)(attribute + 20);
    if (contentOffset + contentSize > attrLength) return "";
    return string(reinterpret_cast<char*>(attribute + contentOffset), contentSize);
}

void dumpDeletedMFTEntries(const string& driveLetter, const string& outputDump, const string& deletedListFile) {
    string volume = "\\\\.\\" + driveLetter;
    string logicalPath = driveLetter + "\\\\";
    cout << "[*] Scanning volume: " << volume << endl;

    HANDLE hDrive = CreateFileA(volume.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        cerr << "[-] Failed to open volume. Error: " << GetLastError() << endl;
        return;
    }

    DWORD sectorsPerCluster = 0, bytesPerSector = 0;
    if (!GetDiskFreeSpaceA(logicalPath.c_str(), &sectorsPerCluster, &bytesPerSector, NULL, NULL)) {
        cerr << "[-] Failed to get disk geometry. Error: " << GetLastError() << endl;
        CloseHandle(hDrive);
        return;
    }
    cout << "[*] Bytes per sector: " << bytesPerSector << endl;
    cout << "[*] Sectors per cluster: " << sectorsPerCluster << endl;

    BYTE bootSector[SECTOR_SIZE];
    DWORD bytesRead = 0;
    if (!ReadFile(hDrive, bootSector, SECTOR_SIZE, &bytesRead, NULL)) {
        cerr << "[-] Failed to read boot sector. Error: " << GetLastError() << endl;
        CloseHandle(hDrive);
        return;
    }

    INT64 mftClusterNumber = *(INT64*)(bootSector + 48);
    INT64 mftOffset = mftClusterNumber * sectorsPerCluster * bytesPerSector;
    cout << "[*] MFT located at offset: " << mftOffset << endl;

    LARGE_INTEGER li;
    li.QuadPart = mftOffset;
    if (!SetFilePointerEx(hDrive, li, NULL, FILE_BEGIN)) {
        cerr << "[-] Failed to seek to MFT. Error: " << GetLastError() << endl;
        CloseHandle(hDrive);
        return;
    }

    ofstream rawOut(outputDump, ios::binary);
    ofstream delOut(deletedListFile);
    if (!delOut.is_open()) {
        cerr << "[-] Failed to open output metadata file." << endl;
        CloseHandle(hDrive);
        rawOut.close();
        return;
    }

    vector<DeletedFile> deletedFiles;
    auto startTime = chrono::steady_clock::now();

    BYTE entry[MFT_ENTRY_SIZE];
    for (int i = 0; i < MAX_MFT_ENTRIES; ++i) {
        if (i % 1000 == 0) {
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(now - startTime).count();
            cout << "\r[*] Scanning entry #" << i << " (" << elapsed << "s elapsed)" << flush;
        }

        if (!ReadFile(hDrive, entry, MFT_ENTRY_SIZE, &bytesRead, NULL)) {
            cerr << "\n[-] Failed to read MFT entry #" << i << ". Error: " << GetLastError() << endl;
            break;
        }

        if (bytesRead < MFT_ENTRY_SIZE) {
            cerr << "\n[!] MFT entry #" << i << " read less than expected: " << bytesRead << " bytes." << endl;
            break;
        }

        rawOut.write(reinterpret_cast<char*>(entry), MFT_ENTRY_SIZE);

        if (memcmp(entry, "FILE", 4) != 0) {
            continue;
        }

        DWORD flags = *(WORD*)(entry + 22);
        bool isDeleted = (flags & 0x01) == 0;

        WORD attrOffset = *(WORD*)(entry + 20);
        BYTE* p = entry + attrOffset;

        DeletedFile df = {};
        df.recordNumber = i;
        df.flags = flags;
        df.isDeleted = isDeleted;
        df.fileSize = 0;
        df.nonResident = false;

        while (*(DWORD*)p != 0xFFFFFFFF) {
            DWORD type = *(DWORD*)p;
            DWORD len = *(DWORD*)(p + 4);
            if (len == 0) break;

            if (type == 0x30) {
                BYTE nameLen = *(p + 88);
                wchar_t* namePtr = (wchar_t*)(p + 90);
                df.name = wideToAscii(namePtr, nameLen);
                ULONGLONG parentRef = *(ULONGLONG*)(p + 24);
                df.parentPath = "Parent MFT: " + to_string(parentRef);
            }
            else if (type == 0x80) {
                BYTE nonResident = *(p + 8);
                df.nonResident = (nonResident != 0);

                if (nonResident) {
                    df.fileSize = *(ULONGLONG*)(p + 0x30);
                    DWORD runOffset = *(WORD*)(p + 0x20);
                    parseDataRuns(p + runOffset, df.dataRuns);
                } else {
                    df.fileSize = *(DWORD*)(p + 0x10);
                }
            }
            p += len;
        }

        if (!df.name.empty() && df.isDeleted) {
            deletedFiles.push_back(df);
        }
    }

    cout << "\n[+] Finished scanning. Deleted files found: " << deletedFiles.size() << endl;

    if (deletedFiles.empty()) {
        delOut << "[-] No deleted files were found on this drive.\n";
        delOut << "[*] Make sure it's an NTFS volume with undeleted MFT entries.\n";
        cout << "[-] No deleted files found. Try another drive or verify NTFS format.\n";
    } else {
        delOut << "Deleted Files Report\n";
        delOut << "Drive: " << driveLetter << "\n";
        delOut << "Total Deleted Files Found: " << deletedFiles.size() << "\n\n";

        for (const auto& file : deletedFiles) {
            delOut << "Record #: " << file.recordNumber << "\n";
            delOut << "Name: " << file.name << "\n";
            delOut << "Parent: " << file.parentPath << "\n";
            delOut << "Size: " << file.fileSize << " bytes\n";
            delOut << "Status: Deleted\n";
            delOut << "Storage: " << (file.nonResident ? "Non-Resident" : "Resident") << "\n";
            if (file.nonResident && !file.dataRuns.empty()) {
                delOut << "Data Runs:\n";
                for (const auto& run : file.dataRuns) {
                    delOut << "  -> Cluster: " << run.startCluster << " | Length: " << run.length << " clusters\n";
                }
            }
            delOut << "----------------------------------------\n";
        }
    }

    CloseHandle(hDrive);
    rawOut.close();
    delOut.close();

    cout << "[+] Raw MFT dumped to: " << outputDump << endl;
    cout << "[+] Deleted files list saved to: " << deletedListFile << endl;
}

int main() {
    cout << "NTFS MFT Parser - Debug Version\n";
    cout << "---------------------------------\n";

    string drive;
    cout << "Enter drive letter to scan (e.g., C, D): ";
    cin >> drive;
    drive += ":";

    string timestamp = to_string(time(nullptr));
    string dumpFile = "mft_dump_" + timestamp + ".bin";
    string listFile = "deleted_files_" + timestamp + ".txt";

    dumpDeletedMFTEntries(drive, dumpFile, listFile);

    cout << "\nPress Enter to exit...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();

    return 0;
}