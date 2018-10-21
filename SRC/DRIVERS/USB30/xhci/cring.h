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
// File: cring.h - This file contains routines which control the XHCI rings.
//----------------------------------------------------------------------------------

#ifndef CXHCDRING_H
#define CXHCDRING_H

class CPhysMem;
class CHW;
class CXhcdEvenHandler;

class CXhcdRing
{
    friend class CHW;
    friend class CXhcdEventHandler;

public:

    CXhcdRing(IN CHW* pCHW, IN CPhysMem * pCPhysMem);

    ~CXhcdRing();

    CPhysMem * const m_pMem;
    CHW*       const m_pCHW;   

    static UCHAR GetEndpointIndex(UCHAR uEndpointAddress);
    static UINT GetTransferLeft(UINT uRemainder);
    static UINT64  TransferVirtualToPhysical(RING_SEGMENT *pSeg, TRB *pTrb);

private:

    static INT EndpointNumber(UCHAR uEndpointAddress);
    static BOOL GetEndpointDirection(UCHAR uEndpointAddress);

    BOOL IsLastTrb(RING *pRing,
                  RING_SEGMENT *pSeg,
                  TRB *pTrb) const;
    VOID GetNextTrb(RING *pRing,
                    RING_SEGMENT **ppSeg,
                    TRB **ppTrb) const;
    VOID InitCompletion(COMPLETION* pComplete) const;
    VOID Complete(COMPLETION* pComplete) const;
    VOID FreeCommand(COMMAND *pCommand) const;
    VOID FreeContainerContext(CONTAINER_CONTEXT *pContext) const;
    INPUT_CONTROL_CONTEXT* GetInputControlContext(CONTAINER_CONTEXT *pContext) const;
    UINT GetLastValidEndpoint(UINT uAddedContexts) const;
    VOID RingEpDoorbell(UCHAR bSlotId, UCHAR bEpIndex) const;
    VOID SetNewDequeueState(UCHAR bSlotId,
                            UCHAR bEpIndex,
                            DEQUEUE_STATE *pDeqState) const;
    INT IssueSetTRDequeue(UCHAR bSlotId,
                          UCHAR bEpIndex,
                          RING_SEGMENT *pDeqSeg,
                          TRB *pDeqPtr,
                          UINT uCycleState) const;
    INT IssueCommand(UINT uField1,
                     UINT uField2,
                     UINT uField3,
                     UINT uField4) const;
    INT IssueConfigureEndpoint(UINT64 u64InContextPtr,
                                    UCHAR bSlotId) const;
    INT FindNewDequeueState(UCHAR bSlotId,
                            UCHAR bEpIndex,
                            TRANSFER_DATA *pCurTd,
                            DEQUEUE_STATE *pState) const;
    BOOL IsSpaceEnough(RING *pRing, UINT uNumTrbs) const;
    BOOL LastTransferBlockInSegment(RING *pRing,
                            RING_SEGMENT *pSeg,
                            TRB *pTrb) const;
    ENDPOINT_CONTEXT_DATA* GetEndpointContext(CONTAINER_CONTEXT *pContext,
                                              UCHAR bEpIndex) const;
    VOID PutTransferToRing(RING *pRing,
                BOOL fConsumer,
                UINT uField1,
                UINT uField2,
                UINT uField3,
                UINT uField4) const;
    VOID IncrementEnqueue(RING *pRing, BOOL fConsumer) const;
    VOID IncrementDequeue(RING *pRing, BOOL fConsumer) const;
    VOID MoveDequeue(RING *pRing, BOOL fConsumer) const;
    SLOT_CONTEXT_DATA* GetSlotContext(CONTAINER_CONTEXT *pContext) const;

    RING *AllocateRing(BOOL fLinkTrbs) const;
    BOOL ReinitializeRing(RING *pRing, BOOL fLinkTrbs) const;
    RING_SEGMENT *AllocateSegment() const;
    VOID LinkSegments(RING_SEGMENT *pPrev,
                        RING_SEGMENT *pNext,
                        BOOL fLinkTrbs) const;
    VOID InitializeRingInfo(RING *pRing) const;
    VOID FreeRing(RING *pRing) const;
    VOID FreeSegment(RING_SEGMENT *pSeg) const;
    INT  InitializeMemory();

