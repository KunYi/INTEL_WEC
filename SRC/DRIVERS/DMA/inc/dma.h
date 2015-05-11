//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2009 Intel Corporation All Rights Reserved. 
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
//



/*Module Name:  

GPIO.h

Abstract:  

Holds definitions private to the implementation of the machine independent
driver interface.

Notes: 


--*/

#ifndef _LPSSDMA_H_
#define _LPSSDMA_H_


#include <windows.h>
#include <nkintr.h>
#include "../../INC/types.h"
#include <notify.h>
#include <memory.h>
#include <nkintr.h>
#include <serdbg.h>
#include <pegdser.h>
#include <devload.h>
#include <ddkreg.h>
#include <ceddk.h>
#include <cebuscfg.h>
#include <linklist.h>
#include <Giisr.h>
#include <Kfuncs.h>
#include <CSync.h>
#include "BYTAllRegisters.h"
#include "Register.h"
#include "DmaPublic.h"
#include "lpssdmaisr.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("LPSSDMA"), {
        TEXT("Errors"),TEXT("Warnings"),TEXT("Init"),TEXT("Deinit"),},
            0xFFFF
};
#endif

    //Register Offset 
#define OFFSET_SARX_REGISTER      0x00
#define OFFSET_DARX_REGISTER      0x08
#define OFFSET_LLPX_REGISTER      0x10
#define OFFSET_CTLX_REGISTER      0x18
#define OFFSET_SSTATX_REGISTER    0x20
#define OFFSET_DSTATX_REGISTER    0x28
#define OFFSET_SSTATARX_REGISTER  0x30
#define OFFSET_DSTATARX_REGISTER  0x38
#define OFFSET_CFGX_REGISTER      0x40
#define OFFSET_SGRX_REGISTER      0x48
#define OFFSET_DSRX_REGISTER      0x50

#define OFFSET_STATUS_TFR_REGISTER  (0x2E8)
#define OFFSET_STATUS_BLK_REGISTER  (0x2F0)
#define OFFSET_STATUS_STR_REGISTER  (0x2F8)
#define OFFSET_STATUS_DTR_REGISTER  (0x300)
#define OFFSET_STATUS_ERR_REGISTER  (0x308)

#define OFFSET_MASK_TFR_REGISTER    (0x310)
#define OFFSET_MASK_BLK_REGISTER    (0x318)
#define OFFSET_MASK_STR_REGISTER    (0x320)
#define OFFSET_MASK_DTR_REGISTER    (0x328)
#define OFFSET_MASK_ERR_REGISTER    (0x330)

#define OFFSET_CLEAR_TFR_REGISTER   (0x338)
#define OFFSET_CLEAR_BLK_REGISTER   (0x340)
#define OFFSET_CLEAR_STR_REGISTER   (0x348)
#define OFFSET_CLEAR_DTR_REGISTER   (0x350)
#define OFFSET_CLEAR_ERR_REGISTER   (0x358)

#define OFFSET_STATUS_INT_REGISTER  (0x360)

#define OFFSET_DMACFG_REGISTER      (0x398)
#define OFFSET_CHENREG_REGISTER     (0x3A0)



#define DEFAULT_POLL_REG_TIME_IN_MS ((ULONG)300)
#define CHANNEL_DEFAULT_TIMEROUT_IN_SEC   ((ULONG)30)
#define CHANNEL_INTERVAL_TIMEROUT_IN_MS   ((ULONG)20)
#define DEFAULT_INTERVAL_TIMEOUT_IN_MS    ((ULONG)10)

__forceinline static void WRITE_DMAC_REGISTER(PUCHAR DMAC_BASE, ULONG RegOffset,UINT64 dVal)
{
    UINT32 low,high;
    low = dVal & MAXUINT32;
    high = (dVal >> 32) & MAXUINT32;
    WRITE_REGISTER_ULONG((volatile ULONG*)(DMAC_BASE+RegOffset),low);
    WRITE_REGISTER_ULONG((volatile ULONG*)(DMAC_BASE+RegOffset+sizeof(UINT32)),high);
}

