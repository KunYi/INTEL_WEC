//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//------------------------------------------------------------------------------
//
//  Header: ioctl_tab.h
//
//  Configuration file for the OAL IOCTL component.
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  RESTRICTION
//
//  This file is included by the platform's ioctl.c file and defines the 
//  global IOCTL table, g_oalIoCtlTable[]. Therefore, this file may ONLY
//  define OAL_IOCTL_HANDLER entries. 
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Architecture
//
//  Desc... - ioctl are pre-emptible, code is multi-threaded, user responsible
//            for managing this.
//
//  Format:
//
//  IOCTL CODE,                                 Flag        Handler Function
//------------------------------------------------------------------------------

    { IOCTL_HAL_REQUEST_SYSINTR,                0,          OALIoCtlHalRequestSysIntr   },
    { IOCTL_HAL_RELEASE_SYSINTR,                0,          OALIoCtlHalReleaseSysIntr   },
    { IOCTL_HAL_REQUEST_IRQ,                    0,          OALIoCtlHalRequestIrq       },

    { IOCTL_HAL_DDK_CALL,                       0,          x86IoCtlHalDdkCall          },

    { IOCTL_HAL_DISABLE_WAKE,                   0,          x86PowerIoctl               },
    { IOCTL_HAL_ENABLE_WAKE,                    0,          x86PowerIoctl               },
    { IOCTL_HAL_GET_WAKE_SOURCE,                0,          x86PowerIoctl               },
    { IOCTL_HAL_PRESUSPEND,                     0,          x86PowerIoctl               },

    { IOCTL_HAL_GET_CACHE_INFO,                 0,          x86IoCtlHalGetCacheInfo     },
    { IOCTL_HAL_GET_DEVICEID,                   0,          OALIoCtlHalGetDeviceId      },
    { IOCTL_HAL_GET_DEVICE_INFO,                0,          OALIoCtlHalGetDeviceInfo    },
    { IOCTL_HAL_SET_DEVICE_INFO,                0,          x86IoCtlHalSetDeviceInfo    },
    { IOCTL_HAL_GET_UUID,                       0,          OALIoCtlHalGetUUID          },
    { IOCTL_HAL_GET_RANDOM_SEED,                0,          OALIoCtlHalGetRandomSeed    },
    
    { IOCTL_PROCESSOR_INFORMATION,              0,          x86IoCtlProcessorInfo       },
    
    { IOCTL_HAL_INIT_RTC,                       0,          x86IoCtlHalInitRTC          },
    { IOCTL_HAL_REBOOT,                         0,          x86IoCtlHalReboot           },

    { IOCTL_HAL_ILTIMING,                       0,          x86IoCtllTiming             },

    { IOCTL_HAL_POSTINIT,                       0,          x86IoCtlPostInit            },
    
    { IOCTL_HAL_QUERY_DISPLAYSETTINGS,          0,          x86IoCtlQueryDispSettings   },

    { IOCTL_HAL_INITREGISTRY,                   0,          x86IoCtlHalInitRegistry     },
	
	{ IOCTL_HAL_POWERSHUTDOWN,					0,			x86PowerIoctl				},
    
    // Required Termination
    { 0,                                        0,          NULL                        }
