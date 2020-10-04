#pragma once

#define IOCTL_DEVMON_ADD_DEVICE			CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_REMOVE_DEVICE		CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_REMOVE_ALL			CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_START_MONITOR		CTL_CODE(0x8000, 0x803, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_STOP_MONITOR		CTL_CODE(0x8000, 0x804, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_ADD_DRIVER			CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEVMON_REMOVE_DRIVER		CTL_CODE(0x8000, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DEVICE_NAME		L"\\Device\\KDevMon"
#define DEVICE_LINK		L"\\??\\KDevMon"