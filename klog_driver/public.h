#pragma once

#define KEYBOARD_INP_BUFFER_SIZE 64

#define DEV_SYMBOLIC_LINK L"\\DosDevices\\klog_symlink"
#define DRIVER_SYMBOLIC_LINK L"\\\\.\\klog_symlink"

#define IOCTL_INDEX             0x800
#define IOCTL_KLOG_GET_INPUT_DATA CTL_CODE(FILE_DEVICE_KEYBOARD,   \
                                           IOCTL_INDEX,            \
                                           METHOD_BUFFERED,        \
                                           FILE_READ_DATA)

#define IOCTL_INDEX2             0x801
#define IOCTL_KLOG_INIT CTL_CODE(FILE_DEVICE_KEYBOARD,          \
                                           IOCTL_INDEX2,        \
                                           METHOD_BUFFERED,     \
                                           FILE_READ_DATA)