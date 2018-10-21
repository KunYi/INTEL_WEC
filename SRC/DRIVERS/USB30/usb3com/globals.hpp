//
// Copyright(c) Microsoft Corporation.  All rights reserved.
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
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//

//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2011-2012 Intel Corporation All Rights Reserved. 
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
//

//----------------------------------------------------------------------------------
// File: globals.hpp This header contains XHCD data that all classes should have
//       access to
//----------------------------------------------------------------------------------

#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <windows.h>
#include <ceddk.h>
#include <hcdi.h>
#include <uhcdddsi.h>

#ifdef DEBUG
    // in debug mode, structures are inited and sometimes written with
    // garbage during execution. This "GARBAGE" can be set to anything
    // and should not affect program execution. Changing this value once
    // in a while may find hidden bugs(i.e. if code is accessing items
    // that it shouldn't be...)
    #define GARBAGE INT(0xAA)

    #define DEBUG_ONLY(x) x
    #define DEBUG_PARAM(x) x,
#else
    #define DEBUG_ONLY(x)
    #define DEBUG_PARAM(x)
#endif

#define MAX_SIZE_PWR_STATE_NAME            (128)

// Spec'ed to be 4kB
#define USBPAGESIZE                         DWORD(4096)

// Spec'ed to be 64kB
#define PAGE_64K_SIZE                       DWORD(0x10000)

// USB addresses are 8 bits long, hence between 0 and 255
#define USB_MAX_ADDRESS                     UCHAR(255)

// minimum packet size of control pipe to endpoint 0
#define ENDPOINT_ZERO_MIN_MAXPACKET_SIZE    USHORT(8)

//
// USB Hub definitions
//

// There can be at most 5 hubs linked in a row, according to USB spec
// 1.1, section 7.1.19(Figure 7-31)
#define USB_MAXIMUM_HUB_TIER                UCHAR(5)

// USB spec(1.1) 11.15.2.1 - hub descriptor type is 29h
#define USB_HUB_DESCRIPTOR_TYPE             UCHAR(0x29)

// USB spec(3.0) 10.13.2.1 - hub descriptor type is 2Ah
#define USB_HUB3_DESCRIPTOR_TYPE             UCHAR(0x2A)

// If the hub has <= 7 ports, the descriptor will be
// 9 bytes long. Else, it will be larger.
#define USB_HUB_DESCRIPTOR_MINIMUM_SIZE     UCHAR(0x9)

#define USB_HUB3_DESCRIPTOR_MINIMUM_SIZE     UCHAR(12)

// device set and clear feature recipients
#define USB_DEVICE_RECIPIENT                UCHAR(0x00)
#define USB_INTERFACE_RECIPIENT             UCHAR(0x01)
#define USB_ENDPOINT_RECIPIENT              UCHAR(0x02)

// Standard device feature selectors
#define USB_DEVICE_REMOTE_WAKEUP            UCHAR(0x01)
#define USB_FEATURE_ENDPOINT_HALT           UCHAR(0x00)

// TT specific requests Table 11-16, Usb spec(2.0)
#define USB_HUB_REQUEST_RESET_TT            UCHAR(0x09)

