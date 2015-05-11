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
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2013 Intel Corporation All Rights Reserved. 
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
// File: cdevice.cpp
//
// Description:
// 
//     This file manages the USB devices
//
//                  CDevice(ADT)
//                /               \
//            CFunction        CHub(ADT)
//                            /          \
//                        CRootHub   CExternalHub
//----------------------------------------------------------------------------------

#include <windows.h>

#include <usb200.h>
#include "cdevice.hpp"
#include "chcd.hpp"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

//----------------------------------------------------------------------------------
// Function: CDeviceGlobal
//
// Description: Constructor for CDeviceGlobal
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CDeviceGlobal::CDeviceGlobal():m_objCountdown(0),
                               m_pUSBDSuspendResumed (NULL)
{
    m_hUSBDInstance = NULL;
    m_pUSBDAttachProc = NULL;
    m_pUSBDDetachProc = NULL;
    m_pvHcdContext = NULL;
    m_pUSBDSelectConfigurationProc = NULL;

#ifdef DEBUG
    g_fAlreadyCalled = FALSE;
#endif // DEBUG
}

//----------------------------------------------------------------------------------
// Function: ~CDeviceGlobal
//
// Description: Destructor forCDeviceGlobal
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CDeviceGlobal::~CDeviceGlobal()
{
    DeInitialize();
}

//----------------------------------------------------------------------------------
// Function: Initialize
//
// Description: Initialize any static variables associated with CDevice, and
//              establish link to USBD. This function should be called only
//              once from CHcd::Initialize
//
// Parameters: pHcd - pointer to the Host Controller Driver object which we
//             pass to USBD
//
// Returns: TRUE if initialization is correct, else FALSE
//----------------------------------------------------------------------------------
BOOL CDeviceGlobal::Initialize(IN PVOID pHcd)
{

    m_pHcd = pHcd;

    m_objCountdown.UnlockCountdown();

    // establish links to USBD.dll
    {
        // this procedure is called to establish a link to USBD
        LPUSBD_HCD_ATTACH_PROC  lpHcdAttachProc = NULL;
        // this is defined in hcddrv.cpp
        extern HCD_FUNCS gc_HcdFuncs;

        DEBUGCHK(m_pHcd != NULL &&
                    m_hUSBDInstance == NULL &&
                    m_pUSBDDetachProc == NULL &&
                    m_pUSBDAttachProc == NULL &&
                    m_pvHcdContext == NULL);

        m_hUSBDInstance = LoadDriver(TEXT("USBD.DLL"));

        if(m_hUSBDInstance == NULL)
        {
            DEBUGMSG(ZONE_ERROR,
                   (TEXT("%s: -CDevice::Initialize - Could not load USBD.DLL\r\n"),
                    GetControllerName()));

            return FALSE;
        }

        lpHcdAttachProc =
           (LPUSBD_HCD_ATTACH_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdAttach"));
        m_pUSBDAttachProc =
           (LPUSBD_ATTACH_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdDeviceAttached"));
        m_pUSBDDetachProc =
           (LPUSBD_DETACH_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdDeviceDetached"));
        // Optional
        m_pUSBDSelectConfigurationProc =
           (LPUSBD_SELECT_CONFIGURATION_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdSelectConfiguration"));
        m_pUSBDSuspendResumed =
           (LPUSBD_SUSPEND_RESUME_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdDeviceSuspendeResumed"));

        if(m_pUSBDAttachProc == NULL ||
            m_pUSBDDetachProc == NULL ||
            lpHcdAttachProc == NULL ||
           (*lpHcdAttachProc)(m_pHcd, &gc_HcdFuncs, &m_pvHcdContext) == FALSE)
        {
            DEBUGMSG(ZONE_ERROR,
                   (TEXT("%s: -CDevice::Initialize - \
                          Could not establish USBD links\n"),
                    GetControllerName()));

            return FALSE;
        }

        DEBUGCHK(m_pvHcdContext != NULL);
    }

    DEBUGMSG(ZONE_INIT,
           (TEXT("%s: -CDevice::Initialize, success!\n"),
            GetControllerName()));

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: DeInitialize
//
// Description: Delete any static variables associated with CDevice. This function
//              should be called only once from CHcd::~CHcd
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDeviceGlobal::DeInitialize()
{
    DEBUGMSG(ZONE_INIT,
           (TEXT("%s: +CDevice::DeInitialize\n"),
            GetControllerName()));

#ifdef DEBUG
    DEBUGCHK(g_fAlreadyCalled == TRUE);
    g_fAlreadyCalled = FALSE;
#endif // DEBUG

    // wait for any stray detach threads
    // This can block waiting for a callback into a client driver to return.
    // Since callbacks aren't supposed to block this oughtn't cause deadlock,
    // but a misbehaving client driver can cause us serious grief.
    // Nonetheless, not waiting means we might free USBD.DLL while it's
    // still in use.
    //DeleteCountdown(&m_objCountdown);
    m_objCountdown.WaitForCountdown(TRUE);

    // unload USBD.dll
    if(m_hUSBDInstance)
    {
        LPUSBD_HCD_DETACH_PROC lpHcdDetachProc;

        lpHcdDetachProc =
           (LPUSBD_HCD_DETACH_PROC)GetProcAddress(m_hUSBDInstance,
                                                    TEXT("HcdDetach"));

        if(lpHcdDetachProc != NULL)
        {
           (*lpHcdDetachProc)(m_pvHcdContext);
        }

        FreeLibrary(m_hUSBDInstance);
        m_hUSBDInstance = NULL;
    }

    m_pUSBDAttachProc = NULL;
    m_pUSBDDetachProc = NULL;
    m_pvHcdContext = NULL;

    DEBUGMSG(ZONE_INIT,
           (TEXT("%s: -CDevice::DeInitialize\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: TransferDoneCallbackSetEvent
//
// Description: This function is a callback for the CPipe class. When a
//              transfer completes, and this function was set in the
//              lpStartAddress field of IssueTransfer, we will be called.
//              Calling this function directly is rather useless(it will
//              just have the same effect as SetEvent(pContext)), so
//              it should only be used as a callback
//
// Parameters: pContext - HANDLE to an event we should signal
//
// Returns: 0
//----------------------------------------------------------------------------------
DWORD CALLBACK CDevice::TransferDoneCallbackSetEvent(PVOID pContext)
{
    DEBUGCHK(pContext);
    SetEvent((HANDLE)pContext);
    return 0;
}

//----------------------------------------------------------------------------------
// Function: CDevice
//
// Description: Constructor for CDevice. Do not initialize static variables here.
//              Do that in the Initialize() routine
//
// Parameters: bAddress - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD
//
//             rDeviceInfo - object containing device's USB descriptors
//
//             bSpeed - indicates speed
//
//             bTierNumber - indicates how far away this device is from the root hub
//
// Returns: None
//----------------------------------------------------------------------------------
CDevice::CDevice(IN const UCHAR bAddress,
                 IN const USB_DEVICE_INFO& rDeviceInfo,
                 IN const UCHAR bSpeed,
                 IN const UCHAR bTierNumber,
                 IN CDeviceGlobal * const pDeviceGlobal,
                 IN CHub * const pAttachedHub,
                 const UCHAR sAttachedPort) : m_bAddress(bAddress), // USB address of this device
                                                m_deviceInfo(rDeviceInfo), // USB descriptors/information about this device
                                                m_bSpeed(bSpeed), // indicates for speed devices
                                                m_bTierNumber(bTierNumber), // tier number of device(0 for root hub, 1 for first tier, etc)
                                                m_pDeviceGlobal(pDeviceGlobal),
                                                m_pAttachedHub(pAttachedHub),
                                                m_bAttachedPort(sAttachedPort),
                                                m_bMaxNumPipes(0), // current size of m_ppCPipe array
                                                m_ppCPipe(NULL) // dynamically allocated array of pointers to open pipes
{
    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: +CDevice::CDevice\n"),
            GetControllerName()));

    m_fIsSuspend = FALSE;
    // set to unknown initial state;
    m_dwDevState = FXN_DEV_STATE_UNDETERMINED;

    DEBUGCHK(m_deviceInfo.dwCount == sizeof(USB_DEVICE) &&
                m_deviceInfo.usbDeviceDescriptor.bDescriptorType ==
                USB_DEVICE_DESCRIPTOR_TYPE &&
                m_deviceInfo.usbDeviceDescriptor.bLength ==
                sizeof(USB_DEVICE_DESCRIPTOR) &&
                bAddress <= USB_MAX_ADDRESS &&
                bTierNumber <= USB_MAXIMUM_HUB_TIER + 1);

    InitializeCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: -CDevice::CDevice\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CDevice
//
// Description: Destructor forCDevice. Do not delete static variables here.
//              Do that in DeInitialize();
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CDevice::~CDevice()
{
    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: +CDevice::~CDevice\n"),
            GetControllerName()));

    // delete m_deviceInfo structure
    if(m_deviceInfo.pnonConstUsbConfigs != NULL)
    {
        DEBUGCHK(m_deviceInfo.usbDeviceDescriptor.bDescriptorType ==
                    USB_DEVICE_DESCRIPTOR_TYPE &&
                    m_deviceInfo.usbDeviceDescriptor.bLength ==
                    sizeof(USB_DEVICE_DESCRIPTOR) &&
                    m_deviceInfo.dwCount == sizeof(USB_DEVICE_INFO) &&
                    m_deviceInfo.usbDeviceDescriptor.bNumConfigurations > 0);

        for(UINT uConfig = 0;
            uConfig < m_deviceInfo.usbDeviceDescriptor.bNumConfigurations;
            uConfig++)
        {
            DeleteUsbConfigurationStructure(m_deviceInfo.pnonConstUsbConfigs[uConfig]);
        }

        delete [] m_deviceInfo.pnonConstUsbConfigs;
        m_deviceInfo.pnonConstUsbConfigs = NULL;
    }

    m_deviceInfo.pnonConstUsbActiveConfig = NULL;

#ifdef DEBUG
{
    DEBUGCHK((m_ppCPipe == NULL && m_bMaxNumPipes == 0) ||
           (m_ppCPipe != NULL && m_bMaxNumPipes > 0));

    // all pipes should have been closed/deleted by HandleDetach
    for(UCHAR bPipe = 0; m_ppCPipe && bPipe < m_bMaxNumPipes; bPipe++)
    {
        DEBUGCHK(m_ppCPipe[bPipe] == NULL);
    }
}

#endif // DEBUG
    delete [] m_ppCPipe;
    m_ppCPipe = NULL;
    m_bMaxNumPipes = 0;

    // nothing to be done with any of these:
    // m_deviceInfo;   // holds device's USB descriptors
    // m_bSpeed;  // indicates the device speed
    // m_bTierNumber;   // indicates tier # of device

    DeleteCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
       (TEXT("%s: -CDevice::~CDevice\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: GetUSB2TT
//
// Description: Found Transaction Translate for Full Speed Device
//
// Parameters: rTTAddr - OUT parameter. Address.
//
//             rTTPort - OUT parameter. Port.
//
//             rTTContext - OUT parameter. Transaction Translate context.
//
// Returns: The Hub object where this TT located
//----------------------------------------------------------------------------------
CHub * CDevice::GetUSB2TT(UCHAR& rTTAddr, UCHAR& rTTPort, PVOID& rTTContext)
{
    if(!((m_bSpeed  == USB_HIGH_SPEED) ||(m_bSpeed  == USB_SUPER_SPEED)))
    {
        CHub * pHub = m_pAttachedHub;
        UCHAR bAttachedPort = m_bAttachedPort;

        while(pHub != NULL && pHub->m_bSpeed != USB_HIGH_SPEED &&
                pHub->m_bSpeed != USB_SUPER_SPEED)
        {
            bAttachedPort = pHub->m_bAttachedPort;
            pHub = pHub->m_pAttachedHub;
        }

        if(pHub)
        {
            rTTAddr = pHub->m_bAddress;
            rTTPort = bAttachedPort;
            ASSERT(bAttachedPort != 0 && bAttachedPort <= 
                pHub->m_usbHubDescriptor.bNumberOfPorts);
            rTTContext = pHub->m_pAddedTT[bAttachedPort - 1];
        }

        return pHub;
    }

    return NULL;
}

//----------------------------------------------------------------------------------
// Function: CreateUsbConfigurationStructure
//
// Description: Fill in rConfig using data from the given pDataBuffer. This should
//              be called after the usbConfigDescriptor field of the configuration
//              has already been filled in. This function is protected
//
// Parameters: rConfig - reference to NON_CONST_USB_CONFIGURATION structure to fill in
//
//             pDataBuffer - data buffer from which to create NON_CONST_USB_CONFIGURATION.
//                           This should have been the data retrieved from the USB
//                           device's GET_CONFIGURATION_DESCRIPTOR request
//
//             uDataBufferLen - length of pDataBuffer
//
// Returns: TRUE if configuration set properly, else FALSE
//----------------------------------------------------------------------------------
BOOL CDevice::CreateUsbConfigurationStructure(IN NON_CONST_USB_CONFIGURATION& rConfig,
                                              __in_bcount(uDataBufferLen) const PUCHAR pDataBuffer,
                                              IN const UINT uDataBufferLen) const
{
    DEBUGMSG(ZONE_DESCRIPTORS && ZONE_VERBOSE,
       (TEXT("%s: +CDevice::CreateUsbConfigurationStructure\n"),
        GetControllerName()));

    DEBUGCHK(pDataBuffer != NULL && uDataBufferLen ==
        rConfig.usbConfigDescriptor.wTotalLength);

    PUSB_CONFIGURATION_DESCRIPTOR pusbConfigDesc =
       (PUSB_CONFIGURATION_DESCRIPTOR)pDataBuffer;

#ifdef DEBUG
    DumpConfigDescriptor(&rConfig.usbConfigDescriptor);
#endif // DEBUG

    rConfig.dwNumInterfaces = 0;
    rConfig.lpbExtended = NULL;
    rConfig.pnonConstUsbInterfaces = NULL;
    rConfig.dwExtendedSize = 0;

    BOOL fRetval = FALSE;

    if(pusbConfigDesc != NULL &&
        pusbConfigDesc->wTotalLength == rConfig.usbConfigDescriptor.wTotalLength &&
        uDataBufferLen == pusbConfigDesc->wTotalLength &&
        pusbConfigDesc->bLength >= sizeof(USB_CONFIGURATION_DESCRIPTOR) &&
        pusbConfigDesc->bDescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE)
    {
        UINT uOffset = pusbConfigDesc->bLength;
        PUSB_COMMON_DESCRIPTOR pusbCommon = NULL;
        rConfig.dwCount = sizeof(NON_CONST_USB_CONFIGURATION);

        // first step - count number of extended bytes for this config descriptor,
        // and copy data if needed
        {
            UINT uConfigDescExtendedBytes = 0;

            while(uOffset + uConfigDescExtendedBytes < uDataBufferLen)
            {
                pusbCommon =
                   (PUSB_COMMON_DESCRIPTOR)(pDataBuffer + uOffset + uConfigDescExtendedBytes);

                if(pusbCommon->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE)
                {
                    uConfigDescExtendedBytes += pusbCommon->bLength;
                }
                else
                {
                    break;
                }
            }

            // next, copy Config usbConfigDescriptor's extended bytes
            if(uConfigDescExtendedBytes > 0)
            {
                rConfig.lpbExtended = new BYTE[uConfigDescExtendedBytes];

                if(rConfig.lpbExtended == NULL)
                {
                    goto configDescMemoryError;
                }

                rConfig.dwExtendedSize = uConfigDescExtendedBytes;
                if (memcpy_s(rConfig.lpbExtended, uConfigDescExtendedBytes, pDataBuffer + uOffset, uConfigDescExtendedBytes) != 0)
                {
                    goto configDescMemoryError;
                }

                uOffset += uConfigDescExtendedBytes;

            #ifdef DEBUG
                DumpExtendedBytes(rConfig.lpbExtended, uConfigDescExtendedBytes);
            #endif // DEBUG
            }
        }

        // second step - get the number of interfaces for this configuration
        // note - this isn't always the same as the bNumInterfaces field of
        // rConfig.usbConfigDescriptor, due to Alternate settings forInterfaces
        {
            UINT uX = 0; // temporary counter
            DEBUGCHK(rConfig.dwNumInterfaces == 0);

            while(uOffset + uX + sizeof(USB_INTERFACE_DESCRIPTOR) <= uDataBufferLen)
            {
                pusbCommon =(PUSB_COMMON_DESCRIPTOR)(pDataBuffer + uOffset + uX);

                if(pusbCommon->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE)
                {
                    rConfig.dwNumInterfaces++;
                }

                uX += pusbCommon->bLength;
            }
        }

        // next - create array forINTERFACE objects
        DEBUGCHK(rConfig.pnonConstUsbInterfaces == NULL);

        if(rConfig.dwNumInterfaces > 0)
        {
            // allocate this many interface objects
            rConfig.pnonConstUsbInterfaces = new NON_CONST_USB_INTERFACE[rConfig.dwNumInterfaces];

            if(rConfig.pnonConstUsbInterfaces == NULL)
            {
                goto configDescMemoryError;
            }

            memset(rConfig.pnonConstUsbInterfaces,
                0,
                rConfig.dwNumInterfaces * sizeof(NON_CONST_USB_INTERFACE));

            for(UCHAR bInterfaceNumber = 0;
                bInterfaceNumber < rConfig.dwNumInterfaces;
                bInterfaceNumber++)
            {
                NON_CONST_USB_INTERFACE & rInterface =
                    rConfig.pnonConstUsbInterfaces[bInterfaceNumber];
                rInterface.dwCount = sizeof(NON_CONST_USB_INTERFACE);

                // for each interface,
                // 1) Copy the interface descriptor
                // 2) Allocate and copy any extended bytes
                // 3) Allocate room for endpoints, if any
                // 4) Copy over endpoints -
                //      for each endpoint:
                //      a) copy the endpoint descriptor
                //      b) copy any extended bytes

                // we should now be pointing to a complete USB_INTERFACE_DESCRIPTOR
                DEBUGCHK(uOffset + sizeof(USB_CONFIGURATION_DESCRIPTOR) <= uDataBufferLen);
                PUSB_INTERFACE_DESCRIPTOR pusbInterfaceDesc =
                   (PUSB_INTERFACE_DESCRIPTOR)(pDataBuffer + uOffset);

                DEBUGCHK(pusbInterfaceDesc->bLength >=
                        sizeof(USB_INTERFACE_DESCRIPTOR) &&
                        pusbInterfaceDesc->bDescriptorType ==
                        USB_INTERFACE_DESCRIPTOR_TYPE);

                // 1) copy interface descriptor, and skip over it
                if (memcpy_s(&rInterface.usbInterfaceDescriptor,
                        pusbInterfaceDesc->bLength,
                        pusbInterfaceDesc,
                        sizeof(USB_INTERFACE_DESCRIPTOR)) != 0)
                {
                    goto configDescMemoryError;
                }
                uOffset += pusbInterfaceDesc->bLength;

            #ifdef DEBUG
                DumpInterfaceDescriptor(&rInterface.usbInterfaceDescriptor);
            #endif // DEBUG

                // 2) copy any extended info, if it exists
                {
                    UINT uInterfaceDescExtendedBytes = 0;

                    while(uOffset + uInterfaceDescExtendedBytes < uDataBufferLen)
                    {
                        pusbCommon =
                           (PUSB_COMMON_DESCRIPTOR)(pDataBuffer + uOffset + uInterfaceDescExtendedBytes);

                        if(pusbCommon->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE &&
                            pusbCommon->bDescriptorType != USB_ENDPOINT_DESCRIPTOR_TYPE &&
                            uOffset + uInterfaceDescExtendedBytes + pusbCommon->bLength <= uDataBufferLen)
                        {
                            uInterfaceDescExtendedBytes += pusbCommon->bLength;
                        }
                        else
                        {
                            break;
                        }
                    }

                    DEBUGCHK(rInterface.lpbExtended == NULL);

                    if(uInterfaceDescExtendedBytes > 0)
                    {
                        rInterface.lpbExtended = new BYTE[uInterfaceDescExtendedBytes];

                        if(rInterface.lpbExtended == NULL)
                        {
                            goto configDescMemoryError;
                        }

                        rInterface.dwExtendedSize = uInterfaceDescExtendedBytes;
                        if (memcpy_s(rInterface.lpbExtended,
                                 uInterfaceDescExtendedBytes,
                                 pDataBuffer + uOffset,
                                 uInterfaceDescExtendedBytes) != 0)
                        {
                            goto configDescMemoryError;
                        }

                        uOffset += uInterfaceDescExtendedBytes;

                    #ifdef DEBUG
                        DumpExtendedBytes(rInterface.lpbExtended, uInterfaceDescExtendedBytes);
                    #endif // DEBUG
                    }
                }

                // 3) allocate any endpoints
                DEBUGCHK(rInterface.pnonConstUsbEndpoints == NULL);

                if(rInterface.usbInterfaceDescriptor.bNumEndpoints == 0)
                {
                    continue; // continue interface loop
                }

                rInterface.pnonConstUsbEndpoints =
                    new NON_CONST_USB_ENDPOINT[rInterface.usbInterfaceDescriptor.bNumEndpoints];

                if(rInterface.pnonConstUsbEndpoints == NULL)
                {
                    goto configDescMemoryError;
                }

                memset(rInterface.pnonConstUsbEndpoints,
                        0,
                        rInterface.usbInterfaceDescriptor.bNumEndpoints * sizeof(NON_CONST_USB_ENDPOINT));

                for(UINT uEndpoint = 0;
                    uEndpoint < rInterface.usbInterfaceDescriptor.bNumEndpoints;
                    uEndpoint++)
                {
                    NON_CONST_USB_ENDPOINT & rEndpoint = rInterface.pnonConstUsbEndpoints[uEndpoint];
                    rEndpoint.dwCount = sizeof(NON_CONST_USB_ENDPOINT);

                    // should now be pointing at an endpoint descriptor
                    PUSB_ENDPOINT_DESCRIPTOR pusbEndpointDesc =
                       (PUSB_ENDPOINT_DESCRIPTOR)(pDataBuffer + uOffset);

                    DEBUGCHK(pusbEndpointDesc->bDescriptorType ==
                        USB_ENDPOINT_DESCRIPTOR_TYPE &&
                            pusbEndpointDesc->bLength >=
                            sizeof(USB_ENDPOINT_DESCRIPTOR));

                    // 4a) copy the endpoint descriptor
                    if (memcpy_s(&rEndpoint.usbEndpointDescriptor,
                             pusbEndpointDesc->bLength,
                             pDataBuffer + uOffset,
                             sizeof(USB_ENDPOINT_DESCRIPTOR)) != 0)
                    {
                       goto configDescMemoryError;
                    }
                    uOffset += pusbEndpointDesc->bLength;

                #ifdef DEBUG
                    DumpEndpointDescriptor(&rEndpoint.usbEndpointDescriptor);
                #endif // DEBUG

                    // 4b) copy any extended info, if it exists
                    UINT uEndpointDescExtendedBytes = 0;

                    while(uOffset + uEndpointDescExtendedBytes < uDataBufferLen)
                    {
                        pusbCommon =
                           (PUSB_COMMON_DESCRIPTOR)(pDataBuffer + uOffset + uEndpointDescExtendedBytes);

                        if(pusbCommon->bDescriptorType != USB_ENDPOINT_DESCRIPTOR_TYPE &&
                            pusbCommon->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE &&
                            uOffset + uEndpointDescExtendedBytes + pusbCommon->bLength <= uDataBufferLen)
                        {
                            uEndpointDescExtendedBytes += pusbCommon->bLength;
                        }
                        else
                        {
                            break;
                        }
                    }

                    DEBUGCHK(rEndpoint.lpbExtended == NULL);

                    if(uEndpointDescExtendedBytes > 0)
                    {
                        rEndpoint.lpbExtended = new BYTE[uEndpointDescExtendedBytes];

                        if(rEndpoint.lpbExtended == NULL)
                        {
                            goto configDescMemoryError;
                        }

                        rEndpoint.dwExtendedSize = uEndpointDescExtendedBytes;
                        if (memcpy_s(rEndpoint.lpbExtended,
                                 uEndpointDescExtendedBytes,
                                 pDataBuffer + uOffset,
                                 uEndpointDescExtendedBytes) != 0)
                        {
                            goto configDescMemoryError;
                        }

                        uOffset += uEndpointDescExtendedBytes;

                    #ifdef DEBUG
                        DumpExtendedBytes(rEndpoint.lpbExtended,
                                            uEndpointDescExtendedBytes);
                    #endif // DEBUG
                    }
                } // end endpoint for loop
            } // end interface for loop
        } // end check for interfaces
        else
        {
            goto configDescMemoryError;
        }

        fRetval = TRUE;
    }

    DEBUGMSG(ZONE_DESCRIPTORS && ZONE_VERBOSE,
           (TEXT("%s: -CDevice::CreateUsbConfigurationStructure - returning %d\n"),
            GetControllerName(),
            fRetval));

    return fRetval;

configDescMemoryError:

    DEBUGMSG(ZONE_ERROR,
           (TEXT("%s: -CDevice::CreateUsbConfigurationStructure - \
                  error creating USB Configuration Descriptors\r\n"),
            GetControllerName()));

    DeleteUsbConfigurationStructure(rConfig);

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: DeleteUsbConfigurationStructure
//
// Description: Free the memory associated with the rConfig structure.
//              This function is protected
//
// Parameters: rConfig - reference to NON_CONST_USB_CONFIGURATION struct to free
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DeleteUsbConfigurationStructure(IN NON_CONST_USB_CONFIGURATION& rConfig) const
{
    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
       (TEXT("%s: +CDevice::DeleteUsbConfigurationStructure\n"),
        GetControllerName()));

    // this code is right out of the destructor of OHCD.cpp
    if(rConfig.pnonConstUsbInterfaces)
    {
        DEBUGCHK(rConfig.usbConfigDescriptor.bDescriptorType ==
                USB_CONFIGURATION_DESCRIPTOR_TYPE &&
                rConfig.usbConfigDescriptor.bLength ==
                sizeof(USB_CONFIGURATION_DESCRIPTOR) &&
                rConfig.dwNumInterfaces >=
                rConfig.usbConfigDescriptor.bNumInterfaces);

        PNON_CONST_USB_INTERFACE lpInterface = rConfig.pnonConstUsbInterfaces;

        for(UINT uInterface = 0;
            uInterface < rConfig.dwNumInterfaces;
            ++uInterface, ++lpInterface)
        {
            if(lpInterface->pnonConstUsbEndpoints)
            {
                DEBUGCHK(lpInterface->usbInterfaceDescriptor.bDescriptorType ==
                    USB_INTERFACE_DESCRIPTOR_TYPE);

                PNON_CONST_USB_ENDPOINT lpEndpoint =
                    lpInterface->pnonConstUsbEndpoints;

                for(UINT uEndpoint = 0;
                    uEndpoint < lpInterface->usbInterfaceDescriptor.bNumEndpoints;
                    ++uEndpoint, ++lpEndpoint)
                {

                    delete [] lpEndpoint->lpbExtended;
                    lpEndpoint->lpbExtended = NULL;
                }

                delete [] lpInterface->pnonConstUsbEndpoints;
                lpInterface->pnonConstUsbEndpoints = NULL;
            }

            if (lpInterface->lpbExtended != NULL) 
            {
                delete [] lpInterface->lpbExtended;
                lpInterface->lpbExtended = NULL;
            }
        }

        delete [] rConfig.pnonConstUsbInterfaces;
        rConfig.pnonConstUsbInterfaces = NULL;
    }

    delete [] rConfig.lpbExtended;
    rConfig.lpbExtended = NULL;

    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: -CDevice::DeleteUsbConfigurationStructure\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: AllocatePipeArray
//
// Description: Allocate memory for the pipe array of this device based on
//              the number of endpoints given in the m_deviceInfo structure.
//              The entries of the array will be set to NULL by this function.
//
// Parameters: None
//
// Returns: TRUE if array allocated, else FALSE
//----------------------------------------------------------------------------------
BOOL CDevice::AllocatePipeArray(VOID)
{
    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: +CDevice(%s tier %d)::AllocatePipeArray\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber));

    BOOL fSuccess = FALSE;

    if ((m_deviceInfo.pnonConstUsbActiveConfig == NULL) ||
        (m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces == NULL))
    {
        return fSuccess;
    }

    EnterCriticalSection(&m_csDeviceLock);

    DEBUGCHK(m_ppCPipe == NULL && // shouldn't be allocated yet
             m_bMaxNumPipes == 0 && // shouldn't be allocated yet
             m_deviceInfo.pnonConstUsbActiveConfig != NULL &&
             m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces != NULL &&
             m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[0].usbInterfaceDescriptor.bNumEndpoints <= 15);

    // number of endpoints does not include the endpoint 0
    INT iNumPipes = 1;
    INT iNumberEndpoints = 0;
    INT iInterfaceNumber =
        m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[0].usbInterfaceDescriptor.bInterfaceNumber;
    INT iCurNumEndpoints;

    for(DWORD i = 0; i < m_deviceInfo.pnonConstUsbActiveConfig->dwNumInterfaces; i++)
    {
        if(iInterfaceNumber ==
            m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[i].usbInterfaceDescriptor.bInterfaceNumber)
        {
            iCurNumEndpoints =
                m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[i].usbInterfaceDescriptor.bNumEndpoints;

            if(iNumberEndpoints < iCurNumEndpoints)
            {
                iNumberEndpoints = iCurNumEndpoints;
            }
        }
        else
        {
            iInterfaceNumber =
                m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[i].usbInterfaceDescriptor.bInterfaceNumber;
            iNumPipes += iNumberEndpoints;
            iNumberEndpoints =
                m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[i].usbInterfaceDescriptor.bNumEndpoints;
        }
    }

    iNumPipes += iNumberEndpoints;

    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: CDevice(%s tier %d)::AllocatePipeArray - \
                  attempting to allocate %d pipes\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            iNumPipes));

    m_ppCPipe = new CPipeAbs* [iNumPipes];

    if(m_ppCPipe != NULL)
    {
        memset(m_ppCPipe, 0, iNumPipes * sizeof(CPipeAbs *));
        m_bMaxNumPipes =(UCHAR)iNumPipes;
        fSuccess = TRUE;
    }

    DEBUGMSG(ZONE_ERROR && !m_ppCPipe,
           (TEXT("%s: CDevice(%s tier %d)::AllocatePipeArray - no memory!\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber));

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_DEVICE && ZONE_VERBOSE,
           (TEXT("%s: -CDevice(%s tier %d)::AllocatePipeArray, \
                  returning BOOL %d\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            fSuccess));

    return fSuccess;
}

#ifdef DEBUG
//----------------------------------------------------------------------------------
// Function: DumpDeviceDescriptor
//
// Description: Print out the contents of the descriptor via DEBUGMSG
//              Used in debug mode only. See USB spec section 9.6.1
//
// Parameters: pDescriptor - pointer to descriptor
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DumpDeviceDescriptor(IN const PUSB_DEVICE_DESCRIPTOR pDescriptor) const
{
    DEBUGCHK(pDescriptor != NULL);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: +Dump USB_DEVICE_DESCRIPTOR\n"),
            GetControllerName()));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbLength = 0x%02x\n"),
            pDescriptor->bLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorType = 0x%02x\n"),
            pDescriptor->bDescriptorType));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbcdUSB = 0x%04x\n"),
            pDescriptor->bcdUSB));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDeviceClass = 0x%02x\n"),
            pDescriptor->bDeviceClass));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDeviceSubClass = 0x%02x\n"),
            pDescriptor->bDeviceSubClass));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDeviceProtocol = 0x%02x\n"),
            pDescriptor->bDeviceProtocol));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbMaxPacketSize0 = 0x%02x\n"),
            pDescriptor->bMaxPacketSize0));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tidVendor = 0x%04x\n"),
            pDescriptor->idVendor));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tidProduct = 0x%04x\n"),
            pDescriptor->idProduct));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbcdDevice = 0x%04x\n"),
            pDescriptor->bcdDevice));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tiManufacturer = 0x%02x\n"),
            pDescriptor->iManufacturer));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tiProduct = 0x%02x\n"),
            pDescriptor->iProduct));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tiSerialNumber = 0x%02x\n"),
            pDescriptor->iSerialNumber));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbNumConfigurations = 0x%02x\n"),
            pDescriptor->bNumConfigurations));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: -Dump USB_DEVICE_DESCRIPTOR\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: DumpConfigDescriptor
