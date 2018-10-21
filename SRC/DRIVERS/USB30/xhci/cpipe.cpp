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
// File: pipe.cpp
//
// Description:
// 
//     This file implements the Pipe class for managing open pipes for XHCI
//                             CPipe 
//                           /       \
//                   CQueuedPipe       CIsochronousPipe
//                /      |       \ 
//              /        |         \
//    CControlPipe   CInterruptPipe  CBulkPipe
//----------------------------------------------------------------------------------

#include <windows.h>

#include "globals.hpp"
#include "hal.h"
#include "datastructures.h"
#include "ctransfer.h"
#include "cpipe.h"
#include "chw.h"
#include "cring.h"
#include "cxhcd.h"

#ifndef _PREFAST_
// Disable pragma warnings
#pragma warning(disable: 4068)
#endif

#define CHECK_CS_TAKEN ASSERT(m_csPipeLock.LockCount > 0 && \
    m_csPipeLock.OwnerThread == (HANDLE)GetCurrentThreadId())

//----------------------------------------------------------------------------------
// Function: CPipe
//
// Description: constructor for CPipe
//              Most of the work associated with setting up the pipe
//              should be done via OpenPipe. The constructor actually
//              does very minimal work.
//              Do not modify static variables here!!!!!!!!!!!
//
// Parameters: lpEndpointDescriptor - pointer to endpoint descriptor for
//                                    this pipe(assumed non-NULL)
//
//             bSpeed - indicates the pipe speed
//
//             bDeviceAddress - USB address of the device assosiated with this pipe. 
//
//             bHubAddress - address of the hub
//
//             bHubPort    - hub port number 
//
//             pTTContext  - Transaction Translate context.
//
//             pCXhcd      - pointer to Xhcd object
//
// Returns: None
//----------------------------------------------------------------------------------
CPipe::CPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
            IN const UCHAR bSpeed,
            IN const UCHAR bDeviceAddress,
            IN const UCHAR bHubAddress,
            IN const UCHAR bHubPort,
            IN const PVOID pTTContext,
            IN CXhcd *const pCXhcd) : CPipeAbs(lpEndpointDescriptor->bEndpointAddress),
                                        m_usbEndpointDescriptor(*lpEndpointDescriptor),
                                        m_bDeviceAddress(bDeviceAddress),
                                        m_pCXhcd(pCXhcd),
                                        m_uiSpeed(bSpeed),
                                        m_fIsHalted(FALSE)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CPipe::CPipe\n"),
        GetControllerName()));
    // CPipe::Initialize should already have been called by now
    // to set up the schedule and init static variables

    InitializeCriticalSection(&m_csPipeLock);

    m_fIsHalted = FALSE;

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CPipe::CPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CPipe
//
// Description: Destructor for CPipe
//              Most of the work associated with destroying the Pipe
//              should be done via ClosePipe
//              Do not delete static variables here!!!!!!!!!!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CPipe::~CPipe()
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CPipe::~CPipe\n"),
        GetControllerName()));
    // transfers should be aborted or closed before deleting object
    DeleteCriticalSection(&m_csPipeLock);

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,(TEXT("-CPipe::~CPipe\n")));
}

//----------------------------------------------------------------------------------
// Function: GetControllerName
//
// Description: Return the name of the HCD controller type
//
// Parameters: None
//
// Returns: Const null-terminated string containing the HCD controller name
//----------------------------------------------------------------------------------
LPCTSTR CPipe::GetControllerName(VOID) const
{
    if(m_pCXhcd)
    {
        return m_pCXhcd->GetControllerName();
    }
    return NULL;
}

//----------------------------------------------------------------------------------
// Function: IsPipeHalted
//
// Description: Return whether or not this pipe is halted(stalled)
//              Caller should check for lpfHalted to be non-NULL
//
// Parameters: lpfHalted - pointer to BOOL which receives
//                         TRUE if pipe halted, else FALSE
//
// Returns: REQUEST_OK 
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CPipe::IsPipeHalted(OUT LPBOOL const lpfHalted)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CPipe(%s)::IsPipeHalted\n"),
        GetControllerName(),
        GetPipeType()));

    // should be checked by CXhcd
    DEBUGCHK(lpfHalted);

    EnterCriticalSection(&m_csPipeLock);
    if(lpfHalted)
    {
        *lpfHalted = m_fIsHalted;
    }
    LeaveCriticalSection(&m_csPipeLock);

    return REQUEST_OK;
}

//----------------------------------------------------------------------------------
// Function: ClearHaltedFlag
//
// Description: Clears the pipe is halted flag
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CPipe::ClearHaltedFlag(VOID)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CPipe(%s)::ClearHaltedFlag\n"),
        GetControllerName(),
        GetPipeType()));

    EnterCriticalSection(&m_csPipeLock);
    m_fIsHalted = FALSE;
    LeaveCriticalSection(&m_csPipeLock);
}

//----------------------------------------------------------------------------------
// Function: PrepareTransfer
//
// Description: This method prepares one transfer for activation
//
// Parameters: ppTd - pointer to transfer data
//
//             uTransferLength - transfer buffer length
//
// Returns: XHCD_OK if success, else error code
//----------------------------------------------------------------------------------
INT CPipe::PrepareTransfer(TRANSFER_DATA** ppTd, UINT uTransferLength)
{
    return m_pCXhcd->PrepareTransfer(m_bDeviceAddress,
                                    m_usbEndpointDescriptor.bEndpointAddress,
                                    ppTd,
                                    uTransferLength,
                                    GetType());
}

