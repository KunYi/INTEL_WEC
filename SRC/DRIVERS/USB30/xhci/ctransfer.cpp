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
// File: ctransfer.cpp
//
// Description:
// 
//     This file implements the CTransfer class for transfers managing
//----------------------------------------------------------------------------------

#include <windows.h>

#include "cphysmem.hpp"
#include "hal.h"
#include "datastructures.h"
#include "ctransfer.h"
#include "cpipe.h"
#include "chw.h"
#include "cxhcd.h"


#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define BAD_DEPTH_INDEX 0xFFFFFFFF

DWORD CTransfer::m_dwGlobalTransferID=0;

//----------------------------------------------------------------------------------
// Function: CTransfer
//
// Description: Constructor for CTransfer
//
// Parameters: pCPipe - Pointer to CPipe object to be associated with transfer
//
//             pCPhysMem - Pointer to CPhysMem to be used
//
//             pSTransfer - Pointer to STransfer containing all the information for USB transfer
//
// Returns: none
//----------------------------------------------------------------------------------
CTransfer::CTransfer(IN CPipe * const pCPipe,
                    IN CPhysMem * const pCPhysMem,
                    STransfer sTransfer) : m_sTransfer(sTransfer),
                                            m_pCPipe(pCPipe),
                                            m_pCPhysMem(pCPhysMem),
                                            m_pTd(NULL)
{
    m_pNextTransfer=NULL;
    m_dwPaControlHeader=0;
    m_pAllocatedForControl=NULL;
    m_pAllocatedForClient=NULL;
    m_dwDataTransferred =0 ;
    m_dwTransferID = m_dwGlobalTransferID++;
    m_fDoneTransferCalled = FALSE;
}

//----------------------------------------------------------------------------------
// Function: ~CTransfer
//
// Description: Destructor for CTransfer
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
CTransfer::~CTransfer()
{
    if(m_pAllocatedForControl != NULL)
    {
        m_pCPhysMem->FreeMemory(PUCHAR(m_pAllocatedForControl),
                                m_dwPaControlHeader,
                                CPHYSMEM_FLAG_NOBLOCK);
    }

    if(m_pAllocatedForClient != NULL)
    {
        m_pCPhysMem->FreeMemory(PUCHAR(m_pAllocatedForClient),
                                m_sTransfer.uPaBuffer,
                                CPHYSMEM_FLAG_NOBLOCK);
    }

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
                (TEXT("%s: CTransfer::~CTransfer()(this=0x%x, \
                      m_pAllocatedForControl=0x%x, \
                      m_pAllocatedForClient=0x%x)\r\n"),
                GetControllerName(),
                this,
                m_pAllocatedForControl,
                m_pAllocatedForClient));
}

//----------------------------------------------------------------------------------
// Function: Init
//
// Description: Function to initialize created CTransfer
//
// Parameters: none
//
// Returns: TRUE - if success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CTransfer::Init(VOID)
{
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: CTransfer::Init(this=0x%x,id=0x%x)\r\n"),
        GetControllerName(),
        this,
        m_dwTransferID));

    // We must allocate the control header memory here so that cleanup works later.
    if(m_sTransfer.lpvControlHeader != NULL &&  m_pAllocatedForControl == NULL)
    {
        // This must be a control transfer. It is asserted elsewhere,
        // but the worst case is we needlessly allocate some physmem.
        if(!m_pCPhysMem->AllocateMemory(DEBUG_PARAM(TEXT("IssueTransfer SETUP Buffer"))
                                        sizeof(USB_DEVICE_REQUEST),
                                        &m_pAllocatedForControl,
                                        CPHYSMEM_FLAG_NOBLOCK,
                                        PAGE_64K_SIZE))
        {
            DEBUGMSG(ZONE_WARNING,
                (TEXT("%s: CPipe(%s)::IssueTransfer - \
                      no memory for SETUP buffer\n"),
                GetControllerName(),
                m_pCPipe->GetPipeType()));

            m_pAllocatedForControl=NULL;
            return FALSE;
        }

        m_dwPaControlHeader = m_pCPhysMem->VaToPa(m_pAllocatedForControl);
        DEBUGCHK(m_pAllocatedForControl != NULL && m_dwPaControlHeader != 0);

        __try
        {
            memcpy_s(m_pAllocatedForControl,
                sizeof(USB_DEVICE_REQUEST),
                m_sTransfer.lpvControlHeader,
                sizeof(USB_DEVICE_REQUEST));
        }
        __except(m_pCPipe->XhciProcessException(GetExceptionCode()))
        {
            // bad lpvControlHeader
            return FALSE;
        }
    }
