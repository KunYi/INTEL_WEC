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
// File: cring.cpp
//
// Description:
// 
//     This module contains ring handling routines.
//----------------------------------------------------------------------------------

#include <windows.h>

#include "cdevice.hpp"
#include "hal.h"
#include "datastructures.h"
#include "cring.h"
#include "chw.h"
#include "ceventhandler.h"

//----------------------------------------------------------------------------------
// Function: CXhcdRing
//
// Description: Initialize variables associated with this class
//
// Parameters: pCHW - pointer to CXhcd object, which is the main entry
//                    point for all HCDI calls by USBD
//
//             pCPhysMem - Memory object created by <f HcdMdd_CreateMemoryObject>.
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcdRing::CXhcdRing(IN CHW* pCHW,
                    IN CPhysMem * pCPhysMem) : m_pMem(pCPhysMem),
                                                m_pCHW(pCHW),
                                                m_uCmdRingReservedTrbs(0),
                                                m_pCmdRing(NULL),
                                                m_pEventRing(NULL)
{    
}

//----------------------------------------------------------------------------------
// Function: ~CXhcdRing
//
// Description: Destroy all memory and objects associated with this class
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcdRing::~CXhcdRing()
{
    if (m_pCmdRing != NULL) 
    {
        FreeRing(m_pCmdRing);
    }

    if (m_pEventRing != NULL) 
    {
        FreeRing(m_pEventRing);
    }
}

//----------------------------------------------------------------------------------
// Function: LastTrb
//
// Description: Check if it is the last trb in the ring
//
// Parameters: pRing - pointer to the ring
//
//             pSeg  - pointer to the ring segment
//
//             pTrb  - pointer to the TRB to be chaecked
//
// Returns: TRUE if it is the last TRB, otherwise FALSE 
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsLastTrb(RING *pRing,
                         RING_SEGMENT *pSeg,
                         TRB *pTrb) const
{
    if(pRing == m_pEventRing)
    {
        return pTrb == &pSeg->pTrbs[BLOCKS_IN_SEGMENT];
    }
    else
    {
        return(pTrb->linkTrb.uControl & TRANSFER_BLOCK_TYPE_BITMASK) == TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_LINK);
    }
}

//----------------------------------------------------------------------------------
// Function: GetNextTrb
//
// Description: Get the next TRB pointer.
//
// Parameters: pRing - pointer to the ring
//
//             ppSeg  - pointer to the ring segment
//
//             ppTrb  - IN - current TRB, OUT - next TRB
//
// Returns: none 
//----------------------------------------------------------------------------------
VOID CXhcdRing::GetNextTrb(RING *pRing,
                        RING_SEGMENT **ppSeg,
                        TRB **ppTrb) const
{
    if(IsLastTrb(pRing, *ppSeg, *ppTrb))
    {
        *ppTrb =((*ppSeg)->pTrbs);
    }
    else
    {
        *ppTrb =(*ppTrb)++;
    }
}

