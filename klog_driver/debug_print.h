#pragma once

#include <ntifs.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#define DRIVER_NAME "KlogDriver"
#define MAX_MSG_SIZE 256

void debugPrint(ULONG dpfltrLevel, PCSTR format, ...);

#if DBG
    #define DebugPrintInfo(format, ...) debugPrint(DPFLTR_INFO_LEVEL, format, __VA_ARGS__);
    #define DebugPrintError(format, ...) debugPrint(DPFLTR_ERROR_LEVEL, format, __VA_ARGS__);
    #define TRAP() DbgBreakPoint()
#else
    #define DebugPrintInfo(...) ((void)0)
    #define DebugPrintError(...) ((void)0)
#endif