//
// Description: Print out the contents of the descriptor via DEBUGMSG
//              Used in debug mode only. See USB spec section 9.6.2
//
// Parameters: pDescriptor - pointer to descriptor
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DumpConfigDescriptor(IN const PUSB_CONFIGURATION_DESCRIPTOR pDescriptor) const
{
    DEBUGCHK(pDescriptor != NULL);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: +Dump USB_CONFIGURATION_DESCRIPTOR\n"),
            GetControllerName()));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbLength = 0x%02x\n"),
            pDescriptor->bLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorType = 0x%02x\n"),
            pDescriptor->bDescriptorType));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\twTotalLength = 0x%04x\n"),
            pDescriptor->wTotalLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbNumInterfaces = 0x%02x\n"),
            pDescriptor->bNumInterfaces));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbConfigurationValue = 0x%02x\n"),
            pDescriptor->bConfigurationValue));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tiConfiguration = 0x%02x\n"),
            pDescriptor->iConfiguration));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbmAttributes = 0x%02x\n"),
            pDescriptor->bmAttributes));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\tbmAttributes, Bus Powered = %d\n"),
            !!(pDescriptor->bmAttributes & BUS_POWERED)));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\tbmAttributes, Self Powered = %d\n"),
            !!(pDescriptor->bmAttributes & SELF_POWERED)));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\tbmAttributes, Remote Wakeup = %d\n"),
            !!(pDescriptor->bmAttributes & REMOTE_WAKEUP)));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tMaxPower = 0x%02x\n"),
            pDescriptor->MaxPower));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: -Dump USB_CONFIGURATION_DESCRIPTOR\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: DumpInterfaceDescriptor
//
// Description: Print out the contents of the descriptor via DEBUGMSG
//              Used in debug mode only. See USB spec section 9.6.3
//
// Parameters: pDescriptor - pointer to descriptor
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DumpInterfaceDescriptor(IN const PUSB_INTERFACE_DESCRIPTOR pDescriptor) const
{
    DEBUGCHK(pDescriptor != NULL);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: +Dump USB_INTERFACE_DESCRIPTOR\n"), 
            GetControllerName()));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbLength = 0x%02x\n"),
            pDescriptor->bLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorType = 0x%02x\n"),
            pDescriptor->bDescriptorType));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbInterfaceNumber = 0x%02x\n"),
            pDescriptor->bInterfaceNumber));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbAlternateSetting = 0x%02x\n"),
            pDescriptor->bAlternateSetting));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbNumEndpoints = 0x%02x\n"),
            pDescriptor->bNumEndpoints));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbInterfaceClass = 0x%02x\n"),
            pDescriptor->bInterfaceClass));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbInterfaceSubClass = 0x%02x\n"),
            pDescriptor->bInterfaceSubClass));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbInterfaceProtocol = 0x%02x\n"),
            pDescriptor->bInterfaceProtocol));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tiInterface = 0x%02x\n"),
            pDescriptor->iInterface));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: -Dump USB_INTERFACE_DESCRIPTOR\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: DumpEndpointDescriptor
//
// Description: Print out the contents of the descriptor via DEBUGMSG
//              Used in debug mode only. See USB spec section 9.6.4
//
// Parameters: pDescriptor - pointer to descriptor
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DumpEndpointDescriptor(IN const PUSB_ENDPOINT_DESCRIPTOR pDescriptor) const
{
    static const TCHAR* cszEndpointTypes[] =
    {
        TEXT("CONTROL"),
        TEXT("ISOCHRONOUS"),
        TEXT("BULK"),
        TEXT("INTERRUPT")
    };

    DEBUGCHK(pDescriptor != NULL);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: +Dump USB_ENDPOINT_DESCRIPTOR\n"),
            GetControllerName()));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbLength = 0x%02x\n"),
            pDescriptor->bLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorType = 0x%02x\n"),
            pDescriptor->bDescriptorType));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbEndpointAddress = 0x%02x\n"),
            pDescriptor->bEndpointAddress));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\tbEndpointAddress, endpoint # = %d\n"),
            pDescriptor->bEndpointAddress & TD_ENDPOINT_MASK));
    if(USB_ENDPOINT_DIRECTION_IN(pDescriptor->bEndpointAddress))
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
               (TEXT("\t\tbEndpointAddress, direction = %s\n"),
                TEXT("IN")));
    }
    else
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
               (TEXT("\t\tbEndpointAddress, direction = %s\n"),
                TEXT("OUT")));
    }
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbmAttributes = 0x%02x\n"),
            pDescriptor->bmAttributes));
    DEBUGCHK((pDescriptor->bmAttributes & USB_ENDPOINT_TYPE_MASK) < 4);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\tbmAttributes, endpoint type = %s\n"),
            cszEndpointTypes[pDescriptor->bmAttributes & USB_ENDPOINT_TYPE_MASK]));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\twMaxPacketSize = 0x%04x\n"),
            pDescriptor->wMaxPacketSize));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbInterval = 0x%02x\n"),
            pDescriptor->bInterval));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: -Dump USB_ENDPOINT_DESCRIPTOR\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: DumpExtendedBytes
//
// Description: Print out the bytes of pByteArray Used in debug mode only.
//
// Parameters: pByteArray - array of extended bytes fora descriptor
//
//             dwSize - number of entries in pByteArray
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CDevice::DumpExtendedBytes(IN BYTE const*const pByteArray, IN const DWORD dwSize) const
{
    DEBUGCHK(pByteArray != NULL && dwSize > 0);
    DEBUGMSG(ZONE_DESCRIPTORS,
       (TEXT("%s: +Dump extended bytes, size = %d\n"),
        GetControllerName(),
        dwSize));

    for(DWORD dwPrinted = 0; dwPrinted < dwSize; dwPrinted += 4)
    {
        DWORD dwFourBytes = 0;

        for(UCHAR bIndex = 0; bIndex < 4; bIndex++)
        {
            dwFourBytes <<= 8;

            if(dwPrinted + bIndex < dwSize)
            {
                dwFourBytes |= pByteArray[dwPrinted + bIndex];
            }
        }

        DEBUGMSG(ZONE_DESCRIPTORS,
               (TEXT("\tBytes %d to %d = 0x%08x\n"),
                dwPrinted + 1,
                dwPrinted + 4,
                dwFourBytes));
    }

    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("%s: -Dump extended bytes, size = %d\n"),
            GetControllerName(),
            dwSize));
}
#endif // DEBUG

//----------------------------------------------------------------------------------
// Function: CHub
//
// Description: Constructor forCHub. Do not initialize static variables here.
//              Do that in the Initialize() routine
//
// Parameters: bAddress, rDeviceInfo, bSpeed, bTierNumber - see CDevice::CDevice
//
//             rUsbHubDescriptor - USB descriptor for a hub
//
// Returns: None
//----------------------------------------------------------------------------------
CHub::CHub(IN const UCHAR bAddress,
           IN const USB_DEVICE_INFO& rDeviceInfo,
           IN const UCHAR bSpeed,
           IN const UCHAR bTierNumber,
           IN const USB_HUB_DESCRIPTOR& rUsbHubDescriptor,
           IN CHcd * const pCHcd ,
           IN CHub * const pAttachedHub,
           const UCHAR bAttachedPort):CDevice(bAddress,
                                                rDeviceInfo,
                                                bSpeed,
                                                bTierNumber,
                                                pCHcd,
                                                pAttachedHub,
                                                bAttachedPort),
                                                    m_pCHcd(pCHcd), // call base class constructor
                                                    m_usbHubDescriptor(rUsbHubDescriptor), // USB descriptor of this hub
                                                    m_ppCDeviceOnPort(NULL), // dynamic array of pointers to the devices on this hub's ports
                                                    m_fHubThreadClosing(FALSE), // indicator to thread that it should close
                                                    m_hHubStatusChangeEvent(NULL), // event for hub status change thread
                                                    m_hHubStatusChangeThread(NULL), // checks for connect changes on the hub's ports
                                                    m_pAddedTT(NULL)
{
    UINT i;

    // Manual Reset Event;
    m_hHubSuspendBlockEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    for (i = 0; i < MAX_PORT_NUMBER; i++) 
    {
        m_fPortFirstConnect [i] = TRUE;
    }
}

//----------------------------------------------------------------------------------
// Function: ~CHub
//
// Description: Destructor forCHub. Do not delete static variables here. Do that in
//              DeInitialize();
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CHub::~CHub()
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: +CHub::~CHub\n"),
            GetControllerName()));

    // this should have been taken care of in HandleDetach,
    // or ifEnterOperationalState failed.
    DEBUGCHK(m_hHubStatusChangeEvent == NULL);
    DEBUGCHK(m_hHubStatusChangeThread == NULL);

#ifdef DEBUG
    if(m_ppCDeviceOnPort != NULL)
    {
        for(UCHAR bPort = 0; bPort < m_usbHubDescriptor.bNumberOfPorts; bPort++)
        {
            // devices should have been freed by HandleDetach
            DEBUGCHK(m_ppCDeviceOnPort[bPort] == NULL);
        }
    }
#endif // DEBUG

    if(m_ppCDeviceOnPort)
    {
        delete [] m_ppCDeviceOnPort;
        m_ppCDeviceOnPort = NULL;
    }

    if(m_pAddedTT)
    {
        delete m_pAddedTT;
        m_pAddedTT = NULL;
    }

    if(m_hHubSuspendBlockEvent)
    {
        CloseHandle(m_hHubSuspendBlockEvent);
    }

    // nothing to do with m_usbHubDescriptor
    // nothing to do with m_fHubThreadClosing
    // rest of work done in ~CDevice

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: -CHub::~CHub\n"),
            GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: HubStatusChangeThreadStub
//
// Description: Stub function for starting HubStatusChangeThread
//
// Parameters: pContext - pointer to descendant of CHub which contains
//             the actual HubStatusChangeThread function
//
// Returns: See HubStatusChangeThread
//----------------------------------------------------------------------------------
DWORD CALLBACK CHub::HubStatusChangeThreadStub(IN PVOID pContext)
{
    return(static_cast<CHub*> (pContext))->HubStatusChangeThread();
}

//----------------------------------------------------------------------------------
// Function: DelayForFirstConnect
//
// Description: Delay first connection for HIGH_SPEED device to get 
//              time for SUPER_SPEED device initializtion (ORIENT device workaround)
//
// Parameters: bPort - port number
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHub::DelayForFirstConnect (UCHAR bPort)
{
    DWORD dwTickCount = GetTickCount ();

    if ((dwTickCount - GetStartTime()) > ORIENT_TIMEOUT) 
    {
        return;
    }

    if ((bPort > 0) && (bPort <= MAX_PORT_NUMBER)) 
    {
        if (m_fPortFirstConnect [bPort - 1]) 
        {
            USB_HUB_AND_PORT_STATUS hubStatus2;

            if(GetStatus(bPort, hubStatus2) != FALSE)
            {
                if(hubStatus2.status.port.usDeviceSpeed != USB_SUPER_SPEED) 
                {
                    //reset to get port speed
                    if(ResetAndEnablePort(bPort))
                    {
                        if(hubStatus2.status.port.usDeviceSpeed == USB_HIGH_SPEED) 
                        {
                            Sleep (ORIENT_TIMEOUT);
                        }
                        m_fPortFirstConnect [bPort - 1] = FALSE;
                    }
                }
                else 
                {
                    m_fPortFirstConnect [bPort - 1] = FALSE;
                }
            } 
        }
    }
}