//----------------------------------------------------------------------------------
// Function: XhciProcessException
//
// Description: checks for a particular exception before handling it
//
// Parameters: dwCode - status code
//
// Returns: exception code
//----------------------------------------------------------------------------------
DWORD CPipe::XhciProcessException (DWORD dwCode)
{
    if (dwCode == STATUS_ACCESS_VIOLATION) {
            return EXCEPTION_EXECUTE_HANDLER;
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

//----------------------------------------------------------------------------------
// Function: CQueuedPipe
//
// Description: Constructor for CQueuedPipe
//              Do not modify static variables here!!
//
// Parameters: See CPipe::CPipe
//
// Returns: None
//----------------------------------------------------------------------------------
CQueuedPipe::CQueuedPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                        IN const UCHAR bSpeed,
                        IN const UCHAR bDeviceAddress,
                        IN const UCHAR bHubAddress,
                        IN const UCHAR bHubPort,
                        IN const PVOID pTTContext,
                        IN CXhcd *const pCXhcd) : CPipe(lpEndpointDescriptor,
                                                        bSpeed,
                                                        bDeviceAddress,
                                                        bHubAddress,
                                                        bHubPort,
                                                        pTTContext,
                                                        pCXhcd)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CQueuedPipe::CQueuedPipe\n"),
        GetControllerName()));
    m_pUnQueuedTransfer = NULL;
    m_pQueuedTransfer = NULL;
    m_uBusyIndex =  0;
    m_uBusyCount =  0;
    m_fSetHaltedAllowed = FALSE;
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CQueuedPipe::CQueuedPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CQueuedPipe
//
// Description: Destructor for CQueuedPipe
//              Do not modify static variables here!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CQueuedPipe::~CQueuedPipe()
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CQueuedPipe::~CQueuedPipe\n"),
        GetControllerName()));
    // queue should be freed via ClosePipe before calling destructor
    ASSERT(m_pUnQueuedTransfer==NULL);
    ASSERT(m_pQueuedTransfer==NULL);
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CQueuedPipe::~CQueuedPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: GetCurrentEventSeg
//
// Description: Check if event dma address is correct and return 
//              corresponding  transfer data
//
// Parameters: pDeqSeg - endpoint transfer ring segment
//
//             pDequeue - dequeue address of event ring
//
//             ppTd - transfer data to be returned
//
//             u64EventDma - dma address of checked event
//
// Returns: TRUE if address found in segment else FALSE
//----------------------------------------------------------------------------------
BOOL CQueuedPipe::GetCurrentEventSeg(RING_SEGMENT* pDeqSeg,
                                     TRB* pDequeue,
                                     TRANSFER_DATA** ppTd,
                                     UINT64 u64EventDma)
{
    BOOL fRet = FALSE;
    CQTransfer* pCurTransfer;
    TRANSFER_DATA* pTd;
    
    EnterCriticalSection(&m_csPipeLock);

    pCurTransfer = m_pQueuedTransfer;

    if(pCurTransfer)
    {
        while(pCurTransfer !=NULL)
        {
            if((pCurTransfer->GetStatus() == STATUS_CQT_ACTIVATED) ||
               (pCurTransfer->GetStatus() == STATUS_CQT_SHORT))
            {
                pTd = pCurTransfer->m_pTd;
                if (m_pCXhcd->IsTrbInSement(pDeqSeg,
                                            pDequeue,
                                            pTd->pLastTrb,
                                            u64EventDma))
                {
                    *ppTd = pTd;
                    fRet = TRUE;
                    break;
                } else {
                    fRet = FALSE;
                }
            }
            pCurTransfer =(CQTransfer*)pCurTransfer->GetNextTransfer();
        }
        ASSERT(fRet);
    } 
    LeaveCriticalSection(&m_csPipeLock);

    return fRet;
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer
//
// Description: Abort any transfer on this pipe if its cancel ID matches
//              that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: REQUEST_OK if transfer aborted
//          REQUEST_FAILED if lpvCancelId doesn't match currently executing
//          transfer, or if there is no transfer in progress
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CQueuedPipe::AbortTransfer( IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                            IN const LPVOID lpvNotifyParameter,
                                            IN LPCVOID lpvCancelId)
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x\n"),
        GetControllerName(),
        GetPipeType(),
        lpvCancelId));

    HCD_REQUEST_STATUS status = REQUEST_FAILED;

    CQTransfer* pPrevTransfer = NULL;
    CQTransfer* pCurTransfer  = m_pUnQueuedTransfer;
    while(pCurTransfer && pCurTransfer->GetSTransfer().lpvCancelId != lpvCancelId)
    {
        pPrevTransfer = pCurTransfer;
        pCurTransfer  =(CQTransfer*)pCurTransfer->GetNextTransfer();
    };

    // Found Transfer that is not activated yet.
    if(pCurTransfer)
    {
        pCurTransfer->AbortTransfer();
        if(pPrevTransfer)
        {
            pPrevTransfer->SetNextTransfer(pCurTransfer->GetNextTransfer());
        }
        else
        {
            m_pUnQueuedTransfer =(CQTransfer *)pCurTransfer->GetNextTransfer();
        }
    }
    else
    {
        if(m_pQueuedTransfer != NULL)
        {
            pPrevTransfer = NULL;
            pCurTransfer  = m_pQueuedTransfer;
            while(pCurTransfer &&
                pCurTransfer->GetSTransfer().lpvCancelId != lpvCancelId)
            {
                pPrevTransfer = pCurTransfer;
                pCurTransfer  =(CQTransfer*)pCurTransfer->GetNextTransfer();
            };

            // if we found it in the schedule
            if(pCurTransfer)
            {
                if(pCurTransfer->m_dwStatus == STATUS_CQT_ACTIVATED)
                {
                    //
                    // trash this transfer
                    //
                    // it includes 2 msec wait
                    pCurTransfer->AbortTransfer();

                     //
                    // do host-side management and housekeeping
                    //
                    CQTransfer* pNextChained =
                        (CQTransfer*)pCurTransfer->GetNextTransfer();
                    if(pPrevTransfer)
                    {
                        pPrevTransfer->SetNextTransfer(pNextChained);
                    }
                    else
                    {
                        // we trashed the head of active Q - just forward it
                        m_pQueuedTransfer = pNextChained;
                        m_uBusyIndex++; 
                        if(m_uBusyCount>0) m_uBusyCount--;
                    }
                } // if STATUS_CQT_ACTIVATED
                else if(pCurTransfer->m_dwStatus == STATUS_CQT_DONE ||
                    pCurTransfer->m_dwStatus == STATUS_CQT_RETIRED)
                {
                    // it is too late - this transfer has been processed already; do nothing
                    DEBUGMSG((ZONE_TRANSFER||ZONE_WARNING),
                        (TEXT("%s:  CQueuedPipe(%s)::AbortTransfer - \
                              lpvCancelId = 0x%x already completed\n"),
                              GetControllerName(),
                              GetPipeType(),
                              lpvCancelId));

                    pCurTransfer = NULL;
                }
            }
        }
    }

    if(pCurTransfer)
    {
        //
        // Any completion flags will be set, and completion callbacks invoked,
        // during DoneTransfer() execution
        //
        pCurTransfer->DoneTransfer();
        //
        // the last duty of this method is to invoke the "Cancel Done" callback
        //
        if(lpCancelAddress)
        {
            __try
            {
               (*lpCancelAddress)(lpvNotifyParameter);
            } 
            __except(XhciProcessException (GetExceptionCode()))
            {
                DEBUGMSG(ZONE_ERROR,
                    (TEXT("%s:  CQueuedPipe(%s)::AbortTransfer - \
                          exception executing cancellation callback function\n"),
                          GetControllerName(),
                          GetPipeType()));
            }
        }
        delete pCurTransfer;
        status = REQUEST_OK;
    }

    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: -CQueuedPipe(%s)::AbortTransfer - \
              lpvCancelId = 0x%x, returning HCD_REQUEST_STATUS %d\n"),
              GetControllerName(),
              GetPipeType(),
              lpvCancelId,
              status));

    if (!m_pCXhcd->CountAbortTransfers (m_bDeviceAddress,
                  m_usbEndpointDescriptor.bEndpointAddress))
    {
        status = REQUEST_FAILED;
    }

    return status;
}

