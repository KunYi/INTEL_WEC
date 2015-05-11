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
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//------------------------------------------------------------------------------
//
//  Header: ioctl_tab_atom.h
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
//  Desc... - ioctl are pre-emptible, code is multi-threaded, the OEM is responsible
//            for managing this.
//
//  Format:
//
//  IOCTL CODE,                                 Flag        Handler Function
//------------------------------------------------------------------------------

// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2011 Intel Corporation All Rights Reserved. 
//  
// @par 
// The source code contained or described herein and all documents 
// related to the source code ("Material") are owned by Intel Corporation 
// or its suppliers or licensors.  Title to the Material remains with 
// Intel Corporation or its suppliers and licensors. 
//  
// @par 
// The Material is protected by worldwide copyright and trade secret laws 
// and treaty provisions. No part of the Material may be used, copied, 
// reproduced, modified, published, uploaded, posted, transmitted, 
// distributed, or disclosed in any way except in accordance with the 
// applicable license agreement . 
//  
// @par 
// No license under any patent, copyright, trade secret or other 
// intellectual property right is granted to or conferred upon you by 
// disclosure or delivery of the Materials, either expressly, by 
// implication, inducement, estoppel, except in accordance with the 
// applicable license agreement. 
//  
// @par 
// Unless otherwise agreed by Intel in writing, you may not remove or 
// alter this notice or any other notice embedded in Materials by Intel 
// or Intel's suppliers or licensors in any way. 
//  
// @par 
// For further details, please see the file README.TXT distributed with 
// this software. 
//  
// @par 
// -- End Intel Copyright Notice -- 


    { IOCTL_HAL_REQUEST_SYSINTR,                0,          OALIoCtlHalRequestSysIntr   },
    { IOCTL_HAL_RELEASE_SYSINTR,                0,          OALIoCtlHalReleaseSysIntr   },
    { IOCTL_HAL_REQUEST_IRQ,                    0,          OALIoCtlHalRequestIrq       },
    { IOCTL_HAL_UNKNOWN_IRQ_HANDLER,            0,          OALUnknownIRQHandler        },

    { IOCTL_HAL_DDK_CALL,                       0,          OALIoCtlHalDdkCall          },

    { IOCTL_HAL_DISABLE_WAKE,                   0,          x86PowerIoctl               },
    { IOCTL_HAL_ENABLE_WAKE,                    0,          x86PowerIoctl               },
    { IOCTL_HAL_GET_WAKE_SOURCE,                0,          x86PowerIoctl               },
    { IOCTL_HAL_PRESUSPEND,                     0,          x86PowerIoctl               },
    { IOCTL_HAL_GET_POWER_DISPOSITION,          0,          x86IoCtlGetPowerDisposition },

    { IOCTL_HAL_GET_CACHE_INFO,                 0,          x86IoCtlHalGetCacheInfo     },
    { IOCTL_HAL_GET_DEVICEID,                   0,          OALIoCtlHalGetDeviceId      },
    { IOCTL_HAL_GET_DEVICE_INFO,                0,          OALIoCtlHalGetDeviceInfo    },
    { IOCTL_HAL_SET_DEVICE_INFO,                0,          x86IoCtlHalSetDeviceInfo    },
    { IOCTL_HAL_GET_UUID,                       0,          OALIoCtlHalGetUUID          },
    { IOCTL_HAL_GET_RANDOM_SEED,                0,          OALIoCtlHalGetRandomSeed    },
    
    { IOCTL_PROCESSOR_INFORMATION,              0,          x86IoCtlProcessorInfo       },
    
    { IOCTL_HAL_INIT_RTC,                       0,          x86IoCtlHalInitRTC          },
    { IOCTL_HAL_REBOOT,                         0,          x86IoCtlHalReboot           },

    { IOCTL_HAL_UPDATE_MODE,                    0,          x86IoCtlHalUpdateMode       },

    { IOCTL_HAL_ILTIMING,                       0,          x86IoCtllTiming             },

    { IOCTL_HAL_POSTINIT,                       0,          x86IoCtlPostInit            },
    
    { IOCTL_HAL_QUERY_DISPLAYSETTINGS,          0,          x86IoCtlQueryDispSettings   },

    { IOCTL_HAL_INITREGISTRY,                   0,          x86IoCtlHalInitRegistry     },

    { IOCTL_HAL_GET_FAST_COUNTER,               0,          x86IoCtlGetFastCounter      },
    { IOCTL_HAL_GET_FAST_COUNTER_INFO,          0,          x86IoCtlGetFastCounterInfo  },

    { IOCTL_HAL_POWERSHUTDOWN,                  0,          x86PowerIoctl               },
    
    // Required Termination
    { 0,                                        0,          NULL                        }