//----------------------------------------------------------------------------------
// Function: FindNewDequeueState
//
// Description: find new dequeue position
//
// Parameters: bSlotId  - device slot ID
//
//             bEpIndex - stalled endpoint index
//
//             pCurTd - stopped transfer data
//
//             pState - found dequeue state
//
// Returns: XHCD_OK if state is found, otherwise < 0
//----------------------------------------------------------------------------------
INT CXhcdRing::FindNewDequeueState(UCHAR bSlotId,
                                   UCHAR bEpIndex,
                                   TRANSFER_DATA *pCurTd,
                                   DEQUEUE_STATE *pState) const
{
    LOGICAL_DEVICE *pDev = m_pCHW->m_pLogicalDevices[bSlotId];
    RING *pEpRing;
    LINK_TRB *pTrb;
    ENDPOINT_CONTEXT_DATA *pEpContext;
    UINT64 uAddr;

    if ((pCurTd == 0) || (pState == NULL) || 
        (bEpIndex >= MAX_EP_NUMBER))
    {
        return -XHCD_FAIL;
    }

    pEpRing = pDev->eps[bEpIndex].pRing;

    pState->iNewCycleState = 0;
    
    pState->pNewDeqSeg = pCurTd->pStartSeg;

    if(!pState->pNewDeqSeg)
    {
        return -XHCD_FAIL;
    }

    pEpContext = GetEndpointContext(pDev->pOutContext, bEpIndex);
    if (pEpContext == NULL)
    {
        return -XHCD_FAIL;
    }
    pState->iNewCycleState = 0x1 & pEpContext->u64Deq;

    pState->pNewDeqPtr = pCurTd->pLastTrb;

    pTrb = &pState->pNewDeqPtr->linkTrb;

    if (pTrb == NULL)
    {
        return -XHCD_FAIL;
    }

    if(TRANSFER_BLOCK_TYPE(pTrb->uControl) == TRANSFER_BLOCK_LINK && (pTrb->uControl & LINK_TGG_BIT))
    {
        pState->iNewCycleState = ~(pState->iNewCycleState) & 0x1;
    }

    GetNextTrb(pEpRing, &pState->pNewDeqSeg, &pState->pNewDeqPtr);

    uAddr =(UINT64) TransferVirtualToPhysical(pState->pNewDeqSeg, pState->pNewDeqPtr);
    if (uAddr == NULL)
    {
        return -XHCD_FAIL;
    }

    pEpRing->pDequeue = pState->pNewDeqPtr;

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: IsSpaceEnough
//
// Description: Check if we have enough space for TRBs
//
// Parameters: pRing - pointer to the ring
//
//             uNumTrbs - TRBs number
//
// Returns: TRUE if we have enough space, otherwise FALSE
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsSpaceEnough(RING *pRing, UINT uNumTrbs) const
{
    UINT i;
    TRB *pEnq; 
    RING_SEGMENT *pEnqSeg;

    if (pRing == NULL)
    {
        return FALSE;
    }
   
    pEnq = pRing->pEnqueue;
    pEnqSeg = pRing->pRingSegment;

    if(pEnq == pRing->pDequeue)
    {
        return TRUE;
    }

    for(i = 0; i <= uNumTrbs; i++)
    {
        if(pEnq == pRing->pDequeue)
        {
            return FALSE;
        }
        pEnq++;
        if (IsLastTrb(pRing, pEnqSeg, pEnq))
        {
            pEnq = pEnqSeg->pTrbs;
        }
    }
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: LastTransferBlockInSegment
//
// Description: Check if it is the last trb in the ring segment
//
// Parameters: pRing - pointer to the ring
//
//             pSeg  - pointer to the ring segment
//
//             pTrb  - pointer to the TRB to be chaecked
//
// Returns: TRUE if it is the last TRB, otherwise FALSE 
//----------------------------------------------------------------------------------
BOOL CXhcdRing::LastTransferBlockInSegment(RING *pRing,
                                    RING_SEGMENT *pSeg,
                                    TRB *pTrb) const
{
    if(pRing == m_pEventRing)
    {
        return (pTrb == &pSeg->pTrbs[BLOCKS_IN_SEGMENT]);
    }
    else
    {
        return pTrb->linkTrb.uControl & LINK_TGG_BIT;
    }
}

//----------------------------------------------------------------------------------
// Function: IncrementEnqueue
//
// Description: increment enqueue pointer 
//
// Parameters: pRing - pointer to the ring
//
//             fConsumer - if it is consumer
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::IncrementEnqueue(RING *pRing, BOOL fConsumer) const
{
    UINT uChain;
    TRB *pNext; 

    uChain = pRing->pEnqueue->linkTrb.uControl & TRANSFER_BLOCK_CHAIN;
    pNext = ++(pRing->pEnqueue);

    pRing->uEnqUpdates++;

    if (IsLastTrb(pRing, pRing->pRingSegment, pNext))
    {
        if(!fConsumer)
        {
            if(pRing != m_pEventRing)
            {
                pNext->linkTrb.uControl &= ~TRANSFER_BLOCK_CHAIN;
                pNext->linkTrb.uControl |= uChain;
                if(pNext->linkTrb.uControl & TRANSFER_BLOCK_CYCLE)
                {
                    pNext->linkTrb.uControl &=(UINT) ~TRANSFER_BLOCK_CYCLE;
                }
                else
                {
                    pNext->linkTrb.uControl |=(UINT) TRANSFER_BLOCK_CYCLE;
                }
            }
            if(LastTransferBlockInSegment(pRing, pRing->pRingSegment, pNext))
            {
                if(pRing->uCycleState)
                {
                    pRing->uCycleState = 0;
                }
                else
                {
                    pRing->uCycleState = 1;
                }
            }
        }
        pRing->pEnqueue = pRing->pRingSegment->pTrbs;
    }
}

//----------------------------------------------------------------------------------
// Function: MoveDequeue
//
// Description: move dequeue pointer until it is equal to the enqueue pointer
//
// Parameters: pRing - pointer to the ring
//
//             fConsumer - if it is consumer
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::MoveDequeue(RING *pRing, BOOL fConsumer) const
{
    while (pRing->pDequeue != pRing->pEnqueue) 
    {
        IncrementDequeue(pRing, fConsumer);
    }
}

//----------------------------------------------------------------------------------
// Function: IncrementDequeue
//
// Description: increment dequeue pointer 
//
// Parameters: pRing - pointer to the ring
//
//             fConsumer - if it is consumer
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::IncrementDequeue(RING *pRing, BOOL fConsumer) const
{
    TRB *pNext = ++(pRing->pDequeue);

    pRing->uDeqUpdates++;

    if(IsLastTrb(pRing, pRing->pRingSegment, pNext))
    {
        if(fConsumer && LastTransferBlockInSegment(pRing, pRing->pRingSegment, pNext))
        {
            if(pRing->uCycleState)
            {
                pRing->uCycleState = 0;
            }
            else
            {
                pRing->uCycleState = 1;
            }
        }
        pRing->pDequeue = pRing->pRingSegment->pTrbs;
    }
}

//----------------------------------------------------------------------------------
// Function: InitCompletion
//
// Description: initialize completion state
//
// Parameters: pComplete - completion state
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::InitCompletion(COMPLETION* pComplete) const
{
    pComplete->iStatus = 0;
}

//----------------------------------------------------------------------------------
// Function: Complete
//
// Description: set completion state to completed
//
// Parameters: pComplete - completion state
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::Complete(COMPLETION* pComplete) const
{
    pComplete->iStatus = 1;
}

//----------------------------------------------------------------------------------
// Function: FreeContainerContext
//
// Description: free context container memory
//
// Parameters: pContext pointer to context container
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeContainerContext(CONTAINER_CONTEXT *pContext) const
{
    m_pMem->FreeMemory((PBYTE)pContext->pbBytes,
                        m_pMem->VaToPa((PBYTE)pContext->pbBytes),
                        CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
    free(pContext);
}

//----------------------------------------------------------------------------------
// Function: FreeCommand
//
// Description: free command
//
// Parameters: pCommand - pointer to command
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeCommand(COMMAND *pCommand) const
{
    free(pCommand);
}

//----------------------------------------------------------------------------------
// Function: GetInputControlContext
//
// Description: Get the pointer to the input control context data 
//
// Parameters: pContext pointer to the context container
//
// Returns: pointer to the input control context data 
//----------------------------------------------------------------------------------
INPUT_CONTROL_CONTEXT* CXhcdRing::GetInputControlContext(CONTAINER_CONTEXT *pContext) const
{
    if(pContext->uType != TYPE_INPUT)
    {
        return NULL;
    }
    
    return(INPUT_CONTROL_CONTEXT *)pContext->pbBytes;
}

//----------------------------------------------------------------------------------
// Function: GetLastValidEndpoint
//
// Description: Get the number of the last endpoint aded to the context
//
// Parameters: uAddedContexts - endpoint add context flag
//
// Returns: number of the last endpoint
//----------------------------------------------------------------------------------
UINT CXhcdRing::GetLastValidEndpoint(UINT uAddedContexts) const
{
    INT i = 0;

    while(!(uAddedContexts &(0x1 << i)) &&(i < BITS_IN_DWORD))
    {
        i++;
    }

    return i;
}

//----------------------------------------------------------------------------------
// Function: RingEpDoorbell
//
// Description: ring endpoint doorbell
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex - endoint index
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::RingEpDoorbell(UCHAR bSlotId, UCHAR bEpIndex) const
{
    LOGICAL_ENDPOINT *pEp;
    UINT uEpState;
    UINT uField;
    PCHAR pDbAddr =(CHAR*) &m_pCHW->m_dba->uDoorbell[bSlotId];

    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return;
    }

    pEp = &m_pCHW->m_pLogicalDevices[bSlotId]->eps[bEpIndex];
    if (pEp == NULL)
    {
        return;
    }
    uEpState = pEp->uEpState;

    if(!(uEpState & ENDPOINT_HALT_PENDING) &&
        !(uEpState & SET_DEQ_PENDING) &&
        !(uEpState & ENDPOINT_HALT)) 
    {
        uField = READ_REGISTER_ULONG((PULONG) pDbAddr) & DB_MASK;
        WRITE_REGISTER_ULONG((PULONG) pDbAddr, uField | EPI_TO_DB(bEpIndex));
        READ_REGISTER_ULONG((PULONG) pDbAddr);
    }
}

//----------------------------------------------------------------------------------
// Function: GetEndpointContext
//
// Description: get pointer to the endpoint context data
//
// Parameters: pContext - pointer to the context container
//
//             bEpIndex - endpoint index
//
// Returns: pointer to the endpoint context data
//----------------------------------------------------------------------------------
ENDPOINT_CONTEXT_DATA* CXhcdRing::GetEndpointContext(CONTAINER_CONTEXT *pContext,
                                                     UCHAR bEpIndex) const
{
    bEpIndex++;
    if(pContext->uType == TYPE_INPUT)
    {
        bEpIndex++;
    }

    return(ENDPOINT_CONTEXT_DATA*)
       (pContext->pbBytes +(bEpIndex * m_pCHW->m_uiContextSize));
}

//----------------------------------------------------------------------------------
// Function: PutTransferToRing
//
// Description: Put TRB to the ring and increment enqueue pointer
//
// Parameters: pRing - pointer to the ring
//
//             fConsumer - is this ring of consumer 
//
//             uField1..4 - fields of TRB
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::PutTransferToRing(RING *pRing,
                            BOOL fConsumer,
                            UINT uField1,
                            UINT uField2,
                            UINT uField3,
                            UINT uField4) const
{
    TRANSFER_EVENT_TRB *pTrb;

    pTrb = &pRing->pEnqueue->transferEventTrb;
    if (pTrb == NULL)
    {
        return;
    }
    pTrb->u64Buffer    = uField1;
    pTrb->u64Buffer   |= (UINT64)((UINT64)uField2 << BITS_IN_DWORD);
    pTrb->uTransferLen = uField3;
    pTrb->uFlags       = uField4;

    IncrementEnqueue(pRing, fConsumer);
}

//----------------------------------------------------------------------------------
// Function: IssueCommand
//
// Description: Put the command to the command ring.
//
// Parameters: uField1..4 - fields of TRB
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueCommand(UINT uField1,
                            UINT uField2,
                            UINT uField3,
                            UINT uField4) const
{
    INT iReservedTrbs;

    iReservedTrbs = m_uCmdRingReservedTrbs;
    if(!IsSpaceEnough(m_pCmdRing, iReservedTrbs))
    {
        return -XHCD_NOMEMORY;
    }


    PutTransferToRing(m_pCmdRing,
                FALSE,
                uField1,
                uField2,
                uField3,
                uField4 | m_pCmdRing->uCycleState);

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: IssueSetTRDequeue
//
// Description: Send Set TR dequeue pointer command to the host controller.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex -  endpoint index
//
//             pDeqSeg  - endpoint ring segment
//
//             pDeqPtr  - dequeue pointer to be set
//
//             uCycleState - cycle stet to be set
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueSetTRDequeue(UCHAR bSlotId,
                                 UCHAR bEpIndex,
                                 RING_SEGMENT *pDeqSeg,
                                 TRB *pDeqPtr,
                                 UINT uCycleState) const
{
    UINT64 uAddr;
    UINT uTrbSlotId = SLOT_TO_TRANSFER_BLOCK(bSlotId);
    UINT uTrbEpId = ENDPOINT_TO_TRANSFER_BLOCK(bEpIndex);
    UINT uType = TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_SET_DEQ);

    if (bSlotId == 0 || bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_FAIL;
    }

    uAddr = TransferVirtualToPhysical(pDeqSeg, pDeqPtr);

    if(uAddr == 0)
    {
        return -XHCD_FAIL;
    }

    return IssueCommand(LEAST_32(uAddr) | uCycleState,
                        MOST_32(uAddr),
                        0,
                        uTrbSlotId | uTrbEpId | uType);
}

//----------------------------------------------------------------------------------
// Function: SetNewDequeueState
//
// Description: Set new dequeue state.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex -  endpoint index
//
//             pDeqState  - dequeue state to be set
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
VOID CXhcdRing::SetNewDequeueState(UCHAR bSlotId,
                                   UCHAR bEpIndex,
                                   DEQUEUE_STATE *pDeqState) const
{
    LOGICAL_ENDPOINT *pEp = &m_pCHW->m_pLogicalDevices[bSlotId]->eps[bEpIndex];

    if (pEp == NULL)
    {
        return;
    }

    if(pDeqState->pNewDeqSeg != NULL)
    {
        if (IssueSetTRDequeue(bSlotId,
                      bEpIndex,
                      pDeqState->pNewDeqSeg,
                      pDeqState->pNewDeqPtr,
                      (UINT)pDeqState->iNewCycleState) == XHCD_OK)
        {
             pEp->uEpState |= SET_DEQ_PENDING;
        }
    }
}

//----------------------------------------------------------------------------------
// Function: GetSlotContext
//
// Description: Get pointer to the slot context data.
//
// Parameters: pContext - pointer to the context container
//
// Returns: Pointer to the slot context data.
//----------------------------------------------------------------------------------
SLOT_CONTEXT_DATA* CXhcdRing::GetSlotContext(CONTAINER_CONTEXT *pContext) const
{
    if(pContext->uType == TYPE_DEVICE)
    {
        return(SLOT_CONTEXT_DATA *)pContext->pbBytes;
    }

    return(SLOT_CONTEXT_DATA *)
       (pContext->pbBytes + m_pCHW->m_uiContextSize);
}


//----------------------------------------------------------------------------------
// Function: IssueConfigureEndpoint
//
// Description: Send the configure end;oint command
//
// Parameters: u64InContextPtr - input context pointer
//
//             bSlotId - device slot ID
//
// Returns: TRUE if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueConfigureEndpoint(UINT64 u64InContextPtr,
                                      UCHAR bSlotId) const
{
    return IssueCommand(LEAST_32(u64InContextPtr),
                        MOST_32(u64InContextPtr),
                        0,
                        (TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_ENDPOINT_CFG) | SLOT_TO_TRANSFER_BLOCK(bSlotId)));
}