__forceinline static UINT64 READ_DMAC_REGISTER(PUCHAR DMAC_BASE, ULONG RegOffset)
{
    UINT32 low,high;
    UINT64 value = 0;
    low = READ_REGISTER_ULONG((volatile ULONG*)(DMAC_BASE+RegOffset));
    high = READ_REGISTER_ULONG((volatile ULONG*)(DMAC_BASE+RegOffset+sizeof(UINT32)));
    value = high;
    value = (value << 32) | low;
    return value;
}

__forceinline static void WRITE_CHANNEL_REGISTER(PUCHAR DMAC_BASE,ULONG channel, ULONG chanRegOffset, UINT64 dVal)
{
    ULONG offset;
    ASSERT(channel<DMAH_NUM_CHANNELS);
    ASSERT(chanRegOffset<STRIDE_CHANNEL);
    offset = channel*STRIDE_CHANNEL + chanRegOffset;
    WRITE_DMAC_REGISTER(DMAC_BASE,offset,dVal);
}

__forceinline static UINT64 READ_CHANNEL_REGISTER(PUCHAR DMAC_BASE,ULONG channel, ULONG chanRegOffset)
{
    ULONG offset;
    ASSERT(channel<DMAH_NUM_CHANNELS);
    ASSERT(chanRegOffset<STRIDE_CHANNEL);
    offset = channel*STRIDE_CHANNEL + chanRegOffset;
    return READ_DMAC_REGISTER(DMAC_BASE,offset);
}

typedef enum _DMA_DIRECTION {
    DmaDirectionReadFromDevice = FALSE,
    DmaDirectionWriteToDevice = TRUE,
} DMA_DIRECTION;

typedef enum _DMAC_TXRX_ENUM{
    DMAC_Peripheral_TX,  //Peripheral transfer 
    DMAC_Peripheral_RX,  //Peripheral receive
    DMAC_Peripheral_RX_DUMMYWRITE,
    DMAC_M2M,
}DMAC_TXRX_ENUM;


/* Create DMA Transaction per device request */
typedef struct _DMA_TRANSACTION_CONTEXT {
    DMA_TRANSACTION_TYPE    TransactionType;

    LPSSDMA_CHANNELS        ChannelRX;
    LPSSDMA_CHANNELS        ChannelTX;
    ULONG                   RequestLineRX;
    ULONG                   RequestLineTX;

    DWORD                   IntervalTimeOutInMS;
    DWORD                   TotalTimeOutInMS;
    DWORD                   StartTickCount;

    ULONG                   PeripheralFIFOPhysicalAddress;
    ULONG                   SysMemPhysicalAddress;
    size_t                  DmaMaxTransferLength;
    ULONG                   DummyDataForI2CSPI;
    ULONG                   DummyDataWidthInByte;

    DWORD                   TotalTransferredLength;
    DWORD                   LastTransferredLength; //data in single dma transfer
    DWORD                   TransferLengthForWatchDog;  // to check whether h/w is hang 
    DWORD                   TransferSizeInByte;

    HANDLE                  hDmaCompleteEvent;   /* system thread */
    DWORD                  *pBytesReturned;

} DMA_TRANSACTION_CONTEXT, *PDMA_TRANS_CONTEXT;

/*Structure that contain Hardware Configuration for single block */
typedef struct _DMA_SBLOCK_CONTEXT{
    LIST_ENTRY Context_List_Entry;
    PUCHAR DMAC_BASE;
    DMAC_TXRX_ENUM TxRx;
    ULONG  RequestLine;
    ULONG SourceAddress;
    ULONG DestinationAddress;
    ULONG  TransferWidth;  //assume SRC_TR_WIDTH same to DST_TR_WIDTH 
    ULONG  TransferSizeInByte;
    HLOCAL FreeBlkAddr;
    HANDLE  CallbackEvent;   /* system thread */
    //void (*CallbackRoutine)(int); Reserve for callback or event trigger
}DMA_SBLOCK_CONTEXT;


typedef enum _DMA_CHANNEL_STATUS{
    DMA_TRANSACTION_IN_TRANSFER = 0, /* DMA channel is enabled to tranfsering data */
    DMA_TRANSACTION_STOP,            /* DMA channel is disabled to finish current transfer */ 
    DMA_TRANSACTION_NOREQUEST,       /* The dma request attached to DMA channel is completed */
    //DMA_TRANSACTION_NEEDCANCEL
}DMA_CHANNEL_STATUS;