//----------------------------------------------------------------------------------
// Function: InsertToQueue
//
// Description: insert transfer to queue
//
// Parameters: pTransfer - transfer to be queued
//
// Returns: always TRUE
//----------------------------------------------------------------------------------
BOOL CQueuedPipe::InsertToQueue(CQTransfer* pTransfer) 
{
    CQTransfer* pCurTransfer = NULL;
    CQTransfer* pLastActiveTransfer = NULL;

    EnterCriticalSection(&m_csPipeLock);
    pCurTransfer = m_pQueuedTransfer;
    while(pCurTransfer)
    { 
        pLastActiveTransfer = pCurTransfer; 
        pCurTransfer =(CQTransfer*)pLastActiveTransfer->GetNextTransfer();
    }
    if(pLastActiveTransfer)
    {
        pLastActiveTransfer->SetNextTransfer(pTransfer);
    }
    else
    {
        m_pQueuedTransfer = pTransfer;
    }
    pTransfer->SetNextTransfer(NULL);
    LeaveCriticalSection(&m_csPipeLock);
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CHcd::IssueTransfer
//
// Returns: REQUEST_OK if transfer issued ok, else REQUEST_FAILED
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS  CQueuedPipe::IssueTransfer(IN const UINT uAddress,
                                                IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                                IN LPVOID const lpvNotifyParameter,
                                                IN const DWORD dwFlags,
                                                IN LPCVOID const lpvControlHeader,
                                                IN const DWORD dwStartingFrame,
                                                IN const DWORD dwFrames,
                                                IN LPCDWORD const aLengths,
                                                IN const DWORD dwBufferSize,     
                                                IN OUT LPVOID const lpvClientBuffer,
                                                IN const ULONG uPaBuffer,
                                                IN LPCVOID const lpvCancelId,
                                                OUT LPDWORD const lpdwIsochErrors,
                                                OUT LPDWORD const lpdwIsochLengths,
                                                OUT LPBOOL const lpfComplete,
                                                OUT LPDWORD const lpdwBytesTransferred,
                                                OUT LPDWORD const lpdwError)
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CQueuedPipe(%s)::IssueTransfer, address = %d\n"),
        GetControllerName(),
        GetPipeType(),
        uAddress));

    HCD_REQUEST_STATUS  status = REQUEST_FAILED;

    // These are the IssueTransfer parameters
    STransfer sTransfer =
    {
         lpStartAddress,
         lpvNotifyParameter,
         dwFlags,
         lpvControlHeader,
         dwStartingFrame,
         dwFrames,
         aLengths,
         dwBufferSize,
         lpvClientBuffer,
         uPaBuffer,
         lpvCancelId,
         lpdwIsochErrors,
         lpdwIsochLengths,
         lpfComplete,
         lpdwBytesTransferred,
         lpdwError
    };


    if(AreTransferParametersValid(&sTransfer) && m_bDeviceAddress == uAddress)
    {
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
        // initializing transfer status parameters
         __try
         {
            *sTransfer.lpfComplete = FALSE;
            *sTransfer.lpdwBytesTransferred = 0;
            *sTransfer.lpdwError = USB_NOT_COMPLETE_ERROR;

            CQTransfer* pTransfer = new CQTransfer(this,
                                                    m_pCXhcd->GetPhysMem(),
                                                    sTransfer);

            if(pTransfer && !m_fIsHalted && pTransfer->Init())
            {
                pTransfer->SetStatus(STATUS_CQT_PREPARED);
                CQTransfer* pCurTransfer = m_pUnQueuedTransfer;
                if(pCurTransfer)
                {
                    while(pCurTransfer->GetNextTransfer() != NULL)
                    {
                        pCurTransfer =(CQTransfer*)pCurTransfer->GetNextTransfer();
                    }
                    pCurTransfer->SetNextTransfer(pTransfer);
                }
                else
                {
                    m_pUnQueuedTransfer = pTransfer;
                }
                status = REQUEST_OK;
            }
            else
            {
                // We return fails here so do not need callback;
                if(pTransfer)
                {
                    if(pTransfer->GetStatus() == STATUS_CQT_HALTED || m_fIsHalted)
                    {
                        if(GetType() != TYPE_CONTROL)
                        {
                            ResetEndPoint();
                            m_fIsHalted = TRUE;
                        }
                        else
                        {
                            m_pCXhcd->CleanupHaltedEndpoint(m_bDeviceAddress,
                                                                m_usbEndpointDescriptor.bEndpointAddress);
                            ClearHaltedFlag();
                        }
                    }
                    pTransfer->SetStatus(STATUS_CQT_CANCELED);
                    pTransfer->DoNotCallBack();
                    delete pTransfer;
                }
                SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            }
        } 
        __except(XhciProcessException (GetExceptionCode()))
        {  
            SetLastError(ERROR_INVALID_PARAMETER);
        }
#pragma prefast(pop)
        if(status == REQUEST_OK)
        {
            status = ScheduleTransfer();
        }
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        ASSERT(FALSE);
    }
    CheckForDoneTransfers();

    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: -CQueuedPipe(%s)::IssueTransfer - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
              GetControllerName(),
              GetPipeType(),
              uAddress,
              status));
    return status;
}


//----------------------------------------------------------------------------------
// Function: ScheduleTransfer
//
// Description: Schedule a Transfer on this pipe.
//
// Parameters: None
//
// Returns: REQUEST_OK if transfer issued ok, else REQUEST_FAILED
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CQueuedPipe::ScheduleTransfer()
{
    HCD_REQUEST_STATUS  schedStatus = REQUEST_FAILED;

    if(m_pUnQueuedTransfer != NULL)
    {
        DWORD dwRestart = 0;

        do
        {
            CQTransfer* pCurTransfer = NULL;

            pCurTransfer = m_pUnQueuedTransfer;
            m_pUnQueuedTransfer =(CQTransfer*)pCurTransfer->GetNextTransfer();

            InsertToQueue(pCurTransfer);
            if(pCurTransfer->Activate() == TRUE)
            {
                schedStatus = REQUEST_OK;
                m_uBusyCount++;
                dwRestart = -1;
            }
            else
            {
                ASSERT(FALSE);
                // this can occur only if we try to queue some non-initialized
                // transfer(internal error - shall never happen!)
                pCurTransfer->AbortTransfer();
                pCurTransfer->DoneTransfer();
                break;
            }
        } while(m_pUnQueuedTransfer != NULL);
    }
    return schedStatus;
}

//----------------------------------------------------------------------------------
// Function: CheckForDoneTransfers
//
// Description: Check which transfers on this pipe are finished, then
//              take the appropriate actions - i.e. remove the transfer
//              data structures and call any notify routines
//              ALWAYS CALLED FROM IST
//
// Parameters: None
//
// Returns: TRUE if transfer is done, else FALSE
//----------------------------------------------------------------------------------
BOOL CQueuedPipe::CheckForDoneTransfers(VOID)
{
    UINT uDoneTransfers = 0;
    CQTransfer* pCurTransfer = NULL;
    CQTransfer* pPrevTransfer = NULL;

    m_pCXhcd->EventLock();
    EnterCriticalSection(&m_csPipeLock);

    pCurTransfer = m_pQueuedTransfer;
    // look up for all finished transfers
    while(pCurTransfer!=NULL)
    {
        // check if pipe is halted due to some error
        if(pCurTransfer->IsHalted())
        {
            m_fIsHalted = TRUE;
        }
        
        // Check if the transfer is done or not.
        if(!pCurTransfer->IsTransferDone())
        {
            pPrevTransfer = pCurTransfer;
            pCurTransfer =(CQTransfer*)pCurTransfer->GetNextTransfer();
            continue;
        }

        // we have some comleted transfers - get ready to advance to next transfer
        uDoneTransfers++;

        if(pCurTransfer == m_pQueuedTransfer)
        {
            m_pQueuedTransfer =(CQTransfer*)pCurTransfer->GetNextTransfer();
            delete pCurTransfer;
            pCurTransfer = m_pQueuedTransfer;
        }
        else
        {
            pPrevTransfer->SetNextTransfer(pCurTransfer->GetNextTransfer());
            delete pCurTransfer;
            pCurTransfer =(CQTransfer*) pPrevTransfer->GetNextTransfer();
        }

        m_uBusyCount--;
    }

    LeaveCriticalSection(&m_csPipeLock);
    m_pCXhcd->EventUnlock();

    return(uDoneTransfers != 0);   
}

//----------------------------------------------------------------------------------
// Function: OpenPipe
//
// Description: Create the data structures necessary to conduct
//              transfers on this pipe - for BULK and CONTROL
//
// Parameters: None
//
// Returns: REQUEST_OK - if pipe opened
//
//          REQUEST_FAILED - if pipe was not opened
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CQueuedPipe::OpenPipe(VOID)
{
    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: +CQueuedPipe(%s)::OpenPipe()\n"),
        GetControllerName(),
        GetPipeType()));
    HCD_REQUEST_STATUS status = REQUEST_FAILED;

    m_pCXhcd->EventLock();

    if(m_pCXhcd->AddToBusyPipeList(this, FALSE))
    {
          status = REQUEST_OK;
    }

    m_pCXhcd->EventUnlock();

    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: -CQueuedPipe(%s)::OpenPipe() status %d\n"),
        GetControllerName(),
        GetPipeType(),
        status));
    return status;
}

//----------------------------------------------------------------------------------
// Function: ClosePipe
//
// Description: Abort any transfers associated with this pipe, and
//              remove its data structures from the schedule
//
// Parameters: None
//
// Returns: REQUEST_OK
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CQueuedPipe::ClosePipe(VOID)
{
    UCHAR uEpAddress;
    CQTransfer* pQTransfer;

    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: +CQueuedPipe(%s)::ClosePipe\n"),
        GetControllerName(),
        GetPipeType()));
    HCD_REQUEST_STATUS status = REQUEST_FAILED;

    m_pCXhcd->EventLock();
    EnterCriticalSection(&m_csPipeLock);
    
    m_pCXhcd->RemoveFromBusyPipeList(this);

    pQTransfer = m_pQueuedTransfer;
    while(pQTransfer)
    {
        CQTransfer* pPrevTransfer;

        pPrevTransfer = pQTransfer;
        pQTransfer =(CQTransfer*) pQTransfer->GetNextTransfer();
        if(!pPrevTransfer->IsTransferDone() && !pPrevTransfer->IsHalted())
        {
            pPrevTransfer->SetDataToTransfer(pPrevTransfer->GetTransferLength());        
            pPrevTransfer->SetStatus(STATUS_CQT_HALTED);
            pPrevTransfer->DoneTransfer();         
        }
        pPrevTransfer->SetStatus(STATUS_CQT_CANCELED);
        delete pPrevTransfer;
    }
    m_pQueuedTransfer = NULL;

    if(GetType() == TYPE_CONTROL)
    {
        if(m_pCXhcd->FreeLogicalDevice(m_bDeviceAddress))
        {
            status = REQUEST_OK;
        }
    }
    else
    {
        uEpAddress = m_usbEndpointDescriptor.bEndpointAddress;
        if(m_pCXhcd->DropEndpoint(m_bDeviceAddress, uEpAddress))
        {
            if(m_pCXhcd->ResetEndpoint(m_bDeviceAddress,
                                            uEpAddress,
                                            GetType()))
            {
                status = REQUEST_OK;
            }
        }
    }

    status = REQUEST_OK;
    
    LeaveCriticalSection(&m_csPipeLock);
    m_pCXhcd->EventUnlock();

    return status;
}

