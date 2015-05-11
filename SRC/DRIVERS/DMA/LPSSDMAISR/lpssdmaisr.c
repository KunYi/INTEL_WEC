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
// GIISR.c
//
// Generic Installable Interrupt Service Routine.
//
//
#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>

#include "lpssdmaisr.h"
#include "BYTAllRegisters.h"
#include "Register.h"


// Globals
#define MAX_LPSSDMA_INSTANCES 0x2
static PLPSSDMAISR_INFO IsrLpssDmaDeviceHandle[MAX_LPSSDMA_INSTANCES]= {NULL,};
static BOOL IsrLpssDmaAllocated[MAX_LPSSDMA_INSTANCES] = { FALSE, };

/*
 @doc INTERNAL
 @func    BOOL | DllEntry | Process attach/detach api.
 *
 @rdesc The return is a BOOL, representing success (TRUE) or failure (FALSE).
 */
BOOL __stdcall
DllEntry(
    HINSTANCE hinstDll,         // @parm Instance pointer.
    DWORD dwReason,             // @parm Reason routine is called.
    LPVOID lpReserved           // @parm system parameter.
     )
{   
    DWORD dwCount;
    UNREFERENCED_PARAMETER(hinstDll);
    UNREFERENCED_PARAMETER(lpReserved);

    if (dwReason == DLL_PROCESS_ATTACH) {
        for (dwCount=0;dwCount<MAX_LPSSDMA_INSTANCES ;dwCount++)
            IsrLpssDmaDeviceHandle[dwCount]=NULL;            
    }
    else
    if (dwReason == DLL_PROCESS_DETACH) { 
        for (dwCount=0;dwCount<MAX_LPSSDMA_INSTANCES;dwCount++)
            if (IsrLpssDmaDeviceHandle[dwCount]!=NULL) {
                IsrLpssDmaDeviceHandle[dwCount]=NULL;
            }
    }
    return TRUE;

}


DWORD CreateInstance(
    void
    )
{
    DWORD dwCount;
    // Search For empty ISR Handle.
    for (dwCount = 0;dwCount < MAX_LPSSDMA_INSTANCES;dwCount++)
        if (IsrLpssDmaDeviceHandle[dwCount] == NULL)
            break;

    if (dwCount >= MAX_LPSSDMA_INSTANCES) // FULL.
        return (DWORD)-1;

    return dwCount;
}


void DestroyInstance(
    DWORD InstanceIndex 
    )
{
    if (InstanceIndex < MAX_LPSSDMA_INSTANCES && IsrLpssDmaDeviceHandle[InstanceIndex] != NULL )
        IsrLpssDmaDeviceHandle[InstanceIndex] = NULL;
}



BOOL 
IOControl(
    DWORD   InstanceIndex,
    DWORD   IoControlCode, 
    LPVOID  pInBuf, 
    DWORD   InBufSize,
    LPVOID  pOutBuf, 
    DWORD   OutBufSize, 
    LPDWORD pBytesReturned
    ) 
{
    if (pBytesReturned) {
        *pBytesReturned = 0;
    }

    switch (IoControlCode) {
    case IOCTL_LPSSDMAISR_INFO:
        {
            if ((InBufSize < sizeof(LPSSDMAISR_INFO)) || !pInBuf || 
                InstanceIndex >= MAX_LPSSDMA_INSTANCES ) {
                // Invalid IOCTL code or size of input buffer or input buffer pointer
                return FALSE;
            }
        
            if (IsrLpssDmaDeviceHandle[InstanceIndex] != NULL
                || GetDirectCallerProcessId() != GetCurrentProcessId()
               )
            {
                return FALSE;
            }

            IsrLpssDmaDeviceHandle[InstanceIndex] = (PLPSSDMAISR_INFO)pInBuf;
            return TRUE;
        }
    case IOCTL_LPSSDMAISR_UNLOAD:
        {
            if (InstanceIndex>=MAX_LPSSDMA_INSTANCES  || IsrLpssDmaDeviceHandle[InstanceIndex]==NULL )
                return FALSE;
            IsrLpssDmaDeviceHandle[InstanceIndex]=NULL;
            return TRUE;
        }

    }
    return FALSE;
}

/*************************************************************************
 @func  HANDLE | ClearStatusInterrupt | Clear status interrupt

 @Description: This routine clear the DMA controller status interrupt for all channel.
 
 *************************************************************************/
