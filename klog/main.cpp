#include "KbInputDataProcessor.h"

#include <iostream>
#include <fstream>
#include <format>

void setConsoleEncoding();
bool isParamsCorrect(int argc, const wchar_t* argv[]);
bool isOutputInFileNeeded(int argc);
std::wstring getFileName(const wchar_t* argv[]);
HANDLE createProgInstanceMutex();

int wmain(int argc, const wchar_t* argv[])
{   
    setConsoleEncoding();

    if (!isParamsCorrect(argc, argv)) {
        return 1;
    }

    bool            isOutputInFile = isOutputInFileNeeded(argc);
    std::wstring    fileName;
    std::wofstream  outFile;

    if (isOutputInFile) {
        fileName = getFileName(argv);
        outFile.open(fileName, std::ios::app);
        if (!outFile.is_open()) {
            std::wcerr << L"Failed to open file " << fileName << L"\n";
            return 1;
        }
    }

    HANDLE instanceMutex = createProgInstanceMutex();
    if (!instanceMutex) {
        return 1;
    }

    std::wostream& out = isOutputInFile ? outFile : std::wcout;
    KbInputDataProcessor inpDataProcessor{out};
    if (!inpDataProcessor.isSetup()) {
        std::wcerr << L"Failed to setup KbInputDataProcessor\n";
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
        return 1;
    }

    std::wcout << L"klog started\n\n";
    inpDataProcessor.start();
    std::wcout << L"klog stopped\n";

    ReleaseMutex(instanceMutex);
    CloseHandle(instanceMutex);

    return 0;
}

inline void setConsoleEncoding()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(std::locale("en_US.UTF-8"));
}

inline bool isParamsCorrect(int argc, const wchar_t* argv[])
{
    if (argc == 3 && (std::wcscmp(argv[1], L"-f") == 0 || std::wcscmp(argv[1], L"--file") == 0)) {
        return true;
    }
    else if (argc != 1) {
        std::wcerr << L"Usage: " << argv[0] << L" [-f|--file file_name]\n";
        return false;
    }

    return true;
}

inline bool isOutputInFileNeeded(int argc)
{
    return argc == 3;
}

inline std::wstring getFileName(const wchar_t* argv[])
{
    return std::wstring(argv[2]);
}

inline HANDLE createProgInstanceMutex()
{
    const WCHAR instanceMutexName[] = L"KlogInstMutex";
    HANDLE instanceMutex = CreateMutexW(NULL, TRUE, instanceMutexName);
    if (!instanceMutex) {
        std::wcerr << std::format(L"Failed to create instance mutex. Error code 0x{:X}\n",
                                  GetLastError());
        return NULL;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {       // Forbid running multiple instances of the program
        std::wcerr << L"klog is already running\n";
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
        return NULL;
    }

    return instanceMutex;
}
