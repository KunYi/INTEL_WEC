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
// File: datastructures.h - Data Structures for XHCI. contains the data
//       structures used by the XHCI Host Controller Driver.
//----------------------------------------------------------------------------------

#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#define LINK_TGG_BIT (0x1 << 1)

#define SET_DEQ_PENDING (1 << 0)

#define ENDPOINT_HALT         (1 << 1)
#define ENDPOINT_HALT_PENDING (1 << 2)

#define TYPE_DEVICE  (0x1)
#define TYPE_INPUT   (0x2)

typedef struct _ERST_TABLE
{
    struct ERST_TABLE_ENTRY  *pEntries;
    UINT                     uNumEntries;
    UINT64                   u64ErstDmaAddr;
    UINT                     uErstSize;
} ERST_TABLE;

typedef struct _COMPLETION
{
    INT iStatus;
} COMPLETION;

typedef struct XHCI_LINK
{
    VOID*               pStruct;
    struct XHCI_LINK*   pLinkFwd;
    struct XHCI_LINK*   pLinkBack;
} LINK, *PLINK;

typedef struct XHCI_LIST_HEAD
{
    PLINK pLink;
} LIST_HEAD, *PLIST_HEAD;

typedef struct _LOGICAL_ENDPOINT
{
    struct _RING           *pRing;
    struct _RING           *pNewRing;
    UINT                    uEpState;
    union _TRB             *pStoppedTrb;
    struct _TRANSFER_DATA  *pStoppedTd;
    UCHAR                   bAbortCounter;
} LOGICAL_ENDPOINT;

typedef struct _CONTAINER_CONTEXT
{
    unsigned    uType;
    INT         iSize;
    UCHAR      *pbBytes;
    UINT64      u64Dma;
} CONTAINER_CONTEXT;

typedef struct _LOGICAL_DEVICE
{
    CONTAINER_CONTEXT         *pOutContext;
    CONTAINER_CONTEXT         *pInContext;
    LOGICAL_ENDPOINT           eps [MAX_EP_NUMBER];
    COMPLETION                 cmdCompletion;
    UINT                       uCmdStatus;
    XHCI_LIST_HEAD             cmdList;
    USB_ENDPOINT_DESCRIPTOR*   pEndpointDescriptors;
    UCHAR                      bPortId;
    UCHAR                      bDeviceSpeed;
} LOGICAL_DEVICE;

typedef struct _COMMAND
{
    UINT                uStatus;
    COMPLETION          completion;
    union _TRB         *pCommandTrb;
    LINK                cmdList;
} COMMAND;

typedef struct _INPUT_CONTROL_CONTEXT
{
    UINT    uDropFlags;
    UINT    uAddFlags;
    UINT    uRsvd2[6];
} INPUT_CONTROL_CONTEXT;

typedef struct _ENDPOINT_CONTEXT_DATA
{
    DWORD   dwEpState   : 3;
    DWORD   dwReserved  : 13;
    DWORD   dwInterval  : 8;
    DWORD   dwReserv    : 8;
    DWORD   dwRes           : 1;
    DWORD   dwCErr          : 2;
    DWORD   dwEPType        : 3;
    DWORD   dwReserZ        : 2;
    DWORD   dwMaxBurstSize  : 8;
    DWORD   dwMaxPacketSize : 16;
    UINT64  u64Deq;
    UINT    uTxInfo;
    UINT    uReserved[3];
} ENDPOINT_CONTEXT_DATA;

#define BLOCK_FIELD_NUMBER    4

typedef struct _TRB_FIELDS
{
    UINT uField[BLOCK_FIELD_NUMBER];
} TRB_FIELDS;

typedef struct _TRANSFER_EVENT_TRB
{
    UINT64  u64Buffer;
    UINT    uTransferLen;
    UINT    uFlags;
} TRANSFER_EVENT_TRB;

typedef struct _PORT_EVENT_TRB
{
    DWORD   dwReserved1 : 24;
    DWORD   dwPortId    : 8;
    DWORD   dwReserved2;
    DWORD   dwTransferLength : 24;
    DWORD   dwCompletionCode : 8;
    DWORD   dwFlags;
} PORT_EVENT_TRB;

