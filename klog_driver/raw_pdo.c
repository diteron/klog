#include "klog.h"
#include "public.h"

#define ID_MAX_LEN 128

NTSTATUS Klog_CreateRawPdo(WDFDEVICE Device, ULONG InstanceNo)
{
    DebugPrintInfo("Entered Klog_CreateRawPdo\n");

    NTSTATUS                        status;
    PWDFDEVICE_INIT                 deviceInit = NULL;
    PRPDO_DEVICE_DATA               pdoData = NULL;
    WDFDEVICE                       pdoDevice = NULL;
    WDF_OBJECT_ATTRIBUTES           pdoAttributes;
    WDF_DEVICE_PNP_CAPABILITIES     pnpCaps;
    WDF_IO_QUEUE_CONFIG             ioQueueConfig;
    WDFQUEUE                        queue;
    WDF_DEVICE_STATE                deviceState;
    PDEVICE_EXTENSION               deviceExtension;

    DECLARE_CONST_UNICODE_STRING(deviceId, KLOG_DEVICE_ID);
    DECLARE_CONST_UNICODE_STRING(hardwareId, KLOG_DEVICE_ID);
    DECLARE_CONST_UNICODE_STRING(deviceLocation, L"Keyboard Filter\0");
    DECLARE_UNICODE_STRING_SIZE(buffer, ID_MAX_LEN);

    deviceInit = WdfPdoInitAllocate(Device);
    if (deviceInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        DebugPrintError("WdfPdoInitAllocate failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfPdoInitAssignRawDevice(deviceInit, &GUID_DEVCLASS_KEYBOARD);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfPdoInitAssignRawDevice failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfDeviceInitAssignSDDLString(deviceInit,
                                           &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfDeviceInitAssignSDDLString failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfPdoInitAssignDeviceID(deviceInit, &deviceId);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfPdoInitAssignDeviceID failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = RtlUnicodeStringPrintf(&buffer, L"%02d", InstanceNo);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("RtlUnicodeStringPrintf failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfPdoInitAssignInstanceID(deviceInit, &buffer);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfPdoInitAssignInstanceID failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = RtlUnicodeStringPrintf(&buffer, L"Keyboard_Filter_%02d", InstanceNo);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("RtlUnicodeStringPrintf failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfPdoInitAddDeviceText(deviceInit, &buffer,
                                     &deviceLocation, 0x409);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfPdoInitAddDeviceText failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    WdfPdoInitSetDefaultLocale(deviceInit, 0x409);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes, RPDO_DEVICE_DATA);

    WdfPdoInitAllowForwardingRequestToParent(deviceInit);

    status = WdfDeviceCreate(&deviceInit, &pdoAttributes, &pdoDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfDeviceCreate failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    pdoData = PdoGetData(pdoDevice);
    pdoData->InstanceNo = InstanceNo;

    deviceExtension = GetDeviceExtension(Device);
    pdoData->ParentQueue = deviceExtension->rawPdoQueue;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoDeviceControl = Klog_EvtIoDeviceControlForRawPdo;

    status = WdfIoQueueCreate(pdoDevice, &ioQueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfIoQueueCreate failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.Removable = WdfTrue;
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    pnpCaps.NoDisplayInUI = WdfTrue;
    pnpCaps.Address = InstanceNo;
    pnpCaps.UINumber = InstanceNo;

    WdfDeviceSetPnpCapabilities(pdoDevice, &pnpCaps);

    WDF_DEVICE_STATE_INIT(&deviceState);
    deviceState.DontDisplayInUI = WdfTrue;
    WdfDeviceSetDeviceState(pdoDevice, &deviceState);

    status = WdfDeviceCreateDeviceInterface(pdoDevice, &GUID_DEVINTERFACE_KLOG, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfDeviceCreateDeviceInterface failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    status = WdfFdoAddStaticChild(Device, pdoDevice);
    if (!NT_SUCCESS(status)) {
        DebugPrintError("WdfFdoAddStaticChild failed. Status code 0x%X\n", status);
        goto error_exit;
    }

    return STATUS_SUCCESS;

error_exit:
    if (deviceInit) {
        WdfDeviceInitFree(deviceInit);
    }
    if (pdoDevice) {
        WdfObjectDelete(pdoDevice);
    }

    return status;
}

VOID Klog_EvtIoDeviceControlForRawPdo(IN WDFQUEUE Queue, IN WDFREQUEST Request,
                                      IN size_t OutputBufferLength, IN size_t InputBufferLength,
                                      IN ULONG IoControlCode)
{
    DebugPrintInfo("Entered Klog_EvtIoDeviceControlForRawPdo\n");

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    WDF_REQUEST_FORWARD_OPTIONS     forwardOptions;
    NTSTATUS                        status = STATUS_SUCCESS;
    WDFDEVICE                       parentDevice = WdfIoQueueGetDevice(Queue);
    PRPDO_DEVICE_DATA               pdoData = PdoGetData(parentDevice);

    switch (IoControlCode) {
        case IOCTL_KLOG_INIT:
        case IOCTL_KLOG_GET_INPUT_DATA:
            WDF_REQUEST_FORWARD_OPTIONS_INIT(&forwardOptions);
            status = WdfRequestForwardToParentDeviceIoQueue(Request, pdoData->ParentQueue, &forwardOptions);
            if (!NT_SUCCESS(status)) {
                DebugPrintError("WdfRequestForwardToParentDeviceIoQueue failed. Status code 0x%X\n", status);
                WdfRequestComplete(Request, status);
            }

            break;

        default:
            WdfRequestComplete(Request, status);
            break;
    }

    return;
}