//----------------------------------------------------------------------------------
// Function: AllocateSegment
//
// Description: Allocate memory for the ring segment TRBs and ring segment container.
//
// Parameters: none
//
// Returns: Pointer to the ring segment container.
//----------------------------------------------------------------------------------
RING_SEGMENT *CXhcdRing::AllocateSegment() const
{
    RING_SEGMENT *pSeg;

    pSeg =(RING_SEGMENT*) malloc(sizeof(RING_SEGMENT));

    if(!pSeg)
    {
        return NULL;
    }
    
    if(!m_pMem->AllocateMemory(DEBUG_PARAM(TEXT("TRBs")) SEGMENT_SIZE,
                                (PUCHAR*)&pSeg->pTrbs,
                                CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK,
                                m_pCHW->m_iPageSize))
    {
        free (pSeg);
        return 0;
    }

    memset(pSeg->pTrbs, 0, SEGMENT_SIZE);
    pSeg->u64Dma =(UINT64)((UINT) m_pMem->VaToPa((PBYTE)pSeg->pTrbs));

    return pSeg;
}

//----------------------------------------------------------------------------------
// Function: LinkSegments
//
// Description: Link segments of the ring.
//
// Parameters: pPrev - pointer to the previous segment
//
//             pNext - pointer to the next segment
//
//             fLinkTrbs - TRBs should be linked
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::LinkSegments(RING_SEGMENT *pPrev,
                             RING_SEGMENT *pNext,
                             BOOL fLinkTrbs) const
{
    UINT uVal;

    if(!pPrev || !pNext)
    {
        return;
    }
    if(fLinkTrbs)
    {
        pPrev->pTrbs[BLOCKS_IN_SEGMENT-1].linkTrb.u64SegmentPtr = pNext->u64Dma;

        uVal = pPrev->pTrbs[BLOCKS_IN_SEGMENT-1].linkTrb.uControl;
        uVal &= ~TRANSFER_BLOCK_TYPE_BITMASK;
        uVal |= TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_LINK);
        pPrev->pTrbs[BLOCKS_IN_SEGMENT-1].linkTrb.uControl = uVal;
    }
}

//----------------------------------------------------------------------------------
// Function: InitializeRingInfo
//
// Description: Initialize ring. 
//
// Parameters: pRing - pointer tothe ring to be initialized
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::InitializeRingInfo(RING *pRing) const
{
    pRing->pEnqueue = pRing->pRingSegment->pTrbs;
    pRing->pDequeue = pRing->pEnqueue;
    pRing->uCycleState = 1;
    pRing->uEnqUpdates = 0;
    pRing->uDeqUpdates = 0;
}