typedef struct _NORMAL_TRB
{
    UINT64  u64Buffer;
    UINT    uTransferLen;
    DWORD   dwCycleBit  : 1;
    DWORD   dwFlags     : 9;
    DWORD   dwBlockType : 6;
    DWORD   dwReserved  : 16;
} NORMAL_TRB;

typedef struct _COMMAND_EVENT_TRB
{
    UINT64  u64CmdTrb;
    UINT    uStatus;
    DWORD   dwCycleBit  : 1;
    DWORD   dwReserved  : 9;
    DWORD   dwTrbType   : 6;
    DWORD   dwVfId      : 8;
    DWORD   dwSlotId    : 8;
} COMMAND_EVENT_TRB;

typedef struct _COMMAND_TRB
{
    UINT64  u64CmdTrb;
    UINT    uStatus;
    DWORD   dwCycleBit   : 1;
    DWORD   dwReserved   : 9;
    DWORD   dwTrbType    : 6;
    DWORD   dwEndpointId : 5;
    DWORD   dwReserve    : 3;
    DWORD   dwSlotId     : 8;
} COMMAND_TRB;

typedef struct _LINK_TRB
{
    UINT64  u64SegmentPtr;
    UINT    uIntrTarget;
    UINT    uControl;
} LINK_TRB;

typedef union _TRB
{
    LINK_TRB            linkTrb;
    TRANSFER_EVENT_TRB  transferEventTrb;
    COMMAND_EVENT_TRB   commandEventTrb;
    PORT_EVENT_TRB      portEventTrb;
    NORMAL_TRB          normalTrb;
    COMMAND_TRB         commandTrb;
} TRB;

typedef struct _SLOT_CONTEXT_DATA
{
    DWORD   dwRouteString : 20;
    DWORD   dwSpeed : 4;
    DWORD   dwRes : 1;
    DWORD   dwMtt : 1;
    DWORD   dwHub : 1;
    DWORD   dwContextEntries : 5;
    DWORD   dwMaxExitLatency : 16;
    DWORD   dwRootHubPort : 8;
    DWORD   dwNumberOfPorts : 8;
    DWORD   dwTtInfoHubSlotId:8;
    DWORD   dwTtInfoPortNumb:8;
    DWORD   dwTtInfoThinkTime:8;
    DWORD   dwTtInfoRes:8;
    DWORD   dwUsbDeviceAddress : 8;
    DWORD   dwReserv : 19;
    DWORD   dwSlotState : 5;
    DWORD   dwReserved[4];
} SLOT_CONTEXT_DATA;

typedef struct _RING_SEGMENT
{
    TRB                *pTrbs;
    UINT64              u64Dma;
} RING_SEGMENT;

class CQTransfer;

typedef struct _TRANSFER_DATA
{
    UCHAR           bEndpointType;
    RING_SEGMENT   *pStartSeg;
    TRB            *pFirstTrb;
    TRB            *pLastTrb;
    UINT            uActualLength;    
    CQTransfer*     pQTransfer;
    UINT64          u64Dma;
} TRANSFER_DATA;

typedef struct _DEQUEUE_STATE
{
    RING_SEGMENT    *pNewDeqSeg;
    TRB             *pNewDeqPtr;
    INT             iNewCycleState;
} DEQUEUE_STATE;

typedef struct _RING
{
    RING_SEGMENT    *pRingSegment;
    TRB             *pEnqueue;
    UINT             uEnqUpdates;
    TRB             *pDequeue;
    UINT            uDeqUpdates;
    UINT            uCycleState;
} RING;

struct ERST_TABLE_ENTRY
{
    UINT64  u64SegAddr;
    UINT    uSegSize;
    UINT    uRsvd;
};
 
typedef struct _SCRATCHPAD
{
    UINT64  *pu64SpArray;
    UINT64  u64SpDma;
    VOID    **ppSpBuffers;
} SCRATCHPAD;

typedef struct _DOORBELL_ARRAY
{
    UINT uDoorbell[256];
} DOORBELL_ARRAY;

typedef struct _DEVICE_CONTEXT_ARRAY
{
    UINT64  u64DevContextPtrs[MAX_SLOTS_NUMBER];
    UINT64  u64Dma;
} DEVICE_CONTEXT_ARRAY;

#endif /* DATASTRUCTURES_H */