//----------------------------------------------------------------------------------
// Function: ResetEndPoint
//
// Description: Endpoint reset operation
//
// Parameters: none
//
// Returns: TRUE - success, FALSE - failure
//----------------------------------------------------------------------------------
BOOL CQueuedPipe::ResetEndPoint(VOID)
{
    if(m_pCXhcd->DropEndpoint(m_bDeviceAddress,
                                    m_usbEndpointDescriptor.bEndpointAddress))
    {
        if(!m_pCXhcd->ResetEndpoint(m_bDeviceAddress,
                                        m_usbEndpointDescriptor.bEndpointAddress,
                                        GetType()))
        {
            return FALSE;
        }
        if (m_pCXhcd->AddEndpoint(m_bDeviceAddress, &m_usbEndpointDescriptor) == XHCD_OK)
        {
            if(!m_pCXhcd->ResetEndpoint(m_bDeviceAddress,
                                        m_usbEndpointDescriptor.bEndpointAddress,
                                        GetType()))
            {
                return FALSE;
            }
        }
    }
    if(!m_pCXhcd->DoConfigureEndpoint(m_bDeviceAddress, NULL, NULL))
    {
        return FALSE;
    }

    Sleep(10);

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: CBulkPipe
//
// Description: Constructor for CBulkPipe
//              Do not modify static variables here!!
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: none
//----------------------------------------------------------------------------------
CBulkPipe::CBulkPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    IN const UCHAR bSpeed,
                    IN const UCHAR bDeviceAddress,
                    IN const UCHAR bHubAddress,
                    IN const UCHAR bHubPort,
                    IN const PVOID pTTContext,
                    IN CXhcd *const pCXhcd) : CQueuedPipe(lpEndpointDescriptor,
                                                            bSpeed,
                                                            bDeviceAddress,
                                                            bHubAddress,
                                                            bHubPort,
                                                            pTTContext,
                                                            pCXhcd)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CBulkPipe::CBulkPipe\n"),
        GetControllerName()));
    DEBUGCHK(m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof(USB_ENDPOINT_DESCRIPTOR) &&
             (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK);

    // bulk pipe must be high speed or full or super
    DEBUGCHK((bSpeed == USB_FULL_SPEED) ||
        (bSpeed == USB_HIGH_SPEED) ||
        (bSpeed == USB_SUPER_SPEED));

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CBulkPipe::CBulkPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CBulkPipe
//
// Description: Destructor for CBulkPipe
//              Do not modify static variables here!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CBulkPipe::~CBulkPipe()
{
    CQTransfer *  pCurTransfer = m_pQueuedTransfer;
    m_pQueuedTransfer = NULL;
    while(pCurTransfer)
    {
         pCurTransfer->AbortTransfer();
         CQTransfer *  pNext =(CQTransfer *)pCurTransfer ->GetNextTransfer();
         delete pCurTransfer;
         pCurTransfer = pNext;
    }
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CBulkPipe::~CBulkPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: AreTransferParametersValid
//
// Description: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: pTransfer - pointer to transfer context
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CBulkPipe::AreTransferParametersValid(const STransfer *pTransfer) const 
{
    if(pTransfer == NULL)
    {
        ASSERT(FALSE);
        return FALSE;
    }
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CBulkPipe::AreTransferParametersValid\n"),
        GetControllerName()));

    // these parameters aren't used by CBulkPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK(pTransfer->lpdwIsochErrors == NULL && // ISOCH
              pTransfer->lpdwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK(!(pTransfer->lpStartAddress == NULL &&
        pTransfer->lpvNotifyParameter != NULL));
    // DWORD pTransfer->dwStartingFrame(ignored - ISOCH)
    // DWORD pTransfer->dwFrames(ignored - ISOCH)

    BOOL fValid = ((pTransfer->lpvBuffer != NULL ||
                    pTransfer->dwBufferSize == 0) &&
                    // paClientBuffer could be 0 or !0
                    m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL);

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE && !fValid,
        (TEXT("%s: -CBulkPipe::AreTransferParametersValid, returning BOOL %d\n"),
        GetControllerName(),
        fValid));
    ASSERT(fValid);
    return fValid;
}

//----------------------------------------------------------------------------------
// Function: DoTransfer
//
// Description:  Send transfer message
//
// Parameters: pTd - transfer data
//
//             uTransferLength - transfer buffer length
//
//             lpvControlHeader - pointer to setup packets, for control transfers, a pointer to the
//                                USB_DEVICE_REQUEST structure
//
//             pTransferBuffer - trnsfer buffer
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CBulkPipe::DoTransfer(TRANSFER_DATA* pTd,
                        UINT uTransferLength, 
                        LPCVOID lpvControlHeader,
                        UINT64 pTransferBuffer)
{
    BOOL bRet = TRUE;
    RING *pEpRing;
    INT iNumTrbs;
    NORMAL_TRB *pStartTrb;
    BOOL pFirstTrb;
    INT iStartCycle;
    UINT uField, uLengthField;
    UINT iRunningTotal, iTrbBuffLen;
    UINT64 uAddr;
    UCHAR bEpIndex;

    if (GetDevicePointer() == NULL) 
    {
        return FALSE;
    }
       
    bEpIndex = CXhcdRing::GetEndpointIndex(m_usbEndpointDescriptor.bEndpointAddress);
    if ((bEpIndex == 0) || (bEpIndex >= MAX_EP_NUMBER))
    {
        return FALSE;
    }

    pEpRing = GetDevicePointer()->eps [bEpIndex].pRing;

    iNumTrbs = 0;

    iRunningTotal = TRB_MAX_BUFF_SIZE;
        
    if(uTransferLength == 0)
    {
        iNumTrbs++;
    }

    while(iRunningTotal < uTransferLength)
    {
        iNumTrbs++;
        iRunningTotal += TRB_MAX_BUFF_SIZE;
    }

    pStartTrb = &pEpRing->pEnqueue->normalTrb;
    if (pStartTrb == NULL)
    {
        return FALSE;
    }
    iStartCycle = pEpRing->uCycleState;

    iRunningTotal = 0;

    uAddr =(UINT64)((UINT) pTransferBuffer);
    iTrbBuffLen = TRB_MAX_BUFF_SIZE;

    if(uTransferLength < iTrbBuffLen)
    {
        iTrbBuffLen = uTransferLength;
    }

    pFirstTrb = TRUE;

    do
    {
        UINT uRemainder = 0;
        uField = 0;

        if(pFirstTrb)
        {
            pFirstTrb = FALSE;
        }
        else
        {
            uField |= pEpRing->uCycleState;
        }

        if(iNumTrbs > 1)
        {
            uField |= TRANSFER_BLOCK_CHAIN;
        }
        else
        {
            pTd->pLastTrb = pEpRing->pEnqueue;
            uField |= TRANSFER_BLOCK_IOC;
        }
        uRemainder = CXhcdRing::GetTransferLeft(uTransferLength - iRunningTotal);
        uLengthField = TRANSFER_BLOCK_LENGTH(iTrbBuffLen) | uRemainder | TRANSFER_BLOCK_TARGET(0);

        pTd->u64Dma = CXhcdRing::TransferVirtualToPhysical(pEpRing->pRingSegment, pEpRing->pEnqueue);

        m_pCXhcd->PutTransferToRing(pEpRing,
                    FALSE,
                    LEAST_32 (uAddr),
                    MOST_32(uAddr),
                    uLengthField,
                    uField | TRANSFER_BLOCK_ISP | TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_NORMAL));

        --iNumTrbs;
        iRunningTotal += iTrbBuffLen;

        uAddr += iTrbBuffLen;
        iTrbBuffLen = uTransferLength - iRunningTotal;
        if(iTrbBuffLen > TRB_MAX_BUFF_SIZE)
        {
            iTrbBuffLen = TRB_MAX_BUFF_SIZE;
        }
    } while(iRunningTotal < uTransferLength);

    m_pCXhcd->RingTransferDoorbell(m_bDeviceAddress, bEpIndex, iStartCycle, pStartTrb);
   
    return bRet;
}