static void ClearStatusInterrupt(PUCHAR DMAC_BASE, ULONG Offset, ULONG InterruptStatus)
{
    int channel;
    for (channel = 0; channel < DMAH_NUM_CHANNELS; channel++)
    {
        if (InterruptStatus & (INT_STATUS << channel))
        {
            ULONG mask= INT_CLEAR;
            mask = mask << channel;
            WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+Offset), mask);
        }
    }
}

LONG g_ChannelISR[DMAH_NUM_CHANNELS] = {0};

LONG g_StatusTfrFailure = 0;

DWORD
ISRHandler(
           DWORD InstanceIndex
           )
{
    DEC_REG_FIELD_TYPE(STATUSINT) StatusAllInt = {0};
    PLPSSDMAISR_INFO        pIsrInfo = IsrLpssDmaDeviceHandle[InstanceIndex];
    DWORD   stat = 0;
    DWORD   StatusTfr = 0;
    DWORD   StatusBlock = 0;
    DWORD   StatusSrcTran = 0;
    DWORD   StatusDstTran = 0;
    DWORD   StatusErr = 0;
    DWORD   ISRRtn = SYSINTR_CHAIN;

    stat = READ_DMA_REGISTER (pIsrInfo,OFFSET_STATUS_INT_REGISTER);

    while (stat & STATUSINT_ALL_MASK)
    {
        PUCHAR DMAC_BASE;
        FORMAT_STRUCT_BY_REG(STATUSINT,stat,&StatusAllInt);

        DMAC_BASE = pIsrInfo->DMAC_BASE;

        if (StatusAllInt.STATUSINT_TFR)
        {
            ULONG i;
            DWORD channel;
            DWORD StatusTfrReadBack = 0;
            StatusTfr = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_TFR_REGISTER);
            ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_TFR_REGISTER,StatusTfr);
            StatusTfrReadBack = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_TFR_REGISTER);

            i = 0;
            while ((StatusTfrReadBack & StatusTfr) && i < 3)
            {
                ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_TFR_REGISTER,StatusTfr);
                StatusTfrReadBack = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_TFR_REGISTER);
                InterlockedIncrement(&g_StatusTfrFailure);
                i++;
            }

            pIsrInfo->InterruptStatusTfr |= StatusTfr;

            for (channel = 0; channel < DMAH_NUM_CHANNELS; channel++)
            {
                DWORD mask = INT_STATUS;
                mask = mask << channel;
                if (StatusTfr & mask)
                {
                    pIsrInfo->InterruptStatusTfrPerChannel[channel]++;
                    InterlockedIncrement(&g_ChannelISR[channel]);
                }
            }
        }

        if (StatusAllInt.STATUSINT_BLOCK)
        {
            StatusBlock = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_BLK_REGISTER);
            ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_BLK_REGISTER,StatusBlock);
            pIsrInfo->InterruptStatusBlock |= StatusBlock;
        }

        if (StatusAllInt.STATUSINT_SRCT)
        {
            StatusSrcTran = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_STR_REGISTER);
            ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_STR_REGISTER,StatusSrcTran);
            pIsrInfo->InterruptStatusSrcTran |= StatusSrcTran;
        }

        if (StatusAllInt.STATUSINT_DSTT)
        {
            StatusDstTran = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_DTR_REGISTER);
            ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_DTR_REGISTER,StatusDstTran);
            pIsrInfo->InterruptStatusDstTran |= StatusDstTran;
        }

        if (StatusAllInt.STATUSINT_ERR)
        {
            StatusErr = READ_DMA_REGISTER(pIsrInfo,OFFSET_STATUS_ERR_REGISTER);
            ClearStatusInterrupt(DMAC_BASE,OFFSET_CLEAR_ERR_REGISTER,StatusErr);
            pIsrInfo->InterruptStatusErr |= StatusErr;
        }

        stat = READ_DMA_REGISTER (pIsrInfo,OFFSET_STATUS_INT_REGISTER);
    }

    ISRRtn = SYSINTR_CHAIN;
    if (StatusBlock
        || StatusDstTran
        || StatusErr
        || StatusSrcTran)
    {
        /* current don't process those interrupt, so return SYSINTR_NOP*/
        ISRRtn = SYSINTR_NOP;
    }

    if (StatusTfr)
    {
        ISRRtn = pIsrInfo->SysIntr;
    }
    
    return ISRRtn;
}
