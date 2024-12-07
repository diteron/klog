#include "klog.h"
#include "debug_print.h"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text (INIT, DriverEntry)
    //#pragma alloc_text (PAGE, KbFilter_EvtDeviceAdd)
    //#pragma alloc_text (PAGE, KbFilter_EvtIoInternalDeviceControl)
#endif

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
                     IN PUNICODE_STRING RegistryPath)
{
    DebugPrintInfo("KlogDriver\n");
    DebugPrintInfo("Built %s %s\n", __DATE__, __TIME__);

    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;
    RtlZeroMemory(&status, sizeof(status));

    WDF_DRIVER_CONFIG_INIT(&config, Klog_EvtDeviceAdd);



}

NTSTATUS KbFilter_EvtDeviceAdd(IN WDFDRIVER Driver,
                               IN PWDFDEVICE_INIT DeviceInit)
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    NTSTATUS                status;
    WDFDEVICE               device;
    WDFQUEUE                queue;
    //PDEVICE_EXTENSION       filterExt;
    WDF_IO_QUEUE_CONFIG     queueConfig;


}