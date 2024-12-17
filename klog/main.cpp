#include "KbInputDataProcessor.h"

#include <iostream>
#include <format>

int main(int argc, const char* argv)
{   
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(std::locale("en_US.UTF-8"));

    // Forbid running multiple instances
    const WCHAR instanceMutexName[] = L"KlogInstMutex";
    HANDLE instanceMutex = CreateMutexW(NULL, TRUE, instanceMutexName);
    if (!instanceMutex) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "klog is already running\n";
            return 1;
        }
        else {
            std::cerr << std::format("Failed to open instance mutex. Error code {:#X}\n",
                                     GetLastError());
            return 1;
        }
    }

    KbInputDataProcessor inpDataProcessor{std::wcout};
    if (!inpDataProcessor.isSetup()) {
        std::cerr << "Failed to setup KbInputDataProcessor\n";
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
        return 1;
    }

    inpDataProcessor.start();

    ReleaseMutex(instanceMutex);
    CloseHandle(instanceMutex);

    return 0;
}