// USB spec(1.1) Table 11-12 : Hub Class Feature Selectors
#define USB_HUB_FEATURE_C_HUB_LOCAL_POWER   UCHAR(0x00)     //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_C_HUB_OVER_CURRENT  UCHAR(0x01)    //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_PORT_CONNECTION     UCHAR(0x00)       //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_PORT_ENABLE         UCHAR(0x01)           //USB2.0
#define USB_HUB_FEATURE_PORT_SUSPEND        UCHAR(0x02)         //USB2.0
#define USB_HUB_FEATURE_PORT_OVER_CURRENT   UCHAR(0x03)    //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_PORT_RESET          UCHAR(0x04)            //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_PORT_LINK_STATE     UCHAR(0x05)       //            USB 3.0
// 0x6 - 0x7 are reserved
#define USB_HUB_FEATURE_PORT_POWER          UCHAR(0x08)          //USB2.0 /USB 3.0
#define USB_HUB_FEATURE_PORT_LOW_SPEED      UCHAR(0x09)
// 0xA - 0xF are reserved
#define USB_HUB_FEATURE_C_PORT_CONNECTION   UCHAR(USB_HUB_FEATURE_PORT_CONNECTION | 0x10)
#define USB_HUB_FEATURE_C_PORT_ENABLE       UCHAR(USB_HUB_FEATURE_PORT_ENABLE | 0x10)                         // 17 : USB2.0 
#define USB_HUB_FEATURE_C_PORT_SUSPEND      UCHAR(USB_HUB_FEATURE_PORT_SUSPEND | 0x10)                     // 18 : USB2.0
#define USB_HUB_FEATURE_C_PORT_OVER_CURRENT UCHAR(USB_HUB_FEATURE_PORT_OVER_CURRENT | 0x10)      // 19 : USB2.0 / USB3.0
#define USB_HUB_FEATURE_C_PORT_RESET        UCHAR(USB_HUB_FEATURE_PORT_RESET | 0x10)                            // 20 : USB2.0 / USB3.0
#define USB_HUB_FEATURE_PORT_TEST                    UCHAR(0x15)      // 21 : USB2.0  Port TEST
#define USB_HUB_FEATURE_PORT_INDICATOR           UCHAR(0x16)    // 22 : USB2.0
#define USB_HUB_FEATURE_PORT_U1_TIMEOUT        UCHAR(0x17)  // 23 : USB3.0
#define USB_HUB_FEATURE_PORT_U2_TIMEOUT        UCHAR(0x18)  // 24 : USB3.0
#define USB_HUB_FEATURE_C_PORT_LINK_STATE     UCHAR(0x19)  // 25 : USB3.0
#define USB_HUB_FEATURE_C_PORT_CONFIG_ERROR             UCHAR(0x1A)  // 26 : USB3.0
#define USB_HUB_FEATURE_PORT_REMOTE_WAKE_MASK       UCHAR(0x1B)  // 27 : USB3.0
#define USB_HUB_FEATURE_BH_PORT_RESET                          UCHAR(0x1C)  // 28 : USB3.0
#define USB_HUB_FEATURE_C_BH_PORT_RESET                      UCHAR(0x1D)  // 29 : USB3.0
#define USB_HUB_FEATURE_FORCE_LINKPM_ACCEPT              UCHAR(0x1E)  // 30 : USB3.0

#define USB_HUB_FEATURE_INVALID             UCHAR(0xFF)

// bits for hub descriptor wHubCharacteristics field
// see USB spec(1.1) Table 11-8, offset 3
#define USB_HUB_CHARACTERISTIC_GANGED_POWER_SWITCHING             USHORT(0x0)
#define USB_HUB_CHARACTERISTIC_INDIVIDUAL_POWER_SWITCHING         USHORT(0x1)
// warning can be 0x3
#define USB_HUB_CHARACTERISTIC_NO_POWER_SWITCHING                 USHORT(0x2)
#define USB_HUB_CHARACTERISTIC_NOT_PART_OF_COMPOUND_DEVICE        USHORT(0 << 2)
#define USB_HUB_CHARACTERISTIC_PART_OF_COMPOUND_DEVICE            USHORT(1 << 2)
#define USB_HUB_CHARACTERISTIC_GLOBAL_OVER_CURRENT_PROTECTION     USHORT(0x0 << 3)
#define USB_HUB_CHARACTERISTIC_INDIVIDUAL_OVER_CURRENT_PROTECTION USHORT(0x1 << 3)
// warning, can be(0x3 << 3)
#define USB_HUB_CHARACTERISTIC_NO_OVER_CURRENT_PROTECTION         USHORT(0x2 << 3)
// Hub ThinkTime (0x3 << 5)
#define USB_HUB_CHARACTERISTIC_TT_THINK_TIME_MASK         USHORT(0x3 << 5)

// UsbInterruptThread
#define DEFAULT_XHCD_IST_PRIORITY          (101)
// HubStatusChangeThread
#define RELATIVE_PRIO_STSCHG               (5)
// DetachDownstreamDeviceThread
#define RELATIVE_PRIO_DOWNSTREAM           (3)

