//
// Copyright (c) Microsoft Corporation.  All rights reserved.
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
// File: ISR16550.h
//
// Purpose: Serial 16550 Installable Interrupt Service Routine definitions.
//
//
#ifndef __ISR16550_H_
#define __ISR16550_H_

#include <pkfuncs.h>
#define USER_IOCTL(X) (IOCTL_KLIB_USER + (X))
#define IOCTL_ISR16550_INFO USER_IOCTL(0)
#define IOCTL_ISR16550_UNLOAD USER_IOCTL(1)

typedef struct _XmitDataBuffer {
    DWORD dwBufferSize;
    DWORD dwWaterMark;
    DWORD dwFIFO_In;
    DWORD dwFIFO_Out;
    BYTE  bBuffer[1];
} XmitDataBuffer;
typedef volatile XmitDataBuffer * PXmitDataBuffer;

typedef struct  {
    BYTE bIntFlag,bIntData;
} IIR_EVENT;
typedef struct _RcvDataBuffer {
    DWORD dwBufferSize;
    DWORD dwWaterMark;
    DWORD dwFIFO_In;
    DWORD dwFIFO_Out;
    IIR_EVENT  bBuffer[1];
} RcvDataBuffer;
typedef volatile RcvDataBuffer * PRcvDataBuffer;

__inline static DWORD GetRcvDataSize(PRcvDataBuffer pReceiveBuffer)
{
    register DWORD dwFIFO_In=pReceiveBuffer->dwFIFO_In,dwFIFO_Out=pReceiveBuffer->dwFIFO_Out;
    if (dwFIFO_In<dwFIFO_Out) 
        return dwFIFO_In+pReceiveBuffer->dwBufferSize - dwFIFO_Out;
    else
        return dwFIFO_In - dwFIFO_Out;
}
__inline static DWORD GetRcvAvailableBuffer(PRcvDataBuffer pReceiveBuffer)
{
    register DWORD dwFIFO_In=pReceiveBuffer->dwFIFO_In,dwFIFO_Out=pReceiveBuffer->dwFIFO_Out;
    if (dwFIFO_In<dwFIFO_Out) 
        return dwFIFO_Out-dwFIFO_In;
    else
        return dwFIFO_Out +pReceiveBuffer->dwBufferSize - dwFIFO_In;
}
__inline static DWORD IncRcvIndex(PRcvDataBuffer pReceiveBuffer,DWORD dwIndex)
{
    return (dwIndex+1<pReceiveBuffer->dwBufferSize)?dwIndex+1:0;

}
__inline static DWORD GetXmitDataSize(PXmitDataBuffer pReceiveBuffer)
{
    register DWORD dwFIFO_In=pReceiveBuffer->dwFIFO_In,dwFIFO_Out=pReceiveBuffer->dwFIFO_Out;
    if (dwFIFO_In<dwFIFO_Out) 
        return dwFIFO_In+pReceiveBuffer->dwBufferSize - dwFIFO_Out;
    else
        return dwFIFO_In - dwFIFO_Out;
}
__inline static DWORD GetXmitAvailableBuffer(PXmitDataBuffer pReceiveBuffer)
{
    register DWORD dwFIFO_In=pReceiveBuffer->dwFIFO_In,dwFIFO_Out=pReceiveBuffer->dwFIFO_Out;
    if (dwFIFO_In<dwFIFO_Out) 
        return dwFIFO_Out-dwFIFO_In;
    else
        return dwFIFO_Out +pReceiveBuffer->dwBufferSize - dwFIFO_In;
}
__inline static DWORD IncXmitIndex(PXmitDataBuffer pReceiveBuffer,DWORD dwIndex)
{
    return (dwIndex+1<pReceiveBuffer->dwBufferSize)?dwIndex+1:0;

}

typedef struct _ISR16550_INFO {
    PVOID pBlockPhysAddr;
    DWORD dwBlockSize; 
    DWORD SysIntr;
    DWORD dwIntrDoneInISR;
    PBYTE pIoAddress;
    LONG  lIoSpace;
    DWORD dwReceiveHWWaterMaker;
    DWORD uMultiplier;
    DWORD bIntrBypass;
    DWORD InBufferOffset;
    DWORD OutBufferOffset;
    DWORD TxInDMA;
    DWORD RxInDMA;
    BOOL bStartRxDma;
    DWORD StartRxDmaReason;
    LONG  ISRLock;
    LONG  ISRLockFailure;
} ISR16550_INFO;

typedef volatile ISR16550_INFO * PISR16550_INFO; 

typedef BOOL (*PF_KERNELLIBIOCONTROL)(HANDLE  hLib, DWORD   dwIoControlCode, LPVOID  lpInBuf,  DWORD   nInBufSize,  LPVOID  lpOutBuf,  DWORD   nOutBufSize,     LPDWORD lpBytesReturned);


#ifdef __cplusplus
extern "C" {
#endif 
DWORD CreateInstance(  void  );
BOOL IOControl(
    DWORD   InstanceIndex,
    DWORD   IoControlCode, 
    LPVOID  pInBuf, 
    DWORD   InBufSize,
    LPVOID  pOutBuf, 
    DWORD   OutBufSize, 
    LPDWORD pBytesReturned
    ) ;
DWORD ISRHandler(
    DWORD InstanceIndex
    );
#ifdef __cplusplus
};
#endif
 
#endif