//----------------------------------------------------------------------------------
// Function: HubStatusChangeThread
//
// Description: Main hub thread for handling changes to the hub's ports.
//              This routine needs to work for both root/external hubs
//
// Parameters: None
//
// Returns: 0 on thread exit
//----------------------------------------------------------------------------------
DWORD CHub::HubStatusChangeThread(VOID)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: +CHub(%s tier %d)::HubStatusChangeThread\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber));

    DEBUGCHK(m_hHubStatusChangeEvent != NULL && m_hHubStatusChangeThread != NULL);

    UCHAR                       bPort;
    USB_HUB_AND_PORT_STATUS     hubStatus;
    BOOL                        fSuccess = FALSE;

    // before we can process port changes, we need to power all ports
    while(!m_fHubThreadClosing && !fSuccess)
    {
        fSuccess = PowerAllHubPorts();
    }

    if(!m_fHubThreadClosing)
    {
        // According to the USB spec 1.1, section 7.1.7.1, there
        // is supposed to be a delay of up to 100ms(t2) before the device
        // can signal attach. I don't know if the software is
        // supposed to implement this delay. No harm in implementing
        // it though.
        Sleep(100 + 2 * m_usbHubDescriptor.bPowerOnToPowerGood);
    }

    SetOrClearRemoteWakup(TRUE);

    while(!m_fHubThreadClosing)
    {
//suspend could be done here
//        fSuccess =
//           (WaitForSingleObject(m_hHubSuspendBlockEvent,INFINITE) == WAIT_OBJECT_0);

        if(m_fHubThreadClosing)
        {
            break;
        }

        fSuccess = WaitForPortStatusChange(bPort, hubStatus);

        if(m_fHubThreadClosing || !fSuccess)
        {
            if(!fSuccess &&
                !m_fHubThreadClosing &&
               (m_pCHcd->GetCapability() & HCD_SUSPEND_ON_REQUEST) != 0)
            {
                // We need check to find out this hub need put into suspend mode.
                PREFAST_ASSERT(m_ppCDeviceOnPort!=NULL);
                EnterCriticalSection(&m_csDeviceLock);
                BOOL fDoSuspend = TRUE;

                for(UCHAR bPort2 = 1;
                    bPort2 <= m_usbHubDescriptor.bNumberOfPorts;
                    bPort2++)
                {
                    // Can not.
                    if(m_ppCDeviceOnPort [bPort2 -1] != NULL)
                    {
                        fDoSuspend = FALSE;
                        break;
                    }
                }

                LeaveCriticalSection(&m_csDeviceLock);

                if(fDoSuspend)
                {
                    BOOL fSuspend;

                    if(m_pAttachedHub != NULL)
                    {
                        fSuspend =
                           (m_pAttachedHub->SuspendResumeOffStreamDevice(m_bAddress,
                                                                            TRUE) == REQUEST_OK);
                    }
                    else
                    {
                        fSuspend = m_pCHcd->SuspendHC();
                    }

                    if(fSuspend)
                    {
                        DEBUGMSG(ZONE_HUB,
                               (TEXT("%s: CHub(%s tier %d):: \
                                      Suspend Device(%d) return %s!\n"),
                                GetControllerName(),
                                GetDeviceType(),
                                m_bTierNumber,
                                m_bAddress,
                                TEXT("Success")));

                        m_fIsSuspend = TRUE;
                        // Stop this thread.
                        ResetEvent(m_hHubSuspendBlockEvent);
                    }
                    else
                    {
                        DEBUGMSG(ZONE_HUB,
                               (TEXT("%s: CHub(%s tier %d):: \
                                      Suspend Device(%d) return %s!\n"),
                                GetControllerName(),
                                GetDeviceType(),
                                m_bTierNumber,
                                m_bAddress,
                                TEXT("FAIL")));
                    }
                }

            }

            else
            {
                DEBUGMSG(ZONE_ERROR && !m_fHubThreadClosing,
                       (TEXT("%s: CHub(%s tier %d)::HubStatusChangeThread - \
                              error reading port status change\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber));
            }

            continue; // loop will exit ifm_fHubThreadClosing is set
        }

        // Port 0 indicate this is hub status.
        if(bPort == 0)
        {
            if(hubStatus.change.hub.usOverCurrentIndicatorChange)
            {
                if(hubStatus.status.hub.usOverCurrentIndicator)
                {
                    RETAILMSG(1,
                           (TEXT("CHub(tier %d)::HubStatusChangeThread - \
                                  addr %d port %d over current!\n"),
                            m_bTierNumber,
                            m_bAddress,
                            bPort));
                }
                else
                {
                    // hub is no longer over current - re-enumerate all ports
                    // We should re-enumerate all hub ports during hub 
                    // over-current recovery
                }
            }

            continue;
        }

        //if port was disabled we do not reset it
        if(hubStatus.status.port.usPortEnabled &&
           hubStatus.status.port.usPortConnected &&
           !hubStatus.change.port.usConnectStatusChange)
        {
            BOOL fDeviceIsPresent;

            EnterCriticalSection(&m_csDeviceLock);
            DEBUGCHK(m_ppCDeviceOnPort != NULL);
            fDeviceIsPresent =(m_ppCDeviceOnPort[bPort - 1] != NULL);
            LeaveCriticalSection(&m_csDeviceLock);

            if(!fDeviceIsPresent)
            {
                hubStatus.change.port.usConnectStatusChange = 1;
            }
        }

        // we will get here if the status of port # "port" has changed.
        // the status information will be in "hubStatus"
        DEBUGCHK(bPort <= m_usbHubDescriptor.bNumberOfPorts);
        DEBUGMSG(ZONE_ATTACH,
               (TEXT("%s: CHub(%s tier %d)::HubStatusChangeThread - \
                      port %d, change = 0x%04x, status = 0x%04x\n"),
                GetControllerName(),
                GetDeviceType(),
                m_bTierNumber,
                bPort,
                hubStatus.change.wWord,
                hubStatus.status.wWord));

        if(hubStatus.change.port.usOverCurrentChange)
        {
            if(hubStatus.status.port.usPortOverCurrent)
            {
                RETAILMSG(1,
                       (TEXT("CHub(tier %d)::HubStatusChangeThread - \
                              addr %d port %d over current!\n"),
                        m_bTierNumber,
                        m_bAddress,
                        bPort));

                DetachDevice(bPort);

  // the "correct" thing to do, according to my reading of the USB spec
                SetOrClearFeature(bPort,
                                    USB_REQUEST_CLEAR_FEATURE,
                                    USB_HUB_FEATURE_PORT_POWER);
            }
            else
            {
                // port is no longer over current - pretend this is a normal attach
                // simulate a connect status change. this has the undesirable but 
                // basically harmless side effect of wasting 100 ms to needlessly
                // debounce the power rail.
                hubStatus.change.port.usConnectStatusChange = 1;
            }
        }

        // Resume Notification.
        EnterCriticalSection(&m_csDeviceLock);

        if(hubStatus.change.port.usSuspendChange &&
            !hubStatus.status.port.usPortSuspended  &&
            m_ppCDeviceOnPort[bPort-1] != NULL)
        {
            m_ppCDeviceOnPort[bPort-1]->ResumeNotification();
        }

        LeaveCriticalSection(&m_csDeviceLock);

        if(hubStatus.change.port.usPortEnableChange &&
            !hubStatus.status.port.usPortEnabled &&
            hubStatus.status.port.usPortConnected)
        {
            // Connected device has become disabled. If the device was
            // already successfully attached, let's try detach/reattach.
            // It is important to check that the device was successfully
            // attached - otherwise, we can get into an infinite loop
            // of try attach, fail, disable port, retry attach.
            BOOL fDeviceIsPresent;

            EnterCriticalSection(&m_csDeviceLock);
            DEBUGCHK(m_ppCDeviceOnPort != NULL);
            fDeviceIsPresent =(m_ppCDeviceOnPort[bPort - 1] != NULL);
            LeaveCriticalSection(&m_csDeviceLock);

            if(fDeviceIsPresent)
            {
                DEBUGMSG(ZONE_WARNING,
                       (TEXT("%s: CHub(%s tier %d)::HubStatusChangeThread - \
                                device on port %d \
                                is connected but has been disabled. \
                                Trying to detach & re-attach\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        bPort));
                DetachDevice(bPort);
                // this will cause device attach below, since
                // hubStatus.status.port.usPortConnected is already set
                hubStatus.change.port.usConnectStatusChange = 1;
                DEBUGCHK(hubStatus.status.port.usPortConnected);
            }
        } // we can ignore all other enabled changes

        // now check for connect changes
        if(hubStatus.change.port.usConnectStatusChange)
        {
            EnterCriticalSection(&m_csDeviceLock);
            BOOL fDeviceAlreadyExists =(m_ppCDeviceOnPort[bPort - 1] != NULL);
            LeaveCriticalSection(&m_csDeviceLock);

            // we got a connect status change notification on this port, so...
            if(fDeviceAlreadyExists)
            {
                // ... a change when the device is already here must be a detach;
                //     if there's still something connected then it must be new.
                DEBUGMSG(ZONE_ATTACH,
                       (TEXT("%s: CHub(%s tier %d)::HubStatusChangeThread - \
                              device detached on port %d\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        bPort));

                DetachDevice(bPort);
#ifdef DEBUG
                if(hubStatus.status.port.usPortConnected)
                {
                    DEBUGMSG((ZONE_WARNING && ZONE_VERBOSE) || ZONE_ATTACH,
                           (TEXT("CHub(%s tier %d)::HubStatusChangeThread -")
                            TEXT(" quick detach and re-attach on port %d\n"),
                            GetDeviceType(),
                            m_bTierNumber,
                            bPort));
                }
#endif // DEBUG
            }
            // ... a change with no device present must be an attach
            //     but section 7.1.7.1 of the USB 1.1 spec says we're
            //     responsible for de-bouncing the attach signaling.
            //
            // we do the de-bouncing by waiting until a 100 ms interval
            //(t3 on figure 7-19 in the spec) elapses with no connection
            // status change on the port. Then we can examine the current
            // connect status reliably.
            BOOL fPoll = TRUE;

            if (hubStatus.status.port.usDeviceSpeed != USB_SUPER_SPEED) 
            {
                DelayForFirstConnect (bPort);
            }

            while(fPoll)
            {
                USB_HUB_AND_PORT_STATUS hubStatus2;

                Sleep(100);

                if(GetStatus(bPort, hubStatus2) == FALSE)
                {
                    // failed to get status; probably power-cycle or
                    // upper-level detach
                    hubStatus.status.port.usPortConnected = FALSE;
                    fPoll = FALSE;
                }
                else if(hubStatus2.change.port.usConnectStatusChange)
                {
                    // ack the status change and wait again
                    SetOrClearFeature(bPort,
                                        USB_REQUEST_CLEAR_FEATURE,
                                        USB_HUB_FEATURE_C_PORT_CONNECTION);
                }
                else if(hubStatus2.change.port.usResetChange)
                {
                    // ack the status change and wait again
                    SetOrClearFeature(bPort,
                                        USB_REQUEST_CLEAR_FEATURE,
                                        USB_HUB_FEATURE_C_PORT_RESET);
                }
                else
                {
                    // ah, stability.
                    hubStatus.status.wWord = hubStatus2.status.wWord;
                    fPoll = FALSE;
                }
            }

            // If there are no devices on a port, need to skip the rest of
            // device attach process, and return to wait for next hub status
            // change event.
            if(!hubStatus.status.port.usPortConnected)
            {
                continue;
            }

            // Resetting the port to find whether the device is LS/FS or HS is moved
            // inside AttachDevice as reseting the port requires address 0 lock.

            if(hubStatus.status.port.usDeviceSpeed == USB_SUPER_SPEED)
            {
                AttachDevice(bPort,
                            hubStatus.status.port.usDeviceSpeed,
                            TRUE,
                            FALSE);
            }
            else
            {
                AttachDevice(bPort);
            }
        } // end of usConnectStatusChange processing
    }

    DEBUGMSG(ZONE_HUB,
           (TEXT("%s: -CHub(%s tier %d)::HubStatusChangeThread, \
                  THREAD EXITING, returning 0\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber));

    return 0;
}

//----------------------------------------------------------------------------------
// Function: DisableDevice
//
// Description: Disable Downstream Device with address "address"
//
// Parameters: See description in CHcd::DisableDevice.
//
// Returns: REQUEST_OK - ifDevice Reset
//
//         REQUEST_FAILED - if device exists, but unable to disable it.
//
//         REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CDevice::DisableDevice(IN const UINT uAddress, IN const BOOL fReset)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: CHub(%s tier %d)::DisableDevice - \
                  address = %d, uPipeIndex = %d\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            uAddress,
            fReset));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // If it is this device
    if(uAddress == m_bAddress)
    {
        if (m_pAttachedHub->DisableOffStreamDevice(uAddress, fReset))
        {
            status = REQUEST_OK;
        }
    }

    return status;
}

//----------------------------------------------------------------------------------
// Function: SuspendResume
//
// Description: Suspend or Resume on device with address "address"
//
// Parameters: See description in CHcd::SuspendResume
//
// Returns: REQUEST_OK - if device suspend or resumed
//
//         REQUEST_FAILED - if device exists, but unable to reset it
//
//         REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CDevice::SuspendResume(IN const UINT uAddress,
                                            IN const BOOL fSuspend)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: +CHub(%s tier %d)::SuspendResume - \
                  address = %d, fSuspend = %d\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            uAddress,
            fSuspend));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    if(uAddress == m_bAddress)
    {
        if(m_pAttachedHub->SuspendResumeOffStreamDevice(uAddress, fSuspend))
        {
            m_fIsSuspend=fSuspend;
            status = REQUEST_OK;
        }
        else
        {
            status = REQUEST_FAILED;
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
           (TEXT("%s: -CHub(%s tier %d)::SuspendResume - \
                  address = %d, returing HCD_REQUEST_STATUS %d\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            uAddress,
            status));

    return status;
}

#ifdef DEBUG
//----------------------------------------------------------------------------------
// Function: DumpHubDescriptor
//
// Description: Print out the contents of the descriptor via DEBUGMSG.
//              Used in debug mode only. Refer to USB spec 1.1, Section 11.15.2.1
//
// Parameters: pDescriptor - pointer to descriptor
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHub::DumpHubDescriptor(IN const PUSB_HUB_DESCRIPTOR pDescriptor)
{
    PREFAST_DEBUGCHK(pDescriptor != NULL);
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("+Dump USB_HUB_DESCRIPTOR\n")));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorLength = 0x%02x\n"),
            pDescriptor->bDescriptorLength));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbDescriptorType = 0x%02x\n"),
            pDescriptor->bDescriptorType));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbNumberOfPorts = 0x%02x\n"),
            pDescriptor->bNumberOfPorts));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\twHubCharacteristics = 0x%04x\n"),
            pDescriptor->wHubCharacteristics));
    if(pDescriptor->wHubCharacteristics & USB_HUB_CHARACTERISTIC_NO_POWER_SWITCHING)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
               (TEXT("\t\twHubCharacteristics, No Port Power Switching\n")));
    }
    else if(pDescriptor->wHubCharacteristics &
                USB_HUB_CHARACTERISTIC_INDIVIDUAL_POWER_SWITCHING)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Individual Port Power Switching\n")));
    }
    else
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Ganged Port Power Switching\n")));
    }

    // DEBUGMSG(ZONE_DESCRIPTORS,
    //         (TEXT("\t\twHubCharacteristics, Hub %s part of compound device\n"),
    //         ((pDescriptor->wHubCharacteristics & USB_HUB_CHARACTERISTIC_PART_OF_COMPOUND_DEVICE) ? TEXT("IS") : TEXT("IS NOT"))));
    if(pDescriptor->wHubCharacteristics &
            USB_HUB_CHARACTERISTIC_PART_OF_COMPOUND_DEVICE)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Hub %s part of compound device\n"),
            TEXT("IS")));
    }
    else
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Hub %s part of compound device\n"),
            TEXT("IS NOT")));
    }

    if(pDescriptor->wHubCharacteristics &
        USB_HUB_CHARACTERISTIC_NO_OVER_CURRENT_PROTECTION)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, No Over Current Protection\n")));
    }
    else if(pDescriptor->wHubCharacteristics &
            USB_HUB_CHARACTERISTIC_INDIVIDUAL_OVER_CURRENT_PROTECTION)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Individual Over Current Protection\n")));
    }
    else
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\t\twHubCharacteristics, Global Over Current Protection\n")));
    }
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbPowerOnToPowerGood = 0x%02x\n"),
            pDescriptor->bPowerOnToPowerGood));
    DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tbHubControlCurrent = 0x%02x\n"),
            pDescriptor->bHubControlCurrent));

    const UCHAR numBytes = 1 +(pDescriptor->bNumberOfPorts >> 3);

    for(UCHAR bOffset = 0; bOffset < numBytes; bOffset++)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tDeviceRemovable bitmask byte #%d = 0x%02x\n"),
            bOffset + 1,
            pDescriptor->bRemoveAndPowerMask[bOffset]));
    }

    for(bOffset = numBytes; bOffset < 2 * numBytes; bOffset++)
    {
        DEBUGMSG(ZONE_DESCRIPTORS,
           (TEXT("\tPortPwrCtrlMask bitmask byte #%d = 0x%02x\n"),
            bOffset - numBytes + 1,
            pDescriptor->bRemoveAndPowerMask[bOffset]));
    }

    DEBUGMSG(ZONE_DESCRIPTORS,(TEXT("-Dump USB_HUB_DESCRIPTOR\n")));
}
#endif // DEBUG

