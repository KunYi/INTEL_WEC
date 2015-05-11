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
// File: ceventhandler.cpp
//
// Description:
// 
//     USB XHCI HCD interrupt handler. Event handler class for XHCI
//----------------------------------------------------------------------------------

#include <windows.h>

#include "globals.hpp"
#include "hal.h"
#include "datastructures.h"
#include "chw.h"
#include "ceventhandler.h"
#include "cring.h"

//----------------------------------------------------------------------------------
// Function: CXhcdEventHandler
//
// Description: Initialize variables associated with this class
//
// Parameters: pCHW - pointer to CXhcd object, which is the main entry
//             point for all HCDI calls by USBD
//
//             pCPhysMem - Memory object created by <f HcdMdd_CreateMemoryObject>.
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcdEventHandler::CXhcdEventHandler(IN CHW* pCHW,
                                    IN CPhysMem * pCPhysMem) : m_pMem(pCPhysMem),
                                                               m_pCHW(pCHW),
                                                               m_pXhcdRing(NULL),
                                                               m_fEventHandlerClosing (FALSE)

{
    m_hUsbHubChangeEvent = NULL;
}

//----------------------------------------------------------------------------------
// Function: ~CXhcdEventHandler
//
// Description: Destroy all memory and objects associated with this class
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcdEventHandler::~CXhcdEventHandler()
{
}

//----------------------------------------------------------------------------------
// Function: Initialize
//
// Description: Initialize event handler data
//
// Parameters: none
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
BOOL CXhcdEventHandler::Initialize()
{
    m_hUsbHubChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(m_hUsbHubChangeEvent == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("-CXhcdEventHandler::Initialize. \
                  Error creating USBInterrupt or USBHubHubChangeEvent event\n")));
        return FALSE;
    }
    m_pXhcdRing = m_pCHW->m_pXhcdRing;
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: DeInitialize
//
// Description: Deinitialize event handler data
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::DeInitialize()
{
    DWORD dwWaitReturn;

    if(m_hUsbHubChangeEvent)
    {
        m_fEventHandlerClosing = TRUE;
        SetEvent (m_hUsbHubChangeEvent);
        dwWaitReturn = WaitForSingleObject(m_hUsbHubChangeEvent, 1000);

        CloseHandle(m_hUsbHubChangeEvent);
        m_hUsbHubChangeEvent = NULL;
        m_fEventHandlerClosing = FALSE;
    }
}

//----------------------------------------------------------------------------------
// Function: HandleEvent
//
// Description: Generic events hadler. It is calledcfrom IST.
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::HandleEvent() 
{
    TRB *pEvent;
    INT iUpdatePtrs = 1;
    RING* pEventRing = m_pXhcdRing->m_pEventRing;
    INT iRet;

    if(!pEventRing || !pEventRing->pDequeue)
    {
        return;
    }

    pEvent = pEventRing->pDequeue;

    if(pEvent->normalTrb.dwCycleBit != pEventRing->uCycleState)
    {
        return;
    }

    switch(pEvent->normalTrb.dwBlockType)
    {
    case TRANSFER_BLOCK_COMPLETE:
        HandleCommandCompletion(&pEvent->commandEventTrb);
        break;

    case TRANSFER_BLOCK_PORT_STATUS:
        HandlePortStatus(&pEvent->portEventTrb);
        iUpdatePtrs = 0;
        break;

    case TRANSFER_BLOCK_TRANSFER:

        iRet = HandleTransferEvent(&pEvent->transferEventTrb);

        if(iRet < 0)
        {
            DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
                (TEXT("handleTxEvent error\n")));
        }
        else
        {
            iUpdatePtrs = 0;
        }

        break;

    default:
        DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,(TEXT("unknown event\n")));
    }

    if(iUpdatePtrs)
    {
        m_pXhcdRing->IncrementDequeue(pEventRing, TRUE);
        m_pCHW->SetHostControllerEventDequeue();    
    }

    HandleEvent();
}

