#include "KbInputDataProcessor.h"

#include <iostream>

int main(int argc, const char* argv)
{   
    std::wcout.imbue(std::locale("en_US.UTF-8"));
    std::wcin.imbue(std::locale("en_US.UTF-8"));

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    std::wcout << L"Hello ÌÈÐ!\n\n";

    KbInputDataProcessor inpDataProcessor{std::wcout};
    if (!inpDataProcessor.isSetup()) {
        std::cerr << "Failed to setup KbInputDataProcessor\n";
        return 1;
    }

    inpDataProcessor.start();

    return 0;
}