//----------------------------------------------------------------------------------
// Function: AttachDevice
//
// Description: This function is called when a new device is attached
//              on port "port". After this procedure finishes, a configured
//              device will be added to the hub's port array.
//              If this function fails, the port will be disabled
//
// Parameters: bPort - indicates hub port on which device was attached
//
//             bSpeed - indicates the device speed
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHub::AttachDevice(IN const UCHAR bPort,
                        IN UCHAR bSpeed,
                        IN const BOOL fSyncronously,
                        IN const BOOL fResetForSpeedInfo)
{
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: +CHub(%s tier %d)::AttachDevice - port = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        bPort));

    DEBUGCHK(bPort > 0 && bPort <= m_usbHubDescriptor.bNumberOfPorts);

    // device related variables
    CDevice*                pNewDevice = NULL;
    UCHAR                   bAddress = 0; // illegal address(slot ID)
    USB_DEVICE_INFO         deviceInfo = {0};
    USB_HUB_DESCRIPTOR      usbHubDescriptor = {0};
    CPipeAbs*               pControlPipe = NULL; // pipe to device's endpoint 0

    // setup process related variables
    DEVICE_CONFIG_STATUS    configStatus;
    UCHAR                   bConfigFailures = 0;
    UINT                    uCurrentConfigDescriptorIndex = 0;
    BOOL                    fPipeHalted = FALSE;
    PVOID                   pLogicalDevice = NULL;

    deviceInfo.dwCount = sizeof(USB_DEVICE_INFO);
    deviceInfo.pnonConstUsbActiveConfig = NULL;
    deviceInfo.pnonConstUsbConfigs = NULL;

    if(fResetForSpeedInfo)
    {
        configStatus = DEVICE_CONFIG_RESET_AND_ENABLEPORT_FOR_SPEEDINFO;
    }
    else
    {
        configStatus = DEVICE_CONFIG_STATUS_OPENING_ENDPOINT0_PIPE;
    }

    while(configStatus != DEVICE_CONFIG_STATUS_DONE)
    {
        if(m_fHubThreadClosing || fPipeHalted || bConfigFailures > 2)
        {
            configStatus = DEVICE_CONFIG_STATUS_FAILED;
        }

        DEBUGMSG(ZONE_ATTACH,
           (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                  status = %s, failures = %d\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber,
            StatusToString(configStatus),
            bConfigFailures));

        switch(configStatus)
        {
        case DEVICE_CONFIG_STATUS_OPENING_ENDPOINT0_PIPE:
        {
            DEBUGCHK(pControlPipe == NULL);
            USB_ENDPOINT_DESCRIPTOR usbEndpointZeroDescriptor = {0};

            usbEndpointZeroDescriptor.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
            usbEndpointZeroDescriptor.bEndpointAddress = 0;
            // usbEndpointZeroDescriptor.bInterval = ; <- ignored forcontrol pipes
            usbEndpointZeroDescriptor.bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
            usbEndpointZeroDescriptor.bmAttributes = USB_ENDPOINT_TYPE_CONTROL;
            usbEndpointZeroDescriptor.wMaxPacketSize = ENDPOINT_ZERO_MIN_MAXPACKET_SIZE;
            UCHAR bTTHubAddr = m_bAddress;
            UCHAR bTTHubPort = bPort;
            PVOID pTTContext = NULL;

            GetUSB2TT(bTTHubAddr, bTTHubPort, pTTContext);

            pControlPipe = CreateControlPipe(&usbEndpointZeroDescriptor,
                                                bSpeed,
                                                0,
                                                bTTHubAddr,
                                                bTTHubPort,
                                                pTTContext,
                                                m_pCHcd);

            if(pControlPipe != NULL && pControlPipe->OpenPipe() == REQUEST_OK)
            {
                // success
                configStatus = DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                       (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                              failure on %s step, unable to open control pipe\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        StatusToString(configStatus)));

                bConfigFailures++;
                delete pControlPipe;
                pControlPipe = NULL;
            }

            break;
        }

        case DEVICE_CONFIG_RESET_AND_ENABLEPORT_FOR_SPEEDINFO:
        {
            USB_HUB_AND_PORT_STATUS hubStatus;

            configStatus = DEVICE_CONFIG_STATUS_DONE;

            // We have to reset port and get speed information.
            // When an LS/FS device is connected to root hub, we need to stop
            // XHCI device connection, and wait next hub status event.
            if(ResetAndEnablePort(bPort))
            {
                Sleep(20);
                GetStatus(bPort, hubStatus);

                if(hubStatus.status.port.usPortConnected)
                {
                    bSpeed = hubStatus.status.port.usDeviceSpeed;
                    configStatus = DEVICE_CONFIG_STATUS_OPENING_ENDPOINT0_PIPE;

                    DEBUGMSG(ZONE_ATTACH,
                       (TEXT("%s: +CHub(%s tier %d)::AttachDevice - \
                              port = %d, bSpeed = %d\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        bPort,
                        bSpeed));
                }
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT:
        {
            if(ResetAndEnablePort(bPort))
            {
                configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_ENABLE_SLOT;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, unable to reset/enable port\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_ENABLE_SLOT:
        {
            pLogicalDevice = EnableSlot(bAddress);
            if (pLogicalDevice != NULL)
            {
                if(bAddress != 0)
                {
                    pControlPipe->SetDevicePointer (pLogicalDevice );
                    configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_ADDRESS_DEVICE;
                }
                else
                {
                    DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                       (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                              failure on %s step, unable to allocate device\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        StatusToString(configStatus)));

                    bConfigFailures++;
                }
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                    failure on %s step, unable to allocate device\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_ADDRESS_DEVICE:
        {
            if(AddressDevice(bAddress, bPort, bSpeed))
            {
                 configStatus =
                     DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR_TEST;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, unable to allocate device\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_RESET_DEVICE:
        {
            if(ResetDevice(bAddress))
            {
                configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR_TEST;
            }
            else
            {
                pControlPipe->IsPipeHalted(&fPipeHalted);
                configStatus = DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT;
                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR_TEST:
        {
            if(GetDescriptor(pControlPipe,
                             bAddress,
                             USB_DEVICE_DESCRIPTOR_TYPE,
                             0, // descriptor index
                             ENDPOINT_ZERO_MIN_MAXPACKET_SIZE,
                             &deviceInfo.usbDeviceDescriptor))
            {
                // success
                configStatus =  DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_DEVICE_DESCRIPTOR;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
                if (fPipeHalted) {
                    configStatus = DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT;
                } else {
                    configStatus =
                        DEVICE_CONFIG_STATUS_SCHEDULING_RESET_DEVICE;
                }
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_DEVICE_DESCRIPTOR:
        {
            if(GetDescriptor(pControlPipe,
                                bAddress,
                                USB_DEVICE_DESCRIPTOR_TYPE,
                                0, // descriptor index
                                ENDPOINT_ZERO_MIN_MAXPACKET_SIZE,
                                &deviceInfo.usbDeviceDescriptor))
            {
                DEBUGCHK(m_bTierNumber <= USB_MAXIMUM_HUB_TIER);

                if(m_bTierNumber == USB_MAXIMUM_HUB_TIER &&
                     deviceInfo.usbDeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB)
                {
                    RETAILMSG(1,
                       (TEXT("USB specification does not allow more than %d hubs in a row\n"),
                        USB_MAXIMUM_HUB_TIER));

                    configStatus = DEVICE_CONFIG_STATUS_FAILED;
                }
                else
                {
                    // success
                    if(deviceInfo.usbDeviceDescriptor.bMaxPacketSize0 >
                        ENDPOINT_ZERO_MIN_MAXPACKET_SIZE)
                    {
                        pControlPipe->ChangeMaxPacketSize(deviceInfo.usbDeviceDescriptor.bMaxPacketSize0);
                    }

                    configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR;
                }
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR:
        {
            if(GetDescriptor(pControlPipe,
                                bAddress,
                                USB_DEVICE_DESCRIPTOR_TYPE,
                                0, // descriptor index
                                sizeof(deviceInfo.usbDeviceDescriptor),
                                &deviceInfo.usbDeviceDescriptor))
            {
                // success
                configStatus =
                    DEVICE_CONFIG_STATUS_SETUP_CONFIGURATION_DESCRIPTOR_ARRAY;
            #ifdef DEBUG
                DumpDeviceDescriptor(&deviceInfo.usbDeviceDescriptor);
            #endif // DEBUG
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                       (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                              failure on %s step\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SETUP_CONFIGURATION_DESCRIPTOR_ARRAY:
        {
            DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig == NULL &&
                      deviceInfo.pnonConstUsbConfigs == NULL);

            const UINT uNumConfigurations =
                deviceInfo.usbDeviceDescriptor.bNumConfigurations;

            deviceInfo.pnonConstUsbConfigs =
                new NON_CONST_USB_CONFIGURATION[uNumConfigurations];

            if(deviceInfo.pnonConstUsbConfigs != NULL)
            {
                memset(deviceInfo.pnonConstUsbConfigs,
                    0,
                    uNumConfigurations * sizeof(NON_CONST_USB_CONFIGURATION));

                for(UINT config = 0; config < uNumConfigurations ; config++)
                {
                    deviceInfo.pnonConstUsbConfigs[config].dwCount =
                        sizeof(NON_CONST_USB_CONFIGURATION);
                }

                uCurrentConfigDescriptorIndex = 0;
                configStatus =
                    DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_CONFIG_DESCRIPTOR;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, no memory\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_CONFIG_DESCRIPTOR:
        {
            DEBUGCHK(uCurrentConfigDescriptorIndex <
                deviceInfo.usbDeviceDescriptor.bNumConfigurations);
            DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig == NULL &&
                      deviceInfo.pnonConstUsbConfigs != NULL);

            if(GetDescriptor(pControlPipe,
                                bAddress,
                                USB_CONFIGURATION_DESCRIPTOR_TYPE,
                               (UCHAR)uCurrentConfigDescriptorIndex,
                                sizeof(deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor),
                                &deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor))
            {
                // success
                configStatus =
                    DEVICE_CONFIG_STATUS_SCHEDULING_GET_CONFIG_DESCRIPTOR;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_CONFIG_DESCRIPTOR:
        {
            DEBUGCHK(uCurrentConfigDescriptorIndex < deviceInfo.usbDeviceDescriptor.bNumConfigurations);
            DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig == NULL &&
                      deviceInfo.pnonConstUsbConfigs != NULL &&
                      deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor.bDescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE &&
                      deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor.bLength == sizeof(USB_CONFIGURATION_DESCRIPTOR) &&
                      deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].lpbExtended == NULL &&
                      deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].pnonConstUsbInterfaces == NULL);

            const USHORT wTotalLength =
                deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor.wTotalLength;
            PUCHAR pDataBuffer = new UCHAR[wTotalLength];

            if(pDataBuffer != NULL &&
                 GetDescriptor(pControlPipe,
                                bAddress,
                                USB_CONFIGURATION_DESCRIPTOR_TYPE,
                               (UCHAR)uCurrentConfigDescriptorIndex,
                                wTotalLength,
                                pDataBuffer) &&
                 memcmp(&deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex].usbConfigDescriptor,
                        pDataBuffer,
                        sizeof(USB_CONFIGURATION_DESCRIPTOR)) == 0 &&
                 CreateUsbConfigurationStructure(deviceInfo.pnonConstUsbConfigs[uCurrentConfigDescriptorIndex],
                        pDataBuffer,
                        wTotalLength))
            {
                // success
                uCurrentConfigDescriptorIndex++;

                if(uCurrentConfigDescriptorIndex <
                    deviceInfo.usbDeviceDescriptor.bNumConfigurations)
                {
                    // need to get more descriptors
                    configStatus =
                        DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_CONFIG_DESCRIPTOR;
                }
                else
                {
                    // done getting config descriptors
                    configStatus =
                        DEVICE_CONFIG_STATUS_DETERMINE_CONFIG_TO_CHOOSE;
                }
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            }

            delete [] pDataBuffer;
            pDataBuffer = NULL;

            break;
        }

        case DEVICE_CONFIG_STATUS_DETERMINE_CONFIG_TO_CHOOSE:
        {
            // We're not terribly smart about picking a
            // config when the first one won't work. C'est la vie.
            // Also, we cannot check the device's actual power
            // status until after it's configured.

            BYTE bConfig = 0;

            // This function is supported by USBD.
            if(m_pCHcd->GetpUSBDSelectConfigurationProc() != NULL
                    && deviceInfo.usbDeviceDescriptor.bNumConfigurations > 1)
            {
                // if can not find anything return to 0.
                if(!(*m_pCHcd->GetpUSBDSelectConfigurationProc())(LPCUSB_DEVICE(&deviceInfo),
                                                                    &bConfig) ||
                        bConfig >= deviceInfo.usbDeviceDescriptor.bNumConfigurations)
                {
                    bConfig = 0;
                }
            }

            DEBUGMSG(ZONE_ATTACH,
               (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                      Select Configuration %d on %s step\n"),
                GetControllerName(),
                GetDeviceType(),
                m_bTierNumber,
                bConfig,
                StatusToString(configStatus)));

            if (deviceInfo.pnonConstUsbConfigs == NULL) 
            {
                configStatus = DEVICE_CONFIG_STATUS_FAILED;
            }
            else if(deviceInfo.pnonConstUsbConfigs[bConfig].usbConfigDescriptor.bmAttributes & SELF_POWERED)
            {
                // the device we're attaching is self-powered so power is of little concern
            }
            else
            {
                // MaxPower is in units of 2mA
                DWORD dwCfgPower =
                    deviceInfo.pnonConstUsbConfigs[bConfig].usbConfigDescriptor.MaxPower * 2;

                if(deviceInfo.pnonConstUsbConfigs[bConfig].usbConfigDescriptor.MaxPower == 0)
                {
                    RETAILMSG(1,
                           (TEXT("!CHub::AttachDevice warning: invalid power configuration\n")));
                    // If MaxPower is illegal, assume the maximum power is required.
                    dwCfgPower = 500;
                }
                {
                    BOOL fIsOK;
                    if(m_deviceInfo.pnonConstUsbConfigs == NULL)
                    {
                        // we must be a root hub; check with the PDD
                        fIsOK = HcdPdd_CheckConfigPower(bPort, dwCfgPower, 0);
                    }
                    else if(m_deviceInfo.pnonConstUsbActiveConfig->usbConfigDescriptor.bmAttributes & SELF_POWERED)
                    {
                        // we're self-powered so we can attach a high-powered device
                        fIsOK = TRUE;
                    }
                    else if(dwCfgPower <= 100)
                    {
                        // we're bus-powered so we can only attach low-powered devices
                        fIsOK = TRUE;
                    }
                    else
                    {
                        fIsOK = FALSE;
                    }

                    if(! fIsOK)
                    {
                        RETAILMSG(1,
                           (TEXT("!USB warning: cannot attach \
                                  high-power device to a bus-powered hub\n")));

                        configStatus = DEVICE_CONFIG_STATUS_FAILED;
                    }
                }
            }

            if(configStatus != DEVICE_CONFIG_STATUS_FAILED)
            {
                DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig == NULL &&
                    deviceInfo.pnonConstUsbConfigs != NULL);

                if (deviceInfo.pnonConstUsbConfigs != NULL) 
                {
                    deviceInfo.pnonConstUsbActiveConfig =
                        &deviceInfo.pnonConstUsbConfigs[bConfig];
                    DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces != NULL);

                    configStatus = DEVICE_CONFIG_STATUS_ADD_ENDPOINTS;
                } 
                else 
                {
                    configStatus = DEVICE_CONFIG_STATUS_FAILED;
                }
            }

            break;
        }
        case DEVICE_CONFIG_STATUS_ADD_ENDPOINTS:
        {
            if(!AddEndpoints(bAddress, &deviceInfo))
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step \n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            } else {
                configStatus = DEVICE_CONFIG_STATUS_BANDWIDTH;
            }
            break;
        }

        case DEVICE_CONFIG_STATUS_BANDWIDTH:
        {
            if(!DoConfigureEndpoint(bAddress))
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            } else {
                configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_SET_CONFIG;
            }
            break;
        }

        case DEVICE_CONFIG_STATUS_SCHEDULING_SET_CONFIG:
        {
            DEBUGCHK(deviceInfo.pnonConstUsbActiveConfig != NULL);

            BOOL                fTransferDone = FALSE;
            DWORD               dwBytesTransferred = 0;
            DWORD               dwErrorFlags = USB_NOT_COMPLETE_ERROR;
            HCD_REQUEST_STATUS  status = REQUEST_FAILED;
            USB_DEVICE_REQUEST  usbRequest;

            usbRequest.bmRequestType = USB_REQUEST_HOST_TO_DEVICE |
                                        USB_REQUEST_STANDARD |
                                        USB_REQUEST_FOR_DEVICE;
            usbRequest.bRequest = USB_REQUEST_SET_CONFIGURATION;
            usbRequest.wValue =
                deviceInfo.pnonConstUsbActiveConfig->usbConfigDescriptor.bConfigurationValue;
            usbRequest.wIndex = 0;
            usbRequest.wLength = 0;
            if(!m_fHubThreadClosing)
            {
                status = pControlPipe->IssueTransfer(bAddress, // device uAddress
                                                        TransferDoneCallbackSetEvent, // callback
                                                        m_hHubStatusChangeEvent, // param for callback
                                                        USB_OUT_TRANSFER | USB_SEND_TO_DEVICE, // transfer flags
                                                        &usbRequest, // control request
                                                        0, // dwStartingFrame(not used)
                                                        0, // dwFrames(not used)
                                                        NULL, // aLengths(not used)
                                                        0, // buffer size
                                                        NULL, // data buffer
                                                        0, // phys addr of buffer(not used)
                                                        this, // cancel ID
                                                        NULL, // lpdwIsochErrors(not used)
                                                        NULL, // lpdwIsochLengths(not used)
                                                        &fTransferDone, // OUT status param
                                                        &dwBytesTransferred, // OUT status param
                                                        &dwErrorFlags); // OUT status param

                if(status == REQUEST_OK)
                {
                    DWORD dwResult =
                        WaitForSingleObject(m_hHubStatusChangeEvent,
                                            STANDARD_REQUEST_TIMEOUT);

                    if(m_fHubThreadClosing || dwResult!= WAIT_OBJECT_0)
                    {
                        pControlPipe->AbortTransfer(NULL, // callback function
                                                     NULL, // callback parameter
                                                     this); // cancel ID
                        ResetEvent(m_hHubStatusChangeEvent);
                    }
                }

                DEBUGCHK(fTransferDone);
            }

            if(status == REQUEST_OK &&
                 fTransferDone &&
                 dwBytesTransferred == 0 &&
                 dwErrorFlags == USB_NO_ERROR)
            {
                // move to next step
                if(deviceInfo.usbDeviceDescriptor.bDeviceClass ==
                    USB_DEVICE_CLASS_HUB)
                {
                    // more steps need to happen for hubs
                    usbHubDescriptor.bDescriptorLength =
                        USB_HUB_DESCRIPTOR_MINIMUM_SIZE;
                    //external hub is not supported
                    configStatus = DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR;
                }
                else
                {
                    configStatus = DEVICE_CONFIG_STATUS_CREATE_NEW_FUNCTION;
                }
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, fTransferDone = %d,\
                          dwBytesTrans = 0x%x, dwErrorFlags = 0x%x\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus),
                    fTransferDone,
                    dwBytesTransferred,
                    dwErrorFlags));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
            }

            break;
        }
        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR:
        case DEVICE_CONFIG_STATUS_SCHEDULING_GET_HUB_DESCRIPTOR:
        {
            DEBUGCHK(deviceInfo.usbDeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB);
            DEBUGCHK((configStatus == DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR &&
                       usbHubDescriptor.bDescriptorLength == USB_HUB_DESCRIPTOR_MINIMUM_SIZE) ||
                     (configStatus == DEVICE_CONFIG_STATUS_SCHEDULING_GET_HUB_DESCRIPTOR &&
                       usbHubDescriptor.bDescriptorLength > USB_HUB_DESCRIPTOR_MINIMUM_SIZE &&
                       usbHubDescriptor.bDescriptorLength <= sizeof(usbHubDescriptor)));
            const UCHAR bDescriptorLengthToGet = usbHubDescriptor.bDescriptorLength;

            if(GetDescriptor(pControlPipe,
                             bAddress,
                             USB_HUB_DESCRIPTOR_TYPE,
                             0, // hub descriptor index is 0
                             bDescriptorLengthToGet,
                             &usbHubDescriptor))
            {
                // success
                if(usbHubDescriptor.bDescriptorLength > bDescriptorLengthToGet)
                {
                    DEBUGCHK(configStatus == DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR);
                    configStatus =
                        DEVICE_CONFIG_STATUS_SCHEDULING_GET_HUB_DESCRIPTOR;
                }
                else
                {
                    DEBUGCHK(usbHubDescriptor.bDescriptorLength == bDescriptorLengthToGet);
                #ifdef DEBUG
                    DumpHubDescriptor(&usbHubDescriptor);
                #endif // DEBUG
                    configStatus = DEVICE_CONFIG_STATUS_CREATE_NEW_HUB;
                }
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                pControlPipe->IsPipeHalted(&fPipeHalted);
                bConfigFailures++;
                // Restore bDescriptorLength for the retry.
                usbHubDescriptor.bDescriptorLength = bDescriptorLengthToGet;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_CREATE_NEW_HUB:
        {
            DEBUGCHK(pNewDevice == NULL);
            DEBUGCHK(deviceInfo.usbDeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB &&
                      usbHubDescriptor.bDescriptorType == USB_HUB_DESCRIPTOR_TYPE &&
                      usbHubDescriptor.bDescriptorLength >= USB_HUB_DESCRIPTOR_MINIMUM_SIZE &&
                      deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[0].usbInterfaceDescriptor.bNumEndpoints == 1);

            DEBUGCHK(m_bTierNumber < USB_MAXIMUM_HUB_TIER);

            DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, no memory\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));
            // external huab is not supported

            bConfigFailures++;
            break;
        }
        case DEVICE_CONFIG_STATUS_CREATE_NEW_FUNCTION:
        {
            DEBUGCHK(pNewDevice == NULL);
            DEBUGCHK(deviceInfo.usbDeviceDescriptor.bDeviceClass != USB_DEVICE_CLASS_HUB);
            DEBUGCHK(m_bTierNumber <= USB_MAXIMUM_HUB_TIER);

            pNewDevice = new CFunction(bAddress,
                                       deviceInfo,
                                       bSpeed,
                                       m_bTierNumber + 1,
                                       m_pCHcd,
                                       this,
                                       bPort,
                                       pLogicalDevice);

            if(pNewDevice != NULL)
            {
                configStatus =
                    DEVICE_CONFIG_STATUS_INSERT_NEW_DEVICE_INTO_UPSTREAM_HUB_PORT_ARRAY;
            }
            else
            {
                DEBUGMSG(ZONE_ATTACH && ZONE_ERROR,
                   (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                          failure on %s step, no memory\n"),
                    GetControllerName(),
                    GetDeviceType(),
                    m_bTierNumber,
                    StatusToString(configStatus)));

                bConfigFailures++;
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_INSERT_NEW_DEVICE_INTO_UPSTREAM_HUB_PORT_ARRAY:
        {
            DEBUGCHK(pNewDevice != NULL);
            EnterCriticalSection(&m_csDeviceLock);
            DEBUGCHK(m_ppCDeviceOnPort != NULL && m_ppCDeviceOnPort[bPort - 1] == NULL);
            m_ppCDeviceOnPort[bPort - 1] = pNewDevice;
            LeaveCriticalSection(&m_csDeviceLock);
            configStatus =
                DEVICE_CONFIG_STATUS_SIGNAL_NEW_DEVICE_ENTER_OPERATIONAL_STATE;

            break;
        }

        case DEVICE_CONFIG_STATUS_SIGNAL_NEW_DEVICE_ENTER_OPERATIONAL_STATE:
        {
            DEBUGCHK(pNewDevice != NULL);
            {
                HANDLE hWorkerThread = NULL;
                if(!fSyncronously)
                {
                    hWorkerThread = CreateThread(0,
                                                0,
                                                AttachDownstreamDeviceThreadStub,
                                                pNewDevice,
                                                CREATE_SUSPENDED,
                                                NULL);
                }

                if(hWorkerThread != NULL)
                {
                    // spin a thread to carry out call to USBD for finding
                    // a suitable Client Driver
                    DEBUGMSG(ZONE_ATTACH,
                       (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                              async completion...\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber));

                    pNewDevice->m_pCtrlPipe0 = pControlPipe;
                    CeSetThreadPriority(hWorkerThread,
                                        CeGetThreadPriority(GetCurrentThread()) + 1);
                    ResumeThread(hWorkerThread);
                    CloseHandle(hWorkerThread);
                    configStatus = DEVICE_CONFIG_STATUS_DONE;

                    DEBUGMSG(ZONE_ATTACH,
                       (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                              async completion %s\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        StatusToString(configStatus)));
                }
                else
                {
                    // proceed w/ direct call to CFunction class which in turn will
                    // invoke Client driver via USBD this may hold the thread
                    // execution for quite some time before return
                    if(FXN_DEV_STATE_OPERATIONAL ==
                        pNewDevice->EnterOperationalState(pControlPipe))
                    {
                        configStatus = DEVICE_CONFIG_STATUS_DONE;
                    }
                    else
                    {
                        // don't do any retries here!
                        // EnterOperationalState should only be tried once
                        DEBUGMSG(ZONE_ATTACH || ZONE_ERROR,
                           (TEXT("%s: CHub(%s tier %d)::AttachDevice - \
                                  failure on %s step, aborting attach process\n"),
                            GetControllerName(),
                            GetDeviceType(),
                            m_bTierNumber,
                            StatusToString(configStatus)));

                        configStatus = DEVICE_CONFIG_STATUS_FAILED;
                        bConfigFailures = 0xff;
                    }
                }
            }

            break;
        }

        case DEVICE_CONFIG_STATUS_FAILED:
        {
            if(pNewDevice != NULL)
            {
                // this means we have placed the device into our array
                EnterCriticalSection(&m_csDeviceLock);
                DEBUGCHK((m_fHubThreadClosing && m_ppCDeviceOnPort[bPort - 1] == NULL) ||
                          m_ppCDeviceOnPort[bPort - 1] == pNewDevice);
                m_ppCDeviceOnPort[bPort - 1] = NULL;
                LeaveCriticalSection(&m_csDeviceLock);
                // address will be freed by destructor
                // deviceInfo will be freed by destructor
                delete pNewDevice;
                pNewDevice = NULL;
            }
            else
            {
                if(deviceInfo.pnonConstUsbConfigs != NULL)
                {
                    for(UINT uConfig = 0;
                        uConfig < deviceInfo.usbDeviceDescriptor.bNumConfigurations;
                        uConfig++)
                    {
                        DeleteUsbConfigurationStructure(deviceInfo.pnonConstUsbConfigs[uConfig]);
                    }

                    delete [] deviceInfo.pnonConstUsbConfigs;
                }
            }

            if(pControlPipe)
            {
                pControlPipe->ClosePipe();
                delete pControlPipe;
                pControlPipe = NULL;
            }

            fPipeHalted = TRUE;
            DisablePort(bPort);

            configStatus = DEVICE_CONFIG_STATUS_DONE;

            break;
        }

        default:
        {
#ifdef DEBUG
            DebugBreak(); // should never get here!
#endif // DEBUG
            break;
        }
        } // end of switch(configStatus)
    } // end of while(configStatus != DEVICE_CONFIG_STATUS_DONE)

    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: -CHub(%s tier %d)::AttachDevice - \
              port = %d, bSpeed = %d, address = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        bPort,
        bSpeed,
        bAddress));
}

//----------------------------------------------------------------------------------
// Function: GetDescriptor
//
// Description: Query a USB device for one of its descriptors. Caller is responsible
//              for buffer alloc/delete
//
// Parameters: pControlPipe - pointer to a control pipe on which to issue request
//
//             address - address of USB device to query
//
//             bSpeed - indicates the device speed
//
//             bDescriptorType - type of descriptor to get. i.e
//                              USB_DEVICE_DESCRIPTOR_TYPE
//
//             bDescriptorIndex - index of descriptor. For instance
//                               we may want config descriptor 3
//
//             wDescriptorSize - size of descriptor to get
//
//             pBuffer - data buffer to receive descriptor
//
// Returns: TRUE if descriptor read, else FALSE
//----------------------------------------------------------------------------------
BOOL CHub::GetDescriptor(IN CPipeAbs * const pControlPipe,
                          IN const UINT uAddress,
                          IN const UCHAR bDescriptorType,
                          IN const UCHAR bDescriptorIndex,
                          IN const USHORT wDescriptorSize,
                          OUT PVOID pBuffer)
{
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::GetDescriptor - \
              address = %d, Type = %d, Index = %d, Size = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        bDescriptorType,
        bDescriptorIndex,
        wDescriptorSize));

    PREFAST_DEBUGCHK(pControlPipe != NULL);
    PREFAST_DEBUGCHK(pBuffer != NULL);
    DEBUGCHK(m_hHubStatusChangeEvent != NULL);

    BOOL                fTransferDone = FALSE;
    DWORD               dwBytesTransferred = 0;
    DWORD               dwErrorFlags = USB_NOT_COMPLETE_ERROR;
    HCD_REQUEST_STATUS  status = REQUEST_FAILED;
    USB_DEVICE_REQUEST  usbRequest;

    if(bDescriptorType == USB_HUB_DESCRIPTOR_TYPE)
    {
        DEBUGCHK(bDescriptorIndex == 0 &&
            wDescriptorSize >= USB_HUB_DESCRIPTOR_MINIMUM_SIZE);
        usbRequest.bmRequestType = USB_REQUEST_DEVICE_TO_HOST |
                                    USB_REQUEST_CLASS |
                                    USB_REQUEST_FOR_DEVICE;
    }
    else
    {
        DEBUGCHK((bDescriptorType == USB_DEVICE_DESCRIPTOR_TYPE &&
            bDescriptorIndex == 0 &&
           (wDescriptorSize == ENDPOINT_ZERO_MIN_MAXPACKET_SIZE ||
            wDescriptorSize == sizeof(USB_DEVICE_DESCRIPTOR))) ||
           (bDescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE &&
            wDescriptorSize >= sizeof(USB_CONFIGURATION_DESCRIPTOR)));
        usbRequest.bmRequestType = USB_REQUEST_DEVICE_TO_HOST |
                                    USB_REQUEST_STANDARD |
                                    USB_REQUEST_FOR_DEVICE;
    }

    usbRequest.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    usbRequest.wValue =
        USB_DESCRIPTOR_MAKE_TYPE_AND_INDEX(bDescriptorType, bDescriptorIndex);
    usbRequest.wIndex = 0;
    usbRequest.wLength = wDescriptorSize;

    if(!m_fHubThreadClosing)
    {
        status = pControlPipe->IssueTransfer(
                                 uAddress, // address of device
                                 TransferDoneCallbackSetEvent, // callback routine
                                 m_hHubStatusChangeEvent, // callback param
                                 USB_IN_TRANSFER | USB_SEND_TO_DEVICE, // transfer flags
                                 &usbRequest, // control request
                                 0, // dwStartingFrame(not used)
                                 0, // dwFrames(not used)
                                 NULL, // aLengths(not used)
                                 wDescriptorSize, // buffer size
                                 pBuffer, // buffer
                                 0, // phys addr of buffer(not used)
                                 this, // cancel ID
                                 NULL, // lpdwIsochErrors(not used)
                                 NULL, // lpdwIsochLengths(not used)
                                 &fTransferDone, // OUT status param
                                 &dwBytesTransferred, // OUT status param
                                 &dwErrorFlags); // OUT status param

        if(status == REQUEST_OK)
        {
            DWORD dwResult =
                WaitForSingleObject(m_hHubStatusChangeEvent,
                                    STANDARD_REQUEST_TIMEOUT);

            if(!fTransferDone || dwResult!= WAIT_OBJECT_0)
            {
                pControlPipe->AbortTransfer(NULL, // callback function
                                             NULL, // callback parameter
                                             this); // cancel ID

                if(!m_fHubThreadClosing && m_hHubStatusChangeEvent != NULL)
                {
                    ResetEvent(m_hHubStatusChangeEvent);
                }
            }
        }

        DEBUGCHK(fTransferDone);
    }

#ifndef USB_STRICT_ENFORCEMENT
    // Some IHVs have lazy firmware writers who didn't bother to set the
    // descriptor type field in all of their descriptors. Sigh.
    if(PUSB_COMMON_DESCRIPTOR(pBuffer)->bDescriptorType == 0)
    {
        DEBUGMSG(ZONE_WARNING,
           (TEXT("CHub::GetDescriptor - forcing descr type 0x%x to 0x%x\n"),
            PUSB_COMMON_DESCRIPTOR(pBuffer)->bDescriptorType,
            bDescriptorType));

        PUSB_COMMON_DESCRIPTOR(pBuffer)->bDescriptorType = bDescriptorType;
    }
#endif

    BOOL fSuccess =(status == REQUEST_OK &&
                    fTransferDone &&
                    dwBytesTransferred == wDescriptorSize &&
                    dwErrorFlags == USB_NO_ERROR &&
                    PUSB_COMMON_DESCRIPTOR(pBuffer)->bDescriptorType == bDescriptorType);
    // note, don't check length since some descriptors can be variable
    // length(i.e. configuration descriptor bLength field will read
    // sizeof(USB_CONFIGURATION_DESCRIPTOR), but we may have requested
    // more than this. Or, we may only have requested USB_HUB_DESCRIPTOR_MINIMUM_SIZE
    // but the hub descriptor can be longer.

    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::GetDescriptor - \
              address = %d, Type = %d, Index = %d, Size = %d, returning %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        bDescriptorType,
        bDescriptorIndex,
        wDescriptorSize,
        fSuccess));

    return fSuccess;
}

DWORD CHub::GetStartTime() const
{
    return m_pCHcd->GetStartTime();
}

//----------------------------------------------------------------------------------
// Function: EnableSlot
//
// Description: Query a USB device for slot id. 
//
// Parameters: rSlotId - slot ID
//
// Returns: pointer to device if slot is enabled, else NULL
//----------------------------------------------------------------------------------
PVOID CHub::EnableSlot(UCHAR& rSlotId)
{
    UCHAR bSlotId;
    PVOID pDevice;

    pDevice = (PVOID)m_pCHcd->EnableSlot(&bSlotId);

    if(pDevice != NULL)
    {
        rSlotId = bSlotId;
    }

    return pDevice;
}

//----------------------------------------------------------------------------------
// Function: AddEndpoints 
//
// Description: query a USB device for add endpoints. Caller is responsible for
//              buffer alloc/delete
//
// Parameters: bSlotId - slot ID
//
//             pDeviceInfo - pointer to device info context
//
// Returns: TRUE if endpoints added correctly, else FALSE
//----------------------------------------------------------------------------------
BOOL CHub::AddEndpoints(UCHAR bSlotId, USB_DEVICE_INFO* pDeviceInfo)
{
   UCHAR bNumEndpoints, bEndpoint = 0;
   INT iInterfaces;

   iInterfaces = pDeviceInfo->pnonConstUsbActiveConfig->dwNumInterfaces;

   if(iInterfaces < 1)
   {
        DEBUGCHK(0); // shouldn't be possible - test is here to make prefast happy.
        return REQUEST_FAILED;
   }

   for(UCHAR bInterface = 0; bInterface < iInterfaces; bInterface++)
   {
       bNumEndpoints =
           pDeviceInfo->pnonConstUsbActiveConfig->pnonConstUsbInterfaces[bInterface].usbInterfaceDescriptor.bNumEndpoints;

       for(bEndpoint = 0; bEndpoint < bNumEndpoints; bEndpoint++)
       {
           if(!m_pCHcd->AddEndpoint(bSlotId,
               &pDeviceInfo->pnonConstUsbActiveConfig->pnonConstUsbInterfaces[bInterface].pnonConstUsbEndpoints[bEndpoint].usbEndpointDescriptor))
           {
               return REQUEST_FAILED;
           }
       }
   }

   return REQUEST_OK;
}

//----------------------------------------------------------------------------------
// Function: AddressDevice 
//
// Description: Send an Address Device command to the controller. 
//
// Parameters: bSlotId - slot ID
//
//             uPortId - port ID
//
//             bSpeed - device speed
//
// Returns: TRUE if command passed correctly, else FALSE
//----------------------------------------------------------------------------------
BOOL CHub::AddressDevice(UCHAR bSlotId, UINT uPortId, UCHAR bSpeed)
{
    BOOL fSuccess;

    fSuccess =(m_pCHcd->AddressDevice(bSlotId, uPortId, bSpeed) == 0);
    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: ResetDevice
//
// Description: Send an Reset Device command to the controller.
//
// Parameters: bAddress - device address
//
// Returns: TRUE if command passed correctly, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CHub::ResetDevice(IN const UCHAR bAddress)
{
    BOOL fSuccess;

    fSuccess =(m_pCHcd->ResetDevice(bAddress) == 0);
    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: DoConfigureEndpoint
//
// Description: Query a USB device for endpoints configuration. Caller is
//              responsible for buffer alloc/delete
//
// Parameters: bSlotId - slot ID
//
// Returns: TRUE if command passed correctly, else FALSE
//----------------------------------------------------------------------------------
BOOL CHub::DoConfigureEndpoint(UCHAR bSlotId)
{
    return m_pCHcd->DoConfigureEndpoint(bSlotId);
}

//----------------------------------------------------------------------------------
// Function: AttachDownstreamDeviceThreadStub
//
// Description: Stub for AttachDownstreamDeviceThread
//
// Parameters: pContext - pointer to CDevice
//
// Returns: see AttachDownstreamDeviceThread
//----------------------------------------------------------------------------------
DWORD CHub::AttachDownstreamDeviceThreadStub(IN PVOID pContext)
{
    CDevice* pDevToAttach =static_cast<CDevice*> (pContext);
    return pDevToAttach->m_pAttachedHub->AttachDownstreamDeviceThread(pDevToAttach);
}

//----------------------------------------------------------------------------------
// Function: AttachDownstreamDeviceThread
//
// Description: This is the worker thread for handling downstream device
//              attach without holding hub's StatusChangeThread.
//              For CFunction devices, EnterOperationalState() implies call
//              to Client's driver via USBD, which may take a long time,
//              especially if errors happen and Client retries many times.
//
// Parameters: Implicitly the device and its control pipe
//
// Returns: Always returs 0
//----------------------------------------------------------------------------------
DWORD CHub::AttachDownstreamDeviceThread(CDevice* pDevToAttach)
{
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: +CHub::AttachDownstreamDeviceThread\n"),
        GetControllerName()));

    UCHAR bPort = pDevToAttach->m_bAttachedPort;
    // <m_pCtrlPipe0> and <m_fIsSuspend> are unionized - do not reverse order!
    CPipeAbs* pControlPipe = pDevToAttach->m_pCtrlPipe0;
    pDevToAttach->m_fIsSuspend = FALSE;

    ASSERT(bPort);
    PREFAST_DEBUGCHK(pDevToAttach != NULL);
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s:  CHub::AttachDownstreamDeviceThread \
              about to EnterOperationalState()...\n"),
        GetControllerName()));
    DWORD dwSuccess = pDevToAttach->EnterOperationalState(pControlPipe);
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s:  CHub::AttachDownstreamDeviceThread \
              returned EnterOperationalState() result=%x\n"),
        GetControllerName(),
        dwSuccess));

    if(dwSuccess != FXN_DEV_STATE_OPERATIONAL)
    {
        // All tasks for"case DEVICE_CONFIG_STATUS_FAILED" must be carried out here,
        // unless DETACH had been discovered and DETACH process had already started.
        // If <dwSuccess> has FXN_DEV_STATE_DETACHMENT bit set by CXxx::HandleDetach(),
        // <pDevToAttach> may had been already invalidated - do not refer to it.
        if((dwSuccess&FXN_DEV_STATE_DETACHMENT) == 0)
        {
            // this means we have placed the device into our array
            EnterCriticalSection(&m_csDeviceLock);
            DEBUGCHK((m_fHubThreadClosing && m_ppCDeviceOnPort[bPort - 1] == NULL) ||
                      m_ppCDeviceOnPort[bPort - 1] == pDevToAttach);
            m_ppCDeviceOnPort[bPort - 1] = NULL;
            LeaveCriticalSection(&m_csDeviceLock);
            // address will be freed by destructor
            // deviceInfo will be freed by destructor
            delete pDevToAttach;

            pControlPipe->ClosePipe();
            delete pControlPipe;

            DisablePort(bPort);
        }
    }

    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: -CHub::AttachDownstreamDeviceThread\r\n\r\n"),
        GetControllerName()));

    return 0;
}