//----------------------------------------------------------------------------------
// Function: CControlPipe
//
// Description: Constructor for CControlPipe
//              Do not modify static variables here!!
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: None
//----------------------------------------------------------------------------------
CControlPipe::CControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                            IN const UCHAR bSpeed,
                            IN const UCHAR bDeviceAddress,
                            IN const UCHAR bHubAddress,
                            IN const UCHAR bHubPort,
                            IN const PVOID pTTContext,
                            IN CXhcd *const pCXhcd) : CQueuedPipe(lpEndpointDescriptor,
                                                                    bSpeed,
                                                                    bDeviceAddress,
                                                                    bHubAddress,
                                                                    bHubPort,
                                                                    pTTContext,
                                                                    pCXhcd)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CControlPipe::CControlPipe\n"),
        GetControllerName()));
    DEBUGCHK(m_usbEndpointDescriptor.bDescriptorType ==
            USB_ENDPOINT_DESCRIPTOR_TYPE &&
            m_usbEndpointDescriptor.bLength >=
            sizeof(USB_ENDPOINT_DESCRIPTOR) &&
            (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
            USB_ENDPOINT_TYPE_CONTROL);

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CControlPipe::CControlPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CControlPipe
//
// Description: Destructor for CControlPipe
//              Do not modify static variables here!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CControlPipe::~CControlPipe()
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CControlPipe::~CControlPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CHcd::IssueTransfer
//
// Returns: REQUEST_OK if transfer issued ok, else REQUEST_FAILED
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS  CControlPipe::IssueTransfer( IN const UINT uAddress,
                                                IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                                IN LPVOID const lpvNotifyParameter,
                                                IN const DWORD dwFlags,
                                                IN LPCVOID const lpvControlHeader,
                                                IN const DWORD dwStartingFrame,
                                                IN const DWORD dwFrames,
                                                IN LPCDWORD const aLengths,
                                                IN const DWORD dwBufferSize,     
                                                IN OUT LPVOID const lpvClientBuffer,
                                                IN const ULONG uPaBuffer,
                                                IN LPCVOID const lpvCancelId,
                                                OUT LPDWORD const lpdwIsochErrors,
                                                OUT LPDWORD const lpdwIsochLengths,
                                                OUT LPBOOL const lpfComplete,
                                                OUT LPDWORD const lpdwBytesTransferred,
                                                OUT LPDWORD const lpdwError)
{
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CControlPipe::IssueTransfer, address = %d\n"),
        GetControllerName(),
        uAddress));
    // Address Changed.
    if(m_bDeviceAddress==0 && uAddress != 0)
    {
        m_bDeviceAddress = uAddress;
    }
    HCD_REQUEST_STATUS status = CQueuedPipe::IssueTransfer(uAddress,
                                                            lpStartAddress,
                                                            lpvNotifyParameter,
                                                            dwFlags,lpvControlHeader,
                                                            dwStartingFrame,
                                                            dwFrames,
                                                            aLengths,
                                                            dwBufferSize,
                                                            lpvClientBuffer,
                                                            uPaBuffer,
                                                            lpvCancelId,
                                                            lpdwIsochErrors,
                                                            lpdwIsochLengths,
                                                            lpfComplete,
                                                            lpdwBytesTransferred,
                                                            lpdwError);

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CControlPipe::IssueTransfer - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
              GetControllerName(),
              uAddress,
              status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ChangeMaxPacketSize
//
// Description: Update the max packet size for this pipe. This should
//              ONLY be done for control endpoint 0 pipes. When the endpoint0
//              pipe is first opened, it has a max packet size of 
//              ENDPOINT_ZERO_MIN_MAXPACKET_SIZE. After reading the device's
//              descriptor, the device attach procedure can update the size.
//              This function should only be called by the Hub AttachDevice
//              procedure
//
// Parameters: wMaxPacketSize - new max packet size for this pipe
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CControlPipe::ChangeMaxPacketSize(IN const USHORT wMaxPacketSize)
{
    INT i;
    USHORT wNewMaxPacketSize;


    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CControlPipe::ChangeMaxPacketSize - new wMaxPacketSize = %d\n"),
        GetControllerName(),
        wMaxPacketSize));

    EnterCriticalSection(&m_csPipeLock);

    // this pipe should be for endpoint 0, control pipe
    DEBUGCHK((m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
            USB_ENDPOINT_TYPE_CONTROL &&
            (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) == 0);

    // update should only be called if the old address was
    // ENDPOINT_ZERO_MIN_MAXPACKET_SIZE
    DEBUGCHK(m_usbEndpointDescriptor.wMaxPacketSize ==
        ENDPOINT_ZERO_MIN_MAXPACKET_SIZE);
    // this function should only be called if we are increasing the max packet size.
    // in addition, the USB spec 1.0 section 9.6.1 states only the following
    // wMaxPacketSize are allowed for endpoint 0

    if(m_uiSpeed == USB_SUPER_SPEED)
    {
        wNewMaxPacketSize = 1;
        for(i = 0; i < wMaxPacketSize; i++)
        {
            wNewMaxPacketSize *= 2;
        }
    }
    else
    {
        wNewMaxPacketSize = wMaxPacketSize;
    }

    DEBUGCHK(wNewMaxPacketSize == 16 ||
             wNewMaxPacketSize == 32 ||
             wNewMaxPacketSize == 64 ||
             wNewMaxPacketSize == 512);
    
    m_usbEndpointDescriptor.wMaxPacketSize = wNewMaxPacketSize;
    m_pCXhcd->ChangeMaxPacketSize(m_bDeviceAddress, m_usbEndpointDescriptor.wMaxPacketSize);

    LeaveCriticalSection(&m_csPipeLock);

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CControlPipe::ChangeMaxPacketSize - \
              new wMaxPacketSize = %d\n"),
              GetControllerName(),
              wMaxPacketSize));
}

//----------------------------------------------------------------------------------
// Function: AreTransferParametersValid
//
// Description: Check whether this class' transfer parameters are valid.
//              This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: pTransfer - pointer to transfer context
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CControlPipe::AreTransferParametersValid(const STransfer *pTransfer) const 
{
    if(pTransfer == NULL)
    {
        return FALSE;
    }
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CControlPipe::AreTransferParametersValid\n"),
        GetControllerName()));

    // these parameters aren't used by CControlPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK(pTransfer->lpdwIsochErrors == NULL && // ISOCH
              pTransfer->lpdwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL); // ISOCH
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK(!(pTransfer->lpStartAddress == NULL &&
        pTransfer->lpvNotifyParameter != NULL));
    // DWORD pTransfer->dwStartingFrame;(ignored - ISOCH)
    // DWORD pTransfer->dwFrames;(ignored - ISOCH)

    BOOL fValid =(m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    pTransfer->lpvControlHeader != NULL &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL);
    if(fValid)
    {
        if(pTransfer->dwFlags & USB_IN_TRANSFER)
        {
            fValid =(pTransfer->lpvBuffer != NULL &&
                      // paClientBuffer could be 0 or !0
                      pTransfer->dwBufferSize > 0);
        }
        else
        {
            fValid =((pTransfer->lpvBuffer == NULL && 
                        pTransfer->uPaBuffer == 0 &&
                        pTransfer->dwBufferSize == 0) ||
                      (pTransfer->lpvBuffer != NULL &&
                        // paClientBuffer could be 0 or !0
                        pTransfer->dwBufferSize > 0));
        }
    }

    ASSERT(fValid);
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CControlPipe::AreTransferParametersValid, returning BOOL %d\n"),
        GetControllerName(),
        fValid));

    return fValid;
}