#define HUB_SET_DEPTH        12

typedef volatile PUCHAR REGISTER;

//
// USBD function typedefs
//

typedef BOOL(* LPUSBD_HCD_ATTACH_PROC)(LPVOID lpvHcd,
                                        LPCHCD_FUNCS lpHcdFuncs,
                                        LPLPVOID lppvContext);

typedef BOOL(* LPUSBD_HCD_DETACH_PROC)(LPVOID lpvContext);

typedef BOOL(* LPUSBD_ATTACH_PROC)(LPVOID lpvContext,
                                    UINT uAddress,
                                    UINT iEndpointZero,
                                    LPCUSB_DEVICE lpDeviceInfo,
                                    LPLPVOID lppvDeviceDetach);

typedef BOOL(* LPUSBD_DETACH_PROC)(LPVOID lpvDeviceDetach);

typedef BOOL(* LPUSBD_SELECT_CONFIGURATION_PROC)(LPCUSB_DEVICE lpDeviceInfo,
                                                    LPBYTE);

typedef BOOL(* LPUSBD_SUSPEND_RESUME_PROC)(LPVOID lpvDeviceDetach,
                                            BOOL fResumed);

//
// These structures MUST have exactly the same
// data types as the USB_* structues in usbtypes.h
//

typedef struct _NON_CONST_USB_ENDPOINT
{
    DWORD                      dwCount;
    USB_ENDPOINT_DESCRIPTOR    usbEndpointDescriptor;
    LPBYTE                     lpbExtended;
    DWORD                      dwExtendedSize;
} NON_CONST_USB_ENDPOINT, *PNON_CONST_USB_ENDPOINT;

typedef struct _NON_CONST_USB_INTERFACE
{
    DWORD                       dwCount;
    USB_INTERFACE_DESCRIPTOR    usbInterfaceDescriptor;
    LPBYTE                      lpbExtended;
    PNON_CONST_USB_ENDPOINT     pnonConstUsbEndpoints;
    DWORD                       dwExtendedSize;
} NON_CONST_USB_INTERFACE, *PNON_CONST_USB_INTERFACE;

typedef struct _NON_CONST_USB_CONFIGURATION
{
    DWORD                           dwCount;
    USB_CONFIGURATION_DESCRIPTOR    usbConfigDescriptor;
    LPBYTE                          lpbExtended;

    // Total number of interfaces(including alternates)
    DWORD                           dwNumInterfaces;
    PNON_CONST_USB_INTERFACE        pnonConstUsbInterfaces;
    DWORD                           dwExtendedSize;
} NON_CONST_USB_CONFIGURATION, *PNON_CONST_USB_CONFIGURATION;

typedef struct _USB_DEVICE_INFO
{
    DWORD                           dwCount;
    USB_DEVICE_DESCRIPTOR           usbDeviceDescriptor;
    PNON_CONST_USB_CONFIGURATION    pnonConstUsbConfigs;
    PNON_CONST_USB_CONFIGURATION    pnonConstUsbActiveConfig;
} USB_DEVICE_INFO, *PUSB_DEVICE_INFO;