//----------------------------------------------------------------------------------
// Function: FreeSegment
//
// Description: Free segment container and TRBs memory.
//
// Parameters: pSeg - pointer to the ring segment container
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeSegment(RING_SEGMENT *pSeg) const
{
    if(!pSeg)
    {
        return;
    }
    if(pSeg->pTrbs)
    {
        m_pMem->FreeMemory((PBYTE)pSeg->pTrbs,
                            m_pMem->VaToPa((PBYTE)pSeg->pTrbs),
                            CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
        pSeg->pTrbs = NULL;
    }
    free(pSeg);
}

//----------------------------------------------------------------------------------
// Function: FreeRing
//
// Description: Free ring memory.
//
// Parameters: pRing - pointer to the ring.
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeRing(RING *pRing) const
{
    RING_SEGMENT *pRingSegment;

    if(!pRing || !pRing->pRingSegment)
    {
        return;
    }

    pRingSegment = pRing->pRingSegment;
    FreeSegment(pRingSegment);

    pRing->pRingSegment = NULL;
    free(pRing);
}

//----------------------------------------------------------------------------------
// Function: AllocateRing
//
// Description: Allocate and initialize memory for the ring and link segments.
//
// Parameters: fLinkTrbs - segments should be linked.
//
// Returns: Pointer to the allocated ring.
//----------------------------------------------------------------------------------
RING *CXhcdRing::AllocateRing(BOOL fLinkTrbs) const
{

    RING       *pRing;

    pRing =(RING*) malloc(sizeof(RING));
    if(pRing == NULL)
    {
        return pRing;
    }

    pRing->pRingSegment = AllocateSegment();
    if(!pRing->pRingSegment)
    {
        FreeRing(pRing);
        return NULL;
    }

    LinkSegments(pRing->pRingSegment, pRing->pRingSegment, fLinkTrbs);


    if(fLinkTrbs)
    {
        pRing->pRingSegment->pTrbs[BLOCKS_IN_SEGMENT-1].linkTrb.uControl |=(LINK_TGG_BIT);
    }

    InitializeRingInfo(pRing);

    return pRing;
}

//----------------------------------------------------------------------------------
// Function: ReinitializeRing
//
// Description: Initialize memory of the allocated ring and link segments.
//
// Parameters: pRing - pointer to the ring.
//
//             fLinkTrbs - segments should be linked.
//
// Returns: Pointer to the allocated ring.
//----------------------------------------------------------------------------------
BOOL CXhcdRing::ReinitializeRing(RING *pRing, BOOL fLinkTrbs) const
{
    if (pRing == NULL)
    {
        return FALSE;
    }

    memset(pRing->pRingSegment->pTrbs, 0, SEGMENT_SIZE);
    LinkSegments(pRing->pRingSegment, pRing->pRingSegment, fLinkTrbs);
    pRing->pRingSegment->pTrbs[BLOCKS_IN_SEGMENT-1].linkTrb.uControl |=(LINK_TGG_BIT);
    InitializeRingInfo(pRing);

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: InitializeMemory
//
// Description: Initialize memory for the command and event rings.
//
// Parameters: none
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::InitializeMemory() 
{
    if (m_pCmdRing == NULL) 
    {
        m_pCmdRing = AllocateRing(TRUE);
        if(m_pCmdRing == NULL)
        {
            return -XHCD_FAIL;
        }
    } 
    else 
    {
        ReinitializeRing(m_pCmdRing, TRUE);
    }

    if (m_pEventRing == NULL) 
    {
        m_pEventRing = AllocateRing(FALSE);
        if(m_pEventRing == NULL)
        {
            return -XHCD_FAIL;
        }
    } 
    else 
    {
        ReinitializeRing(m_pEventRing, FALSE);
    }
    
    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: TransferVirtualToPhysical
//
// Description: Convert virtual address of TRB to physical.
//
// Parameters: pSeg - pointer to the ring segment
//
//             pTrb - virtual adres of the TRB
//
// Returns: Physical address of the TRB.
//----------------------------------------------------------------------------------
UINT64 CXhcdRing::TransferVirtualToPhysical(RING_SEGMENT *pSeg, TRB *pTrb)
{
    UINT64 u64SegmentOffset;

    if(!pSeg || !pTrb || pTrb < pSeg->pTrbs)
    {
        return 0;
    }

    u64SegmentOffset =(UINT64)(pTrb - pSeg->pTrbs);

    if(u64SegmentOffset > BLOCKS_IN_SEGMENT)
    {
        return 0;
    }
    return pSeg->u64Dma + (u64SegmentOffset * sizeof(*pTrb));
}

//----------------------------------------------------------------------------------
// Function: IssueSlotControl
//
// Description:  Send slot enable/disable comand to the host controller
//
// Parameters:  uTrbType - TRB typy (disable/enable)
//
//              bSlotId - ddevice slot ID
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueSlotControl(UINT uTrbType, UCHAR bSlotId) const
{
    return IssueCommand(0,
                        0,
                        0,
                        TRANSFER_BLOCK_TYPE(uTrbType) | SLOT_TO_TRANSFER_BLOCK(bSlotId));
}

//----------------------------------------------------------------------------------
// Function: AllocateContainerContext
//
// Description: Allocate context container memory.
//
// Parameters: iType - context type.
//
// Returns: Pointer to the context container.
//----------------------------------------------------------------------------------
CONTAINER_CONTEXT* CXhcdRing::AllocateContainerContext(INT iType) const
{
    CONTAINER_CONTEXT* pContext;

    if((iType != TYPE_DEVICE) &&(iType != TYPE_INPUT))
    {
        return NULL;
    }

    pContext =(CONTAINER_CONTEXT*) malloc(sizeof(CONTAINER_CONTEXT));

    if(!pContext)
    {
        return NULL;
    }

    pContext->uType = iType;
    if(HOST_CONTROLLER_64_CONTEXT(m_pCHW->m_uHccParams))
    {
        pContext->iSize = HOST_CONTROLLER_64_CONTEXT_SIZE;
    }
    else
    {
        pContext->iSize = HCC_32BYTE_CONTEXT_SIZE;
    }

    if(iType == TYPE_INPUT)
    {
        pContext->iSize += m_pCHW->m_uiContextSize;
    }

    if(!m_pMem->AllocateMemory(DEBUG_PARAM(TEXT("Context bytes"))(pContext->iSize),
                                (PUCHAR*)&pContext->pbBytes,
                                CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK,
                                PAGE_64K_SIZE))
    {
        free (pContext);
        return NULL;
    }

    memset(pContext->pbBytes, 0, pContext->iSize);

    pContext->u64Dma = m_pMem->VaToPa((PUCHAR)pContext->pbBytes);
    return pContext;
}

//----------------------------------------------------------------------------------
// Function: FreeLogicalDevice
//
// Description: Free memory allocated for the logical device.
//
// Parameters: bSlotId - device slot ID
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeLogicalDevice(UCHAR bSlotId) const
{
    LOGICAL_DEVICE *pDev;
    INT i;

    if(bSlotId == 0)
    {
        return;
    }

    m_pCHW->m_pDcbaa->u64DevContextPtrs[bSlotId] = 0;

    pDev = m_pCHW->m_pLogicalDevices[bSlotId];
    if(!pDev)
    {
        return;
    }

    for(i = 0; i < MAX_EP_NUMBER; i++)
    {
        if(pDev->eps[i].pRing)
        {
            FreeRing(pDev->eps[i].pRing);
        }
    }

    if(pDev->pInContext)
    {
        FreeContainerContext(pDev->pInContext);
    }
    if(pDev->pOutContext)
    {
        FreeContainerContext(pDev->pOutContext);
    }

    free(m_pCHW->m_pLogicalDevices[bSlotId]);

    m_pCHW->m_pLogicalDevices[bSlotId] = NULL;
}

//----------------------------------------------------------------------------------
// Function: PrepareRing
//
// Description: Check if the ring is ready for the transfer
//
// Parameters: pEpRing - endpoint ring
//
//             uEpState - endpoint current state
//
//             uNumTrbs - number of TRBs to be transferred
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::PrepareRing(RING *pEpRing, UINT uEpState, UINT uNumTrbs) const
{
    switch(uEpState)
    {
    case ENDPOINT_STATE_DISABLED:
        return -XHCD_EP_STATE_ERROR;

    case ENDPOINT_STATE_ERROR:
        return -XHCD_INVALID;

    case ENDPOINT_STATE_HALTED:
    case ENDPOINT_STATE_STOPPED:
    case ENDPOINT_STATE_RUNNING:
        break;

    default:
        DEBUGMSG(ZONE_TRANSFER && ZONE_VERBOSE,
            (TEXT("unknown ERROR\n")));
        return -XHCD_INVALID;
    }

    if(!IsSpaceEnough(pEpRing, uNumTrbs))
    {
        return -XHCD_NOMEMORY;
    }

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: RingTransferDoorbell
//
// Description: Ring endpoint for the transfer.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex -  endpoint index
//
//             iStartCycle - cycle state
//
//             pStartTrb - first TRB of transfer
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::RingTransferDoorbell(UCHAR bSlotId,
                                UCHAR bEpIndex,
                                INT iStartCycle,
                                NORMAL_TRB *pStartTrb) const
{
    pStartTrb->dwCycleBit = iStartCycle;
    RingEpDoorbell(bSlotId, bEpIndex);
}

//----------------------------------------------------------------------------------
// Function: InitializeLogicalDevice
//
// Description: Initialize logical device data.
//
// Parameters: bSlotId - device slot ID
//          
//             bSpeed - device speed
//
//             uPortId - port number
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::InitializeLogicalDevice(UCHAR bSlotId, UCHAR bSpeed, UINT uPortId, UCHAR bRootHubPort, UCHAR bHubSID, UINT uDevRoute) const
{
    LOGICAL_DEVICE *pDev;
    LOGICAL_DEVICE *pParentDev;
    ENDPOINT_CONTEXT_DATA *pEp0Context;
    SLOT_CONTEXT_DATA    *pSlotContext;
    SLOT_CONTEXT_DATA    *pParentSlot;
    INPUT_CONTROL_CONTEXT *pCtrlContext;

    if(bSlotId == 0)
    {
        return -XHCD_INVALID;
    }

    pDev = m_pCHW->m_pLogicalDevices[bSlotId];

    pEp0Context = GetEndpointContext(pDev->pInContext, 0);
    pCtrlContext = GetInputControlContext(pDev->pInContext);
    pSlotContext = GetSlotContext(pDev->pInContext);
    if(pEp0Context == NULL ||
       pCtrlContext == NULL ||
       pSlotContext == NULL)
    {
        return -XHCD_INVALID;
    }

    pCtrlContext->uAddFlags = SLOT | ENDPOINT_ZERO;

    pSlotContext->dwContextEntries = 1;

    switch(bSpeed)
    {
    case USB_SUPER_SPEED:
        pSlotContext->dwSpeed = USB_SUPER_SPEED;
        break;

    case USB_HIGH_SPEED:
        pSlotContext->dwSpeed = USB_HIGH_SPEED;
        break;

    case USB_FULL_SPEED:
        pSlotContext->dwSpeed = USB_FULL_SPEED;
        break;

    case USB_LOW_SPEED:
        pSlotContext->dwSpeed = USB_LOW_SPEED;
        break;

    default:
        return -XHCD_INVALID;
    }

    pDev->bPortId = uPortId;
    pDev->bDeviceSpeed = bSpeed;

    if (bHubSID > 0) {
       pSlotContext->dwRouteString = uDevRoute & 0xFFFFF;
       pSlotContext->dwRootHubPort = bRootHubPort;
       pParentDev = m_pCHW->m_pLogicalDevices[bHubSID];
       pParentSlot = GetSlotContext(pParentDev->pInContext);
       if ((pParentDev->bDeviceSpeed == USB_LOW_SPEED) ||(pParentDev->bDeviceSpeed == USB_FULL_SPEED)){
           pSlotContext->dwTtInfoHubSlotId = pParentSlot->dwTtInfoHubSlotId;
           pSlotContext->dwTtInfoPortNumb =  pParentSlot->dwTtInfoPortNumb;
       }
       if (((bSpeed==USB_LOW_SPEED) || (bSpeed==USB_FULL_SPEED))
           &&(pParentDev->bDeviceSpeed == USB_HIGH_SPEED)) {
           pSlotContext->dwTtInfoHubSlotId = bHubSID;
           pSlotContext->dwTtInfoPortNumb =  uPortId;
           pSlotContext->dwMtt = pParentSlot->dwMtt;
       }
    }
    else
    {
        pSlotContext->dwRootHubPort = uPortId;
    }

    pEp0Context->dwEPType = ENDPOINT_CTRL;

    switch(bSpeed)
    {
    case USB_SUPER_SPEED:
        pEp0Context->dwMaxPacketSize = MAX_PACKET_SS;
        break;

    case USB_HIGH_SPEED:
        pEp0Context->dwMaxPacketSize = MAX_PACKET_HS;
        break;

    case USB_LOW_SPEED:
    case USB_FULL_SPEED:
        pEp0Context->dwMaxPacketSize = MAX_PACKET_LS;
        break;

    default:
        return -XHCD_INVALID;
    }

    pEp0Context->dwMaxBurstSize = 0;
    pEp0Context->dwCErr = 3;

    pEp0Context->u64Deq = pDev->eps[0].pRing->pRingSegment->u64Dma;
    pEp0Context->u64Deq |= pDev->eps[0].pRing->uCycleState;

    return 0;
}

//----------------------------------------------------------------------------------
// Function: IssueAddressDevice
//
// Description: Put Address Device command to the ring.
//
// Parameters: u64InContextPtr - physical address of the input context 
//
//             bSlotId - device slot ID
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueAddressDevice(UINT64 u64InContextPtr, UCHAR bSlotId) const
{

    return IssueCommand(LEAST_32(u64InContextPtr),
                        MOST_32(u64InContextPtr),
                        0,
                        TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_ADDRESS) | SLOT_TO_TRANSFER_BLOCK(bSlotId));
}

//----------------------------------------------------------------------------------
// Function: IssueEvaluateContext
//
// Description: Put Evaluate Context command to the ring.
//
// Parameters: u64InContextPtr - physical address of the input context 
//
//             bSlotId - device slot ID
//
// Returns: XHCD_OK if success, FALSE otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueEvaluateContext(UINT64 u64InContextPtr, UCHAR bSlotId) const
{
    return IssueCommand(LEAST_32(u64InContextPtr),
                        MOST_32(u64InContextPtr),
                        0,
                        TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_EVALUATE_CTX) | SLOT_TO_TRANSFER_BLOCK(bSlotId));
}
//----------------------------------------------------------------------------------
// Function: GetTransferLeft
//
// Description: Get TD size contents of Normal TRB
//
// Parameters: uRemainder - TRB remainder
//
// Returns: TD size contents of Normal TRB
//----------------------------------------------------------------------------------
UINT CXhcdRing::GetTransferLeft(UINT uRemainder)
{
    UINT ShiftedValue = uRemainder >> REMAINDER_SHIFT;


    if(ShiftedValue >= REMAINDER_MAX_SHIFT) 
    {
        return REMAINDER_MAX_SHIFT << REMAINDER_BIT_START;
    }
    else
    {
        return(uRemainder >> REMAINDER_SHIFT) << REMAINDER_BIT_START;
    }
}


//----------------------------------------------------------------------------------
// Function: IsTrbInSement
//
// Description: Check if event dma address is correspondent to
//              the transfer data.
//
// Parameters: pStartSeg - endpoint transfer ring segment
//
//             pDequeueTrb - dequeue address of event ring
//
//             pTrb - transfer data to be checked
//
//             u64EventDma - dma address of checked event
//
// Returns: TRUE if dma address is correspondent to
//          the transfer data, else FALSE
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsTrbInSement(RING_SEGMENT *pStartSeg,
                              TRB *pDequeueTrb,
                              TRB *pTrb,
                              UINT64 u64EventDma) const
{
    UINT64 u64DequeueDma;
    UINT64 u64TrbDma;

    u64DequeueDma =(UINT64)TransferVirtualToPhysical(pStartSeg, pDequeueTrb);

    if(u64DequeueDma == 0)
    {
        return 0;
    }

    u64TrbDma = (UINT64) TransferVirtualToPhysical(pStartSeg, pTrb);
    if(u64TrbDma !=  0)
    {
        if(u64DequeueDma <= u64TrbDma)
        {
            if(u64EventDma >= u64DequeueDma && u64EventDma <= u64TrbDma)
            {
                return TRUE;
            }
        }
        else
        {
            UINT64 u64EndSegDma;

            u64EndSegDma = pStartSeg->u64Dma + BLOCKS_IN_SEGMENT * sizeof(*pTrb);
            if((u64EventDma >= u64DequeueDma &&
                u64EventDma <= u64EndSegDma) ||
               (u64EventDma >= pStartSeg->u64Dma &&
                u64EventDma <= u64TrbDma))
            {
                return TRUE;
            }
        }
        return FALSE;
    }
    else
    {
        return FALSE;
    }

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: IsControlEndpoint
//
// Description: Check if it is control endpoint.
//
// Parameters: bEndpointType - endpoint type
//
// Returns: true if the endpoint is control, otherwise it returns false
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsControlEndpoint(UCHAR bEndpointType) const
{
    return(TYPE_CONTROL == bEndpointType);
}

//----------------------------------------------------------------------------------
// Function: IsIsochronousEndpoint
//
// Description: Check if it is isochronous endpoint.
//
// Parameters: bEndpointType - endpoint type
//
// Returns: true if the endpoint is isochronous, otherwise it returns false
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsIsochronousEndpoint(UCHAR bEndpointType) const
{
    return(TYPE_ISOCHRONOUS == bEndpointType);
}

//----------------------------------------------------------------------------------
// Function: IsInterruptEndpoint
//
// Description: Check if it is interrupt endpoint.
//
// Parameters: bEndpointType - endpoint type
//
// Returns: true if the endpoint is interrupt, otherwise it returns false
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsInterruptEndpoint(UCHAR bEndpointType) const
{
    return(TYPE_INTERRUPT == bEndpointType);
}

//----------------------------------------------------------------------------------
// Function: IsBulkEndpoint
//
// Description: Check if it is bulk endpoint.
//
// Parameters: bEndpointType - endpoint type
//
// Returns: true if the endpoint is bulk, otherwise false.
//----------------------------------------------------------------------------------
BOOL CXhcdRing::IsBulkEndpoint(UCHAR bEndpointType) const
{
    return(TYPE_BULK == bEndpointType);
}

//----------------------------------------------------------------------------------
// Function: CleanupHaltedEndpoint
//
// Description: Reset halted endpoint and set new dequeue state for
//              stalled ring.
//
// Parameters: bSlotId - device slot ID
//
//             bEpAddress - endpoint address
//
//             pTd - stopped transfer data
//
//             pEventTrb - halted event TRB
//
// Returns: XHCD_OK if endpoint successfully initialized, else <0
//----------------------------------------------------------------------------------
INT CXhcdRing::CleanupHaltedEndpoint(UCHAR bSlotId,
                                      UCHAR bEpAddress,
                                      TRANSFER_DATA *pTd,
                                      TRB *pEventTrb) const
{
    LOGICAL_ENDPOINT *pEp;
    UCHAR bEpIndex;
    INT iRet = 0;
    LOGICAL_DEVICE *pVirtDev;

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];

    if(pVirtDev == NULL)
    {
        return -XHCD_FAIL;
    }

    bEpIndex = GetEndpointIndex(bEpAddress);
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_FAIL;
    }
    pEp = &pVirtDev->eps [bEpIndex];
    if (pEp == NULL)
    {
        return -XHCD_FAIL;
    }

    pEp->uEpState |= ENDPOINT_HALT;
    if(pTd != NULL)
    {
        pEp->pStoppedTd = pTd;
    }

    if(pEventTrb != NULL)
    {
        pEp->pStoppedTrb = pEventTrb;
    }

    iRet = IssueResetEndpoint(bSlotId, bEpIndex);
    if(iRet != XHCD_OK)
    {
        return iRet;
    }
    CleanupStalledRing(bSlotId, bEpIndex);
    m_pCHW->RingCommandDoorbell();
    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: CountAbortTransfers
//
// Description: Count aborted transfers and reset driver if aborts counter > 5
//
// Parameters: bSlotId - device slot id
//
//             bEpAddress - endpoint address
//
// Returns: TRUE if bAbortCounter > MAX_ABORTS_NUMBER, else FALSE
//----------------------------------------------------------------------------------
BOOL CXhcdRing::CountAbortTransfers(UCHAR bSlotId,
                                    UCHAR bEpAddress) const
{
    LOGICAL_ENDPOINT *pEp;
    UCHAR bEpIndex;
    LOGICAL_DEVICE *pVirtDev;

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];

    if (bSlotId == 0 || pVirtDev == NULL)
    {
        return FALSE;
    }

    bEpIndex = GetEndpointIndex(bEpAddress);
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return FALSE;
    }
    pEp = &pVirtDev->eps [bEpIndex];

    if (pEp != NULL) 
    {
        pEp->bAbortCounter++;

        if (pEp->bAbortCounter > MAX_ABORTS_NUMBER) 
        {
            pEp->bAbortCounter = 0;
            return TRUE;
        }
    }

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: CleanupStalledRing
//
// Description: Find new dequeue state for stalled ring and issue
//              Set TR dequeue command.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex - endpoint index
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::CleanupStalledRing(UCHAR bSlotId, UCHAR bEpIndex) const
{
    DEQUEUE_STATE deqState;
    LOGICAL_ENDPOINT *pEp;
    CQTransfer* pQTransfer = NULL;

    if (bSlotId == 0 || bEpIndex >= MAX_EP_NUMBER) 
    {
        return;
    }

    pEp = &m_pCHW->m_pLogicalDevices [bSlotId]->eps[bEpIndex];
    
    if(pEp == NULL)
    {
        return;
    }

    if(pEp->pStoppedTd != NULL)
    {
        pQTransfer = pEp->pStoppedTd->pQTransfer;
    }

    if(FindNewDequeueState(bSlotId,
                           bEpIndex,
                           pEp->pStoppedTd,
                           &deqState) == XHCD_OK)
    {
        SetNewDequeueState(bSlotId, bEpIndex, &deqState);
    }

    pEp->pStoppedTd = NULL;

    if(pQTransfer)
    {
        pQTransfer->SetStatus(STATUS_CQT_CANCELED);
    }
}

//----------------------------------------------------------------------------------
// Description: Add endpoint to the input control context,
//              modify slot context,
//              initialize endpoint ring and context
//
// Parameters: bSlotId - slot ID
//
//             peptDescr - endpoint descriptor
//
// Returns: TRUE if endpoint successfully added, else FALSE
//----------------------------------------------------------------------------------
INT CXhcdRing::AddEndpoint(UCHAR bSlotId, USB_ENDPOINT_DESCRIPTOR* peptDescr) const
{
    CONTAINER_CONTEXT *pInContext, *pOutContext;
    SLOT_CONTEXT_DATA *pSlotContext;
    INPUT_CONTROL_CONTEXT *pCtrlContext;

    UINT uAddedContexts;
    UINT uLastContext;
    INT iRet = XHCD_OK;
    LOGICAL_DEVICE *pVirtDev;
    UCHAR bSpeed;
    UCHAR bEndpointType;
    
    bEndpointType = peptDescr->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    
    uAddedContexts = GetEndpointFlag(peptDescr->bEndpointAddress);

    //this is reserved flags
    if(uAddedContexts == SLOT || uAddedContexts == ENDPOINT_ZERO)
    {
        return -XHCD_INVALID;
    }
    
    uLastContext = GetLastValidEndpoint(uAddedContexts);

    if(bSlotId == 0 || !m_pCHW->m_pLogicalDevices || !m_pCHW->m_pLogicalDevices [bSlotId])
    {
        DEBUGMSG(ZONE_WARNING && ZONE_VERBOSE,
            (TEXT("%s - wrong address device\n"),
            bSlotId));
        return -XHCD_INVALID;
    }

    pVirtDev = m_pCHW->m_pLogicalDevices[bSlotId];
    pInContext = pVirtDev->pInContext;
    pOutContext = pVirtDev->pOutContext;
    pCtrlContext = GetInputControlContext(pInContext);
    if(pCtrlContext == NULL)
    {
        return -XHCD_INVALID;
    }

    if(pCtrlContext->uAddFlags & GetEndpointFlag(peptDescr->bEndpointAddress))
    {
        return iRet;
    }

    bSpeed = pVirtDev->bDeviceSpeed;

    if(InitializeEndpoint(pVirtDev, bEndpointType, bSpeed, peptDescr) < 0)
    {
        return -XHCD_NOMEMORY;
    }

    pCtrlContext->uAddFlags |= uAddedContexts;

    pSlotContext = GetSlotContext(pInContext);

    if (pSlotContext == NULL)
    {
        return -XHCD_FAIL;
    }

    if(pSlotContext->dwContextEntries < uLastContext)
    {
        pSlotContext->dwContextEntries = uLastContext;
    }
 
    return iRet;
}

//----------------------------------------------------------------------------------
// Function: GetEndpointFlag
//
// Description: Get endpoint flag which will be used for input control context.
//
// Parameters: bEndpointAddress - endpoint address
//
// Returns: Endpoint flag.
//----------------------------------------------------------------------------------
UCHAR CXhcdRing::GetEndpointFlag(UCHAR bEndpointAddress) const
{
    return 1 <<(GetEndpointIndex(bEndpointAddress) + 1);
}

//----------------------------------------------------------------------------------
// Function: GetEndpointIndex
//
// Description: Get endpoint index by endpoint address.
//
// Parameters: bEndpointAddress - endpoint address.
//
// Returns: endpoint index
//----------------------------------------------------------------------------------
UCHAR CXhcdRing::GetEndpointIndex(UCHAR bEndpointAddress)
{
    UCHAR bIndex;

    if(bEndpointAddress == 0)
    {
        bIndex = 0;
    }
    else
    {
        UCHAR   bIncr = 0;

        if(GetEndpointDirection(bEndpointAddress))
        {
            bIncr = 1;
        }

        bIndex = (EndpointNumber(bEndpointAddress) * 2) + bIncr - 1;
    }
    return bIndex;
}

//----------------------------------------------------------------------------------
// Function: GetEndpointAddressByIndex
//
// Description: Get endpoint address by endpoint index.
//
// Parameters: bEpIndex  - endpoint index
//
// Returns: endpoint address
//----------------------------------------------------------------------------------
UCHAR CXhcdRing::GetEndpointAddressByIndex(UCHAR bEpIndex) const
{
    UCHAR bAddress;

    if(bEpIndex == 0)
    {
        bAddress = 0;
    }
    else
    {
        UCHAR   bIncr = 0x80;

        if(bEpIndex % 2)
        {
            bIncr = 0x0;
        }

        bAddress =(UCHAR)((bEpIndex + 1) / 2 + bIncr);
    }
    return bAddress;
}

//----------------------------------------------------------------------------------
// Function: InitializeEndpoint
//
// Description: Initialize endpoint ring and context 
//
// Parameters: pVirtDev - poinnter to the correspondent logical device
//
//             bEndpointType - endpoint type
//
//             uSpeed        - endpoint speed      
//
//             peptDescr     - ndpoint descriptor
//
// Returns: XHCD_OK if endpoint successfully initialized, else <0
//----------------------------------------------------------------------------------
INT CXhcdRing::InitializeEndpoint(LOGICAL_DEVICE *pVirtDev,
                                UCHAR bEndpointType,
                                UINT uSpeed,
                                USB_ENDPOINT_DESCRIPTOR* peptDescr) const
{    
    ENDPOINT_CONTEXT_DATA *pEndpointContext;
    RING *pEpRing;
    UINT uMaxPacket;
    UINT uMaxBurst;
    UCHAR bEpIndex;

    bEpIndex = GetEndpointIndex(peptDescr->bEndpointAddress);

    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_FAIL;
    }

    pEndpointContext = GetEndpointContext(pVirtDev->pInContext, bEpIndex);
    if (pEndpointContext == NULL)
    {
        return -XHCD_FAIL;
    }

    pVirtDev->eps[bEpIndex].pNewRing = AllocateRing(TRUE);
    if(!pVirtDev->eps[bEpIndex].pNewRing)
    {
        return -XHCD_NOMEMORY;
    }
    pEpRing = pVirtDev->eps[bEpIndex].pNewRing;
    pEndpointContext->u64Deq =(UINT64) pEpRing->pRingSegment->u64Dma | pEpRing->uCycleState;

    pEndpointContext->dwInterval = GetEndpointInterval(bEndpointType,
                                                uSpeed,
                                                peptDescr);

    if(!IsIsochronousEndpoint(bEndpointType))
    {
        pEndpointContext->dwCErr = MAX_ISOC_ERRORS;
    }
    else
    {
        pEndpointContext->dwCErr = MAX_BULK_ERRORS;
    }

    pEndpointContext->dwEPType = GetEndpointType(bEndpointType,
                                      peptDescr->bEndpointAddress);

    switch(uSpeed)
    {
    case USB_SUPER_SPEED:
        uMaxPacket = peptDescr->wMaxPacketSize;
        pEndpointContext->dwMaxPacketSize = uMaxPacket;
        uMaxPacket = 0;
        pEndpointContext->dwMaxBurstSize = uMaxPacket;
        break;

    case USB_HIGH_SPEED:
        if(IsIsochronousEndpoint(bEndpointType) ||
                IsInterruptEndpoint(bEndpointType))
        {
            uMaxBurst =(peptDescr->wMaxPacketSize & 0x1800) >> 11;
            pEndpointContext->dwMaxBurstSize = uMaxBurst;
        }

    case USB_FULL_SPEED:
    case USB_LOW_SPEED:
        uMaxPacket = peptDescr->wMaxPacketSize & 0x3ff;
        pEndpointContext->dwMaxPacketSize = uMaxPacket;
        break;

    default:
        return -XHCD_FAIL;
    }

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: IssueResetEndpoint
//
// Description: Put Reset Endpoint command to the ring.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex - endpoint index
//
// Returns: XHCD_OK if successful, < 0 if error occurs.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueResetEndpoint(UCHAR bSlotId, UCHAR bEpIndex) const
{
    UINT uTrbSlotId = SLOT_TO_TRANSFER_BLOCK(bSlotId);
    UINT uTrbEpId = ENDPOINT_TO_TRANSFER_BLOCK(bEpIndex);
    UINT uType = TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_RESET_ENDPOINT);

    if (bSlotId == 0 || bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_FAIL;
    }

    return IssueCommand(0, 0, 0, uTrbSlotId | uTrbEpId | uType);
}