//----------------------------------------------------------------------------------
// Function: HandleCommandCompletion
//
// Description: Command completion event handler
//
// Parameters: pEvent - command event TRB
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::HandleCommandCompletion(COMMAND_EVENT_TRB *pEvent)
{
    UCHAR bSlotId = pEvent->dwSlotId;
    UINT64 u64CmdDma;
    UINT64 u64CmdDequeueDma;
    LOGICAL_DEVICE *pVirtDev;
    RING* pCmdRing = m_pXhcdRing->m_pCmdRing;


    u64CmdDma = pEvent->u64CmdTrb;
    u64CmdDequeueDma = m_pXhcdRing->TransferVirtualToPhysical(pCmdRing->pRingSegment,
                                                    pCmdRing->pDequeue);
    
    if(u64CmdDequeueDma == 0)
    {
        return;
    }

    if(u64CmdDma != (UINT64) u64CmdDequeueDma)
    {
        return;
    }

    if(m_pCHW->m_pLogicalDevices == NULL)
    {
        return;
    }

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];


    switch(pCmdRing->pDequeue->commandTrb.dwTrbType)
    {
    case TRANSFER_BLOCK_ENABLE_SLOT:
        if(COMPLETION_STATUS(pEvent->uStatus) == STATUS_DB_SUCCESS)
        {
            m_pCHW->m_bSlotId = bSlotId;
        }
        else
        {
            m_pCHW->m_bSlotId = 0;
        }
        m_pXhcdRing->Complete(&m_pCHW->m_addrDev);
        break;

    case TRANSFER_BLOCK_DISABLE_SLOT:
        if(pVirtDev)
        {
            pVirtDev->uCmdStatus = COMPLETION_STATUS(pEvent->uStatus);
            m_pXhcdRing->Complete(&pVirtDev->cmdCompletion);
        }
        break;

    case TRANSFER_BLOCK_ENDPOINT_CFG:
        if(pVirtDev != NULL)
        {
            if(HandleCommandWaitList(pVirtDev, pEvent))
            {
                break;
            }
            pVirtDev->uCmdStatus = COMPLETION_STATUS(pEvent->uStatus);
            m_pXhcdRing->Complete(&pVirtDev->cmdCompletion);
        }
        break;

    case TRANSFER_BLOCK_ADDRESS:
        if(pVirtDev != NULL)
        {
            pVirtDev->uCmdStatus = COMPLETION_STATUS(pEvent->uStatus);
            m_pXhcdRing->Complete(&m_pCHW->m_addrDev);
        }
        break;

    case TRANSFER_BLOCK_SET_DEQ:
        HandleSetDequeueCompletion(pEvent, pCmdRing->pDequeue);
        break;

    case TRANSFER_BLOCK_NOOP:
        ++m_iNoopsHandled;
        break;

    case TRANSFER_BLOCK_RESET_ENDPOINT:
        HandleResetEpCompletion(pCmdRing->pDequeue);
        break;

    case TRANSFER_BLOCK_RESET_DEVICE:
        bSlotId = pCmdRing->pDequeue->commandTrb.dwSlotId;
        pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];
        if(pVirtDev)
        {
            HandleCommandWaitList(pVirtDev, pEvent);
        }
        else
        {
            DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
                (TEXT("Reset device command for the disabled slot %u\n"),
                bSlotId));
        }
        break;
        
    default:
        DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
            (TEXT("HandleCommandCompletion : unknoww error\n")));
        break;
    }
    m_pXhcdRing->IncrementDequeue(pCmdRing, FALSE);
}

//----------------------------------------------------------------------------------
// Function: HandlePortStatus
//
// Description: Port status change handler
//
// Parameters: pEvent - port event TRB
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::HandlePortStatus(PORT_EVENT_TRB *pEvent) const
{
    UINT uPortId;

    if(pEvent->dwCompletionCode != STATUS_DB_SUCCESS)
    {
        return;
    }

    uPortId = pEvent->dwPortId;

    m_pXhcdRing->IncrementDequeue(m_pXhcdRing->m_pEventRing, TRUE);
    m_pCHW->SetHostControllerEventDequeue();

    SetEvent(m_hUsbHubChangeEvent);
}

