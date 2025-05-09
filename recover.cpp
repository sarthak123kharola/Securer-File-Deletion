#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define CLUSTER_SIZE 4096 // Adjust based on actual cluster size

struct ClusterInfo {
    ULONGLONG startLCN;
    ULONGLONG clusterCount;
    DWORD fileSize;
};

// Reads the raw data from physical drive
bool ReadRawClusters(const ClusterInfo& info, const std::string& outputPath) {
    std::string physicalDrivePath = "\\\\.\\PhysicalDrive0"; // Adjust for your target drive
    HANDLE hDrive = CreateFileA(
        physicalDrivePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDrive == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open physical drive. Error: " << GetLastError() << "\n";
        return false;
    }

    ULONGLONG byteOffset = info.startLCN * CLUSTER_SIZE;
    DWORD totalBytes = static_cast<DWORD>(info.clusterCount * CLUSTER_SIZE);

    LARGE_INTEGER offset;
    offset.QuadPart = byteOffset;
    SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN);

    std::vector<char> buffer(totalBytes);
    DWORD bytesRead = 0;
    if (!ReadFile(hDrive, buffer.data(), totalBytes, &bytesRead, NULL)) {
        std::cerr << "Failed to read raw disk data. Error: " << GetLastError() << "\n";
        CloseHandle(hDrive);
        return false;
    }

    // Save recovered data
    std::ofstream out(outputPath, std::ios::binary);
    out.write(buffer.data(), info.fileSize); // write only original size
    out.close();

    std::cout << "Recovered data written to: " << outputPath << "\n";
    CloseHandle(hDrive);
    return true;
}

ClusterInfo GetFileClusterInfo(const std::string& filePath) {
    ClusterInfo info = {};

    HANDLE hFile = CreateFileA(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file. Error: " << GetLastError() << "\n";
        return info;
    }

    LARGE_INTEGER fileSize = {};
    GetFileSizeEx(hFile, &fileSize);
    info.fileSize = static_cast<DWORD>(fileSize.QuadPart);

    STARTING_VCN_INPUT_BUFFER input = {};
    input.StartingVcn.QuadPart = 0;

    // Dynamically allocate output buffer
    DWORD outBufferSize = sizeof(RETRIEVAL_POINTERS_BUFFER) + sizeof(RETRIEVAL_POINTERS_BUFFER) * 10;
    BYTE* buffer = new BYTE[outBufferSize];
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
        hFile,
        FSCTL_GET_RETRIEVAL_POINTERS,
        &input,
        sizeof(input),
        buffer,
        outBufferSize,
        &bytesReturned,
        NULL)) {
        std::cerr << "DeviceIoControl failed. Error: " << GetLastError() << "\n";
        delete[] buffer;
        CloseHandle(hFile);
        return info;
    }

    RETRIEVAL_POINTERS_BUFFER* output = reinterpret_cast<RETRIEVAL_POINTERS_BUFFER*>(buffer);
    if (output->ExtentCount > 0) {
        info.startLCN = output->Extents[0].Lcn.QuadPart;
        info.clusterCount = output->Extents[0].NextVcn.QuadPart - output->StartingVcn.QuadPart;
    }

    delete[] buffer;
    CloseHandle(hFile);
    return info;
}

int main() {
    std::string originalPath;
    std::cout << "Enter full path to file you want to test: ";
    std::getline(std::cin, originalPath);

    // Step 1: Get file cluster info
    ClusterInfo info = GetFileClusterInfo(originalPath);
    if (info.startLCN == 0 || info.clusterCount == 0) {
        std::cerr << "Failed to retrieve file cluster info.\n";
        return 1;
    }

    std::cout << "Cluster Start: " << info.startLCN << ", Count: " << info.clusterCount << "\n";
    std::cout << "File Size: " << info.fileSize << " bytes\n";

    // Step 2: Delete file
    if (!DeleteFileA(originalPath.c_str())) {
        std::cerr << "Failed to delete file. Error: " << GetLastError() << "\n";
        return 1;
    }
    std::cout << "File deleted.\n";

    // Step 3: Recover
    std::string outputPath = "recovered_file.bin";
    if (!ReadRawClusters(info, outputPath)) {
        std::cerr << "Failed to recover file.\n";
        return 1;
    }

    return 0;
}