#ifdef DEBUG
    if(m_sTransfer.dwFlags & USB_IN_TRANSFER)
    {
        // I am leaving this in for two reasons:
        //  1. The memset ought to work even on zero bytes to NULL.
        //  2. Why would anyone really want to do a zero length IN?
        DEBUGCHK(m_sTransfer.dwBufferSize > 0 && m_sTransfer.lpvBuffer != NULL);
        // IN buffer, trash it
        __try
        {
            memset(PUCHAR(m_sTransfer.lpvBuffer),
                GARBAGE,
                m_sTransfer.dwBufferSize);
        } __except(m_pCPipe->XhciProcessException(GetExceptionCode()))
        {
           return FALSE;
        }
    }
#endif // DEBUG

    if(m_sTransfer.dwBufferSize > 0 && m_sTransfer.uPaBuffer == 0)
    { 

        // ok, there's data on this transfer and the client
        // did not specify a physical address for the
        // buffer. So, we need to allocate our own.

        if(!m_pCPhysMem->AllocateMemory(DEBUG_PARAM(TEXT("IssueTransfer Buffer"))
                                        m_sTransfer.dwBufferSize,
                                        &m_pAllocatedForClient,
                                        CPHYSMEM_FLAG_NOBLOCK,
                                        PAGE_64K_SIZE))
        {
            DEBUGMSG(ZONE_WARNING,
                (TEXT("%s: CPipe(%s)::IssueTransfer - no memory for TD buffer\n"),
                GetControllerName(),
                m_pCPipe->GetPipeType()));

            m_pAllocatedForClient = NULL;
            return FALSE;
        }

        m_sTransfer.uPaBuffer = m_pCPhysMem->VaToPa(m_pAllocatedForClient);
        PREFAST_DEBUGCHK(m_pAllocatedForClient != NULL);
        PREFAST_DEBUGCHK(m_sTransfer.lpvBuffer!=NULL);
        DEBUGCHK(m_sTransfer.uPaBuffer != 0);

        if(!(m_sTransfer.dwFlags & USB_IN_TRANSFER))
        {
            // copying client buffer for OUT transfer
            __try
            {
                memcpy_s(m_pAllocatedForClient,
                         m_sTransfer.dwBufferSize,
                         m_sTransfer.lpvBuffer,
                         m_sTransfer.dwBufferSize);
            } 
            __except(m_pCPipe->XhciProcessException(GetExceptionCode()))
            {
                  // bad lpvClientBuffer
                  return FALSE;
            }
        }
    }
    
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: CTransfer::Init(this=0x%x,id=0x%x), \
              m_pAllocatedForControl=0x%x, \
              m_pAllocatedForClient=0x%x\r\n"),
        GetControllerName(),
        this,
        m_dwTransferID,
        m_pAllocatedForControl,
        m_pAllocatedForClient));

    return AddTransfer();
}

//----------------------------------------------------------------------------------
// Function: GetControllerName
//
// Description: see CPipe::GetControllerName
//
// Parameters: none
//
// Returns: Const null-terminated string containing the HCD controller name
//----------------------------------------------------------------------------------
LPCTSTR CTransfer::GetControllerName(VOID) const
{
    return m_pCPipe->GetControllerName();
}

//----------------------------------------------------------------------------------
// Function: AddTransfer
//
// Description: This method prepares one transfer for activation 
//
// Parameters: none
//
// Returns: TRUE - if success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CQTransfer::AddTransfer() 
{    
    INT iRet;

    iRet = m_pCPipe->PrepareTransfer(&m_pTd, m_sTransfer.dwBufferSize);
    ASSERT((m_pTd != NULL) ||(iRet != XHCD_OK));
    if(iRet == -XHCD_EP_STATE_ERROR)
    {
        m_dwStatus = STATUS_CQT_HALTED;
    }
    return(iRet == XHCD_OK);
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer
//
// Description: Aborting a transfer
//
// Parameters: none
//
// Returns: TRUE - if success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CQTransfer::AbortTransfer() 
{
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CQTransfer::AbortTransfer(this=0x%x,id=0x%x)\r\n"),
        GetControllerName(),
        this,
        m_dwTransferID));


    if(m_dwStatus != STATUS_CQT_CANCELED)
    {

        //
        // mark it as cancelled
        //
        m_dwUsbError = USB_CANCELED_ERROR;
        m_dwStatus = STATUS_CQT_CANCELED;
    }

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CQTransfer::AbortTransfer(this=0x%x) return %d \r\n"),
        GetControllerName(),
        this,
        m_dwStatus));

    return(m_dwStatus==STATUS_CQT_CANCELED);
}