//----------------------------------------------------------------------------------
// Function: IssueResetDevice
//
// Description: Put Reset Device command to the ring.
//
// Parameters: uSlotId - device slot ID
//
// Returns: XHCD_OK if successful, < 0 if error occurs.
//----------------------------------------------------------------------------------
INT CXhcdRing::IssueResetDevice(UCHAR bSlotId) const
{
    return IssueCommand(0, 0, 0, TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_RESET_DEVICE) | SLOT_TO_TRANSFER_BLOCK(bSlotId));
}

//----------------------------------------------------------------------------------
// Function: EndpointNumber
//
// Description: Convert endpoint address to the number.
//
// Parameters: uEndpointAddress - endpoint address
//
// Returns: number: 0 to 15.
//----------------------------------------------------------------------------------
INT CXhcdRing::EndpointNumber(UCHAR uEndpointAddress)
{
    return uEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

//----------------------------------------------------------------------------------
// Function: GetEndpointDirection
//
// Description: Get endpoint direction from endpoint address.
//
// Parameters: uEndpointAddress - endpoint address
//
// Returns: TRUE if IN direction
//----------------------------------------------------------------------------------
BOOL CXhcdRing::GetEndpointDirection(UCHAR uEndpointAddress)
{
    return((uEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN);
}

//----------------------------------------------------------------------------------
// Function: GetEndpointInterval
//
// Description: Get interrupt endpoint interval.
//
// Parameters: bEndpointType - endpoint type
//
//             uSpeed - endpoint speed
//
//             peptDescr - pointer to the endpoint description
//
// Returns: endpoint interval
//----------------------------------------------------------------------------------
UINT CXhcdRing::GetEndpointInterval(UCHAR bEndpointType,
                                    UINT uSpeed,
                                    USB_ENDPOINT_DESCRIPTOR* peptDescr) const
{
    UINT uInterval = 0;

    switch(uSpeed)
    {
    case USB_HIGH_SPEED:

        if(IsControlEndpoint(bEndpointType) ||
                IsBulkEndpoint(bEndpointType))
        {
            uInterval = ParseMicroframeInterval(peptDescr->bInterval);
            break;
        }

    case USB_SUPER_SPEED:
        if(IsInterruptEndpoint(bEndpointType) ||
                IsIsochronousEndpoint(bEndpointType))
        {
            if(peptDescr->bInterval == 0)
            {
                uInterval = 0;
            }
            else
            {
                uInterval = peptDescr->bInterval - 1;
            }
            if(uInterval > MAX_SS_INTERVAL)
            {
                uInterval = MAX_SS_INTERVAL;
            }
        }
        break;

    case USB_FULL_SPEED:
    case USB_LOW_SPEED:
        if(IsInterruptEndpoint(bEndpointType) ||
                IsIsochronousEndpoint(bEndpointType))
        {
            uInterval = ParseMicroframeInterval(8*peptDescr->bInterval);
            if(uInterval > 11)
            {
                uInterval = 11;
            }
            if(uInterval < 3){
                uInterval = 3;
            }
        }
        break;

    default:
        DEBUGMSG(ZONE_WARNING,(TEXT("wrong speed\n")));
    }

    return uInterval;
}

//----------------------------------------------------------------------------------
// Function: GetEndpointType
//
// Description: Get endpoint type which will be placed to endpoint context.
//
// Parameters: uEndpointType - endpoint type taken from endpoint descriptor
//
//             uEndpointAddress - endpoint address
//
// Returns: endpoint type which will be placed to endpoint context
//----------------------------------------------------------------------------------
UINT CXhcdRing::GetEndpointType(UINT uEndpointType, UCHAR uEndpointAddress) const
{
    UINT uType;
    BOOL iIn;

    iIn = GetEndpointDirection(uEndpointAddress);
    if(IsControlEndpoint(uEndpointType))
    {
        uType = ENDPOINT_CTRL;
    }
    else if(IsBulkEndpoint(uEndpointType))
    {
        if(iIn)
        {
            uType = ENDPOINT_BULK_IN;
        }
        else
        {
            uType = ENDPOINT_BULK_OUT;
        }
    }
    else if(IsIsochronousEndpoint(uEndpointType))
    {
        if(iIn)
        {
            uType = ENDPOINT_ISOC_IN;
        }
        else
        {
            uType = ENDPOINT_ISOC_OUT;
        }
    }
    else if(IsInterruptEndpoint(uEndpointType))
    {
        if(iIn)
        {
            uType = ENDPOINT_INT_IN;
        }
        else
        {
            uType = ENDPOINT_INT_OUT;
        }
    }
    else
    {
        uType = 0;
    }

    return uType;
}

//----------------------------------------------------------------------------------
// Function: ParseMicroframeInterval
//
// Description: Find correct interavl value.
//
// Parameters: uInterval - interval taken from endpoint descriptor
//
// Returns: corrected interval
//----------------------------------------------------------------------------------
UINT CXhcdRing::ParseMicroframeInterval(UINT uInterval) const
{
    UINT uEpInterval = 0;
    UINT uPower = 1;

    while(uPower < uInterval)
    {
        uPower <<= 1;
        uEpInterval++;
    }

    if(uEpInterval != 0)
    {
        uEpInterval--;
    }

    return uEpInterval;
}

//----------------------------------------------------------------------------------
// Function: DoConfigureEndpoint
//
// Description: Modify context, send configure endpoint command, reset context and
//              set up new endpoint ring.
//
// Parameters: bSlotId - device slot ID
//
// Returns: Returns XHCD_OK if configure endpoint command passed, < 0 otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::DoConfigureEndpoint(UCHAR bSlotId, USB_DEVICE_DESCRIPTOR* pDevDesc, USB_HUB_DESCRIPTOR* pHubDesc) const
{
    INT i;
    INT iRet = 0;
    LOGICAL_DEVICE* pVirtDev;
    INPUT_CONTROL_CONTEXT *pCtrlContext;
    SLOT_CONTEXT_DATA *pSlotContext;

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];
    
    pCtrlContext = GetInputControlContext(pVirtDev->pInContext);
    if(pCtrlContext == NULL)
    {
        return -XHCD_INVALID;
    }
    pCtrlContext->uAddFlags |= SLOT;
    pCtrlContext->uAddFlags &= ~ENDPOINT_ZERO;
    pCtrlContext->uDropFlags &= ~SLOT;
    pCtrlContext->uDropFlags &= ~ENDPOINT_ZERO;

    pSlotContext = GetSlotContext(pVirtDev->pInContext);
    if (pDevDesc && (pDevDesc->bDeviceClass == USB_DEVICE_CLASS_HUB))
    {
            pSlotContext->dwNumberOfPorts = pHubDesc->bNumberOfPorts;
            pSlotContext->dwHub = 1;
            if (pSlotContext->dwSpeed == USB_HIGH_SPEED) {
                /* Following info will be initialized for USB High Speed HUB */
                pSlotContext->dwTtInfoThinkTime = (pHubDesc->wHubCharacteristics & USB_HUB_CHARACTERISTIC_TT_THINK_TIME_MASK)>> 5;
                pSlotContext->dwMtt = (pDevDesc->bDeviceProtocol == 2) ?  1 : 0;
            }
    }

    iRet = ConfigureEndpoint(bSlotId);
    if(iRet)
    {
        return iRet;
    }

    ResetInContext(pVirtDev);

    for(i = 1; i < MAX_EP_NUMBER; i++)
    {
        if(!pVirtDev->eps[i].pNewRing)
        {
            continue;
        }

        if(pVirtDev->eps[i].pRing)
        {
            FreeEndpointRing(pVirtDev, i);
        }
        pVirtDev->eps[i].pRing = pVirtDev->eps[i].pNewRing;
        pVirtDev->eps[i].pNewRing = NULL;
    }

    return iRet;
}

//----------------------------------------------------------------------------------
// Function: ConfigureEndpoint
//
// Description: Send configure endpoint command and wait for the completion.
//
// Parameters: bSlotId - device slot ID
//
// Returns: Returns XHCD_OK if configure endpoint command passed, < 0 otherwise.
//----------------------------------------------------------------------------------
INT CXhcdRing::ConfigureEndpoint(UCHAR bSlotId) const
{
    INT iRet;
    CONTAINER_CONTEXT *pInContext;
    COMPLETION *pCmdCompletion;
    INT *piCmdStatus;
    LOGICAL_DEVICE *pVirtDev;

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];
    pInContext = pVirtDev->pInContext;
    pCmdCompletion = &pVirtDev->cmdCompletion;
    piCmdStatus =(INT*)&pVirtDev->uCmdStatus;

    iRet = IssueConfigureEndpoint(pInContext->u64Dma, bSlotId);
    if(iRet < 0)
    {
        return -XHCD_NOMEMORY;
    }

    InitCompletion(pCmdCompletion);
    m_pCHW->RingCommandDoorbell();

    if(m_pCHW->WaitForCompletion(pCmdCompletion, USB_CTRL_SET_TIMEOUT) <= 0)
    {
        MoveDequeue(m_pCmdRing, FALSE);
        return -XHCD_TIMEDOUT;
    }

    return ConfigureEndpointResult(piCmdStatus);
}

