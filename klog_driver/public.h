#pragma once

#define KEYBOARD_INP_BUFFER_SIZE 64

#define KLOG_DEVICE_ID L"{2071646A-D54F-473E-9AF8-D2008FF7203F}\\KeyboardFilter\0"

#define KLOG_GET_INPUT_DATA_FUN 0x800
#define IOCTL_KLOG_GET_INPUT_DATA   CTL_CODE(FILE_DEVICE_KEYBOARD,        \
                                             KLOG_GET_INPUT_DATA_FUN,     \
                                             METHOD_BUFFERED,             \
                                             FILE_READ_DATA)

#define KLOG_INIT_FUN 0x801
#define IOCTL_KLOG_INIT     CTL_CODE(FILE_DEVICE_KEYBOARD,  \
                                     KLOG_INIT_FUN,         \
                                     METHOD_BUFFERED,       \
                                     FILE_READ_DATA)

#define KM_NOTIF_EVENT_NAME L"\\BaseNamedObjects\\KeyboardInputDataAddedEvent"
#define UM_NOTIF_EVENT_NAME L"Global\\KeyboardInputDataAddedEvent"