//----------------------------------------------------------------------------------
// Function: DetachDownstreamDeviceThreadStub
//
// Description: Stub for DetachDownstreamDeviceThread
//
// Parameters: pContext - pointer to CDevice
//
// Returns: Always returns 0
//----------------------------------------------------------------------------------
DWORD CHub::DetachDownstreamDeviceThreadStub(IN PVOID pContext)
{
    CDevice* pDevToDetach =static_cast<CDevice*> (pContext);
    return pDevToDetach->m_pAttachedHub->DetachDownstreamDeviceThread(pDevToDetach);
}

//----------------------------------------------------------------------------------
// Function: DetachDownstreamDeviceThread
//
// Description: This is the worker thread for handling downstream device
//              detach when the current hub is still active(i.e. the
//              hub itself has not been detached). This thread allows
//              the hub to continue merrily on its way processing
//              attach/remove, without having to wait for the detach
//              to completely finish. That's a good thing, because we
//              can be detaching a downstream hub, which has more hubs/functions
//              attached to it, etc, and we don't want to block the active hub.
//              The memory associated with CDevice passed in is our
//              responsibility to free
//
// Parameters: pContext - a CDevice* device to detach
//
// Returns: Always returns 0
//----------------------------------------------------------------------------------
DWORD CHub::DetachDownstreamDeviceThread(CDevice* pDevToDetach)
{
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: +CHub::DetachDownstreamDeviceThread\n"),
        GetControllerName()));

    //
    // Claim this critical section right away!
    // If invoked by Client, then HCD's CS is already taken in
    // "CHcd::DisableDevice()". This thread has higher priority than
    // Client's thread, and priority inversion will happen. Client's
    // thread will get out of XHCI fast, and only then this thread will
    // be released.
    //

    ASSERT(pDevToDetach);
    PREFAST_DEBUGCHK(pDevToDetach != NULL);

    m_pCHcd->Lock();
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: Locking and Unlocking m_pCHcd to sync it.\r\n"),
        GetControllerName()));
    m_pCHcd->Unlock();
    pDevToDetach->HandleDetach();
    delete pDevToDetach;

    //DecrCountdown(&m_objCountdown);
    m_pDeviceGlobal->DecObjCountdown();

    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: -CHub::DetachDownstreamDeviceThread\n"),
        GetControllerName()));

    return 0;
}

//----------------------------------------------------------------------------------
// Function: DetachDevice
//
// Description: This function is called when a device on port "port" needs
//              to be detached, and we don't want to block the hub. So,
//              we spin off a thread to do the work. The port will be disabled.
//
// Parameters: bPort - indicates hub port on which device was detached
//
//             fSyncronously - is synchrony operation
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHub::DetachDevice(IN const UCHAR bPort, IN const BOOL fSyncronously)
{
    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: +CHub(%s tier %d)::DetachDevice - port = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        bPort));

    CDevice*    pDevToDetach = NULL;

    // Remove the device from our array.
    // This will prevent USBD from issuing
    // any more requests to the device or its pipes.
    EnterCriticalSection(&m_csDeviceLock);
    PREFAST_DEBUGCHK(m_ppCDeviceOnPort != NULL);
    DEBUGCHK(bPort >= 1 && bPort <= m_usbHubDescriptor.bNumberOfPorts);
    pDevToDetach = m_ppCDeviceOnPort[bPort - 1];
    m_ppCDeviceOnPort[bPort - 1] = NULL;

    if(pDevToDetach)
    {
        BOOL fSuccess;
        //fSuccess = IncrCountdown(&m_objCountdown);
        fSuccess = m_pDeviceGlobal->IncObjCountdown();

        if(fSuccess)
        {
            pDevToDetach->m_fIsSuspend = TRUE;
        }
        // if the countdown couldn't be incremented then it must have
        // been deleted already, which would mean that all devices
        // have already been detached. Which would mean that the current
        // thread has already exited which is clearly not the case.
        DEBUGCHK(fSuccess);
    }

    LeaveCriticalSection(&m_csDeviceLock);
    // It is possible that we get a device detach on a NULL device.
    // It happens when AttachDevice() failed to configure the device
    // and it was left plugged into the USB port.
    // When unplugged, detach message will be read from the port.
    DEBUGMSG(ZONE_ATTACH && ZONE_WARNING && !pDevToDetach,
       (TEXT("%s: CHub(%s tier %d)::DetachDevice - \
              reading NULL device detached on port %d, doing nothing.\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        bPort));

    if(pDevToDetach != NULL)
    {
        //
        // This routine may be invoked either by App/Client, or on
        // IST-via-HubStatusChangeThread.
        // When invoked by App/Client, "m_pCHcd->m_CSection" is taken
        // via "pCHW->Lock()"
        // When invoked on IST, "m_pCHcd->m_CSection" is free.
        // Inside "DetachDownstreamDeviceThreadStub" code path, same
        // "m_pCHcd->Lock()" is always called.
        //
        HANDLE hWorkerThread = NULL;

        if(!fSyncronously)
        {
            hWorkerThread = CreateThread(0,
                                        0,
                                        DetachDownstreamDeviceThreadStub,
                                        pDevToDetach,
                                        CREATE_SUSPENDED,
                                        NULL);
        }

        if(hWorkerThread != NULL)
        {
            // Worker thread priority will be higher than Application priority
            //(supposedly, Client Driver runs at this priority) yet it will be
            // lower than IST and HubStatusChangeThread's priority.
            // Thus, hub status changes will be promptly taken care of.
            DEBUGMSG(ZONE_ATTACH,
               (TEXT("%s: CHub(%s tier %d)::DetachDevice - \
                      using worker thread.\n"),
                GetControllerName(),
                GetDeviceType(),
                m_bTierNumber));

            CeSetThreadPriority(hWorkerThread,
                g_dwIstThreadPriority + RELATIVE_PRIO_DOWNSTREAM);
            ResumeThread(hWorkerThread);

            // closing the handle does not imply "killing" the thread - only
            // that this reference is not needed any more
            CloseHandle(hWorkerThread);

        }
        else
        {
            // Invoked on Application thread, or no threads available.
            // Complete call synchronously; this may block
            // HubStatusChangeThread fora while.
            // callback to USBD will free USBD objects, and upon return from here
            // benign "LeaveCriticalSection()" error may occur in USBD.
            DEBUGMSG(ZONE_ATTACH,
               (TEXT("%s: CHub(%s tier %d)::DetachDevice - \
                      no threads available, synchronous execution.\n"),
                GetControllerName(),
                GetDeviceType(),
                m_bTierNumber));

            // While in CFunction::DetachDevice(); priority may be adjusted.
            DWORD dwOriginalPrio = CeGetThreadPriority(GetCurrentThread());
            DetachDownstreamDeviceThread(pDevToDetach);
            CeSetThreadPriority(GetCurrentThread(), dwOriginalPrio);
        }

        DisablePort(bPort);
    }

    if(m_pAddedTT[bPort - 1])
    {
        // This is TT, we need delete this.
        m_pAddedTT[bPort-1] = NULL;
    }

    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: -CHub(%s tier %d)::DetachDevice - port = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        bPort));
}