    INT IssueSlotControl(UINT uTrbType, UCHAR bSlotId) const;
    CONTAINER_CONTEXT* AllocateContainerContext(INT iType) const;
    VOID FreeLogicalDevice(UCHAR bSlotId) const;

    INT PrepareRing(RING *pEpRing, UINT uEpState, UINT uNumTrbs) const;
    VOID RingTransferDoorbell(UCHAR bSlotId,
                            UCHAR bEpIndex,
                            INT iStartCycle,
                            NORMAL_TRB *pStartTrb) const;
    INT InitializeLogicalDevice(UCHAR bSlotId, UCHAR bSpeed, UINT uPortId, UCHAR bRootHubPort, UCHAR bHubSID, UINT uDevRoute) const;

    INT IssueAddressDevice(UINT64 u64InContextPtr, UCHAR bSlotId) const;
    INT IssueEvaluateContext(UINT64 u64InContextPtr, UCHAR bSlotId) const;
    BOOL IsTrbInSement(RING_SEGMENT *pStartSeg,
                            TRB  *pDequeueTrb,
                            TRB  *pTrb,
                            UINT64 u64EventDma) const;
    BOOL IsControlEndpoint(UCHAR bEndpointType) const;
    BOOL IsIsochronousEndpoint(UCHAR bEndpointType) const;
    BOOL IsInterruptEndpoint(UCHAR bEndpointType) const;
    BOOL IsBulkEndpoint(UCHAR bEndpointType) const;
    INT CleanupHaltedEndpoint(UCHAR bSlotId,
                                UCHAR bEpAddress,
                                TRANSFER_DATA *pTd,
                                TRB *pEventTrb) const;
    INT CountAbortTransfers(UCHAR bSlotId, UCHAR bEpAddress) const;
    VOID CleanupStalledRing(UCHAR bSlotId, UCHAR bEpIndex) const;
    INT AddEndpoint(UCHAR bSlotId, USB_ENDPOINT_DESCRIPTOR* peptDescr) const;
    UCHAR GetEndpointFlag(UCHAR uEndpointAddress) const;
    INT InitializeEndpoint(LOGICAL_DEVICE *pVirtDev,
                        UCHAR bEndpointType,
                        UINT uSpeed,
                        USB_ENDPOINT_DESCRIPTOR* peptDescr) const;
    INT IssueResetEndpoint(UCHAR bSlotId, UCHAR bEpIndex) const;
    UINT GetEndpointInterval(UCHAR bEndpointType,
                             UINT uSpeed,
                             USB_ENDPOINT_DESCRIPTOR* peptDescr) const;
    UINT GetEndpointType(UINT uEndpointType, UCHAR uEndpointAddress) const;
    UINT ParseMicroframeInterval(UINT uInterval) const;
    INT DoConfigureEndpoint(UCHAR bSlotId, USB_DEVICE_DESCRIPTOR* pDevDesc, USB_HUB_DESCRIPTOR* pHubDesc) const;
    INT ConfigureEndpoint(UCHAR bSlotId) const;
    VOID ResetInContext(LOGICAL_DEVICE* pVirtDev) const;
    VOID FreeEndpointRing(LOGICAL_DEVICE* pVirtDev,
                                    UCHAR bEpIndex) const;
    INT ConfigureEndpointResult(INT* pCmdStatus) const;
    INT PrepareTransfer(UCHAR bSlotId,
                        UINT uEpAddress,
                        TRANSFER_DATA** ppTd,
                        UINT uTransferLength,
                        UCHAR bEndpointType) const;
    UCHAR GetEndpointAddressByIndex(UCHAR bEpIndex) const;
    INT ResetEndpoint(UCHAR bSlotId, UINT uEpAddress, UCHAR bEndpointType) const;
    INT DropEndpoint(UCHAR bSlotId, UINT uEpAddress) const;
    VOID CleanEndpoint(LOGICAL_DEVICE *pVirtDev,
                            UCHAR uEndpointAddress) const;
    INT IssueResetDevice(UCHAR bSlotId) const;

    RING*   m_pCmdRing;
    RING*   m_pEventRing;

    UINT    m_uCmdRingReservedTrbs;
};

#endif /* CXHCDRING_H */