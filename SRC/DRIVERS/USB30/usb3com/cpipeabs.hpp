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
// File: pipeabs.hpp CPipeAbs implements the abstract pipe interface.
//----------------------------------------------------------------------------------

#ifndef PIPEABS_HPP
#define PIPEABS_HPP

#define TD_ENDPOINT_MASK DWORD(0xF)

class CHcd;

class CPipeAbs;

CPipeAbs * CreateBulkPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                          IN const UCHAR bSpeed,
                          IN const UCHAR bDeviceAddress,
                          IN const UCHAR bHubAddress,
                          IN const UCHAR bHubPort,
                          const PVOID pTTContext,
                          IN CHcd * const pChcd);

CPipeAbs * CreateControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                             IN const UCHAR bSpeed,
                             IN const UCHAR bDeviceAddress,
                             IN const UCHAR bHubAddress,
                             IN const UCHAR bHubPort,
                             const PVOID pTTContext,
                             IN CHcd * const pChcd);

CPipeAbs * CreateInterruptPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                               IN const UCHAR bSpeed,
                               IN const UCHAR bDeviceAddress,
                               IN const UCHAR bHubAddress,
                               IN const UCHAR bHubPort,
                               const PVOID pTTContext,
                               IN CHcd * const pChcd);

CPipeAbs * CreateIsochronousPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                 IN const UCHAR bSpeed,
                                 IN const UCHAR bDeviceAddress,
                                 IN const UCHAR bHubAddress,
                                 IN const UCHAR bHubPort,
                                 const PVOID pTTContext,
                                 IN CHcd * const pChcd);

//
// Abstract Pipe class.
//
class CPipeAbs
{
public:

    CPipeAbs(UCHAR const bEndpointAddress):m_bEndpointAddress(bEndpointAddress),
                                           m_pLogicalDevice(NULL)
    {
        ;
    };

    virtual ~CPipeAbs()
    {
        ;
    };

    virtual HCD_REQUEST_STATUS  OpenPipe(VOID) = 0;

    virtual HCD_REQUEST_STATUS  ClosePipe(VOID) = 0;

    virtual HCD_REQUEST_STATUS  IssueTransfer(IN const UINT uAddress,
                                                IN LPTRANSFER_NOTIFY_ROUTINE const lpfnCallback,
                                                IN LPVOID const lpvCallbackParameter,
                                                IN const DWORD dwFlags,
                                                IN LPCVOID const lpvControlHeader,
                                                IN const DWORD dwStartingFrame,
                                                IN const DWORD dwFrames,
                                                IN LPCDWORD const aLengths,
                                                IN const DWORD dwBufferSize,     
                                                IN OUT LPVOID const lpvBuffer,
                                                IN const ULONG uPaBuffer,
                                                IN LPCVOID const lpvCancelId,
                                                OUT LPDWORD const lpdwIsochErrors,
                                                OUT LPDWORD const lpdwIsochLengths,
                                                OUT LPBOOL const lpfComplete,
                                                OUT LPDWORD const lpdwBytesTransferred,
                                                OUT LPDWORD const lpdwError) = 0;

    virtual HCD_REQUEST_STATUS AbortTransfer(IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                                IN const LPVOID lpvNotifyParameter,
                                                IN LPCVOID lpvCancelId) = 0;

    virtual HCD_REQUEST_STATUS IsPipeHalted(OUT LPBOOL const lpfHalted) = 0;

    virtual VOID ClearHaltedFlag(VOID) = 0;

    // only available for Control endpoint
    virtual VOID ChangeMaxPacketSize(IN const USHORT)
    {
        ASSERT(FALSE);
    }

    UCHAR const m_bEndpointAddress;
    PVOID m_pLogicalDevice;
    virtual VOID SetDevicePointer(PVOID pLogicalDevice) = 0;

private:
    CPipeAbs& operator=(CPipeAbs&);
};

#endif /* PIPEABS_HPP */