//----------------------------------------------------------------------------------
// Function: HandleCommandWaitList
//
// Description: handle commands placed into wait list
//
// Parameters: pVirtDev - pointer to the logical device
// 
//             pEvent - pointer to the command completion event
//
// Returns:  TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
BOOL CXhcdEventHandler::HandleCommandWaitList(LOGICAL_DEVICE *pVirtDev,
                                              COMMAND_EVENT_TRB *pEvent) const
{
    COMMAND *pCommand = NULL;
    PLINK pLink;

    if(m_pCHW->UsbListEmpty(&pVirtDev->cmdList))
    {
        return FALSE;
    }

    pLink = pVirtDev->cmdList.pLink;

    while (pLink != NULL) 
    {
        pCommand = (COMMAND*) pLink->pStruct;
        if (pCommand == NULL ||
            pCommand->pCommandTrb == m_pXhcdRing->m_pCmdRing->pDequeue) 
        {
            break;
        }
        pLink = pLink->pLinkFwd;
    }
    
    if (pLink == NULL || pCommand == NULL)
    {
        return FALSE;
    }

    pCommand->uStatus = COMPLETION_STATUS(pEvent->uStatus);

    m_pCHW->UsbListUnlink(&pVirtDev->cmdList, &pCommand->cmdList);

    m_pXhcdRing->Complete(&pCommand->completion);

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: WaitForPortStatusChange
//
// Description: wait for one of the ports status change
//
// Parameters: m_hHubChanged - event for hub status change thread
//
// Returns: Returns TRUE if port status changed, FALSE if event handle is closing.
//----------------------------------------------------------------------------------
BOOL CXhcdEventHandler::WaitForPortStatusChange(HANDLE m_hHubChanged) const
{
    if(m_hUsbHubChangeEvent && !m_fEventHandlerClosing)
    {
        if(m_hHubChanged != NULL)
        {
            HANDLE hArray[2];
            hArray[0]=m_hHubChanged;
            hArray[1]=m_hUsbHubChangeEvent;
            WaitForMultipleObjects(2,hArray,FALSE,INFINITE);
        }
        else
        {
            WaitForSingleObject(m_hUsbHubChangeEvent,INFINITE);
        }

        return TRUE;
    }

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: HandleSetDequeueCompletion
//
// Description: handle Set Dequeue command completion
//
// Parameters: pEvent - event generated by Set Dequeue command 
//
//             pTrb - command ring dequeue address
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::HandleSetDequeueCompletion(COMMAND_EVENT_TRB *pEvent,
                                                   TRB *pTrb) const
{
    UCHAR bSlotId;
    UCHAR bEpIndex;
    RING *pEpRing;
    LOGICAL_DEVICE *pVirtDev;
    SLOT_CONTEXT_DATA *pSlotContext;

    bSlotId = pTrb->commandTrb.dwSlotId;
    bEpIndex = ENDPOINT_ID_TO_INDEX (pTrb->commandTrb.dwEndpointId);

    if((bSlotId == 0) || (bEpIndex >= MAX_EP_NUMBER))
    {
        return;
    }

    pVirtDev = m_pCHW->m_pLogicalDevices[bSlotId];
    pEpRing = pVirtDev->eps[bEpIndex].pRing;
    pSlotContext = m_pXhcdRing->GetSlotContext(pVirtDev->pOutContext);

    if (pSlotContext == NULL)
    {
        return;
    }

    if(COMPLETION_STATUS(pEvent->uStatus) != STATUS_DB_SUCCESS)
    {
        UINT uSlotState;

        switch(COMPLETION_STATUS(pEvent->uStatus))
        {
        case STATUS_TRANSFER_BLOCK_ERROR:
            break;

        case STATUS_CONTEXT_STATE:
            uSlotState = pSlotContext->dwSlotState;
            break;

        case STATUS_BAD_SPLIT:
            break;

        default:
            DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
                (TEXT("Set Transfer Dequeue Pointer failed.\n")));
            break;
        }
    }
    else
    {
        DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
            (TEXT("Successful Set Transfer Dequeue Pointer slot %x\n"),
            bSlotId));
    }

    pVirtDev->eps[bEpIndex].uEpState &= ~SET_DEQ_PENDING;
    m_pXhcdRing->RingEpDoorbell(bSlotId, bEpIndex);
}

//----------------------------------------------------------------------------------
// Function: HandleResetEpCompletion
//
// Description: Check the result of reset endpoint command.
//
// Parameters: pTrb - command ring dequeue address
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdEventHandler::HandleResetEpCompletion(TRB *pTrb) const
{
    UCHAR bSlotId;
    UCHAR bEpIndex;
    LOGICAL_DEVICE *pVirtDev;

    bSlotId = pTrb->commandTrb.dwSlotId;
    bEpIndex = ENDPOINT_ID_TO_INDEX (pTrb->commandTrb.dwEndpointId);
    if((bSlotId == 0) || (bEpIndex >= MAX_EP_NUMBER))
    {
        return;
    }
    pVirtDev = m_pCHW->m_pLogicalDevices[bSlotId];
    
    if (pVirtDev != NULL)
    {
        pVirtDev->eps[bEpIndex].uEpState &= ~ENDPOINT_HALT;
        m_pXhcdRing->RingEpDoorbell(bSlotId, bEpIndex);
    } else {
        ASSERT (FALSE);
    }
}