//----------------------------------------------------------------------------------
// Function: DoTransfer
//
// Description:  Send transfer message
//
// Parameters: pTd - transfer data
//
//             uTransferLength - transfer buffer length
//
//             lpvControlHeader - pointer to setup packets, for control transfers, a pointer to the
//                                USB_DEVICE_REQUEST structure
//
//             pTransferBuffer - trnsfer buffer
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CControlPipe::DoTransfer(TRANSFER_DATA* pTd,
                        UINT uTransferLength, 
                        LPCVOID lpvControlHeader,
                        UINT64 pTransferBuffer)
{
    NORMAL_TRB *pStartTrb;
    USB_DEVICE_REQUEST*  pUsbRequest = (USB_DEVICE_REQUEST*) lpvControlHeader;
    INT iStartCycle;
    UINT uField, uLengthField;
    LOGICAL_DEVICE* pLogicalDevice = NULL;
    RING *pEpRing; 
    UINT bEpIndex;


    pLogicalDevice = GetDevicePointer();
    if(pLogicalDevice == NULL)
    {
        return FALSE;
    }
   
    bEpIndex = CXhcdRing::GetEndpointIndex(m_usbEndpointDescriptor.bEndpointAddress);
    if (bEpIndex != 0)
    {
        return FALSE;
    }

    pEpRing = pLogicalDevice->eps [bEpIndex].pRing;

    pStartTrb = &pEpRing->pEnqueue->normalTrb;
    if (pStartTrb == NULL)
    {
        return FALSE;
    }
    iStartCycle = pEpRing->uCycleState;

    m_pCXhcd->PutTransferToRing(pEpRing,
            FALSE,
            pUsbRequest->bmRequestType |
            pUsbRequest->bRequest << 8 |
            pUsbRequest->wValue << 16,
            pUsbRequest->wIndex | pUsbRequest->wLength << 16,
            TRANSFER_BLOCK_LENGTH(8) | TRANSFER_BLOCK_TARGET(0),
            TRANSFER_BLOCK_IDT | TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_SETUP));
    
    uField = 0;

    uLengthField = TRANSFER_BLOCK_LENGTH(uTransferLength) |
                    CXhcdRing::GetTransferLeft(uTransferLength) |
                    TRANSFER_BLOCK_TARGET(0);
    if(uTransferLength > 0)
    {
        if(pUsbRequest->bmRequestType & USB_DIR_IN)
        {
            uField |= TRANSFER_BLOCK_DIRECTION_IN;
        }
            
        m_pCXhcd->PutTransferToRing(pEpRing,
                FALSE,
                LEAST_32(pTransferBuffer),
                MOST_32(pTransferBuffer),
                uLengthField,
                uField | TRANSFER_BLOCK_ISP | TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_DATA) | pEpRing->uCycleState);
    }

    pTd->pLastTrb = pEpRing->pEnqueue;

    if(uTransferLength > 0 && pUsbRequest->bmRequestType & USB_DIR_IN)
    {
        uField = 0;
    }
    else
    {
        uField = TRANSFER_BLOCK_DIRECTION_IN;
    }

    pTd->u64Dma = CXhcdRing::TransferVirtualToPhysical(pEpRing->pRingSegment, pEpRing->pEnqueue);

    m_pCXhcd->PutTransferToRing(pEpRing,
            FALSE,
            0,
            0,
            TRANSFER_BLOCK_TARGET(0),
            uField | TRANSFER_BLOCK_IOC | TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_STATUS) | pEpRing->uCycleState);

    m_pCXhcd->RingTransferDoorbell(m_bDeviceAddress, bEpIndex, iStartCycle, pStartTrb);
   
    return TRUE;

}


//----------------------------------------------------------------------------------
// Function: CInterruptPipe
//
// Description: Constructor for CInterruptPipe
//              Do not modify static variables here!!
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: None
//----------------------------------------------------------------------------------
CInterruptPipe::CInterruptPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                IN const UCHAR bSpeed,
                                IN const UCHAR bDeviceAddress,
                                IN const UCHAR bHubAddress,
                                IN const UCHAR bHubPort,
                                IN const PVOID pTTContext,
                                IN CXhcd *const pCXhcd) : CQueuedPipe(lpEndpointDescriptor,
                                                                    bSpeed,
                                                                    bDeviceAddress,
                                                                    bHubAddress,
                                                                    bHubPort,
                                                                    pTTContext,
                                                                    pCXhcd)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CInterruptPipe::CInterruptPipe\n"),
        GetControllerName()));
    DEBUGCHK(m_usbEndpointDescriptor.bDescriptorType ==
        USB_ENDPOINT_DESCRIPTOR_TYPE &&
        m_usbEndpointDescriptor.bLength >=
        sizeof(USB_ENDPOINT_DESCRIPTOR) &&
        (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
        USB_ENDPOINT_TYPE_INTERRUPT);

    BYTE bInterval = lpEndpointDescriptor->bInterval;
    if(bInterval == 0)
    {
        bInterval=1;
    }

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CInterruptPipe::CInterruptPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CInterruptPipe
//
// Description: Destructor for CInterruptPipe
//              Do not modify static variables here!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CInterruptPipe::~CInterruptPipe()
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CInterruptPipe::~CInterruptPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: AreTransferParametersValid
//
// Description: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: pTransfer - pointer to transfer context
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CInterruptPipe::AreTransferParametersValid(const STransfer *pTransfer) const
{
    if(pTransfer == NULL) {
        return FALSE;
    }
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CInterruptPipe::AreTransferParametersValid\n"),
        GetControllerName()));

    // these parameters aren't used by CInterruptPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK(pTransfer->lpdwIsochErrors == NULL && // ISOCH
              pTransfer->lpdwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK(!(pTransfer->lpStartAddress == NULL &&
        pTransfer->lpvNotifyParameter  != NULL));
    // DWORD pTransfer->dwStartingFrame(ignored - ISOCH)
    // DWORD pTransfer->dwFrames(ignored - ISOCH)

    BOOL fValid =(m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
                (pTransfer->lpvBuffer != NULL || pTransfer->dwBufferSize == 0) &&
                // paClientBuffer could be 0 or !0
                pTransfer->lpfComplete != NULL &&
                pTransfer->lpdwBytesTransferred != NULL &&
                pTransfer->lpdwError != NULL);

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CInterruptPipe::AreTransferParametersValid, \
              returning BOOL %d\n"),
        GetControllerName(),
        fValid));

    return fValid;
}

