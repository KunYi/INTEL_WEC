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
// File: cpipe.h - This file implements the Pipe class for managing open pipes for XHCI
//----------------------------------------------------------------------------------

#ifndef CPIPE_H
#define CPIPE_H

#include <celog.h>
#include "globals.hpp"
#include "cpipeabs.hpp"
#include "cdevice.hpp"

typedef enum
{
    TYPE_UNKNOWN = 0,
    TYPE_CONTROL,
    TYPE_BULK,
    TYPE_INTERRUPT,
    TYPE_ISOCHRONOUS
} PIPE_TYPE;

//typedef struct STRANSFER STransfer;
typedef struct _STRANSFER STransfer;

class CPhysMem;
class CXhcd;
class CTransfer ;

class CPipe : public CPipeAbs
{
public:

    CPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
            IN const UCHAR bSpeed,
            IN const UCHAR bDeviceAddress,
            IN const UCHAR bHubAddress,
            IN const UCHAR bHubPort,
            IN const PVOID pTTContext,
            IN CXhcd * const pCXhcd);

    virtual ~CPipe();

    virtual PIPE_TYPE GetType(VOID)
    {
        return TYPE_UNKNOWN;
    };

    virtual HCD_REQUEST_STATUS OpenPipe(VOID) = 0;
    virtual HCD_REQUEST_STATUS ClosePipe(VOID) = 0;

    virtual HCD_REQUEST_STATUS AbortTransfer( IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                                IN const LPVOID lpvNotifyParameter,
                                                IN LPCVOID lpvCancelId) = 0;
    virtual BOOL DoTransfer(TRANSFER_DATA* pTd,
                          UINT uTransferLength,
                          LPCVOID lpvControlHeader,
                          UINT64 pTransferBuffer)=0;

    HCD_REQUEST_STATUS IsPipeHalted(OUT LPBOOL const lpfHalted);
    virtual BOOL CheckForDoneTransfers(VOID) = 0;
#ifdef DEBUG
    virtual const TCHAR* GetPipeType(VOID) const = 0;
#endif // DEBUG

    virtual VOID ClearHaltedFlag(VOID);
    USB_ENDPOINT_DESCRIPTOR GetEndptDescriptor()
    {
        return m_usbEndpointDescriptor;
    };
    UCHAR GetDeviceAddress()
    {
        return m_bDeviceAddress;
    };
    VOID SetDeviceAddress(UCHAR bAddress)
    {
        m_bDeviceAddress = bAddress;
    };
    VOID SetDevicePointer(PVOID pLogicalDevice)
    {
        m_pLogicalDevice = pLogicalDevice;
    };
    LOGICAL_DEVICE* GetDevicePointer(VOID)
    {
        return (LOGICAL_DEVICE*) m_pLogicalDevice;
    };
    LPCTSTR GetControllerName(VOID) const;
    INT PrepareTransfer(TRANSFER_DATA** ppTd, UINT uTransferLength);
    static DWORD XhciProcessException (DWORD dwCode);

    CXhcd * const   m_pCXhcd;

    // indicates speed of this pipe
    UINT            m_uiSpeed;

private:

    CPipe&operator=(CPipe&);

protected:

    virtual HCD_REQUEST_STATUS ScheduleTransfer() = 0;
    virtual BOOL AreTransferParametersValid(const STransfer *pTransfer = NULL) const = 0;

    //
    // This critical section must be taken by all 'public' and 'protected' 
    // methods within all children classes of this "CPipe" class.
    // It is not necessary only for 'private' fuctions in pipe classes.
    //
    CRITICAL_SECTION        m_csPipeLock;
    // descriptor for this pipe's endpoint
    USB_ENDPOINT_DESCRIPTOR m_usbEndpointDescriptor;
    // Device Address that assigned by HCD.
    UCHAR                   m_bDeviceAddress;
    // indicates pipe is halted
    BOOL                    m_fIsHalted;
};

class CQueuedPipe : public CPipe
{
friend class CQTransfer;

public:

    CQueuedPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                IN const UCHAR bSpeed,
                IN const UCHAR bDeviceAddress,
                IN const UCHAR bHubAddress,
                IN const UCHAR bHubPort,
                IN const PVOID pTTContext,
                IN CXhcd * const pCXhcd);

    virtual ~CQueuedPipe();

    HCD_REQUEST_STATUS OpenPipe(VOID);
    HCD_REQUEST_STATUS ClosePipe(VOID);

    HCD_REQUEST_STATUS IssueTransfer( IN const UINT uAddress,
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
                                        OUT LPDWORD const lpdwError);

    BOOL GetCurrentEventSeg(IN RING_SEGMENT* pDeqSeg,
                            IN TRB* pDequeue, 
                            OUT TRANSFER_DATA** ppTd,
                            IN UINT64 u64EventDma);

    HCD_REQUEST_STATUS AbortTransfer( IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                        IN const LPVOID lpvNotifyParameter,
                                        IN LPCVOID lpvCancelId);

    BOOL CheckForDoneTransfers(VOID);
    BOOL ResetEndPoint(VOID);

private:

    CQueuedPipe&operator=(CQueuedPipe&);

    HCD_REQUEST_STATUS ScheduleTransfer();

    virtual BOOL InsertToQueue(CQTransfer* pTransfer);

    // allow flipping "Halted" bit for CONTROL pipes only
    BOOL m_fSetHaltedAllowed;