// Data returned by GetHubStatus request is 4 bytes
// and structures as follows:(USB spec 1.1 - Table 11-13/14)
//
// Data returned by GetPortStatus request is 4 bytes
// and structured as follows:(USB spec 1.1 - Table 11-15/16)
typedef struct _USB_HUB_AND_PORT_STATUS
{
    union
    {
        struct
        {
            USHORT usLocalPowerStatus:1;              // wHubStatus bit 0
            USHORT usOverCurrentIndicator:1;          // wHubStatus bit 1
            USHORT usReserved:14;                     // wHubStatus bits 2-15
        } hub;
        struct
        {
            USHORT usPortConnected:1;                 // wPortStatus bit 0
            USHORT usPortEnabled:1;                   // wPortStatus bit 1
            USHORT usPortSuspended:1;                 // wPortStatus bit 2
            USHORT usPortOverCurrent:1;               // wPortStatus bit 3
            USHORT usPortReset:1;                     // wPortStatus bit 4
            USHORT usReserved:3;                      // wPortStatus bits 5-7
            USHORT usPortPower:1;                     // wPortStatus bit 8
            USHORT usDeviceSpeed:2;                   // wPortStatus bit 9-10
            USHORT usPortTestMode:1;                // bit 11
            USHORT usPortIndicatorControl:1;     // bit 12
            USHORT usReserved2:3;                     // wPortStatus bits 13-15
        } port;
        struct
        {
            USHORT usPortConnected:1;                 // wPortStatus bit 0
            USHORT usPortEnabled:1;                   // wPortStatus bit 1
            USHORT usReserved:1;                 // wPortStatus bit 2
            USHORT usPortOverCurrent:1;               // wPortStatus bit 3
            USHORT usPortReset:1;                     // wPortStatus bit 4
            USHORT usPortLinkState:4;                      // wPortStatus bits 5-8
            USHORT usPortPower:1;                     // wPortStatus bit 9
            USHORT usDeviceSpeed:3;                   // wPortStatus bit 10-12
            USHORT usReserved2:3;                     // wPortStatus bits 13-15
        } port30;
        USHORT wWord;
    } status;
    union
    {
        struct
        {
            USHORT usLocalPowerChange:1;              // wHubChange bit 0
            USHORT usOverCurrentIndicatorChange:1;    // wHubChange bit 1
            USHORT usReserved2:14;                    // wHubChange bits 2-15
        } hub;
        struct
        {
            USHORT usConnectStatusChange:1;           // wPortChange bit 0
            USHORT usPortEnableChange:1;              // wPortChange bit 1
            USHORT usSuspendChange:1;                 // wPortChange bit 2
            USHORT usOverCurrentChange:1;             // wPortChange bit 3
            USHORT usResetChange:1;                   // wPortChange bit 4
            USHORT usWarmResetChange:1;               // wPortChange bit 5
            USHORT usReserved:10;                     // wPortChange bits 6-15
        } port;
        struct
        {
            USHORT usConnectStatusChange:1;           // wPortChange bit 0
            USHORT usReserved:2;              // wPortChange bit 1-2
            USHORT usOverCurrentChange:1;             // wPortChange bit 3
            USHORT usResetChange:1;                   // wPortChange bit 4
            USHORT usWarmResetChange:1;               // wPortChange bit 5
            USHORT usPortLinkStateChange:1;               // wPortChange bit 6
            USHORT usPortConfigError:1;               // wPortChange bit 7
            USHORT usReserved2:8;                     // wPortChange bits 8-15
        } port30;
        USHORT wWord;
    } change;
} USB_HUB_AND_PORT_STATUS, *PUSB_HUB_AND_PORT_STATUS;

#ifdef __cplusplus
extern "C" extern DWORD g_dwIstThreadPriority;
#endif

typedef struct _ISSUE_TRANSFER_PARAMS
{
    UINT                      uAddress;
    UINT                      uPipeIndex;
    LPTHREAD_START_ROUTINE    lpfnStartAddress;
    LPVOID                    lpvParameter;
    DWORD                     dwFlags;
    LPCVOID                   lpcvControlHeader;
    DWORD                     dwStartingFrame;
    DWORD                     dwFrames;
    LPCDWORD                  lpcdwLengths;
    DWORD                     dwBufferSize;
    LPVOID                    lpvBuffer;
    ULONG                     ulBuffer;
    LPCVOID                   lpcvCancelId;
    LPDWORD                   lpdwIsochErrors;
    LPDWORD                   lpdwIsochLengths;
    LPBOOL                    lpfComplete;
    LPDWORD                   lpdwBytesTransfered;
    LPDWORD                   lpdwError;
} ISSUE_TRANSFER_PARAMS, *PISSUE_TRANSFER_PARAMS;

typedef enum _HCD_REQUEST_STATUS
{
    REQUEST_FAILED = 0,
    REQUEST_OK,
    REQUEST_IGNORED
} HCD_REQUEST_STATUS;

#define ZONE_XHCD    ZONE_HCD

#endif /* GLOBALS_HPP */