//----------------------------------------------------------------------------------
// Function: AllocateDeviceArray
//
// Description: Allocate memory for the device array of this hub based on
//              the number of ports given in the m_usbHubDescriptor structure
//              The entries of the array will be set to NULL by this function
//
// Parameters: None
//
// Returns: TRUE if array allocated, else FALSE
//----------------------------------------------------------------------------------
BOOL CHub::AllocateDeviceArray(VOID)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::AllocateDeviceArray\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber));

    BOOL fSuccess = FALSE;

    EnterCriticalSection(&m_csDeviceLock);

    DEBUGCHK(m_ppCDeviceOnPort == NULL && m_usbHubDescriptor.bNumberOfPorts > 0);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: CHub(%s tier %d)::AllocateDeviceArray - \
              attempting to allocate %d devices\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        m_usbHubDescriptor.bNumberOfPorts));

    if(NULL == m_ppCDeviceOnPort)
    {
        m_ppCDeviceOnPort = new CDevice* [m_usbHubDescriptor.bNumberOfPorts];

        if(m_ppCDeviceOnPort != NULL)
        {
            memset(m_ppCDeviceOnPort,
                    0,
                    m_usbHubDescriptor.bNumberOfPorts * sizeof(CDevice*));
        }
    }

    if(NULL == m_pAddedTT)
    {
        m_pAddedTT = new PVOID [m_usbHubDescriptor.bNumberOfPorts];

        if(m_pAddedTT != NULL)
        {
            for(DWORD dwIndex = 0;
                dwIndex < m_usbHubDescriptor.bNumberOfPorts;
                dwIndex++)
            {
                m_pAddedTT[dwIndex] = NULL;
            }
        }
    }

    if((NULL!=m_ppCDeviceOnPort) &&(NULL != m_pAddedTT))
    {
        fSuccess = TRUE;
    }

    DEBUGMSG(ZONE_HUB && ZONE_ERROR && !m_ppCDeviceOnPort,
       (TEXT("%s: CHub(%s tier %d)::AllocateDeviceArray - no memory!\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber));

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::AllocateDeviceArray, returning BOOL %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: HandleDetach
//
// Description: This function is called when the hub is to be detached.
//              This function has to work for both root and external hubs
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHub::HandleDetach(VOID)
{
    DEBUGMSG(ZONE_HUB || ZONE_ATTACH,
       (TEXT("%s: +CHub(%s tier %d)::HandleDetach\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber));

    // close our hub thread
    DEBUGCHK(m_hHubStatusChangeEvent && m_hHubStatusChangeThread);
    // tell the thread to abort port status change processing.
    // ***DO NOT*** enter m_csDeviceLock here, otherwise we will
    // block the thread trying to close
    m_fHubThreadClosing = TRUE;
    SetEvent(m_hHubStatusChangeEvent);
    SetEvent(m_hHubSuspendBlockEvent);
    m_dwDevState |= FXN_DEV_STATE_DETACHMENT; // mark state as detach-in-progress

    // In the case where the hub was detached right when a new function was
    // being attached on one of its ports, the hub thread can be at
    // AttachDevice - DEVICE_CONFIG_STATUS_SIGNAL_NEW_DEVICE_ENTER_OPERATIONAL_STATE.
    // At this point, the thread is somewhere in USBD.DLL, and not in XHCI.DLL, and we
    // need to wait until USBD is finished. USBD can have a dialog up asking the
    // user for the driver's DLL name. So, we don't want to just blindly call
    // TerminateThread if the wait fails.
    DWORD dwWaitReturn = WAIT_FAILED;
#ifdef DEBUG
    DWORD dwTickCountStart = GetTickCount();
#endif // DEBUG

    do
    {
        dwWaitReturn = WaitForSingleObject(m_hHubStatusChangeThread, 1000);
        DEBUGMSG(ZONE_WARNING && dwWaitReturn != WAIT_OBJECT_0,
           (TEXT("%s: CHub(%s tier %d)::HandleDetach - hub thread blocked - \
                  could be waiting foruser input\n"),
            GetControllerName(),
            GetDeviceType(),
            m_bTierNumber));
    } while(dwWaitReturn != WAIT_OBJECT_0);

    DEBUGMSG(ZONE_ATTACH,
       (TEXT("%s: CHub(%s tier %d)::HandleDetach - \
              status change thread closed in %d ms\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        GetTickCount() - dwTickCountStart));

    EnterCriticalSection(&m_csDeviceLock);

#ifdef DEBUG
    if(m_bAddress == 0)
    {
        // root hub - no pipes
        DEBUGCHK(m_ppCPipe == NULL && m_bMaxNumPipes == 0);
    }
    else
    {
        // external hub - should have two pipes
        DEBUGCHK(m_ppCPipe != NULL &&
                  m_bMaxNumPipes == 2 &&
                  m_ppCPipe[ENDPOINT0_CONTROL_PIPE] != NULL &&
                  m_ppCPipe[STATUS_CHANGE_INTERRUPT_PIPE] != NULL);
    }

#endif // DEBUG
    for(UCHAR bPipe = 0; m_ppCPipe != NULL && bPipe < m_bMaxNumPipes; bPipe++)
    {
        m_ppCPipe[bPipe]->ClosePipe();
        delete m_ppCPipe[bPipe];
        m_ppCPipe[bPipe] = NULL;
    }
    // m_ppCPipe[] will be freed in ~CDevice

    // ifm_ppCDeviceOnPort was not allocated ok, EnterOperationalState
    // would have failed, and we should never be at HandleDetach stage.
    PREFAST_DEBUGCHK(m_ppCDeviceOnPort != NULL);
    // we need to detach all the devices on our ports
    for(UCHAR bPort = 0; bPort < m_usbHubDescriptor.bNumberOfPorts; bPort++)
    {
        // Don't call DetachDevice, because that function is intended for
        // when this hub is active. It has the extra overhead of spinning off
        // a thread to do the detach work. Instead, just call the HandleDetach
        // procedure directly
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            m_ppCDeviceOnPort[bPort]->HandleDetach();
            delete m_ppCDeviceOnPort[bPort];
            m_ppCDeviceOnPort[bPort] = NULL;
        }
    }
    // m_ppCDeviceOnPort[] will be freed in ~CHub

    // Now that all pipes are closed, we can close our thread/event handles.
    // If we did this earlier, we would risk having a callback from an
    // active pipe, and having TransferDoneCallbackSetEvent accidentally
    // set a dead m_hHubStatusChangeEvent
    CloseHandle(m_hHubStatusChangeThread);
    m_hHubStatusChangeThread = NULL;

    CloseHandle(m_hHubStatusChangeEvent);
    m_hHubStatusChangeEvent = NULL;

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB || ZONE_ATTACH,
       (TEXT("%s: -CHub(%s tier %d)::HandleDetach\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber));
}

//----------------------------------------------------------------------------------
// Function: OpenPipe
//
// Description: Open a new pipe to the device with the given address. The
//              pipe information is passed in by lpEndpointDescriptor, and
//              the index of the new pipe is returned in *lpPipeIndex
//
// Parameters: see CHcd::OpenPipe
//
// Returns: REQUEST_OK - if pipe opened successfully, and index stored
//                      in lpPipeIndex.
//
//          REQUEST_FAILED - if unable to open the pipe on the device
//                          with the given address
//
//          REQUEST_IGNORED - if we were unable to find a device with
//                           the given address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::OpenPipe(IN const UINT uAddress,
                                   IN LPCUSB_ENDPOINT_DESCRIPTOR const lpEndpointDescriptor,
                                   OUT LPUINT const lpPipeIndex,
                                   OUT LPVOID* const plpCPipe)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::OpenPipe - address = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    // no one should be calling OpenPipe on hubs, since we handle hubs internally.
    DEBUGCHK(uAddress != m_bAddress);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->OpenPipe(uAddress,
                                                          lpEndpointDescriptor,
                                                          lpPipeIndex,
                                                          plpCPipe);
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::OpenPipe - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ClosePipe
//
// Description: Close pipe "uPipeIndex" on device with address "address"
//
// Parameters: See description in CHcd::ClosePipe
//
// Returns: REQUEST_OK - if pipe closed
//
//          REQUEST_FAILED - if pipe exists, but unable to close it
//
//          REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::ClosePipe(IN const UINT uAddress,
                                    IN const UINT uPipeIndex)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::ClosePipe - \
              address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    // no one should be calling ClosePipe on hubs, since we handle hubs internally.
    DEBUGCHK(uAddress != m_bAddress);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->ClosePipe(uAddress, uPipeIndex);

            if(status != REQUEST_IGNORED)
            {
                BOOL fRet = ResetTT(bPort);
                if(!fRet)
                {
                    DEBUGMSG(ZONE_ERROR,
                       (TEXT("%s: -CHub(%s tier %d)::ClosePipe ResetTT failed\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber));
                }
            }
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::ClosePipe - address = %d, \
              uPipeIndex = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a USB transfer to the pipe "pipe" on the device whose
//              address is "address"
//
// Parameters: see CHcd::IssueTransfer
//
// Returns: REQUEST_OK - if transfer issued
//
//          REQUEST_FAILED - if unable to issue transfer
//
//          REQUEST_IGNORED - if we were unable to find a device with
//                           the given address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::IssueTransfer(ISSUE_TRANSFER_PARAMS* pITP)
{
    // no one should be calling IssueTransfer on hubs, since we handle
    // hubs internally.
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::IssueTransfer, address = %d, pipe = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        pITP->uAddress,
        pITP->uPipeIndex));

    DEBUGCHK(pITP->uAddress != m_bAddress);

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->IssueTransfer(pITP);
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::IssueTransfer address = %d, pipe = %d, \
              returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        pITP->uAddress,
        pITP->uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer
//
// Description: Abort transfer currently occurring on device with USB address
//              "address", pipe "uPipeIndex".
//
// Parameters: See description in CHcd::AbortTransfer
//
// Returns: REQUEST_OK - if transfer aborted
//
//          REQUEST_FAILED - if transfer exists, but unable to abort it
//
//          REQUEST_IGNORED - if address doesn't match this device's address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::AbortTransfer(
                            IN const UINT uAddress,
                            IN const UINT uPipeIndex,
                            IN LPTRANSFER_NOTIFY_ROUTINE const lpCancelAddress,
                            IN LPVOID const lpvNotifyParameter,
                            IN LPCVOID const lpvCancelId)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::AbortTransfer - \
              address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    // no one should be calling AbortTransfer on hubs, since we handle
    // hubs internally.
    DEBUGCHK(uAddress != m_bAddress);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->AbortTransfer(uAddress,
                                                               uPipeIndex,
                                                               lpCancelAddress,
                                                               lpvNotifyParameter,
                                                               lpvCancelId);
            if(status != REQUEST_IGNORED)
            {
                BOOL fRet = ResetTT(bPort);

                if(!fRet)
                {
                    DEBUGMSG(ZONE_ERROR,
                       (TEXT("%s: -CHub(%s tier %d)::AbortTransfer \
                              ResetTT failed\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber));
                }
            }
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::AbortTransfer - address = %d, \
              uPipeIndex = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: IsPipeHalted
//
// Description: Determine if the pipe "uPipeIndex" on device at
//              address "address" is halted, and return result in
//              lpfHalted
//
// Parameters: See description in CHcd::IsPipeHalted
//
// Returns: REQUEST_OK - iflpbHalted set to pipe's correct status
//
//          REQUEST_FAILED - if pipe exists, but unable to set lpfHalted
//
//          REQUEST_IGNORED - if pipe does not exist
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::IsPipeHalted(IN const UINT uAddress,
                                       IN const UINT uPipeIndex,
                                       OUT LPBOOL const lpfHalted)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::IsPipeHalted - \
              address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    // no one should be calling IsPipeHalted on hubs, since we handle
    // hubs internally.
    DEBUGCHK(uAddress != m_bAddress);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->IsPipeHalted(uAddress,
                                                              uPipeIndex,
                                                              lpfHalted);
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::IsPipeHalted - address = %d, \
              pipe = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ResetPipe
//
// Description: Reset pipe "uPipeIndex" on device with address "address"
//
// Parameters: See description in CHcd::ResetPipe
//
// Returns: REQUEST_OK - if pipe reset
//
//          REQUEST_FAILED - if pipe exists, but unable to reset it
//
//          REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::ResetPipe(IN const UINT uAddress,
                                    IN const UINT uPipeIndex)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::ResetPipe - \
              address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    // no one should be calling ResetPipe on hubs, since we handle
    // hubs internally.
    DEBUGCHK(uAddress != m_bAddress);
    PREFAST_DEBUGCHK(m_ppCDeviceOnPort!=NULL);

    for(UCHAR bPort = 0;
        status == REQUEST_IGNORED &&
        m_ppCDeviceOnPort != NULL &&
        bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            status = m_ppCDeviceOnPort[bPort]->ResetPipe(uAddress, uPipeIndex);

            if(status != REQUEST_IGNORED)
            {
                BOOL fRet = ResetTT(bPort);

                if(!fRet)
                {
                    DEBUGMSG(ZONE_ERROR,
                       (TEXT("%s: -CHub(%s tier %d)::ResetPipe ResetTT failed\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber));
                }
            }
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::ResetPipe - address = %d, \
              uPipeIndex = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: DisableDevice
//
// Description: Disable Downstream Device with address "address"
//
// Parameters: See description in CHcd::DisableDevice.
//
// Returns: REQUEST_OK - ifDevice Reset
//
//          REQUEST_FAILED - if device exists, but unable to disable it.
//
//          REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::DisableDevice(IN const UINT uAddress,
                                     IN const BOOL fReset)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::DisableDevice - \
              address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fReset));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    if(uAddress != m_bAddress)
    {
        for(UCHAR bPort = 0;
            status == REQUEST_IGNORED &&
            m_ppCDeviceOnPort != NULL &&
            bPort < m_usbHubDescriptor.bNumberOfPorts;
            bPort++)
        {
            if(m_ppCDeviceOnPort[bPort] != NULL)
            {
                status =  m_ppCDeviceOnPort[bPort]-> DisableDevice(uAddress, fReset);
            }
        }
    }
    else
    {
        ASSERT(FALSE);
        status = REQUEST_FAILED;
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::DisableDevice - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        status));
    return status;
}

//----------------------------------------------------------------------------------
// Function: NotifyOnSuspendedResumed
//
// Description: Notify all down stream device
//
// Parameters: fResumed - if resume notification needed
//
// Returns: Always TRUE
//----------------------------------------------------------------------------------
BOOL CHub::NotifyOnSuspendedResumed(BOOL fResumed)
{
    for(UCHAR bPort = 0;
        m_ppCDeviceOnPort != NULL && bPort < m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort] != NULL)
        {
            // If it is resume call resume notification to unblock downstream hubs.
            if(fResumed)
            {
                m_ppCDeviceOnPort[bPort]-> ResumeNotification();
            }

            m_ppCDeviceOnPort[bPort]-> NotifyOnSuspendedResumed(fResumed);
        }
    }
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: SuspendResume
//
// Description:  Suspend or Resume on device with address "address"
//
// Parameters: See description in CHcd::SuspendResume
//
// Returns: REQUEST_OK - if device suspend or resumed
//
//          REQUEST_FAILED - if device exists, but unable to reset it
//
//          REQUEST_IGNORED - if no device found with matching address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CHub::SuspendResume(IN const UINT uAddress,
                                     IN const BOOL fSuspend)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::SuspendResume - \
              address = %d, fSuspend = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fSuspend));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    EnterCriticalSection(&m_csDeviceLock);

    if(uAddress != m_bAddress)
    {
        for(UCHAR bPort = 0;
            status == REQUEST_IGNORED &&
            m_ppCDeviceOnPort != NULL &&
            bPort < m_usbHubDescriptor.bNumberOfPorts;
            bPort++)
        {
            if(m_ppCDeviceOnPort[bPort] != NULL)
            {
                status = m_ppCDeviceOnPort[bPort]->SuspendResume(uAddress, fSuspend);
            }
        }
    }
    // It should not happens
    else
    {
        ASSERT(FALSE);
        status = REQUEST_FAILED;
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::SuspendResume - address = %d, \
              returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: DisableOffStreamDevice
//
// Description: Disable all down stream device
//
// Parameters: uAddress - device address
//
//             fReset - is reset needed
//
// Returns: 0
//----------------------------------------------------------------------------------
BOOL CHub::DisableOffStreamDevice(IN const UINT uAddress, IN const BOOL fReset)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::DisableOffStreamDevice - \
              address = %d, reset = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fReset));

    EnterCriticalSection(&m_csDeviceLock);
    BOOL fReturn = FALSE;

    for(UCHAR bPort = 1;
        bPort <= m_usbHubDescriptor.bNumberOfPorts;
        bPort++)
    {
        if(m_ppCDeviceOnPort[bPort - 1] != NULL &&
            m_ppCDeviceOnPort[bPort - 1]->GetDeviceAddress() == uAddress)
        {
            USB_HUB_AND_PORT_STATUS hubStatus;

            if(GetStatus(bPort, hubStatus) && hubStatus.status.port.usPortConnected)
            {
                // Hub Status change thread will NOT wake up by Disable Port because
                // DisablePort will NOT generate
                // hubStatus.change.port.usPortEnableChange
                // So after DisablePort(port), we have to call AttachDevice Manually
                DetachDevice(bPort,TRUE);
                if(fReset)
                {
                    DEBUGMSG(ZONE_HUB || ZONE_ATTACH,
                       (TEXT("%s: +CHub(%s tier %d)::DisableOffStreamDevice - \
                              address = %d, port = %d about to re-attach\n"),
                        GetControllerName(),
                        GetDeviceType(),
                        m_bTierNumber,
                        uAddress,
                        bPort));

                    LeaveCriticalSection(&m_csDeviceLock);
                    // 0.05 sec is what should take for peripheral to
                    // complete its reset cycle
                    Sleep(50);
                    EnterCriticalSection(&m_csDeviceLock);

                    AttachDevice(bPort,
                        hubStatus.status.port.usDeviceSpeed,
                        TRUE,
                        FALSE);
                }
            }

            fReturn = TRUE;

            break;
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::DisableOffStreamDevice - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fReturn));

    ASSERT(fReturn == TRUE);
    return fReturn;
}

//----------------------------------------------------------------------------------
// Function: SuspendResumeOffStreamDevice
//
// Description: Resume all down stream device
//
// Parameters: uAddress - device address
//
//             fSuspend - if resume notification needed
//
// Returns: 0
//----------------------------------------------------------------------------------
BOOL CHub::SuspendResumeOffStreamDevice(IN const UINT uAddress,
                                        IN const BOOL fSuspend)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CHub(%s tier %d)::SuspendResume - \
              address = %d, fSuspend = %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fSuspend));

    BOOL fReturn = FALSE;

    for(UCHAR bPort = 1; bPort <= m_usbHubDescriptor.bNumberOfPorts; bPort++)
    {
        if(m_ppCDeviceOnPort[bPort - 1]!= NULL &&
            m_ppCDeviceOnPort[bPort - 1]->GetDeviceAddress()== uAddress)
        {
            UCHAR bSetOrClearFeature = USB_REQUEST_CLEAR_FEATURE;

            if(fSuspend)
            {
                bSetOrClearFeature = USB_REQUEST_SET_FEATURE;
            }

            // if(SetOrClearFeature(bPort,
            // fSuspend USB_REQUEST_SET_FEATURE: USB_REQUEST_CLEAR_FEATURE,
            // USB_HUB_FEATURE_PORT_SUSPEND))
            if(SetOrClearFeature(bPort, bSetOrClearFeature, USB_HUB_FEATURE_PORT_SUSPEND))
            {
                fReturn = TRUE;

                // If it is resume.
                if(!fSuspend)
                {
                    Sleep(20);
                    m_ppCDeviceOnPort[bPort - 1]->ResumeNotification();
                }

                m_ppCDeviceOnPort[bPort - 1]->NotifyOnSuspendedResumed(!fSuspend);
            }

            break;
        }
    }

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CHub(%s tier %d)::SuspendResume - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetDeviceType(),
        m_bTierNumber,
        uAddress,
        fReturn));

    return fReturn;
}

//----------------------------------------------------------------------------------
// Function: ResetTT
//
// Description: This function will reset TT buffer for low/full speed
//
// Parameters: bPort - port for which the TT needs to be reset.
//
// Returns: TRUE if the request succeeded, else FALSE
//----------------------------------------------------------------------------------
BOOL  CHub::ResetTT(IN const UCHAR bPort)
{
    DEBUGMSG(ZONE_HUB,
       (TEXT("%s: +CHub::ResetTT\n"),
        GetControllerName()));

    DEBUGCHK(m_deviceInfo.usbDeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB);

    BOOL                fTransferDone = FALSE;
    DWORD               dwBytesTransferred = 0;
    DWORD               dwErrorFlags = USB_NOT_COMPLETE_ERROR;
    HCD_REQUEST_STATUS  status = REQUEST_FAILED;
    UCHAR               bTTHubAddr= 0;
    PVOID               pTTContext = NULL;
    UCHAR               bTTPort=0;
    USB_DEVICE_REQUEST  usbRequest;

    // Usb Spec 2.0 11.24.2.9
    usbRequest.bmRequestType = USB_REQUEST_HOST_TO_DEVICE |
                                USB_REQUEST_CLASS |
                                USB_REQUEST_FOR_OTHER;
    usbRequest.bRequest = USB_HUB_REQUEST_RESET_TT;
    usbRequest.wValue = 0;
    usbRequest.wLength = 0;

    // Check if we are going to reset the TT fort he correct hub.
    if(m_ppCDeviceOnPort[bPort]->GetUSB2TT(bTTHubAddr, bTTPort, pTTContext) != NULL)
    {
        if(bTTHubAddr == m_bAddress)
        {
            // m_deviceInfo.usbDeviceDescriptor.bDeviceProtocol is set to 1
            //(Usb spec 2.0, section 11.23.1) if there is only one TT,
            // m_deviceInfo.usbDeviceDescriptor.bDeviceProtocol is set to 2
            //(Usb spec 2.0, section 11.23.1) for multiple TT,
            if(m_deviceInfo.usbDeviceDescriptor.bDeviceProtocol == 1)
            {
                // reset single TT. Port number need to set to 1.
                usbRequest.wIndex = 1;
            }
            else if(m_deviceInfo.usbDeviceDescriptor.bDeviceProtocol == 2)
            {
                // reset multiple TT
                usbRequest.wIndex = bTTPort;
            }
        }
        else
        {
            // no need to reset as this is not the TT to be reset.
            return TRUE;
        }
    }
    else
    {
        // no hubs associated
        return TRUE;
    }

    if (m_ppCPipe == NULL) 
    {
        return FALSE;
    }

    __try
    {
        if(!m_fHubThreadClosing)
        {
            status = m_ppCPipe[ENDPOINT0_CONTROL_PIPE]->IssueTransfer(
                             m_bAddress, // address of this hub
                             TransferDoneCallbackSetEvent, // callback func
                             m_hHubStatusChangeEvent, // callback param
                             USB_OUT_TRANSFER, // transfer flags
                             &usbRequest, // control request
                             0, // dwStartingFrame(not used)
                             0, // dwFrames(not used)
                             NULL, // aLengths(not used)
                             0, // buffer size
                             NULL, // buffer
                             0, // phys addr of buffer(not used)
                             this, // cancel id
                             NULL, // lpdwIsochErrors(not used)
                             NULL, // lpdwIsochLengths(not used)
                             &fTransferDone, // OUT param for transfer
                             &dwBytesTransferred, // OUT param for transfer
                             &dwErrorFlags); // OUT param for transfer

            if(status == REQUEST_OK)
            {
                WaitForSingleObject(m_hHubStatusChangeEvent, STANDARD_REQUEST_TIMEOUT);

                if(m_fHubThreadClosing)
                {
                    m_ppCPipe[ENDPOINT0_CONTROL_PIPE]->AbortTransfer(
                                                 NULL, // callback function
                                                 NULL, // callback parameter
                                                 this); // cancel ID
                }
            }

            DEBUGCHK(fTransferDone);
        }
    }
    __except(::XhcdProcessException (GetExceptionCode()))
    {
        DEBUGMSG(ZONE_ERROR,
           (TEXT("%s: CHub::ResetTT Exception whiletrying to reset the TT\n"),
            GetControllerName()));
    }

    BOOL fSuccess =(status == REQUEST_OK &&
                     fTransferDone &&
                     dwErrorFlags == USB_NO_ERROR);

    DEBUGMSG(ZONE_HUB,
       (TEXT("%s: -CHub::ResetTT, returing BOOL %d\n"),
        GetControllerName(),
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: CRootHub
//
// Description: Constructor for CRootHub. Do not initialize static variables here.
//              Do that in the Initialize() routine
//              The root hub gets address 0, since real devices are not
//              allowed to communicate on address 0 except at config time,
//              and since the RootHub never needs to send transfers for itself.
//              Also, root hub is by definition at tier 0
//
// Parameters: See description in CHub
//
// Returns: None
//----------------------------------------------------------------------------------
CRootHub::CRootHub(IN const USB_DEVICE_INFO& rDeviceInfo,
                    IN const UCHAR bSpeed,
                    IN const USB_HUB_DESCRIPTOR& rUsbHubDescriptor,
                    IN CHcd * const pCHcd) : CHub(0,
                                                rDeviceInfo,
                                                bSpeed,
                                                0,
                                                rUsbHubDescriptor,
                                                pCHcd,
                                                NULL,
                                                0) // call base constructor
{
}

//----------------------------------------------------------------------------------
// Function: CRootHub
//
// Description: Destructor for CRootHub. Do not delete static variables here.
//              Do that in DeInitialize();
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CRootHub::~CRootHub()
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::~CRootHub\n"),
        GetControllerName()));
    // Nothing to do here yet...
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::~CRootHub\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: EnterOperationalState
//
// Description: Do processing needed to get this hub into a working state.
//              For now, -create device on ports array
//                       -start status change thread
//
// Parameters: pStartEndpointPipe - pointer to open pipe for this device's
//                              endpoint0.(ignored for root hub)
//
// Returns: attachment flags for the device - either ZERO(Ok) or
//          FXN_DEV_STATE_ENABFAILED - should not happen to root hub
//----------------------------------------------------------------------------------
DWORD CRootHub::EnterOperationalState(IN CPipeAbs * const DEBUG_ONLY(pStartEndpointPipe))
{
    DEBUGMSG(ZONE_HUB,
       (TEXT("%s: +CRootHub::EnterOperationalState\n"),
        GetControllerName()));

    DWORD dwSuccess = 0xFF;

    EnterCriticalSection(&m_csDeviceLock);

    // don't need to allocate a Pipe array forRoot Hubs
    DEBUGCHK(m_bAddress == 0 &&
              m_bMaxNumPipes == 0 &&
              m_ppCPipe == NULL &&
              pStartEndpointPipe == NULL &&
              m_deviceInfo.usbDeviceDescriptor.bDeviceClass == USB_DEVICE_CLASS_HUB &&
              m_deviceInfo.usbDeviceDescriptor.bNumConfigurations == 0 &&
              m_deviceInfo.pnonConstUsbActiveConfig == NULL &&
              m_deviceInfo.pnonConstUsbConfigs == NULL);

    DEBUGCHK(m_hHubStatusChangeEvent == NULL && m_hHubStatusChangeThread == NULL);

    // m_hHubStatusChangeEvent - Auto Reset, and Initial State = non-signaled
    if(NULL == m_hHubStatusChangeEvent)
    {
        m_hHubStatusChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    if(m_hHubStatusChangeEvent != NULL && AllocateDeviceArray())
    {
        m_hHubStatusChangeThread = CreateThread(0,
                                                0,
                                                HubStatusChangeThreadStub,
                                                this,
                                                0,
                                                NULL);

        if(m_hHubStatusChangeThread != NULL)
        {
            CeSetThreadPriority(m_hHubStatusChangeThread,
                                g_dwIstThreadPriority + RELATIVE_PRIO_STSCHG);
            dwSuccess = 0;
        }
    }

    if(dwSuccess != 0)
    {
        // m_ppCDeviceOnPort will be freed in ~CHub if needed
        // terminal failed state during attach
        m_dwDevState |= FXN_DEV_STATE_ENABFAILED;

        if(m_hHubStatusChangeEvent)
        {
            CloseHandle(m_hHubStatusChangeEvent);
            m_hHubStatusChangeEvent = NULL;
        }

        DEBUGCHK(m_hHubStatusChangeThread == NULL);
    }

    // set state to "operational" - clear "attach" bits but preserve "detach" bits
    m_dwDevState &= FXN_DEV_STATE_DETACH_BITS;
    dwSuccess = m_dwDevState;

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_HUB,
       (TEXT("%s: -CRootHub::EnterOperationalState, returning state %d\n"),
        GetControllerName(),
        dwSuccess));

    return dwSuccess;
}

//----------------------------------------------------------------------------------
// Function: PowerAllHubPorts
//
// Description: Power all of this hub's ports so that they can send
//              status change notifications
//
// Parameters: None
//
// Returns: TRUE - the root hub's ports are already powered
//----------------------------------------------------------------------------------
BOOL CRootHub::PowerAllHubPorts(VOID)
{
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::PowerAllHubPorts\n"),
        GetControllerName()));
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::PowerAllHubPorts\n"),
        GetControllerName()));
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: WaitForPortStatusChange
//
// Description: Wait until a status change occurs on one of this hub's ports,
//              then return the status change information in rPort and rStatus
//              This routine is also responsible for clearing the status
//              change notifications from the port itself
//
// Parameters: rPort - out param to get port # of port whose status changed
//
//             rStatus - out structure describing port's status change
//
// Returns: TRUE if rPort and rStatus set properly, else FALSE
//----------------------------------------------------------------------------------
BOOL CRootHub::WaitForPortStatusChange(OUT UCHAR& rPort,
                                        OUT USB_HUB_AND_PORT_STATUS& rStatus)
{
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::WaitForPortStatusChange\n"),
        GetControllerName()));

    BOOL fSuccess = FALSE;
    USB_HUB_AND_PORT_STATUS portStatus = {0};

    // root hub - we need to poll for status changes.
    while(!m_fHubThreadClosing && !fSuccess)
    {
        for(UCHAR bPort = 1;
            !fSuccess && bPort <= m_usbHubDescriptor.bNumberOfPorts;
            bPort++)
        {
            portStatus.change.wWord = 0;

            if(GetStatus(bPort, portStatus) && portStatus.change.wWord != 0)
            {
                // port status changed on this port
                rPort    = bPort;
                rStatus  = portStatus;
                fSuccess=TRUE;
            }
        }

        if(!m_fHubThreadClosing &&
            !fSuccess &&
            !m_pCHcd->WaitForPortStatusChange(m_hHubStatusChangeEvent))
        {
            // If HCD does not support Root Hub Status Change. We do follows
            WaitForSingleObject(m_hHubStatusChangeEvent, STANDARD_STATUS_TIMEOUT);
        }

        if((m_pCHcd->GetCapability() & HCD_SUSPEND_ON_REQUEST) != 0)
        {
            break;
        }
    }

    if(fSuccess)
    {
        // acknowledge the change bits
        for(USHORT wBit = 0; wBit < 16; ++wBit)
        {
            if(rStatus.change.wWord &(1 << wBit))
            {
                SetOrClearFeature(rPort,
                    USB_REQUEST_CLEAR_FEATURE,
                    wBit | 0x10);
            }
        }
    }

    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::WaitForPortStatusChange, \
              rPort = %d, fSuccess = %d\n"),
        GetControllerName(),
        rPort,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: GetStatus
