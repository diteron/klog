#include "debug_print.h"

void debugPrint(ULONG dpfltrLevel, PCSTR format, ...)
{
    va_list args;
    va_start(args, format);

    CHAR msg[MAX_MSG_SIZE];
    NTSTATUS status = RtlStringCbVPrintfA(msg, MAX_MSG_SIZE, format, args);
    
    va_end(args);

    if (NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, dpfltrLevel,
                   "[%s] %s", DRIVER_NAME, msg);
    }
    else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[%s] Failed to print debug message. "
                   "Status code 0x%X", DRIVER_NAME, status);
    }
}