//----------------------------------------------------------------------------------
// Function: IsTransferDone
//
// Description: Check for transfer completion
//
// Parameters: none
//
// Returns: TRUE - if success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CQTransfer::IsTransferDone() 
{
    BOOL fDone = FALSE;

    if(m_dwStatus == STATUS_CQT_CANCELED || m_dwStatus == STATUS_CQT_RETIRED)
    {
        fDone = TRUE;
    }

    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CQTransfer::IsTransferDone(this=0x%x) return %d \r\n"),
        GetControllerName(),
        this,
        fDone));

    return fDone;
}

//----------------------------------------------------------------------------------
// Function: DoneTransfer
//
// Description: Complete a transfer; invoke callbacks(if any)
//
// Parameters: none
//
// Returns: always TRUE
//----------------------------------------------------------------------------------
BOOL CQTransfer::DoneTransfer(VOID)
{
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: +CQTransfer::DoneTransfer(this=0x%x,id=0x%x)\r\n"),
        GetControllerName(),
        this,
        m_dwTransferID));

    if(!m_fDoneTransferCalled)
    {
        // same as STATUS_CQT_RETIRED
        m_fDoneTransferCalled = TRUE;

        ASSERT(m_dwDataNotTransferred <= m_sTransfer.dwBufferSize);
        if(m_dwDataNotTransferred > m_sTransfer.dwBufferSize)
        {
             m_dwDataNotTransferred = m_sTransfer.dwBufferSize;
        }
        m_dwDataTransferred = m_sTransfer.dwBufferSize - m_dwDataNotTransferred;

        if(m_dwStatus == STATUS_CQT_HALTED)
        {
             m_dwUsbError = USB_STALL_ERROR;
        }
 
        // STATUS_CQT_CANCELED and STATUS_CQT_RETIRED are terminal states.
        // Do not change to RETIRED if it is already CANCELED.
        if((m_dwStatus!=STATUS_CQT_CANCELED) &&(m_dwStatus!=STATUS_CQT_HALTED))
        {
            m_dwStatus = STATUS_CQT_RETIRED;
        }

        __try
        {
            // We have to update the buffer when this is IN Transfer.
            if((m_sTransfer.dwFlags&USB_IN_TRANSFER) != 0 &&
                m_pAllocatedForClient != NULL &&
                m_dwDataTransferred!=0)
            {
                    memcpy_s(m_sTransfer.lpvBuffer,
                             m_sTransfer.dwBufferSize,
                             m_pAllocatedForClient,
                             m_dwDataTransferred);

            }

            if(m_sTransfer.lpfComplete != NULL)
            {
               *m_sTransfer.lpfComplete = TRUE;
            }

            if(m_sTransfer.lpdwError != NULL)
            {
               *m_sTransfer.lpdwError = m_dwUsbError;
            }

            if(m_sTransfer.lpdwBytesTransferred)
            {
               *m_sTransfer.lpdwBytesTransferred = m_dwDataTransferred;
            }

            if(m_sTransfer.lpStartAddress)
            {
                (*m_sTransfer.lpStartAddress)(m_sTransfer.lpvNotifyParameter);
                // Make sure only do once.
                m_sTransfer.lpStartAddress = NULL ;
            }
        } __except(m_pCPipe->XhciProcessException(GetExceptionCode()))
        {
            DEBUGMSG(ZONE_ERROR,
                (TEXT("%s:  CQTransfer{%s}::DoneTransfer - \
                      exception during transfer completion\n"),
                      GetControllerName(),
                      m_pCPipe->GetPipeType()));

            if(m_dwUsbError == USB_NO_ERROR)
            {
               m_dwUsbError = USB_CLIENT_BUFFER_ERROR;
            }
        }
    }
    else
    {
        // no transfer shall ever be retired twice
        ASSERT(0);
    }
    DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
        (TEXT("%s: -CQTransfer::DoneTransfer(this=0x%x) usberr %d \r\n"),
        GetControllerName(),
        this,
        m_dwUsbError));

    return m_fDoneTransferCalled;
}

//----------------------------------------------------------------------------------
// Function: Activate
//
// Description: Put transfer parameters into the ring
//
// Parameters: none
//
// Returns: TRUE - if success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL CQTransfer::Activate(VOID)
{
    BOOL fRet;

    if(!m_pTd)
    {
        return FALSE;
    }
    m_pTd->pQTransfer = this;
    m_dwStatus = STATUS_CQT_ACTIVATED;
    fRet = m_pCPipe->DoTransfer(m_pTd,
                                m_sTransfer.dwBufferSize,
                                m_pAllocatedForControl,
                                m_sTransfer.uPaBuffer);


    return fRet;
}

