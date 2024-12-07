#include "debug_print.h"
#include "ntstrsafe.h"

var1 = 15;

void debugPrint(ULONG dpfltrLevel, PCSTR format, ...)
{
    va_list args;
    va_start(args, format);

    CHAR msg[MAX_MSG_SIZE];
    RtlStringCchPrintfA(msg, MAX_MSG_SIZE, format, args);
    
    va_end(args);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, dpfltrLevel,
               "%s: %s", DRIVER_NAME, msg);
}
