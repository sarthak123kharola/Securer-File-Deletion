#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

const DWORD SECTOR_SIZE = 512;
const DWORD MFT_ENTRY_SIZE = 1024;

// Converts UTF-16LE string to ASCII (naive, works for simple names)
string wideToAscii(const wchar_t* wstr, int len) {
    string result;
    for (int i = 0; i < len; ++i) {
        result += static_cast<char>(wstr[i] & 0xFF);
    }
    return result;
}

struct DataRun {
    ULONGLONG startCluster;
    ULONGLONG length;
};

// Structure to hold more complete deleted file info
struct DeletedFile {
    string name;
    DWORD recordNumber;
    DWORD flags;
    bool nonResident;
    vector<DataRun> dataRuns;
    ULONGLONG fileSize;
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

        if (offsetSize > 0 && (runList[-1] & 0x80)) // sign extension for negative
            offset |= -((ULONGLONG)1 << (offsetSize * 8));

        lastLCN += offset;
        runs.push_back({ lastLCN, length });
    }
    return 0;
}

void dumpDeletedMFTEntries(const string& driveLetter, const string& outputDump, const string& deletedListFile) {
    string volume = "\\\\.\\" + driveLetter;

    HANDLE hDrive = CreateFileA(volume.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        cerr << "Failed to open volume: " << volume << "\n";
        return;
    }

    BYTE bootSector[SECTOR_SIZE];
    DWORD bytesRead;
    if (!ReadFile(hDrive, bootSector, SECTOR_SIZE, &bytesRead, NULL)) {
        cerr << "Failed to read boot sector.\n";
        CloseHandle(hDrive);
        return;
    }

    DWORD bytesPerSector = *(WORD*)(bootSector + 11);
    DWORD sectorsPerCluster = *(BYTE*)(bootSector + 13);
    INT64 mftClusterNumber = *(INT64*)(bootSector + 48);
    INT64 mftOffset = mftClusterNumber * sectorsPerCluster * bytesPerSector;

    cout << "[*] MFT located at offset: " << mftOffset << "\n";

    LARGE_INTEGER li;
    li.QuadPart = mftOffset;
    SetFilePointerEx(hDrive, li, NULL, FILE_BEGIN);

    ofstream rawOut(outputDump, ios::binary);
    ofstream delOut(deletedListFile);

    vector<DeletedFile> deletedFiles;

    const int maxEntries = 100000;
    BYTE entry[MFT_ENTRY_SIZE];

    for (int i = 0; i < maxEntries; ++i) {
        if (!ReadFile(hDrive, entry, MFT_ENTRY_SIZE, &bytesRead, NULL) || bytesRead < MFT_ENTRY_SIZE)
            break;

        rawOut.write(reinterpret_cast<char*>(entry), MFT_ENTRY_SIZE);

        if (memcmp(entry, "FILE", 4) != 0)
            continue;

        DWORD flags = *(WORD*)(entry + 22);
        if ((flags & 0x01) == 0) {
            WORD attrOffset = *(WORD*)(entry + 20);
            BYTE* p = entry + attrOffset;

            DeletedFile df = {};
            df.recordNumber = i;
            df.flags = flags;
            df.fileSize = 0;
            df.nonResident = false;

            while (*(DWORD*)p != 0xFFFFFFFF) {
                DWORD type = *(DWORD*)p;
                DWORD len = *(DWORD*)(p + 4);
                if (len == 0) break;

                if (type == 0x30 && df.name.empty()) { // $FILE_NAME
                    BYTE nameLen = *(p + 88);
                    wchar_t* namePtr = (wchar_t*)(p + 90);
                    df.name = wideToAscii(namePtr, nameLen);
                }
                else if (type == 0x80) { // $DATA
                    BYTE nonResident = *(p + 8);
                    df.nonResident = (nonResident != 0);

                    if (nonResident) {
                        ULONGLONG logicalSize = *(ULONGLONG*)(p + 0x30);
                        df.fileSize = logicalSize;
                        DWORD runOffset = *(WORD*)(p + 0x20);
                        BYTE* runList = p + runOffset;
                        parseDataRuns(runList, df.dataRuns);
                    } else {
                        ULONGLONG logicalSize = *(DWORD*)(p + 0x10);
                        df.fileSize = logicalSize;
                    }
                }
                p += len;
            }

            if (!df.name.empty())
                deletedFiles.push_back(df);
        }
    }

    for (const auto& file : deletedFiles) {
        delOut << "Deleted File: " << file.name
               << " | Record #: " << file.recordNumber
               << " | Flags: 0x" << hex << file.flags
               << " | Size: " << dec << file.fileSize
               << " | NonResident: " << (file.nonResident ? "Yes" : "No")
               << "\n";
        if (file.nonResident) {
            for (const auto& run : file.dataRuns) {
                delOut << "  -> Cluster: " << run.startCluster << " | Length: " << run.length << "\n";
            }
        }
    }

    cout << "[+] Dumped MFT to: " << outputDump << "\n";
    cout << "[+] Extracted deleted file metadata (incl. $DATA) to: " << deletedListFile << "\n";

    CloseHandle(hDrive);
    rawOut.close();
    delOut.close();
}

int main() {
    string drive = "D:"; // Replace with your target NTFS drive
    dumpDeletedMFTEntries(drive, "mft_dump.bin", "deleted_file_table.txt");
    return 0;
}