//----------------------------------------------------------------------------------
// Function: DoTransfer
//
// Description:  Send transfer message
//
// Parameters: pTd - transfer data
//
//             uTransferLength - transfer buffer length
//
//             lpvControlHeader - pointer to setup packets
//
//             pTransferBuffer - trnsfer buffer
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CInterruptPipe::DoTransfer(TRANSFER_DATA* pTd,
                        UINT uTransferLength, 
                        LPCVOID lpvControlHeader,
                        UINT64 pTransferBuffer)
{
    BOOL bRet = TRUE;
    RING *pEpRing;
    INT iNumTrbs;
    NORMAL_TRB *pStartTrb;
    BOOL pFirstTrb;
    INT iStartCycle;
    UINT uField, uLengthField;
    UINT uRunningTotal, uTrbBuffLen;
    UINT64 uAddr;
    UCHAR bEpIndex;
    ENDPOINT_CONTEXT_DATA *pEpContext;
    INT iXhciInterval;

    if (GetDevicePointer() == NULL) 
    {
        return FALSE;
    }
   
    bEpIndex = CXhcdRing::GetEndpointIndex(m_usbEndpointDescriptor.bEndpointAddress);
    if ((bEpIndex == 0) || (bEpIndex >= MAX_EP_NUMBER))
    {
        return FALSE;
    }

    pEpContext = m_pCXhcd->GetEndpointContext(GetDevicePointer()->pOutContext, bEpIndex);
    if (pEpContext == NULL)
    {
        return FALSE;
    }

    iXhciInterval = ENDPOINT_INTERVAL_TO_FRAMES(pEpContext->dwInterval);

    // Convert to frames
    if(m_uiSpeed == USB_LOW_SPEED ||
        m_uiSpeed == USB_FULL_SPEED)
    {
        iXhciInterval /= 8;
    }

    if(m_usbEndpointDescriptor.bInterval  != iXhciInterval)
    {
        m_usbEndpointDescriptor.bInterval  = iXhciInterval;
        DEBUGMSG(ZONE_ERROR,(TEXT("Uses different intervals\n")));
    }

    pEpRing = GetDevicePointer()->eps [bEpIndex].pRing;

    iNumTrbs = 0;

    uRunningTotal = TRB_MAX_BUFF_SIZE;
        
    if(uTransferLength == 0)
    {
        iNumTrbs++;
    }

    while(uRunningTotal < uTransferLength)
    {
        iNumTrbs++;
        uRunningTotal += TRB_MAX_BUFF_SIZE;
    }

    pStartTrb = &pEpRing->pEnqueue->normalTrb;
    if (pStartTrb == NULL)
    {
        return FALSE;
    }
    iStartCycle = pEpRing->uCycleState;

    uRunningTotal = 0;

    uAddr =(UINT64)((UINT) pTransferBuffer);
    uTrbBuffLen = TRB_MAX_BUFF_SIZE;

    if(uTransferLength < uTrbBuffLen)
    {
        uTrbBuffLen = uTransferLength;
    }

    pFirstTrb = TRUE;

    do
    {
        UINT uRemainder = 0;
        uField = 0;

        if(pFirstTrb)
        {
            pFirstTrb = FALSE;
        }
        else
        {
            uField |= pEpRing->uCycleState;
        }

        if(iNumTrbs > 1)
        {
            uField |= TRANSFER_BLOCK_CHAIN;
        }
        else
        {
            pTd->pLastTrb = pEpRing->pEnqueue;
            uField |= TRANSFER_BLOCK_IOC;
        }
        uRemainder = CXhcdRing::GetTransferLeft(uTransferLength - uRunningTotal);
        uLengthField = TRANSFER_BLOCK_LENGTH(uTrbBuffLen) | uRemainder | TRANSFER_BLOCK_TARGET(0);

        pTd->u64Dma = CXhcdRing::TransferVirtualToPhysical(pEpRing->pRingSegment, pEpRing->pEnqueue);

        m_pCXhcd->PutTransferToRing(pEpRing,
                    FALSE,
                    LEAST_32(uAddr),
                    MOST_32(uAddr),
                    uLengthField,
                    uField | TRANSFER_BLOCK_ISP | TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_NORMAL));

        --iNumTrbs;
        uRunningTotal += uTrbBuffLen;

        uAddr += uTrbBuffLen;
        uTrbBuffLen = uTransferLength - uRunningTotal;
        if(uTrbBuffLen > TRB_MAX_BUFF_SIZE)
        {
            uTrbBuffLen = TRB_MAX_BUFF_SIZE;
        }
    } while(uRunningTotal < uTransferLength);

    m_pCXhcd->RingTransferDoorbell(m_bDeviceAddress, bEpIndex, iStartCycle, pStartTrb);
   
    return bRet;
}

#define NUM_OF_PRE_ALLOCATED_TD (0x100)
//----------------------------------------------------------------------------------
// Function: CIsochronousPipe (CIsochronousPipe Pipe is not supported)
//
// Description: Constructor for CIsochronousPipe
//              Do not modify static variables here!!
//
// Parameters: See CPipe::CPipe
//
// Returns: None
//----------------------------------------------------------------------------------
CIsochronousPipe::CIsochronousPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                    IN const UCHAR bSpeed,
                                    IN const UCHAR bDeviceAddress,
                                    IN const UCHAR bHubAddress,
                                    IN const UCHAR bHubPort,
                                    IN const PVOID pTTContext,
                                    IN CXhcd *const pCXhcd) : CPipe(lpEndpointDescriptor,
                                                                    bSpeed,
                                                                    bDeviceAddress,
                                                                    bHubAddress,
                                                                    bHubPort,
                                                                    pTTContext,
                                                                    pCXhcd)
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CIsochronousPipe::CIsochronousPipe\n"),
        GetControllerName()));
    DEBUGCHK(m_usbEndpointDescriptor.bDescriptorType ==
            USB_ENDPOINT_DESCRIPTOR_TYPE &&
            m_usbEndpointDescriptor.bLength >=
            sizeof(USB_ENDPOINT_DESCRIPTOR) &&
            (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
            USB_ENDPOINT_TYPE_ISOCHRONOUS);
    
    BYTE bInterval = lpEndpointDescriptor->bInterval;
    if(bInterval == 0)
    {
         bInterval = 1;
    }
 
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: -CIsochronousPipe::CIsochronousPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: ~CIsochronousPipe (CIsochronousPipe Pipe is not supported)
//
// Description: Destructor for CIsochronousPipe
//              Do not modify static variables here!!
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CIsochronousPipe::~CIsochronousPipe()
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("%s: +CIsochronousPipe::~CIsochronousPipe\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: OpenPipe (CIsochronousPipe Pipe is not supported)
//
// Description: Inserting the necessary(empty) items into the
//              schedule to permit future transfers
//
// Parameters: None
//
// Returns: REQUEST_OK if pipe opened successfuly
//          REQUEST_FAILED if pipe not opened
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CIsochronousPipe::OpenPipe(VOID)
{
    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: +CIsochronousPipe::OpenPipe\n"),
        GetControllerName()));

    HCD_REQUEST_STATUS status = REQUEST_FAILED;

    EnterCriticalSection(&m_csPipeLock);

    DEBUGCHK(m_usbEndpointDescriptor.bDescriptorType ==
        USB_ENDPOINT_DESCRIPTOR_TYPE &&
        m_usbEndpointDescriptor.bLength >=
        sizeof(USB_ENDPOINT_DESCRIPTOR) &&
        (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
        USB_ENDPOINT_TYPE_ISOCHRONOUS);

    m_dwLastValidFrame = 0;
    m_pCXhcd->GetFrameNumber(&m_dwLastValidFrame);  

    LeaveCriticalSection(&m_csPipeLock);
    if(status == REQUEST_OK)
    {
        VERIFY(m_pCXhcd->AddToBusyPipeList(this, FALSE));
    }
    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: -CIsochronousPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ClosePipe (CIsochronousPipe Pipe is not supported)
//
// Description: Abort any transfers associated with this pipe, and
//              remove its data structures from the schedule
//
// Parameters: None
//
// Returns: REQUEST_OK
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CIsochronousPipe::ClosePipe(VOID)
{
    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: +CIsochronousPipe::ClosePipe\n"),
        GetControllerName()));

    DEBUGMSG(ZONE_PIPE,
        (TEXT("%s: -CIsochronousPipe::ClosePipe\n"),
        GetControllerName()));
    return REQUEST_FAILED;
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer (CIsochronousPipe Pipe is not supported)
//
// Description: Abort any transfer on this pipe if its cancel ID matches
//              that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: REQUEST_OK if transfer aborted
//          REQUEST_FAILED if lpvCancelId doesn't match currently executing
//                 transfer, or if there is no transfer in progress
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CIsochronousPipe::AbortTransfer(IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                                    IN const LPVOID lpvNotifyParameter,
                                                    IN LPCVOID lpvCancelId)
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CIsochronousPipe::AbortTransfer is not implemented\n"),
        GetControllerName()));

    HCD_REQUEST_STATUS status = REQUEST_FAILED;

    return status;
}