//----------------------------------------------------------------------------------
// Function: ResetInContext
//
// Description: Reset input context.
//
// Parameters: pVirtDev - pointer to the logical device
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::ResetInContext(LOGICAL_DEVICE* pVirtDev) const
{
    INPUT_CONTROL_CONTEXT *pCtrlContext;
    SLOT_CONTEXT_DATA *pSlotContext;
    INT i;

    pCtrlContext = GetInputControlContext(pVirtDev->pInContext);
    if(pCtrlContext != NULL)
    {
        pCtrlContext->uDropFlags = 0;
        pCtrlContext->uAddFlags = 0;
    }
    pSlotContext = GetSlotContext(pVirtDev->pInContext);
    pSlotContext->dwContextEntries = 1;
    for(i = 1; i < MAX_EP_NUMBER; i++)
    {
        ENDPOINT_CONTEXT_DATA *pEpContext;

        pEpContext = GetEndpointContext(pVirtDev->pInContext, i);
        if (pEpContext != NULL)
        {
            memset(pEpContext, 0, sizeof (ENDPOINT_CONTEXT_DATA));
        }
    }
}

//----------------------------------------------------------------------------------
// Function: FreeEndpointRing
//
// Description: Free endpoint ring memory.
//
// Parameters:  pVirtDev - pointer to the logical device
//
//              bEpIndex - endpoint index
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::FreeEndpointRing(LOGICAL_DEVICE* pVirtDev,
                                            UCHAR bEpIndex) const
{
    if (pVirtDev != NULL && bEpIndex < MAX_EP_NUMBER) 
    {
        FreeRing(pVirtDev->eps [bEpIndex].pRing);
        pVirtDev->eps [bEpIndex].pRing = NULL;
    }
}