//
// Description: This function will get the status of the port and return it
//
// Parameters: bPort - 0 for the hub itself, otherwise the hub port number
//
//             rStatus - reference to USB_HUB_AND_PORT_STATUS to get the
//                       status
//
// Returns: Returns value of CHW::GetPortStatus function
//----------------------------------------------------------------------------------
BOOL  CRootHub::GetStatus(IN const UCHAR bPort,
                           OUT USB_HUB_AND_PORT_STATUS& rStatus)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::GetStatus - port = %d\n"),
        GetControllerName(),
        bPort));

    DEBUGCHK(bPort <= m_usbHubDescriptor.bNumberOfPorts);

    // CHW::GetPortStatus will not clear the change bits
    BOOL fSuccess = m_pCHcd->GetPortStatus(bPort, rStatus);

    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::GetStatus - port = %d, returing BOOL %d\n"),
        GetControllerName(),
        bPort,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: ResetAndEnablePort
//
// Description: reset/enable device on the given port so that when this
//              function completes, the device is listening on address 0
//              Assumes address0 lock is held
//
// Parameters: port - port # to reset/enable
//
// Returns: Returns value of CHW::ResetAndEnablePort function
//----------------------------------------------------------------------------------
BOOL CRootHub::ResetAndEnablePort(IN const UCHAR bPort)
{
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::ResetAndEnablePort - port = %d\n"),
        GetControllerName(),
        bPort));

    DEBUGCHK(bPort >= 1 && bPort <= m_usbHubDescriptor.bNumberOfPorts);

    BOOL fSuccess = m_pCHcd->ResetAndEnablePort(bPort);

    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::ResetAndEnablePort - port = %d, returning %d\n"),
        GetControllerName(),
        bPort,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: DisablePort
//
// Description: Disable the given port
//
// Parameters: bPort - port # to disable
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CRootHub::DisablePort(IN const UCHAR bPort)
{
    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::DisablePort - port = %d\n"),
        GetControllerName(),
        bPort));

    DEBUGCHK(bPort >= 1 && bPort <= m_usbHubDescriptor.bNumberOfPorts);

    m_pCHcd->DisablePort(bPort);

    DEBUGMSG(ZONE_ATTACH && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::DisablePort - port = %d\n"),
        GetControllerName(),
        bPort));
}

//----------------------------------------------------------------------------------
// Function: SetOrClearFeature
//
// Description: This function will set or clear a feature on the given port
//              if that feature exists.
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
//             bSetOrClearFeature - this is USB_REQUEST_SET_FEATURE or
//                                 USB_REQUEST_CLEAR_FEATURE
//
//             wFeature - one of the features to set/clear - this should
//                       be USB_FEATURE_ENDPOINT_STALL or one of the
//                       USB_HUB_FEATURE_* features
//
// Returns: TRUE if the request succeeded, else FALSE
//----------------------------------------------------------------------------------
BOOL CRootHub::SetOrClearFeature(IN const WORD wPort,
                                  IN const UCHAR bSetOrClearFeature,
                                  IN const USHORT wFeature)
{
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: +CRootHub::SetOrClearFeature - port = %d, \
              set/clear = 0x%x, feature = 0x%x\n"),
        GetControllerName(),
        wPort,
        bSetOrClearFeature,
        wFeature));

#ifdef DEBUG
{
    if(bSetOrClearFeature == USB_REQUEST_CLEAR_FEATURE)
    {
        if(wPort == 0)
        {
            // USB spec 1.1, 11.16.2.2 - these are the only
            // features which should be cleared for ports
            DEBUGCHK(wFeature == USB_HUB_FEATURE_C_HUB_LOCAL_POWER ||
                      wFeature == USB_HUB_FEATURE_C_HUB_OVER_CURRENT ||
                      wFeature == USB_FEATURE_ENDPOINT_STALL);
        }
        else
        {
            // USB spec 1.1, 11.16.2.2 - these are the only
            // features which should be cleared for ports
            DEBUGCHK(wFeature == USB_HUB_FEATURE_PORT_ENABLE ||
                      wFeature == USB_HUB_FEATURE_PORT_SUSPEND ||
                      wFeature == USB_HUB_FEATURE_PORT_POWER ||
                      wFeature == USB_HUB_FEATURE_C_PORT_CONNECTION ||
                      wFeature == USB_HUB_FEATURE_C_PORT_RESET ||
                      wFeature == USB_HUB_FEATURE_C_WARM_PORT_RESET ||
                      wFeature == USB_HUB_FEATURE_C_PORT_ENABLE ||
                      wFeature == USB_HUB_FEATURE_C_PORT_SUSPEND ||
                      wFeature == USB_HUB_FEATURE_C_PORT_OVER_CURRENT);
        }
    }
    else if(bSetOrClearFeature == USB_REQUEST_SET_FEATURE)
    {
        // should only be setting port features
        DEBUGCHK(wPort > 0 &&
                 (wFeature == USB_HUB_FEATURE_PORT_RESET ||
                   wFeature == USB_HUB_FEATURE_PORT_SUSPEND ||
                   wFeature == USB_HUB_FEATURE_PORT_INDICATOR ||
                   wFeature == USB_HUB_FEATURE_PORT_POWER));
    }
    else
    {
        // shouldn't be here
        ASSERT(0);
    }

    DEBUGCHK(wPort <= m_usbHubDescriptor.bNumberOfPorts);
}
#endif // DEBUG

    BOOL fSuccess = m_pCHcd->RootHubFeature((UCHAR)wPort,
                                            bSetOrClearFeature,
                                            wFeature);

    DEBUGMSG(ZONE_ERROR && !fSuccess,
       (TEXT("%s: CRootHub::SetOrClearFeature - port = %d, \
              set/clear = 0x%x, feature = 0x%x, FAILED\n"),
        GetControllerName(),
        wPort,
        bSetOrClearFeature,
        wFeature));
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CRootHub::SetOrClearFeature - port = %d, \
              set/clear = 0x%x, feature = 0x%x, returing BOOL %d\n"),
        GetControllerName(),
        wPort,
        bSetOrClearFeature,
        wFeature,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: SetOrClearRemoteWakup
//
// Description: This function will set or clear a remove wakeup feature
//              External HUB.
//
// Parameters: not used
//
// Returns: Always TRUE
//----------------------------------------------------------------------------------
BOOL  CRootHub::SetOrClearRemoteWakup(BOOL /*fSet*/)
{
    return TRUE;
}

#define IS_FUNCTION_DEVICE_UNPLUGGED \
    do {\
        if(m_dwDevState>=FXN_DEV_STATE_ENABFAILED) \
        {SetLastError(ERROR_DEVICE_NOT_AVAILABLE);}\
    }while(0)

//----------------------------------------------------------------------------------
// Function: CFunction
//
// Description: Constructor for CFunction. Do not initialize static variables here.
//              Do that in the Initialize() routine
//
//              Do not call into USBD yet, that will be done in EnterOperationalState
//
// Parameters: address, rDeviceInfo, bSpeed, bTierNumber - see CDevice::CDevice
//
// Returns: None
//----------------------------------------------------------------------------------
CFunction::CFunction(IN const UCHAR bAddress,
                      IN const USB_DEVICE_INFO& rDeviceInfo,
                      IN const UCHAR bSpeed,
                      IN const UCHAR bTierNumber,
                      IN CHcd * const pCHcd,
                      IN CHub * const pAttachedHub,
                      const UCHAR bAttachedPort,
                      IN PVOID pDevicePointer) : CDevice(bAddress,
                                                            rDeviceInfo,
                                                            bSpeed,
                                                            bTierNumber,
                                                            pCHcd,
                                                            pAttachedHub,
                                                            bAttachedPort), // call base constructor
                                                                m_pCHcd(pCHcd),
                                                                m_lpvDetachId(NULL), // detach id forUSBD
                                                                m_hFunctionFeatureEvent(NULL),
                                                                m_pLogicalDevice(pDevicePointer)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::CFunction\n"),
        GetControllerName(),
        bTierNumber));

    // flag as Function; it may be useful to tell Function from Hub
    m_dwDevState |= FXN_DEV_STATE_FUNCTION;

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::CFunction\n"),
        GetControllerName(),
        bTierNumber));
}

//----------------------------------------------------------------------------------
// Function: CFunction
//
// Description: Destructor for CFunction. Do not delete static variables here.
//              Do that in the DeInitialize() routine
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CFunction::~CFunction()
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::~CFunction\n"),
        GetControllerName(),
        m_bTierNumber));

    DEBUGCHK(m_lpvDetachId == NULL);

    // rest of processing done in base destructors...
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::~CFunction\n"),
        GetControllerName(),
        m_bTierNumber));
}

//----------------------------------------------------------------------------------
// Function: EnterOperationalState
//
// Description: Enable this function to start processing USB transactions.
//              The return bits are examined in the calling thread
//              to determine if rapid attach-detach had happened.
//              If detachment had started while this thread invoked
//              USBD and not back yet, Detach will set flag in
//              FXN_DEV_STATE_DETACH_BITS and wait forAttach to end.
//              Returning <m_dwDevState> allows caller to check bits
//              in the returned value on the stack, without reference
//              to the(likely) already destroyed CFunction class.
//              Once <LeaveCriticalSection> is reached, the instance
//              of the class may be destroyed immediately by Detach.
//
// Parameters: pStartEndpointPipe - pointer to an already open control pipe
//                              to the function's endpoint 0
//
// Returns: bit flags for attachment state - ZERO for success or
//          FXN_DEV_STATE_ENABFAILED with progress bits
//----------------------------------------------------------------------------------
DWORD CFunction::EnterOperationalState(IN CPipeAbs* const pStartEndpointPipe)
{
    DEBUGMSG(ZONE_FUNCTION,
       (TEXT("%s: +CFunction(tier %d)::EnterOperationalState\n"),
        GetControllerName(),
        m_bTierNumber));

    DWORD dwSuccess = 0xFF;

    EnterCriticalSection(&m_csDeviceLock);
    m_dwDevState |= FXN_DEV_STATE_ATTACHMENT; // attach-in-progress

    DEBUGCHK(m_pCHcd->GetpUSBDAttachProc() != NULL &&
              m_pCHcd->GetpHcdContext() != NULL &&
              m_lpvDetachId == NULL &&  // device not attached yet
              m_ppCPipe == NULL && // not allocated yet
              m_bMaxNumPipes == 0 && // refers to m_ppCPipe, which is not allocated
              m_bAddress > 0 &&
              m_bAddress <= USB_MAX_ADDRESS &&
              pStartEndpointPipe != NULL); // control pipe to endpoint 0

    DEBUGCHK(m_hFunctionFeatureEvent == NULL);
    // m_hFunctionFeatureEvent - Auto Reset, and Initial State = non-signaled
    m_hFunctionFeatureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(AllocatePipeArray())
    {
        m_ppCPipe[ENDPOINT0_CONTROL_PIPE] = pStartEndpointPipe;
        LeaveCriticalSection(&m_csDeviceLock);

        //
        // ifUser pulls out USB cable from port during USBD & Client call, DETACH will trigger
        // State of the function class will be marked as "detach-in-progress"
        //
        DEBUGMSG(ZONE_ATTACH || ZONE_FUNCTION,
           (TEXT("%s: CFunction(tier %d)::EnterOperationalState, \
                  calling USBD...\r\n\r\n"),
            GetControllerName(),
            m_bTierNumber));

        if((*m_pCHcd->GetpUSBDAttachProc())(m_pCHcd->GetpHcdContext(), // context forHost Controller Driver
                                                m_bAddress, // used to indicate this specific device
                                                ENDPOINT0_CONTROL_PIPE, // index of endpoint 0 pipe
                                                LPCUSB_DEVICE(&m_deviceInfo), // USB descriptors
                                                &m_lpvDetachId)) // used to tell USBD this device is being detached
        {
            dwSuccess = 0;
        }

        DEBUGMSG(ZONE_ATTACH || ZONE_FUNCTION,
           (TEXT("%s: CFunction(tier %d)::EnterOperationalState, \
                  USBD returns %x, err %x\r\n\r\n"),
            GetControllerName(),
            m_bTierNumber,
            m_lpvDetachId,
            dwSuccess));

        // only upon success Id for detach must be provided - ASSERT that
        DEBUGCHK((m_lpvDetachId==NULL && dwSuccess!=0) ||
           (m_lpvDetachId!=NULL && dwSuccess==0));

        EnterCriticalSection(&m_csDeviceLock);
    }

    if(dwSuccess != 0)
    {
        DEBUGMSG(ZONE_ERROR,
           (TEXT("%s: CFunction(tier %d)::EnterOperationalState - failed\n"),
            GetControllerName(),
            m_bTierNumber));

        // caller will handle pStartEndpointPipe
        delete [] m_ppCPipe;
        m_ppCPipe = NULL;
        m_bMaxNumPipes = 0;
        m_lpvDetachId = NULL;
        // terminal failed state during attach
        m_dwDevState |= FXN_DEV_STATE_ENABFAILED;
    }

    // set state to "operational" - clear "attach" bits but preserve "detach" bits
    m_dwDevState &= FXN_DEV_STATE_DETACH_BITS;
    dwSuccess = m_dwDevState;

    LeaveCriticalSection(&m_csDeviceLock);

    DEBUGMSG(ZONE_FUNCTION,
       (TEXT("%s: -CFunction(tier %d)::EnterOperationalState, \
              returning state %d\n"),
        GetControllerName(),
        m_bTierNumber,
        dwSuccess));

    return dwSuccess;
}

//----------------------------------------------------------------------------------
// Function: NotifyOnSuspendedResumed
//
// Description: Notify all down stream device
//
// Parameters: fResumed - if resume notification needed
//
// Returns: TRUE if success
//----------------------------------------------------------------------------------
BOOL CFunction::NotifyOnSuspendedResumed(BOOL fResumed)
{
    EnterCriticalSection(&m_csDeviceLock);
    LPUSBD_SUSPEND_RESUME_PROC pProc = m_pCHcd->GetpUSBDSuspendedResumed();
    BOOL fSuccess = FALSE;

    if(pProc && m_lpvDetachId)
    {
        DEBUGMSG(ZONE_FUNCTION || ZONE_INIT,
           (TEXT("%s:    CFunction::NotifyOnSuspendedResumed(%u) \
                  callback to Client...\n"),
            GetControllerName(),
            fResumed));
        fSuccess =(*pProc)(m_lpvDetachId, fResumed);
    }

    LeaveCriticalSection(&m_csDeviceLock);

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: OpenPipe
//
// Description: Open a new pipe to the device with the given address. The
//              pipe information is passed in by lpEndpointDescriptor, and
//              the index of the new pipe is returned in *lpPipeIndex
//
// Parameters: see CHcd::OpenPipe
//
// Returns: REQUEST_OK - if pipe opened successfully, and index stored
//                      in lpPipeIndex.
//
//         REQUEST_FAILED - if unable to open the pipe on the device
//                          with the given address
//
//         REQUEST_IGNORED - if we were unable to find a device with
//                           the given address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::OpenPipe(IN const UINT uAddress,
                                        IN LPCUSB_ENDPOINT_DESCRIPTOR const lpEndpointDescriptor,
                                        OUT LPUINT const lpPipeIndex,
                                        OUT LPVOID* const plpCPipe)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::OpenPipe - address = %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;
    UCHAR bPipe, bNumEndpoints, bEndpoint = 0, bMaxEndpoints;
    INT iInterfaces;
    BOOL fFound;

    if(plpCPipe) *plpCPipe = NULL;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(uAddress == m_bAddress)
    {
        EnterCriticalSection(&m_csDeviceLock);
        // OpenPipe is referring to this device. Must return
        // something other than REQUEST_IGNORED.
        status = REQUEST_FAILED;

        if(m_fIsSuspend)
        {
            IS_FUNCTION_DEVICE_UNPLUGGED;
            RETAILMSG(1,
               (TEXT("!!!CFunction(tier %d)::OpenPipe \
                      in suspend state not allowed, address = %d\n"),
                m_bTierNumber,
                m_bAddress));
            ASSERT(FALSE);
        }
        else
        {
            PREFAST_DEBUGCHK(m_ppCPipe != NULL);
            DEBUGCHK(m_bMaxNumPipes > 0 &&
                  m_deviceInfo.pnonConstUsbActiveConfig != NULL &&
                  m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces != NULL);

            // There are m_bMaxNumPipes, but the endpoint descriptor for
            // the control pipe to endpoint 0 is not stored in our array of
            // endpoint descriptors. Thus, we only need to check
            // m_bMaxNumPipes - 1 pipes. The pipe needs to be indexed from 1,
            // since 0 already refers to the endpoint0 control pipe.
            DEBUGCHK(m_ppCPipe[ENDPOINT0_CONTROL_PIPE] != NULL);
            bPipe = 0;
            fFound = FALSE;
            bMaxEndpoints = 0;

            if ((m_deviceInfo.pnonConstUsbActiveConfig == NULL) ||
                (m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces == NULL))
            {
                LeaveCriticalSection(&m_csDeviceLock);
                return status;
            }

            iInterfaces = m_deviceInfo.pnonConstUsbActiveConfig->dwNumInterfaces;

            if(iInterfaces < 1)
            {
                DEBUGCHK(0); // shouldn't be possible - test is here to make prefast happy.
                LeaveCriticalSection(&m_csDeviceLock);
                return status;
            }

            for(UCHAR bInterface = 0;
                bInterface < iInterfaces && !fFound;
                bInterface++)
            {
                bNumEndpoints =
                    m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[bInterface].usbInterfaceDescriptor.bNumEndpoints;

                for(bEndpoint = 0;
                    bEndpoint < bNumEndpoints && !fFound;
                    bEndpoint++)
                {
                    if(memcmp(&m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[bInterface].pnonConstUsbEndpoints[bEndpoint].usbEndpointDescriptor,
                        lpEndpointDescriptor,
                        sizeof(USB_ENDPOINT_DESCRIPTOR)) == 0)
                    {
                        fFound = TRUE;
                    }
                }

                if(m_deviceInfo.pnonConstUsbActiveConfig->pnonConstUsbInterfaces[bInterface].usbInterfaceDescriptor.bAlternateSetting != 0)
                {
                    if(bMaxEndpoints < bNumEndpoints)
                    {
                        bMaxEndpoints = bNumEndpoints;
                    }
                }
                else
                {
                    bPipe += bMaxEndpoints;
                    bMaxEndpoints = bNumEndpoints;
                }
            }

            DEBUGCHK(fFound);

            if(fFound)
            {
                bPipe += bEndpoint;

                DEBUGMSG(ZONE_ERROR && bPipe == m_bMaxNumPipes,
                   (TEXT("%s: CFunction(tier %d)::OpenPipe - \
                          endpoint descriptor doesn't match any \
                          of this device's endpoints!\n"),
                    GetControllerName(),
                    m_bTierNumber));

                if(bPipe < m_bMaxNumPipes)
                {
                    if(m_ppCPipe[bPipe] != NULL)
                    {
                        DEBUGMSG(ZONE_WARNING,
                           (TEXT("%s: CFunction(tier %d)::OpenPipe - \
                                  address %d pipe %d appears to have \
                                  been opened before\n"),
                            GetControllerName(),
                            m_bTierNumber,
                            uAddress,
                            bPipe));

                        status = REQUEST_OK;
                        *lpPipeIndex = bPipe;
                    }
                    else
                    {
                        UCHAR bTTAddress=0;
                        UCHAR bTTPort=0;
                        PVOID pTTContext = NULL;

                        GetUSB2TT(bTTAddress, bTTPort,pTTContext);

                        switch(lpEndpointDescriptor->bmAttributes & USB_ENDPOINT_TYPE_MASK)
                        {
                        case USB_ENDPOINT_TYPE_BULK:
                            m_ppCPipe[bPipe] =
                                CreateBulkPipe(lpEndpointDescriptor,
                                                m_bSpeed,
                                                m_bAddress,
                                                bTTAddress,
                                                bTTPort,
                                                pTTContext,
                                                m_pCHcd);
                            break;

                        case USB_ENDPOINT_TYPE_CONTROL:
                            m_ppCPipe[bPipe] =
                                CreateControlPipe(lpEndpointDescriptor,
                                                    m_bSpeed,
                                                    m_bAddress,
                                                    bTTAddress,
                                                    bTTPort,
                                                    pTTContext,
                                                    m_pCHcd);
                            break;

                        case USB_ENDPOINT_TYPE_INTERRUPT:
                            m_ppCPipe[bPipe] =
                                CreateInterruptPipe(lpEndpointDescriptor,
                                                        m_bSpeed,
                                                        m_bAddress,
                                                        bTTAddress,
                                                        bTTPort,
                                                        pTTContext,
                                                        m_pCHcd);
                            break;

                        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                            m_ppCPipe[bPipe] =
                                CreateIsochronousPipe(lpEndpointDescriptor,
                                                        m_bSpeed,
                                                        m_bAddress,
                                                        bTTAddress,
                                                        bTTPort,
                                                        pTTContext,
                                                        m_pCHcd);
                            break;
                        default:
#ifdef DEBUG
                            // shouldn't be here
                            ASSERT(0);
#endif // DEBUG
                            break;
                        }

                        if(m_ppCPipe[bPipe] != NULL &&
                            m_ppCPipe[bPipe]->OpenPipe() == REQUEST_OK)
                        {
                            DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
                               (TEXT("%s: CFunction(tier %d)::OpenPipe - \
                                      opened new pipe, address %d, pipe %d\n"),
                                GetControllerName(),
                                m_bTierNumber,
                                uAddress,
                                bPipe));

                            status = REQUEST_OK;
                            m_ppCPipe[bPipe]->SetDevicePointer (m_pLogicalDevice);
                            *lpPipeIndex = bPipe;
                            if(plpCPipe)
                            {
                                *plpCPipe =(LPVOID)(m_ppCPipe[bPipe]);
                            }
                        }
                        else
                        {
                            delete m_ppCPipe[bPipe];
                            m_ppCPipe[bPipe] = NULL;
                        }
                    }
                }
            }
        }

        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::OpenPipe - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ClosePipe