protected:
    volatile UINT   m_uBusyIndex;
    volatile UINT   m_uBusyCount;
    CQTransfer*     m_pUnQueuedTransfer;
    CQTransfer*     m_pQueuedTransfer;
};

class CBulkPipe : public CQueuedPipe
{
public:

    CBulkPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                IN const UCHAR bSpeed,
                IN const UCHAR bDeviceAddress,
                IN const UCHAR bHubAddress,
                IN const UCHAR bHubPort,
                IN const PVOID pTTContext,
                IN CXhcd * const pCXhcd);

    ~CBulkPipe();

    virtual PIPE_TYPE GetType()
    {
        return TYPE_BULK;
    };

#ifdef DEBUG
    const TCHAR* GetPipeType(VOID) const
    {
        return TEXT("Bulk");
    }
#endif // DEBUG

private:

    CBulkPipe&operator=(CBulkPipe&);
    BOOL AreTransferParametersValid(const STransfer *pTransfer = NULL) const;
    BOOL DoTransfer(TRANSFER_DATA* pTd,
                  UINT uTransferLength, 
                  LPCVOID lpvControlHeader,
                  UINT64 pTransferBuffer);
};

class CControlPipe : public CQueuedPipe
{
public:

    CControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    IN const UCHAR bSpeed,
                    IN const UCHAR bDeviceAddress,
                    IN const UCHAR bHubAddress,
                    IN const UCHAR bHubPort,
                    IN const PVOID pTTContext,
                    IN CXhcd * const pCXhcd);

    ~CControlPipe();

    virtual PIPE_TYPE GetType()
    {
        return TYPE_CONTROL;
    };

    // we believe that CQueuedPipe() can handle either one of BULK & CONTROL types
    // HCD_REQUEST_STATUS  OpenPipe(VOID);
    // HCD_REQUEST_STATUS  ClosePipe(VOID);

    VOID ChangeMaxPacketSize(IN const USHORT wMaxPacketSize);

    HCD_REQUEST_STATUS IssueTransfer( IN const UINT uAddress,
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
                                        OUT LPDWORD const lpdwError);

#ifdef DEBUG
    const TCHAR* GetPipeType(VOID) const
    {
        return TEXT("Control");
    }
#endif // DEBUG

private:

    CControlPipe&operator=(CControlPipe&);
    BOOL AreTransferParametersValid(const STransfer *pTransfer = NULL) const;
    BOOL DoTransfer(TRANSFER_DATA* pTd,
                  UINT uTransferLength, 
                  LPCVOID lpvControlHeader,
                  UINT64 pTransferBuffer);
};

class CInterruptPipe : public CQueuedPipe
{
public:

    CInterruptPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    IN const UCHAR bSpeed,
                    IN const UCHAR bDeviceAddress,
                    IN const UCHAR bHubAddress,
                    IN const UCHAR bHubPort,
                    IN const PVOID pTTContext,
                    IN CXhcd * const pCXhcd);

    ~CInterruptPipe();

    virtual PIPE_TYPE GetType()
    {
        return TYPE_INTERRUPT;
    };

#ifdef DEBUG
    const TCHAR*  GetPipeType(VOID) const
    {
        return TEXT("Interrupt");
    }
#endif // DEBUG

private:

    CInterruptPipe&operator=(CInterruptPipe&);
    BOOL AreTransferParametersValid(const STransfer *pTransfer = NULL) const;
    BOOL DoTransfer(TRANSFER_DATA* pTd,
                  UINT uTransferLength, 
                  LPCVOID lpvControlHeader,
                  UINT64 pTransferBuffer);
};

class CIsochronousPipe : public CPipe
{
public:

    CIsochronousPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                        IN const UCHAR bSpeed,
                        IN const UCHAR bDeviceAddress,
                        IN const UCHAR bHubAddress,
                        IN const UCHAR bHubPort,
                        IN const PVOID pTTContext,
                        IN CXhcd *const pCXhcd);

    ~CIsochronousPipe();

    virtual PIPE_TYPE GetType()
    {
        return TYPE_ISOCHRONOUS;
    };

    HCD_REQUEST_STATUS OpenPipe(VOID);
    HCD_REQUEST_STATUS ClosePipe(VOID);

    virtual HCD_REQUEST_STATUS IssueTransfer( IN const UINT uAddress,
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
                                                OUT LPDWORD const lpdwError);

    HCD_REQUEST_STATUS  AbortTransfer( IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                        IN const LPVOID lpvNotifyParameter,
                                        IN LPCVOID lpvCancelId);

    BOOL CheckForDoneTransfers(VOID);

#ifdef DEBUG
    const TCHAR* GetPipeType(VOID) const
    {
        return TEXT("Isochronous");
    }
#endif // DEBUG

private:
    HCD_REQUEST_STATUS  AddTransfer(STransfer *pTransfer);
    HCD_REQUEST_STATUS  ScheduleTransfer();

    DWORD           m_dwLastValidFrame;  
    DWORD           m_dwNumOfTD;

protected:

    BOOL AreTransferParametersValid(const STransfer *pTransfer = NULL) const;
    BOOL DoTransfer(TRANSFER_DATA* pTd,
                  UINT uTransferLength, 
                  LPCVOID lpvControlHeader,
                  UINT64 pTransferBuffer);

};

#endif /* CPIPE_H */