//----------------------------------------------------------------------------------
// Function: ConfigureEndpointResult
//
// Description: Check result of configure endpoint command.
//
// Parameters: pCmdStatus - pointer to the command status
//
// Returns: XHCD_OK - if command passed, else < 0
//----------------------------------------------------------------------------------
INT CXhcdRing::ConfigureEndpointResult(INT* pCmdStatus) const
{
    INT iRet;

    switch(*pCmdStatus)
    {
    case STATUS_NERES:
        iRet = -XHCD_NOMEMORY;
        break;

    case STATUS_BANDWITH_ERROR:
        iRet = -XHCD_NOMEMORY;
        break;

    case STATUS_TRANSFER_BLOCK_ERROR:
        iRet = -XHCD_INVALID;
        break;

    case STATUS_INVALID:
        iRet = -XHCD_INVALID;
        break;

    case STATUS_DB_SUCCESS:
        iRet = XHCD_OK;
        break;

    default:
        iRet = -XHCD_INVALID;
        break;
    }
    return iRet;
}



//----------------------------------------------------------------------------------
// Function: PrepareTransfer
//
// Description: This method prepares one transfer for activation.
//
// Parameters: bSlotId - device slot ID
//
//             uEpAddress - endpoint address
//
//             ppTd - pointer to transfer data
//
//             uTransferLength - transfer buffer length
//
//             bEndpointType -  endpoint type
//
// Returns: XHCD_OK if success, else error code
//----------------------------------------------------------------------------------
INT CXhcdRing::PrepareTransfer(UCHAR bSlotId,
                               UINT uEpAddress,
                               TRANSFER_DATA** ppTd,
                               UINT uTransferLength,
                               UCHAR bEndpointType) const
{
    RING *pEpRing;
    INT iNumTrbs;
    SLOT_CONTEXT_DATA *pSlotContext;
    ENDPOINT_CONTEXT_DATA *pEpContext;
    LOGICAL_DEVICE* pVirtDev = NULL;
    INT iRet;
    UINT uRunningTotal;
    UCHAR bEpIndex;

    if (bSlotId == 0)
    {
        return -XHCD_INVALID;
    }
    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];

    if(!pVirtDev)
    {
        return -XHCD_INVALID;
    }

    pSlotContext = GetSlotContext(pVirtDev->pOutContext);
    if (pSlotContext->dwSlotState == DISABLED_SLOT)
    {
        return -XHCD_SLOT_DISABLED;
    }

    bEpIndex = GetEndpointIndex(uEpAddress);
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_INVALID;
    }

    pEpRing = pVirtDev->eps [bEpIndex].pRing;
    
    if(bEndpointType == TYPE_CONTROL)
    {
        iNumTrbs = 2;
        if(uTransferLength > 0)
        {
            iNumTrbs++;
        }
    }
    else
    {
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
    }

    pEpContext = GetEndpointContext(pVirtDev->pOutContext, bEpIndex);

    if(pEpContext->dwEpState == ENDPOINT_STATE_HALTED)
    {
        return -XHCD_EP_STATE_ERROR;
    }

    iRet = PrepareRing(pVirtDev->eps[bEpIndex].pRing,
                       pEpContext->dwEpState,
                       iNumTrbs);

    if(iRet != XHCD_OK)
    {
        return iRet;
    }
    
    *ppTd =(TRANSFER_DATA*) malloc(sizeof(TRANSFER_DATA));
    if(!(*ppTd))
    {
        return -XHCD_NOMEMORY;
    }

   (*ppTd)->uActualLength = 0;
   (*ppTd)->bEndpointType = bEndpointType;
   (*ppTd)->u64Dma = 0; 
   (*ppTd)->pQTransfer = NULL;

   (*ppTd)->pStartSeg = pVirtDev->eps[bEpIndex].pRing->pRingSegment;
   (*ppTd)->pFirstTrb = pVirtDev->eps[bEpIndex].pRing->pEnqueue;

    return iRet;
}