//----------------------------------------------------------------------------------
// Function: CheckCompCode
//
// Description: Check event completion code
//
// Parameters: uTrbCompCode - completion code
//
// Returns: -XHCD_FAIL if bad endpoint, else XHCD_OK
//----------------------------------------------------------------------------------
INT CXhcdEventHandler::CheckCompCode(UINT uTrbCompCode)
{
    INT iRet = XHCD_OK;

    switch(uTrbCompCode) {
    case STATUS_DB_SUCCESS:
        break;

    case STATUS_SHORT_TRANSFER:
        break;

    case STATUS_BAD_ENDPOINT:
        DEBUGMSG(ZONE_ERROR,(TEXT("Endpoint is not enabled\n")));
        iRet = -XHCD_FAIL;
        break;

    case STATUS_STOP:
        break;

    case STATUS_STOP_INVALID:
        break;

    case STATUS_STALL:
        DEBUGMSG(ZONE_ERROR,(TEXT("Stalled endpoint\n")));
        break;

    case STATUS_TRANSFER_BLOCK_ERROR:
        DEBUGMSG(ZONE_ERROR,(TEXT("Transfer block error\n")));
        break;

    case STATUS_SPLIT_ERROR:
        DEBUGMSG(ZONE_ERROR,(TEXT("Split error\n")));
        break;

    case STATUS_TRANSFER_ERROR:
        DEBUGMSG(ZONE_ERROR,(TEXT("Transfer error\n")));
        break;

    case STATUS_BABBLE:
        DEBUGMSG(ZONE_ERROR,(TEXT("Babble error\n")));
        break;

    case STATUS_DB_ERROR:
        break;

    case STATUS_BANDWITH_OVERRUN:
        DEBUGMSG(ZONE_ERROR,(TEXT("Bandwidth overrun event\n")));
        break;

    case STATUS_UNDERRUN:
        DEBUGMSG(ZONE_ERROR,(TEXT("Ring Underrun\n")));
        break;

    default:
        iRet = -XHCD_FAIL;
    }
    return iRet;
}

