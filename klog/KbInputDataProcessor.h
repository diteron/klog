#pragma once

#include "KbMakeCodeConverter.h"
#include "klog_driver/public.h"

#include <ostream>

#include <initguid.h>
#include <ntddkbd.h>
#pragma warning(disable:4201)
#include <setupapi.h>
#include <winioctl.h>
#pragma warning(default:4201)

// {2BC64703-9672-4D43-9142-F444592D836C}
DEFINE_GUID(GUID_DEVINTERFACE_KLOG,
            0x2bc64703, 0x9672, 0x4d43, 0x91, 0x42, 0xf4, 0x44, 0x59, 0x2d, 0x83, 0x6c);

class KbInputDataProcessor {
public:
    KbInputDataProcessor(std::wostream& out_, bool isVerboseMode = false);
    KbInputDataProcessor(const KbInputDataProcessor&) = delete;
    KbInputDataProcessor& operator=(KbInputDataProcessor&) = delete;

    ~KbInputDataProcessor();

    bool isSetup() const;
    void start();

private:
    bool setupHardwareDeviceInfo();
    bool findKlogDeviceInterface();
    bool connectToKlogDriver();

    void printInputData();
    void printInputDataVerbose();

    DWORD getForegroundWindowThreadProcessId();
    void determineKeboardState(KEYBOARD_INPUT_DATA inpData);

    enum Key {
        LShift = 0x2A,
        RShift = 0x36,
        CapsLock = 0x3A,
        NumLock = 0x45
    };

    bool                                isVerboseMode = false;
    bool                                isAllSetup = false;
    KEYBOARD_INPUT_DATA                 inpData[KEYBOARD_INP_BUFFER_SIZE] = {0};
    HDEVINFO                            hardwareDeviceInfo = NULL;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0, bytesReturned = 0;
    HANDLE                              device = NULL;
    HANDLE                              inputDataAddedEvent = NULL;
    std::wostream&                      out;

    DWORD                               currentThreadId;
    DWORD                               currentProcessId;
    KbMakeCodeConverter                 kbMakeCodeConverted;
    bool                                isShiftPressed = false;
    bool                                isCapsLockEnabled = false;
    bool                                isNumLockEnabled = false;
};

