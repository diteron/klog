#pragma once

#define KEYBOARD_INP_BUFFER_SIZE 64

#define KLOG_DEVICE_ID L"{A65C87F9-BE02-4ed9-92EC-012D416169FA}\\KeyboardFilter\0"

#define IOCTL_INDEX             0x800
#define IOCTL_KLOG_GET_INPUT_DATA CTL_CODE(FILE_DEVICE_KEYBOARD,   \
                                           IOCTL_INDEX,            \
                                           METHOD_BUFFERED,        \
                                           FILE_READ_DATA)

#define IOCTL_INDEX2             0x801
#define IOCTL_KLOG_INIT CTL_CODE(FILE_DEVICE_KEYBOARD,  \
                                 IOCTL_INDEX2,          \
                                 METHOD_BUFFERED,       \
                                 FILE_READ_DATA)

#define KM_NOTIF_EVENT_NAME L"\\BaseNamedObjects\\KeyboardInputDataAddedEvent"
#define UM_NOTIF_EVENT_NAME L"Global\\KeyboardInputDataAddedEvent"