struct DMA_DEVICE_CONTEXT;
typedef struct _DMA_CHANNEL_CONTEXT
{
    struct DMA_DEVICE_CONTEXT   *pDeviceContext;
    DWORD                       Channel;
    HANDLE                      ChannelThread;
    HANDLE                      ChannelEvent;
    DMA_CHANNEL_STATUS          ChannelState;
    CLockObject*                pChannelLock;
    DMA_TRANSACTION_CONTEXT     TransContext;
    DWORD                       ChannelNotifyCount;
}DMA_CHANNEL_CONTEXT,*PDMA_CHANNEL_CONTEXT;
typedef struct DMA_DEVICE_CONTEXT
{
    DWORD                   dwDeviceId;  
    PUCHAR                  pMemBaseAddr;    //Memory base address
    DWORD                   dwMemLen;         //Size of memory base address
    DWORD                   dwIrq;            /* DMA irq */
    DWORD                   dwSysIntr;   /* system interrupt */
    DWORD                    InterruptMask; /* Mask for Interrupt*/
    BOOL                    InterruptEnabled;
    HANDLE                  hInterruptEvent;  /* event mapped to system interrupt */
    HANDLE                  hInterruptThread;   /* system thread */ 
    BOOL                    InterruptThreadStarted;
    PUCHAR                  DMAC_BASE;
    DMA_TRANSACTION_TYPE    TransactionType;
    PHYSICAL_ADDRESS        CommonBufferPhysAdd;  //Physical address of DMA Common Buffer
    PUCHAR                    CommonBufferVirtualAdd;
    DWORD                    dwISTLoopTrack;
    BOOL                    bRequestIST;
    DWORD                    dwTransferredLength;

    CLockObject*            pDeviceLock;

    BOOL                bIsTerminated;
    DMA_CHANNEL_CONTEXT ChannelContext[DMAH_NUM_CHANNELS];

    LPSSDMAISR_INFO      IsrInfo;
    HANDLE              hBusAccess;
    HKEY                hDeviceKey;
    HANDLE              hIsrHandler;

} DMA_DEVICE_CTXT, *PDEVICE_CONTEXT;

DWORD ChannelThreadFunc(LPVOID pvarg);

DWORD DMA_IST(PDEVICE_CONTEXT pDevice);

/*DMA driver init function*/
DWORD DMA_Init( LPCTSTR pContext, LPCVOID lpvBusContext);

/*DMA driver open*/
DWORD DMA_Open(DWORD pDevice, DWORD AccessCode, DWORD ShareMode);

void DisableChannel(PUCHAR DMAC_BASE,ULONG channel);
void EnableDMAController(PUCHAR DMAC_BASE);

void DisableDMAController(PUCHAR DMAC_BASE);

BOOL PollDMACRegisterBit(PUCHAR DMAC_BASE,ULONG RegOffset,ULONG BitOffset,ULONG PollValue,ULONG PollTimeInMicroSec);

void SingleBlockTransfer(DMA_SBLOCK_CONTEXT* pRegsContext, int channel);

void ClearInterruptOnChannel(PUCHAR DMAC_BASE,ULONG channel);

void EnableInterruptOnChannel(PUCHAR DMAC_BASE,ULONG channel);

void ClearStatusInterrupt(PUCHAR DMAC_BASE, ULONG Offset, ULONG InterruptStatus);

void ClearStatusBlockInterrupt(PUCHAR DMAC_BASE, ULONG InterruptStatus, PDEVICE_CONTEXT pDevice);

void CheckLastBlock(PUCHAR DMAC_BASE, ULONG InterruptStatus, PDEVICE_CONTEXT pDevice);

/*DMA driver IOControl function*/
BOOL DMA_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut, 
    DWORD dwLenOut,PDWORD pdwActualOut);

/*DMA close function*/
BOOL DMA_Close(DWORD hOpenContext);

/*DMA driver deinit function*/
BOOL DMA_Deinit(DWORD hDeviceContext);


BOOL ProgramDmaForSingleBlockRX(PDEVICE_CONTEXT pDevice,PDMA_TRANS_CONTEXT pTransContext);

BOOL ProgramDmaForSingleBlockTX(PDEVICE_CONTEXT pDevice,PDMA_TRANS_CONTEXT pTransContext);

#ifdef __cplusplus
}
#endif 

#endif // __GPIO_H__

