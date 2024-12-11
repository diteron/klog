#pragma once

#include "debug_print.h"
#include "public.h"

#include <ntddk.h>
#include <wdf.h>
#include <kbdmou.h>
#include <ntddkbd.h>
#include <ntdd8042.h>
#include <initguid.h>
#include <devguid.h>

#define KBFILTER_POOL_TAG (ULONG) 'tlfK'

typedef struct DEVICE_EXTENSION_T {
    WDFDEVICE WdfDevice;
    CONNECT_DATA UpperConnectData;

    PVOID UpperContext;
    PI8042_KEYBOARD_INITIALIZATION_ROUTINE UpperInitializationRoutine;
    PI8042_KEYBOARD_ISR UpperIsrHook;

    IN PVOID CallContext;
    WDFWORKITEM workItem;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                   GetDeviceExtension)

typedef struct _KEYBOARD_INPUT_BUFFER {
    KEYBOARD_INPUT_DATA InputData[KEYBOARD_INP_BUFFER_SIZE];
    ULONG Tail;

    WDFSPINLOCK SpinLock;
    PKEVENT DataAddedEvent;
} KEYBOARD_INPUT_BUFFER, *PKEYBOARD_INPUT_BUFFER;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD Klog_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL Klog_EvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Klog_EvtIoDeviceControl;

NTSTATUS InitKeyboardInputBuffer();
NTSTATUS InitNotificationEvent();
VOID Klog_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart,
                          IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed);
VOID AddKeyboardInputDataToBuffer(PKEYBOARD_INPUT_DATA Entry);
VOID NotifyUserModeApp();
NTSTATUS ReadKeyboardInputDataFromBuffer(OUT WDFMEMORY Destination, OUT size_t* BytesRead);

// Used to identify kbfilter bus. This guid is used as the enumeration string
// for the device id.
DEFINE_GUID(GUID_BUS_KBFILTER,
            0xa65c87f9, 0xbe02, 0x4ed9, 0x92, 0xec, 0x1, 0x2d, 0x41, 0x61, 0x69, 0xfa);
// {A65C87F9-BE02-4ed9-92EC-012D416169FA}

DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
            0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);
// {3FB7299D-6847-4490-B0C9-99E0986AB886}

#define KBFILTR_DEVICE_ID L"{A65C87F9-BE02-4ed9-92EC-012D416169FA}\\KeyboardFilter\0"
