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

typedef struct DEVICE_EXTENSION_T {
    WDFDEVICE       WdfDevice;
    WDFQUEUE        rawPdoQueue;
    CONNECT_DATA    UpperConnectData;
    HANDLE          NotifEventHandle;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION,
                                   GetDeviceExtension)

typedef struct KEYBOARD_INPUT_BUFFER_T {
    KEYBOARD_INPUT_DATA     InputData[KEYBOARD_INP_BUFFER_SIZE];
    ULONG                   Tail;
    WDFSPINLOCK             SpinLock;
    PKEVENT                 DataAddedEvent;
} KEYBOARD_INPUT_BUFFER, *PKEYBOARD_INPUT_BUFFER;

typedef struct RPDO_DEVICE_DATA_T {
    ULONG       InstanceNo;
    WDFQUEUE    ParentQueue;
} RPDO_DEVICE_DATA, *PRPDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RPDO_DEVICE_DATA, PdoGetData)

DRIVER_INITIALIZE           DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD   Klog_EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL  Klog_EvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL           Klog_EvtIoDeviceControlForRawPdo;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL           Klog_EvtIoDeviceControlFromRawPdo;

NTSTATUS    Klog_CreateRawPdo(WDFDEVICE Device, ULONG InstanceNo);
NTSTATUS    InitKeyboardInputBuffer();
NTSTATUS    InitNotificationEvent(PDEVICE_EXTENSION deviceExtension);
VOID        Klog_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart,
                                 IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed);
VOID        AddKeyboardInputDataToBuffer(PKEYBOARD_INPUT_DATA Entry);
VOID        NotifyUserModeApp();
NTSTATUS    ReadKeyboardInputDataFromBuffer(OUT WDFMEMORY Destination, OUT size_t* BytesRead);

// {2BC64703-9672-4D43-9142-F444592D836C}
DEFINE_GUID(GUID_DEVINTERFACE_KLOG,
            0x2bc64703, 0x9672, 0x4d43, 0x91, 0x42, 0xf4, 0x44, 0x59, 0x2d, 0x83, 0x6c);