//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a Transfer on this pipe (CIsochronousPipe Pipe is not supported)
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CHcd::IssueTransfer
//
// Returns: REQUEST_OK if transfer issued ok, else REQUEST_FAILED
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS  CIsochronousPipe::IssueTransfer(IN const UINT uAddress,
                                                    IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                                    IN LPVOID const lpvNotifyParameter,
                                                    IN const DWORD dwFlags,
                                                    IN LPCVOID const lpvControlHeader,
                                                    IN const DWORD dwStartingFrame,
                                                    IN const DWORD dwFrames,
                                                    IN LPCDWORD const aLengths,
                                                    IN const DWORD dwBufferSize,     
                                                    IN OUT LPVOID const lpvClientBuffer,
                                                    IN const ULONG uPaBuffer,
                                                    IN LPCVOID const lpvCancelId,
                                                    OUT LPDWORD const lpdwIsochErrors,
                                                    OUT LPDWORD const lpdwIsochLengths,
                                                    OUT LPBOOL const lpfComplete,
                                                    OUT LPDWORD const lpdwBytesTransferred,
                                                    OUT LPDWORD const lpdwError)
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CPipe(%s)::IssueTransfer is not implemented, address = %d\n"),
        GetControllerName(),
        GetPipeType(),
        uAddress));

    DWORD dwEarliestFrame = 0;
    HCD_REQUEST_STATUS  status = REQUEST_FAILED;

    m_pCXhcd->GetFrameNumber(&dwEarliestFrame);
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: -CPipe(%s)::IssueTransfer - \
              address = %d, returing HCD_REQUEST_STATUS %d\n"),
        GetControllerName(),
        GetPipeType(),
        uAddress,
        status));

    return status;
}

//----------------------------------------------------------------------------------
// Function: ScheduleTransfer (CIsochronousPipe Pipe is not supported)
//
// Description: Schedule a USB Transfer on this pipe
//
// Parameters: None(all parameters are in m_transfer)
//
// Returns: REQUEST_OK if transfer issued ok, else REQUEST_FAILED
//----------------------------------------------------------------------------------
HCD_REQUEST_STATUS CIsochronousPipe::ScheduleTransfer() 
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CIsochronousPipe::ScheduleTransfer is not implemented\n"),
        GetControllerName()));

    HCD_REQUEST_STATUS status = REQUEST_FAILED;
    return status;
}

//----------------------------------------------------------------------------------
// Function: CheckForDoneTransfers (CIsochronousPipe Pipe is not supported)
//
// Description: Check if the transfer on this pipe is finished, and 
//              take the appropriate actions - i.e. remove the transfer
//              data structures and call any notify routines
//
// Parameters: None
//
// Returns: TRUE if this pipe is no longer busy; FALSE if there are still
//          some pending transfers.
//----------------------------------------------------------------------------------
BOOL CIsochronousPipe::CheckForDoneTransfers(VOID)
{
    DEBUGMSG(ZONE_TRANSFER,
        (TEXT("%s: +CIsochronousPipe::CheckForDoneTransfers is not implemented\n"),
        GetControllerName()));

    BOOL fTransferDone = FALSE;

    return fTransferDone;

};

//----------------------------------------------------------------------------------
// Function: AreTransferParametersValid (CIsochronousPipe Pipe is not supported)
//
// Description: Check whether this class' transfer parameters, stored in
//              m_transfer, are valid.
//              Assumes m_csPipeLock already held
//
// Parameters: None(all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CIsochronousPipe::AreTransferParametersValid(const STransfer *pTransfer) const
{
    if(pTransfer == NULL)
    {
        ASSERT(FALSE);
        return FALSE;
    }
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CIsochronousPipe::AreTransferParametersValid\n"),
        GetControllerName()));

    // these parameters aren't used by CIsochronousPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK(pTransfer->lpvControlHeader == NULL); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK(!(pTransfer->lpStartAddress == NULL &&
        pTransfer->lpvNotifyParameter != NULL));

    BOOL fValid =
        (m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
        pTransfer->dwStartingFrame >= m_dwLastValidFrame &&
        pTransfer->lpvBuffer != NULL &&
        // paClientBuffer could be 0 or !0
        pTransfer->dwBufferSize > 0 &&
        pTransfer->lpdwIsochErrors != NULL &&
        pTransfer->lpdwIsochLengths != NULL &&
        pTransfer->aLengths != NULL &&
        pTransfer->dwFrames > 0 &&
        pTransfer->lpfComplete != NULL &&
        pTransfer->lpdwBytesTransferred != NULL &&
        pTransfer->lpdwError != NULL);

    ASSERT(fValid);
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CIsochronousPipe::AreTransferParametersValid, \
              returning BOOL %d\n"),
        GetControllerName(),
        fValid));

    return fValid;
}

//----------------------------------------------------------------------------------
// Function: DoTransfer
//
// Description:  Send transfer message
//
// Parameters: pTd - transfer data
//
//             uTransferLength - transfer buffer length
//
//             lpvControlHeader - pointer to setup packets
//
//             pTransferBuffer - trnsfer buffer
//
// Returns: TRUE if parameters valid, else FALSE
//----------------------------------------------------------------------------------
BOOL CIsochronousPipe::DoTransfer(TRANSFER_DATA* pTd,
                        UINT uTransferLength, 
                        LPCVOID lpvControlHeader,
                        UINT64 pTransferBuffer)
{
    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: CreateBulkPipe
//
// Description: created new CBulkPipe object
//
// Parameters: see CBulkPipe::CBulkPipe
//
// Returns: new CBulkPipe object
//----------------------------------------------------------------------------------
CPipeAbs * CreateBulkPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                            IN const UCHAR bSpeed,
                            IN const UCHAR bDeviceAddress,
                            IN const UCHAR bHubAddress,
                            IN const UCHAR bHubPort,
                            const PVOID pTTContext,
                            IN CHcd * const pChcd)
{ 
    return new CBulkPipe(lpEndpointDescriptor,
                            bSpeed,
                            bDeviceAddress,
                            bHubAddress,
                            bHubPort,
                            pTTContext,
                            (CXhcd * const)pChcd);
}

//----------------------------------------------------------------------------------
// Function: CreateControlPipe
//
// Description: created new CControlPipe object
//
// Parameters: see CControlPipe::CControlPipe
//
// Returns: new CControlPipe object
//----------------------------------------------------------------------------------
CPipeAbs * CreateControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                IN const UCHAR bSpeed,
                                IN const UCHAR bDeviceAddress,
                                IN const UCHAR bHubAddress,
                                IN const UCHAR bHubPort,
                                const PVOID pTTContext,
                                IN CHcd * const pChcd)
{ 
    return new CControlPipe(lpEndpointDescriptor,
                            bSpeed,
                            bDeviceAddress,
                            bHubAddress,
                            bHubPort,
                            pTTContext,
                            (CXhcd * const)pChcd);
}

//----------------------------------------------------------------------------------
// Function: CreateInterruptPipe
//
// Description: created new CInterruptPipe object
//
// Parameters: see CInterruptPipe::CInterruptPipe
//
// Returns: new CInterruptPipe object
//----------------------------------------------------------------------------------
CPipeAbs * CreateInterruptPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                IN const UCHAR bSpeed,
                                IN const UCHAR bDeviceAddress,
                                IN const UCHAR bHubAddress,
                                IN const UCHAR bHubPort,
                                const PVOID pTTContext,
                                IN CHcd * const pChcd)
{ 
    return new CInterruptPipe(lpEndpointDescriptor,
                                bSpeed,
                                bDeviceAddress,
                                bHubAddress,
                                bHubPort,
                                pTTContext,
                                (CXhcd * const)pChcd);
}

//----------------------------------------------------------------------------------
// Function: CreateIsochronousPipe
//
// Description: created new CIsochronousPipe object
//
// Parameters: see CIsochronousPipe::CIsochronousPipe
//
// Returns: new CIsochronousPipe object
//----------------------------------------------------------------------------------
CPipeAbs * CreateIsochronousPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                                    IN const UCHAR bSpeed,
                                    IN const UCHAR bDeviceAddress,
                                    IN const UCHAR bHubAddress,
                                    IN const UCHAR bHubPort,
                                    const PVOID pTTContext,
                                    IN CHcd * const pChcd)
{ 
    return new CIsochronousPipe(lpEndpointDescriptor,
                                bSpeed,
                                bDeviceAddress,
                                bHubAddress,
                                bHubPort,
                                pTTContext,
                                (CXhcd * const)pChcd);
}