//----------------------------------------------------------------------------------
// Function: HandleTransferEvent
//
// Description: handle transfer event
//
// Parameters: pEvent - transfer event TRB
//
// Returns: XHCD_OK if successful, < 0 if error occurs.
//----------------------------------------------------------------------------------
INT CXhcdEventHandler::HandleTransferEvent(TRANSFER_EVENT_TRB *pEvent)
{
    LOGICAL_DEVICE *pLogicalDevice;
    LOGICAL_ENDPOINT *pEp;
    RING *pEpRing;
    UCHAR bSlotId;
    UCHAR bEpIndex;
    TRANSFER_DATA *pTd = 0;
    UINT64 u64EventDma;
    RING_SEGMENT *pEndpointRingSegment;
    TRB *pEventTrb;
    ENDPOINT_CONTEXT_DATA *pEpContext;
    UINT uTrbCompCode;
    CQTransfer* pQTransfer;

    bSlotId = TRANSFER_BLOCK_TO_SLOT(pEvent->uFlags);
    if(bSlotId == 0)
    {
        return -XHCD_FAIL;
    }

    pLogicalDevice = m_pCHW->m_pLogicalDevices [bSlotId];
    if(!pLogicalDevice)
    {
        return -XHCD_NODEVICE;
    }
    
    bEpIndex = TRANSFER_BLOCK_TO_ID(pEvent->uFlags) - 1;
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_FAIL;
    }
    pEpContext = m_pXhcdRing->GetEndpointContext(pLogicalDevice->pOutContext, bEpIndex);
    if (pEpContext == NULL)
    {
        return -XHCD_FAIL;
    }

    uTrbCompCode = COMPLETION_STATUS(pEvent->uTransferLen);
    if(CheckCompCode(uTrbCompCode) != XHCD_OK)
    {
        return -XHCD_FAIL;
    } 

    pEp = &pLogicalDevice->eps [bEpIndex];
    if(pEp == NULL)
    {
        return -XHCD_FAIL;
    }
    pEp->bAbortCounter = 0;

    m_pCHW->EventLock();
    pEpRing = pEp->pRing;

    u64EventDma =(UINT64)pEvent->u64Buffer;

    ASSERT(u64EventDma);

    if(!pEpRing || pEpContext->dwEpState == ENDPOINT_STATE_DISABLED)
    {
        m_pCHW->EventUnlock();
        return -XHCD_NODEVICE;
    }

    pEndpointRingSegment = pEpRing->pRingSegment;

    if (!m_pCHW->GetCurrentEventSeg(bSlotId,
                                    bEpIndex,
                                    pEndpointRingSegment,
                                    pEpRing->pDequeue,
                                    &pTd,
                                    u64EventDma))
    {
        m_pCHW->EventUnlock();
        return -XHCD_EVENT_ERROR;
    }

    pEventTrb = &pEndpointRingSegment->pTrbs[(u64EventDma - pEndpointRingSegment->u64Dma) / sizeof(*pEventTrb)];
    if (pEventTrb == NULL)
    {
        m_pCHW->EventUnlock();
        return -XHCD_FAIL;
    }
    pQTransfer = pTd->pQTransfer;

    if(m_pXhcdRing->IsControlEndpoint(pTd->bEndpointType))
    {
        switch(uTrbCompCode)
        {
        case STATUS_DB_SUCCESS:
            ASSERT(u64EventDma == pTd->u64Dma);
            pQTransfer->SetStatus(STATUS_CQT_DONE);
            break;

        case STATUS_SHORT_TRANSFER:
            if (pTd->u64Dma == u64EventDma) {
                pQTransfer->SetStatus(STATUS_CQT_DONE);
            } else {
                pQTransfer->SetStatus(STATUS_CQT_SHORT);
            }
            pQTransfer->SetDataToTransfer(TRANSFER_BLOCK_LENGTH(pEvent->uTransferLen));
            break;

        case STATUS_TRANSFER_ERROR:
        case STATUS_BABBLE:
        case STATUS_SPLIT_ERROR:
        case STATUS_STALL:
            if(pEventTrb != pEpRing->pDequeue && pEventTrb != pTd->pLastTrb)
            {
                pQTransfer->SetDataToTransfer(TRANSFER_BLOCK_LENGTH(pEvent->uTransferLen));
            }
            else
            {
                pQTransfer->SetDataToTransfer(pQTransfer->GetTransferLength());        
            }

            pQTransfer->SetStatus(STATUS_CQT_HALTED);
            pEp->uEpState |= ENDPOINT_HALT;
            pEp->pStoppedTd = pTd;
            pEp->pStoppedTrb = pEventTrb;

            goto cleanup;

        default:
            break;
        }
        
    }
    else
    {
        switch(uTrbCompCode)
        {
        case STATUS_DB_SUCCESS:
            ASSERT(u64EventDma == pTd->u64Dma);
            pQTransfer->SetStatus(STATUS_CQT_DONE);
            break;

        case STATUS_SHORT_TRANSFER:
            pQTransfer->SetDataToTransfer(TRANSFER_BLOCK_LENGTH(pEvent->uTransferLen));
            if (pTd->u64Dma == u64EventDma) {
                pQTransfer->SetStatus(STATUS_CQT_DONE);
            } else {
                pQTransfer->SetStatus(STATUS_CQT_SHORT);
            }
            break;

        case STATUS_TRANSFER_ERROR:
        case STATUS_BABBLE:
        case STATUS_SPLIT_ERROR:
        case STATUS_STALL:

            if(pEventTrb != pEpRing->pDequeue && pEventTrb != pTd->pLastTrb)
            {
                pQTransfer->SetDataToTransfer(TRANSFER_BLOCK_LENGTH(pEvent->uTransferLen));
            }
            else
            {
                pQTransfer->SetDataToTransfer(pQTransfer->GetTransferLength());        
            }

            pQTransfer->SetStatus(STATUS_CQT_HALTED);
            pEp->uEpState |= ENDPOINT_HALT;
            pEp->pStoppedTd = pTd;
            pEp->pStoppedTrb = pEventTrb;

            goto cleanup;
            break;
        default:
            break;
        }
    }

    if(uTrbCompCode == STATUS_STOP_INVALID || uTrbCompCode == STATUS_STOP)
    {
        pEp->pStoppedTd = pTd;
        pEp->pStoppedTrb = pEventTrb;
    }
    else
    {
        if(uTrbCompCode == STATUS_DB_SUCCESS)
        {
            while(pEpRing->pDequeue != pTd->pLastTrb)
            {
                m_pXhcdRing->IncrementDequeue(pEpRing, FALSE);        
            }
        }
        m_pXhcdRing->IncrementDequeue(pEpRing, FALSE);
    }

cleanup:
    m_pXhcdRing->IncrementDequeue(m_pXhcdRing->m_pEventRing, TRUE);
    m_pCHW->SetHostControllerEventDequeue();

    if(pQTransfer->GetStatus() != STATUS_CQT_SHORT)
    {
        pQTransfer->DoneTransfer();         
    }
    
    m_pCHW->EventUnlock();

    return XHCD_OK;
}