//----------------------------------------------------------------------------------
// Function: ResetEndpoint
//
// Description: Issue reset endpoint command and cleanup endpoint ring.
//
// Parameters: bSlotId - device slot ID
//
//             uEpAddress - endpoint address
//
//             bEndpointType -  endpoint type
//
// Returns: XHCD_OK if success, else error code
//----------------------------------------------------------------------------------
INT CXhcdRing::ResetEndpoint(UCHAR bSlotId,
                             UINT uEpAddress,
                             UCHAR bEndpointType) const
{
    LOGICAL_DEVICE *pVirtDev;
    UCHAR bEpIndex;
    INT iRet = XHCD_OK;

    pVirtDev = m_pCHW->m_pLogicalDevices[bSlotId];

    if(pVirtDev == NULL)
    {
        return -XHCD_INVALID;
    }

    bEpIndex = GetEndpointIndex(uEpAddress);

    if(bEpIndex == 0 || bEpIndex >= MAX_EP_NUMBER || 
       IsControlEndpoint(bEndpointType))
    {
        return -XHCD_INVALID;
    }

    iRet = IssueResetEndpoint(bSlotId, bEpIndex);

    if(iRet == XHCD_OK)
    {
        CleanupStalledRing(bSlotId, bEpIndex);
        m_pCHW->RingCommandDoorbell();
    }

    return iRet;
}

//----------------------------------------------------------------------------------
// Function: DropEndpoint
//
// Description: Drop endpoint from the input control context,
//              reset slot context
//
// Parameters: bSlotId - slot ID
//
//             uEpAddress - endpoint address
//
// Returns: XHCD_OK if endpoint successfully added, else < 0
//----------------------------------------------------------------------------------
INT CXhcdRing::DropEndpoint(UCHAR bSlotId, UINT uEpAddress) const
{
    CONTAINER_CONTEXT *pInContext, *pOutContext;
    INPUT_CONTROL_CONTEXT *pCtrlContext;
    SLOT_CONTEXT_DATA *pSlotContext;
    UINT uLastContext;
    UCHAR bEpIndex;
    ENDPOINT_CONTEXT_DATA *pEpContext;
    UINT uDropFlag;
    UINT uNewAddFlags, uNewDropFlags;
    LOGICAL_DEVICE *pVirtDev;

    if(bSlotId == 0)
    {
        return -XHCD_INVALID;
    }

    uDropFlag = GetEndpointFlag(uEpAddress);
    if(uDropFlag == SLOT || uDropFlag == ENDPOINT_ZERO)
    {
        return -XHCD_DROP_ZERO_ERROR;
    }

    pVirtDev = m_pCHW->m_pLogicalDevices [bSlotId];

    if (pVirtDev == NULL)
    {
        return -XHCD_INVALID;
    }

    pInContext = pVirtDev->pInContext;
    if (pInContext == NULL)
    {
        return -XHCD_INVALID;
    }

    pOutContext = pVirtDev->pOutContext;
    pCtrlContext = GetInputControlContext(pInContext);
    if (pCtrlContext == NULL ||
        pOutContext == NULL )
    {
        return -XHCD_INVALID;
    }

    bEpIndex = GetEndpointIndex(uEpAddress);
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        return -XHCD_INVALID;
    }
    pEpContext = GetEndpointContext(pOutContext, bEpIndex);
    if (pEpContext == NULL) 
    {
        return -XHCD_INVALID;
    }

    if(pEpContext->dwEpState == ENDPOINT_STATE_DISABLED ||
       pCtrlContext->uDropFlags & GetEndpointFlag(uEpAddress))
    {
        return XHCD_OK;
    }

    pCtrlContext->uDropFlags |= uDropFlag;
    uNewDropFlags = pCtrlContext->uDropFlags;

    pCtrlContext->uAddFlags &= ~uDropFlag;
    uNewAddFlags = pCtrlContext->uAddFlags;

    uLastContext = GetLastValidEndpoint(pCtrlContext->uAddFlags);
    pSlotContext = GetSlotContext(pInContext);

    if(pSlotContext->dwContextEntries > uLastContext)
    {
        pSlotContext->dwContextEntries = uLastContext;
    }

    CleanEndpoint(pVirtDev, uEpAddress);

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: CleanEndpoint
//
// Description: reset endpoint context content.
//
// Parameters:  pVirtDev - pointer to the logical device
//
//              uEndpointAddress - endpint address
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcdRing::CleanEndpoint(LOGICAL_DEVICE *pVirtDev,
                                UCHAR uEndpointAddress) const
{
    UCHAR bEpIndex;

    bEpIndex = GetEndpointIndex(uEndpointAddress);
    if (bEpIndex >= MAX_EP_NUMBER)
    {
        ENDPOINT_CONTEXT_DATA *pEpContext;

        pEpContext = GetEndpointContext(pVirtDev->pInContext, bEpIndex);
        if (pEpContext != NULL) 
        {
            memset(pEpContext, 0, sizeof (ENDPOINT_CONTEXT_DATA));
        }
    }
}