//
// Description: Close the pipe "uPipeIndex" if this device's address is "address"
//
// Parameters: See description in CHcd::ClosePipe
//
// Returns: REQUEST_OK - if pipe closed or doesn't exist
//
//         REQUEST_FAILED - if pipe exists, but unable to close it
//
//         REQUEST_IGNORED - if this device's address is not "address"
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::ClosePipe(IN const UINT uAddress,
                                         IN const UINT uPipeIndex)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::ClosePipe - \
              address = %d, pipe = %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(uAddress == m_bAddress)
    {
        EnterCriticalSection(&m_csDeviceLock);
        // if the pipe doesn't exists, we can pretend that ClosePipe worked
        status = REQUEST_OK;

        if(uPipeIndex < m_bMaxNumPipes && m_ppCPipe[uPipeIndex] != NULL)
        {
            status = m_ppCPipe[uPipeIndex]->ClosePipe();

            DEBUGMSG(ZONE_FUNCTION,
               (TEXT("%s: CFunction(tier %d)::ClosePipe - \
                      address = %d, deleting pipe %d\n"),
                GetControllerName(),
                m_bTierNumber,
                uAddress,
                uPipeIndex));

            delete m_ppCPipe[uPipeIndex];
            m_ppCPipe[uPipeIndex] = NULL;
        }
    #ifdef DEBUG
        else
        {
            DEBUGMSG(ZONE_WARNING,
               (TEXT("%s: CFunction(tier %d)::ClosePipe - \
                      warning, pipe does not exist. Returning REQUEST_OK\n"),
                GetControllerName(),
                m_bTierNumber));
        }
    #endif // DEBUG
        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::ClosePipe - \
              address = %d, pipe = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a USB transfer to the pipe "pipe" on the device whose
//              address is "address"
//
// Parameters: see CHcd::IssueTransfer
//
// Returns: REQUEST_OK - if transfer issued
//
//         REQUEST_FAILED - if unable to issue transfer
//
//         REQUEST_IGNORED - if we were unable to find a device with
//                           the given address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::IssueTransfer(ISSUE_TRANSFER_PARAMS* pITP)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::IssueTransfer\n"),
        GetControllerName(),
        m_bTierNumber));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(pITP->uAddress == m_bAddress)
    {
        EnterCriticalSection(&m_csDeviceLock);

        status = REQUEST_FAILED;

        if(m_fIsSuspend)
        {
            IS_FUNCTION_DEVICE_UNPLUGGED;
            RETAILMSG(1,
               (TEXT("!!!CFunction(tier %d)::IssueTransfer \
                      in suspend state not allowed, address = %d, pipe = %d\n"),
                m_bTierNumber,
                pITP->uAddress,
                pITP->uPipeIndex));
            ASSERT(FALSE);
        }
        else if(pITP->uPipeIndex < m_bMaxNumPipes && m_ppCPipe[pITP->uPipeIndex] != NULL)
        {

            // Note: we don't modify CPipeAbs::IssueTransfer() to take the
            // single parameter struct, because it would break backwards
            // compatibility with existing drivers.
            status = m_ppCPipe[pITP->uPipeIndex]->IssueTransfer(
                                                       (const UCHAR)(pITP->uAddress),
                                                        pITP->lpfnStartAddress,
                                                        pITP->lpvParameter,
                                                        pITP->dwFlags,
                                                        pITP->lpcvControlHeader,
                                                        pITP->dwStartingFrame,
                                                        pITP->dwFrames,
                                                        pITP->lpcdwLengths,
                                                        pITP->dwBufferSize,
                                                        pITP->lpvBuffer,
                                                        pITP->ulBuffer,
                                                        pITP->lpcvCancelId,
                                                        pITP->lpdwIsochErrors,
                                                        pITP->lpdwIsochLengths,
                                                        pITP->lpfComplete,
                                                        pITP->lpdwBytesTransfered,
                                                        pITP->lpdwError);
        }

        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::IssueTransfer - \
              returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer
//
// Description: Abort the transfer occurring on pipe "uPipeIndex"
//
// Parameters: See description in CHcd::AbortTransfer
//
// Returns: REQUEST_OK - if transfer aborted
//
//         REQUEST_FAILED - if no transfer exists, or transfer exists
//                          and unable to abort it
//
//         REQUEST_IGNORED - if"address" doesn't match this device's
//                           address
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::AbortTransfer(
                            IN const UINT uAddress,
                            IN const UINT uPipeIndex,
                            IN LPTRANSFER_NOTIFY_ROUTINE const lpCancelAddress,
                            IN LPVOID const lpvNotifyParameter,
                            IN LPCVOID const lpvCancelId)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::AbortTransfer - \
              address = %d, pipe = %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(uAddress == m_bAddress)
    {
        EnterCriticalSection(&m_csDeviceLock);
        status = REQUEST_FAILED;

        if(uPipeIndex < m_bMaxNumPipes && m_ppCPipe[uPipeIndex] != NULL)
        {
            status = m_ppCPipe[uPipeIndex]->AbortTransfer(lpCancelAddress,
                                                            lpvNotifyParameter,
                                                            lpvCancelId);
        }

        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::AbortTransfer - \
              address = %d, pipe = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: IsPipeHalted
//
// Description: Determine if the pipe "uPipeIndex" on device at address "address"
//              is halted, and return result in lpfHalted
//
// Parameters: See description in CHcd::IsPipeHalted
//
// Returns: REQUEST_OK - iflpbHalted set to pipe's correct status
//
//         REQUEST_FAILED - if pipe exists, but unable to set lpfHalted
//
//         REQUEST_IGNORED - if pipe does not exist
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::IsPipeHalted(IN const UINT uAddress,
                                            IN const UINT uPipeIndex,
                                            OUT LPBOOL const lpfHalted)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::IsPipeHalted - \
              address = %d, pipe = %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(uAddress == m_bAddress)
    {
        EnterCriticalSection(&m_csDeviceLock);
        status = REQUEST_FAILED;

        if(m_fIsSuspend)
        {
            IS_FUNCTION_DEVICE_UNPLUGGED;
            RETAILMSG(1,
               (TEXT("!!!CFunction(tier %d)::IsPipeHalted \
                      in suspend state not allowed, address = %d, pipe = %d\n"),
                m_bTierNumber,
                uAddress,
                uPipeIndex));
            ASSERT(FALSE);
        }
        else if(uPipeIndex < m_bMaxNumPipes && m_ppCPipe[uPipeIndex] != NULL)
        {
            status = m_ppCPipe[uPipeIndex]->IsPipeHalted(lpfHalted);
        }

        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::IsPipeHalted - \
              address = %d, pipe = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ResetPipe
//
// Description: Reset the pipe "uPipeIndex" if this device's address is "address"
//
// Parameters: See description in CHcd::ResetPipe
//
// Returns: REQUEST_OK - if pipe reset or doesn't exist
//
//         REQUEST_FAILED - if pipe exists, but unable to reset it
//
//         REQUEST_IGNORED - if this device's address is not "address"
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CFunction::ResetPipe(IN const UINT uAddress,
                                         IN const UINT uPipeIndex)
{
    // Some [broken] devices panic if you try to clear the HALT feature on EP0.
    // Since it's an unnecessary operation in any case, we'll translate it to a NOP.
    if(uPipeIndex == 0)
    {
        // but do not pretend success if device is suspended or even unplugged!
        if(m_fIsSuspend)
        {
            IS_FUNCTION_DEVICE_UNPLUGGED;
            return REQUEST_FAILED;
        }

        return REQUEST_OK;
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction(tier %d)::ResetPipe - address = %d, pipe = %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex));

    HCD_REQUEST_STATUS status = REQUEST_IGNORED;

    // don't have to enter critical section until after checking
    // m_bAddress(since m_bAddress is const). That ensures we
    // won't wait for the critical section unless we actually
    // have something to do
    if(uAddress == m_bAddress)
    {
        status = REQUEST_FAILED;
        EnterCriticalSection(&m_csDeviceLock);

        if(m_fIsSuspend)
        {
            IS_FUNCTION_DEVICE_UNPLUGGED;
            RETAILMSG(1,
               (TEXT("!!!CFunction(tier %d)::ResetPipe \
                      in suspend state not allowed, address = %d, pipe = %d\n"),
                m_bTierNumber,
                uAddress,
                uPipeIndex));
            ASSERT(FALSE);
        }
        else if(uPipeIndex < m_bMaxNumPipes && m_ppCPipe[uPipeIndex] != NULL)
        {
            m_ppCPipe[uPipeIndex]->ClearHaltedFlag();

            if(SetOrClearFeature(USB_ENDPOINT_RECIPIENT,
                                  m_ppCPipe[uPipeIndex]->m_bEndpointAddress,
                                  USB_REQUEST_CLEAR_FEATURE,
                                  USB_FEATURE_ENDPOINT_HALT))
            {
                status = REQUEST_OK;
            }
        }

        LeaveCriticalSection(&m_csDeviceLock);
    }

    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: -CFunction(tier %d)::ResetPipe - \
              address = %d, pipe = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        m_bTierNumber,
        uAddress,
        uPipeIndex,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: HandleDetach
//
// Description: This function is called when the device is to be detached.
//              Always from a controlling class "above" -- never internally!
//              For CFunction devices, we need to notify USBD. This procedure
//              needs to try and exit as fast as possible, so we try to create
//              a worker thread to do all of the processing in a separate
//              static thread. This allows the hub which called HandleDetach
//              to delete this CFunction class without having to wait for
//              detach to fully complete.
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CFunction::HandleDetach(VOID)
{
    DEBUGMSG(ZONE_ATTACH || ZONE_FUNCTION,
       (TEXT("%s: +CFunction(tier %d)::HandleDetach\n"),
        GetControllerName(),
        m_bTierNumber));

    LPVOID lpvDetachId = NULL;

    EnterCriticalSection(&m_csDeviceLock);
    // mark state as detach-in-progress
    m_dwDevState |= FXN_DEV_STATE_DETACHMENT;

    // Detach happened before Operational
    if(m_dwDevState!=FXN_DEV_STATE_DETACHMENT)
    {
        LeaveCriticalSection(&m_csDeviceLock);

        // we have to wait until Client gives up on ATTACH - ifClient is doing retries
        // EnterOperationalState() clears progress bits before exit,
        // may set FXN_DEV_STATE_ENABFAILED
        while((m_dwDevState&FXN_DEV_STATE_PROGR_BITS) != 0)
        {
            Sleep(0x100);
        }

        EnterCriticalSection(&m_csDeviceLock);
    }
    {
        // prevent Clients from using pipes in this CFunction
        m_fIsSuspend = TRUE;

        if(m_dwDevState==FXN_DEV_STATE_DETACHMENT)
        {
            // clear cached shortcuts to CPipe classes -- OpenedPipesCache[]
            ::InvalidateOpenedPipesCacheEntries(m_bAddress, m_pCHcd);

            PREFAST_DEBUGCHK(m_ppCPipe != NULL);
            DEBUGCHK(m_bMaxNumPipes > 0 && m_ppCPipe[ENDPOINT0_CONTROL_PIPE] != NULL);

            for(UCHAR bPipe = 0; bPipe < m_bMaxNumPipes; bPipe++)
            {
                if(m_ppCPipe[bPipe] != NULL)
                {
                    m_ppCPipe[bPipe]->ClosePipe();
                    delete m_ppCPipe[bPipe];
                    m_ppCPipe[bPipe] = NULL;
                }
            }
            // m_ppCPipe[] freed in ~CDevice

            DEBUGCHK(m_lpvDetachId != NULL);
            lpvDetachId = m_lpvDetachId;
            m_lpvDetachId = NULL;
        }
        else
        {
            // ifAttach process is incomplete, then all these member vars
            // should not be populated
            DEBUGCHK(m_bMaxNumPipes == 0 && m_ppCPipe == NULL && m_lpvDetachId == NULL);
            DEBUGMSG(ZONE_WARNING || ZONE_ATTACH || ZONE_FUNCTION,
               (TEXT("%s: CFunction(tier %d)::HandleDetach invoked whileAttaching, \
                      DevState=%x!\r\n\r\n"),
                GetControllerName(),
                m_bTierNumber,
                m_dwDevState));
        }

        DEBUGCHK(m_pCHcd->GetpUSBDDetachProc() != NULL);

        if(m_hFunctionFeatureEvent)
        {
            SetEvent(m_hFunctionFeatureEvent);
            CloseHandle(m_hFunctionFeatureEvent);
        }

        m_hFunctionFeatureEvent = NULL;
        // mark state as detach-ready-to-invoke-Client
        m_dwDevState |= FXN_DEV_STATE_DETACHUSBD;
    }

    LeaveCriticalSection(&m_csDeviceLock);

    if(lpvDetachId != NULL)
    {
        DEBUGMSG(ZONE_ATTACH || ZONE_FUNCTION,
           (TEXT("%s: CFunction(tier %d)::HandleDetach calling USBD...\r\n\r\n"),
            GetControllerName(),
            m_bTierNumber));

        // Inform USBD of device detach, so the message can be passed
        // along to the client driver.
        ASSERT((m_dwDevState&FXN_DEV_STATE_PROGR_BITS) == 0);
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
        __try
        {
           (*m_pCHcd->GetpUSBDDetachProc())(lpvDetachId);
        } __except(::XhcdProcessException (GetExceptionCode()))
        {
            return;
        }
#pragma prefast(pop)
    }

    DEBUGMSG(ZONE_ATTACH || ZONE_FUNCTION,
       (TEXT("%s: -CFunction(tier %d)::HandleDetach\n"),
        GetControllerName(),
        m_bTierNumber));
}

//----------------------------------------------------------------------------------
// Function: SetOrClearFeature
//
// Description: This function will set or clear a feature for the giver recipient
//         (device, endpoint, or interface).
//
// Parameters: bRecipient - 0 for device, 1 for interface, 2 for endpoint
//
//             wIndex - recipient number(0 for device)
//
//             bSetOrClearFeature - this is USB_REQUEST_SET_FEATURE or
//                                 USB_REQUEST_CLEAR_FEATURE
//
//             wFeature - one of the features to set/clear
//
// Returns: TRUE if the request succeeded, else FALSE
//----------------------------------------------------------------------------------
BOOL CFunction::SetOrClearFeature(IN const UCHAR bRecipient,
                                   IN const WORD  wIndex,
                                   IN const UCHAR bSetOrClearFeature,
                                   IN const USHORT wFeature)
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE,
       (TEXT("%s: +CFunction::SetOrClearFeature - \
              recipient = %d, wIndex = %d, set/clear = 0x%x, feature = 0x%x\n"),
        GetControllerName(),
        bRecipient,
        wIndex,
        bSetOrClearFeature,
        wFeature));

#ifdef DEBUG
    {
    switch(bRecipient)
    {
    case USB_DEVICE_RECIPIENT:
        // USB spec 1.1, 9.4 - there is only one
        // features which should be set or cleared for ports
        DEBUGCHK(wIndex == 0 && wFeature == USB_DEVICE_REMOTE_WAKEUP);
        break;

    case USB_ENDPOINT_RECIPIENT:
        // USB spec 1.1, 9.4 - there is only one
        // features which should be set or cleared for endpoints
        DEBUGCHK(wFeature == USB_FEATURE_ENDPOINT_HALT);
        break;

    case USB_INTERFACE_RECIPIENT:
        // USB spec 1.1, 9.4 - there are not features available at this time
        DEBUGCHK(0);
        break;

    default:
        DEBUGCHK(0);
    }
        DEBUGCHK(m_ppCPipe != NULL && m_ppCPipe[ENDPOINT0_CONTROL_PIPE] != NULL);
    }
#endif // DEBUG

    BOOL                fTransferDone = FALSE;
    DWORD               dwBytesTransferred = 0;
    DWORD               dwErrorFlags = USB_NOT_COMPLETE_ERROR;
    HCD_REQUEST_STATUS  status = REQUEST_FAILED;
    USB_DEVICE_REQUEST  usbRequest;

    usbRequest.bmRequestType = bRecipient;
    usbRequest.bRequest = bSetOrClearFeature;
    usbRequest.wValue = wFeature;
    usbRequest.wIndex = wIndex;
    usbRequest.wLength = 0;
    PREFAST_DEBUGCHK(m_ppCPipe!=NULL);
    EnterCriticalSection(&m_csDeviceLock);

    if(m_fIsSuspend)
    {
        IS_FUNCTION_DEVICE_UNPLUGGED;
        RETAILMSG(1,
           (TEXT("!!!CFunction(tier %d)::SetOrClearFeature \
                  in suspend state not allowed, address = %d\n"),
            m_bTierNumber,
            m_bAddress));
        ASSERT(FALSE);
    }
    else
    {
        status = m_ppCPipe[ENDPOINT0_CONTROL_PIPE]->IssueTransfer(
                         m_bAddress, // address of this function
                         TransferDoneCallbackSetEvent, // callback func
                         m_hFunctionFeatureEvent, // callback param
                         USB_OUT_TRANSFER | USB_SEND_TO_DEVICE, // transfer flags
                         &usbRequest, // control request
                         0, // dwStartingFrame(not used)
                         0, // dwFrames(not used)
                         NULL, // aLengths(not used)
                         0, // buffer size
                         NULL, // buffer
                         0, // phys addr of buffer(not used)
                         this, // cancel id
                         NULL, // lpdwIsochErrors(not used)
                         NULL, // lpdwIsochLengths(not used)
                         &fTransferDone, // OUT param for transfer
                         &dwBytesTransferred, // OUT param for transfer
                         &dwErrorFlags); // OUT param for transfer

        if(status == REQUEST_OK)
        {
            LeaveCriticalSection(&m_csDeviceLock);

            DWORD dwReturn = WaitForSingleObject(m_hFunctionFeatureEvent,
                                                STANDARD_REQUEST_TIMEOUT);

            EnterCriticalSection(&m_csDeviceLock);

            if(!fTransferDone || dwReturn!= WAIT_OBJECT_0)
            {
                VERIFY(REQUEST_OK ==
                    m_ppCPipe[ENDPOINT0_CONTROL_PIPE]->AbortTransfer(NULL,
                                                                      NULL,
                                                                      this));

                if(m_hFunctionFeatureEvent)
                {
                    ResetEvent(m_hFunctionFeatureEvent);
                }
            }
        }
    }

    LeaveCriticalSection(&m_csDeviceLock);

    BOOL fSuccess =(status == REQUEST_OK &&
                     fTransferDone &&
                     dwBytesTransferred == 0 &&
                     dwErrorFlags == USB_NO_ERROR);

    DEBUGMSG(ZONE_ERROR && !fSuccess,
       (TEXT("%s: CFunction::SetOrClearFeature - \
              recipient = %d, wIndex = %d, set/clear = 0x%x, \
              feature = 0x%x, FAILED\n"),
        GetControllerName(),
        bRecipient,
        wIndex,
        bSetOrClearFeature,
        wFeature));
    DEBUGMSG(ZONE_HUB && ZONE_VERBOSE,
       (TEXT("%s: -CFunction::SetOrClearFeature - \
              recipient = %d, wIndex = %d, set/clear = 0x%x, \
              feature = 0x%x, returing BOOL %d\n"),
        GetControllerName(),
        bRecipient,
        wIndex,
        bSetOrClearFeature,
        wFeature,
        fSuccess));

    return fSuccess;
}
