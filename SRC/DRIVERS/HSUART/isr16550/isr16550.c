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
// ISR16550.c
//
// UART 16550 Installable Interrupt Service Routine.
//
//
#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>
#include <pkfuncs.h>
#include "hw16550.h"

#include "isr16550.h"

#define READ_UART_BYTE_COUNT_REG(_pBlockVirt) \
    READ_REGISTER_USHORT((USHORT*)(_pBlockVirt->pIoAddress + UART_BYTE_COUNT_REG))

#define READ_UART_OVERFLOW_INTR_STAT_REG(_pBlockVirt) \
    READ_REGISTER_UCHAR(_pBlockVirt->pIoAddress + UART_OVERFLOW_INTR_STAT_REG)

#define WRITE_UART_OVERFLOW_INTR_STAT_REG(_pBlockVirt,_Value) \
    WRITE_REGISTER_UCHAR(_pBlockVirt->pIoAddress + UART_OVERFLOW_INTR_STAT_REG,_Value)


static UCHAR ReadByte(PISR16550_INFO pBlockVirt,DWORD dwOffset) {
    if (pBlockVirt->lIoSpace)
        return READ_PORT_UCHAR(pBlockVirt->pIoAddress + (dwOffset*(pBlockVirt->uMultiplier)));
    else
        return READ_REGISTER_UCHAR(pBlockVirt->pIoAddress + (dwOffset*(pBlockVirt->uMultiplier)));
}
static void  WriteByte(PISR16550_INFO pBlockVirt,DWORD dwOffset,UCHAR bData) {
    if (pBlockVirt->lIoSpace)
        WRITE_PORT_UCHAR(pBlockVirt->pIoAddress + (dwOffset*(pBlockVirt->uMultiplier)), bData);
    else
        WRITE_REGISTER_UCHAR(pBlockVirt->pIoAddress + (dwOffset*(pBlockVirt->uMultiplier)), bData);
}


PF_KERNELLIBIOCONTROL pfnKLibCtl = NULL;

static DWORD Isr16550(PISR16550_INFO pBlockVirt,DWORD InstanceIndex)
{
    DWORD dwReturn=SYSINTR_CHAIN;
    DWORD dwTotalReturn=SYSINTR_CHAIN;
    UCHAR EnableIER;

    if (pBlockVirt)
    {
        if (InterlockedCompareExchange(&pBlockVirt->ISRLock,1,0))
        {
            /* we find ISR can be re-entry sometimes, so we have this w/a*/
            pBlockVirt->ISRLockFailure++;
            return SYSINTR_CHAIN;
        }
    }

    
    EnableIER = 0;
    if (pBlockVirt  && pBlockVirt->pIoAddress) {
        BYTE bIntFlag=SERIAL_IIR_INT_INVALID;
        BOOL  bContinueLoop=TRUE;

        while (bContinueLoop) {
            bIntFlag=ReadByte(pBlockVirt,INTERRUPT_IDENT_REGISTER);
            /* Start from BIOS WW31.3, controller will continuously issues CTI interrupt even though
               there is actually no receive operation is requested. WA is to check line status to skip 
               service this CTI interrupt
            */
            if ((bIntFlag & SERIAL_IIR_INT_MASK) == SERIAL_IIR_CTI)
            {
                BYTE bLineStatus = ReadByte(pBlockVirt,LINE_STATUS_REGISTER);
                if (!(bLineStatus & SERIAL_LSR_DR))
                {
                    ReadByte(pBlockVirt,RECEIVE_BUFFER_REGISTER);
                    continue;
                }
            }

            DEBUGMSG(0, (TEXT("Isr165550::ISRHandler 0x%x \r\n"),bIntFlag));
            if (bIntFlag & SERIAL_IIR_INT_INVALID) {
                if (READ_UART_OVERFLOW_INTR_STAT_REG(pBlockVirt) & 0x1) {
                    READ_UART_BYTE_COUNT_REG(pBlockVirt);
                    dwTotalReturn = SYSINTR_NOP;
                }
                break;
            } else {
                PRcvDataBuffer pReceiveBuffer= (PRcvDataBuffer)((PBYTE)pBlockVirt + pBlockVirt->InBufferOffset);
                dwReturn=SYSINTR_NOP;

                switch ( bIntFlag & SERIAL_IIR_INT_MASK ) {
                case SERIAL_IIR_RDA:
                case SERIAL_IIR_CTI:
                case SERIAL_IIR_CTI_2: {
                    WriteByte(pBlockVirt, INTERRUPT_ENABLE_REGISTER, ReadByte(pBlockVirt,INTERRUPT_ENABLE_REGISTER) & (~SERIAL_IER_RDA));

                    if (pBlockVirt->RxInDMA)
                    {
                        pBlockVirt->bStartRxDma = TRUE;
                        dwReturn= pBlockVirt->SysIntr;
                        bContinueLoop = FALSE;
                    }
                    else
                    {
                        dwReturn= pBlockVirt->SysIntr;
                        bContinueLoop = FALSE;
                        if (dwReturn == pBlockVirt->SysIntr && GetRcvAvailableBuffer(pReceiveBuffer)>1 ) { // Want to notify the IST. So
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntFlag =bIntFlag;
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntData=0;
                            pReceiveBuffer->dwFIFO_In=IncRcvIndex(pReceiveBuffer,pReceiveBuffer->dwFIFO_In);
                        }
                    }
                    break;
                }
                case SERIAL_IIR_THRE : {
                    if (pBlockVirt->TxInDMA)
                    {
                        dwReturn = pBlockVirt->SysIntr;
                        if (dwReturn ==pBlockVirt->SysIntr && GetRcvAvailableBuffer(pReceiveBuffer)>1 ) { // Want to notify the IST. So
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntFlag =SERIAL_IIR_THRE;
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntData=0;
                            pReceiveBuffer->dwFIFO_In=IncRcvIndex(pReceiveBuffer,pReceiveBuffer->dwFIFO_In);
                        }
                    }
                    else
                    {
                        WriteByte(pBlockVirt, INTERRUPT_ENABLE_REGISTER, ReadByte(pBlockVirt,INTERRUPT_ENABLE_REGISTER) & (~SERIAL_IER_THR));
                        dwReturn= pBlockVirt->SysIntr;
                        bContinueLoop = FALSE;
                        if (dwReturn == pBlockVirt->SysIntr && GetRcvAvailableBuffer(pReceiveBuffer)>1 ) { // Want to notify the IST. So
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntFlag =bIntFlag;
                            pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntData=0;
                            pReceiveBuffer->dwFIFO_In=IncRcvIndex(pReceiveBuffer,pReceiveBuffer->dwFIFO_In);
                        }
                    }
                    break;
                }
                
                case SERIAL_IIR_RLS: {
                    if (GetRcvAvailableBuffer(pReceiveBuffer)>1) {
                        pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntFlag =SERIAL_IIR_RLS;
                        pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntData=ReadByte(pBlockVirt,LINE_STATUS_REGISTER);
                        pReceiveBuffer->dwFIFO_In=IncRcvIndex(pReceiveBuffer,pReceiveBuffer->dwFIFO_In);
                    }
                    dwReturn= pBlockVirt->SysIntr;
                    bContinueLoop=FALSE;
                    break;
                }
                case SERIAL_IIR_MS: {
                    if (GetRcvAvailableBuffer(pReceiveBuffer)>1) {
                        pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntFlag =SERIAL_IIR_MS;
                        pReceiveBuffer->bBuffer[pReceiveBuffer->dwFIFO_In].bIntData=ReadByte(pBlockVirt,MODEM_STATUS_REGISTER);
                        pReceiveBuffer->dwFIFO_In=IncRcvIndex(pReceiveBuffer,pReceiveBuffer->dwFIFO_In);
                    }
                    dwReturn= pBlockVirt->SysIntr;
                    bContinueLoop=FALSE;
                    break;
                }
                default:
					break;
                }
            }
            if (dwTotalReturn!=dwReturn) {
                if (dwTotalReturn == SYSINTR_CHAIN){
                    dwTotalReturn = dwReturn;
                }
                else 
                if (dwTotalReturn == SYSINTR_NOP && dwReturn == pBlockVirt->SysIntr) {
                    dwTotalReturn = dwReturn;
                }
                    
            }
        };
        if (pfnKLibCtl!=NULL && dwTotalReturn == pBlockVirt->SysIntr && pBlockVirt->dwIntrDoneInISR!=0 ) {
            (*pfnKLibCtl)((HANDLE)KMOD_CORE,IOCTL_KLIB_INTERRUPTDONE,NULL,dwTotalReturn,NULL,0,NULL);
        }
    }

    InterlockedExchange(&pBlockVirt->ISRLock,0);
    if (pBlockVirt  && pBlockVirt->pIoAddress && EnableIER)
    {
        WriteByte(pBlockVirt, INTERRUPT_ENABLE_REGISTER, ReadByte(pBlockVirt,INTERRUPT_ENABLE_REGISTER) | EnableIER);
    }
    return dwTotalReturn ;
}

// Globals
#define MAX_ISR16550_INSTANCES 0x10
static PISR16550_INFO Isr16550Handle[MAX_ISR16550_INSTANCES]= {NULL,};
static BOOL Isr16550Allocated[MAX_ISR16550_INSTANCES] = { FALSE, };
#define INVALID_ISR16550_HANDLE (PISR16550_INFO)(-1)

static BOOL Init(DWORD dwIndex,PISR16550_INFO pBuffer,DWORD dwSize)
{
    if (dwIndex<MAX_ISR16550_INSTANCES && Isr16550Handle[dwIndex]==INVALID_ISR16550_HANDLE){
        if (GetDirectCallerProcessId() == GetCurrentProcessId()) {
            Isr16550Handle[dwIndex]=pBuffer;
            Isr16550Allocated[dwIndex] = FALSE;
            return TRUE;
        }
        else {
            Isr16550Handle[dwIndex] = (PISR16550_INFO)VirtualAllocCopyEx((HANDLE)GetDirectCallerProcessId(),GetCurrentProcess(),(LPVOID)pBuffer,dwSize,PAGE_READWRITE|PAGE_NOCACHE);
            if (Isr16550Handle[dwIndex]) {
                Isr16550Allocated[dwIndex] = TRUE;
                return TRUE;
            }
        }
    }
    return FALSE;
}
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
    if (dwReason == DLL_PROCESS_ATTACH) {
        for (dwCount=0;dwCount<MAX_ISR16550_INSTANCES;dwCount++)
            Isr16550Handle[dwCount]=NULL;
        if (pfnKLibCtl == NULL && lpReserved != NULL) {
#pragma warning(suppress:4055) //'type cast' : from data pointer 'LPVOID' to function pointer 'PF_KERNELLIBIOCONTROL'
            pfnKLibCtl = (PF_KERNELLIBIOCONTROL)lpReserved;
        }
            
    }
    else
    if (dwReason == DLL_PROCESS_DETACH) { 
        // Current We had nowhere else can call this routine. 
        //It should can another call call DeCreateInstance that invoke when ISR is uninstalled.
        for (dwCount=0;dwCount<MAX_ISR16550_INSTANCES;dwCount++)
            if (Isr16550Handle[dwCount]!=NULL) {
                PISR16550_INFO pCur16550Info = Isr16550Handle[dwCount];
                Isr16550Handle[dwCount]=NULL;
                if (Isr16550Allocated[dwCount]) {
                    VirtualFreeEx(GetCurrentProcess(),(LPVOID)((DWORD)pCur16550Info & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE) ;
                }
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
    for (dwCount=0;dwCount<MAX_ISR16550_INSTANCES;dwCount++)
        if (Isr16550Handle[dwCount]==NULL)
            break;
    if (dwCount>=MAX_ISR16550_INSTANCES) // FULL.
        return (DWORD)-1;
    else
        Isr16550Handle[dwCount]=INVALID_ISR16550_HANDLE;
    return dwCount;
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
    UNREFERENCED_PARAMETER(pOutBuf);
    UNREFERENCED_PARAMETER(OutBufSize);
    if (pBytesReturned) {
        *pBytesReturned = 0;
    }
    switch (IoControlCode) {
    case IOCTL_ISR16550_INFO:
        if ((InBufSize < sizeof(ISR16550_INFO)) || !pInBuf || 
            InstanceIndex>=MAX_ISR16550_INSTANCES ) {
            // Invalid IOCTL code or size of input buffer or input buffer pointer
            return FALSE;
        }
        else 
            return Init(InstanceIndex,(PISR16550_INFO)pInBuf,InBufSize);
    case IOCTL_ISR16550_UNLOAD:
        if (InstanceIndex>=MAX_ISR16550_INSTANCES || Isr16550Handle[InstanceIndex]==NULL )
            return FALSE;
        else {
            if (Isr16550Allocated[InstanceIndex]) {
                VirtualFreeEx(GetCurrentProcess(),(LPVOID)((DWORD)Isr16550Handle[InstanceIndex] & ~VM_BLOCK_OFST_MASK),0, MEM_RELEASE) ;
            }
            Isr16550Handle[InstanceIndex]=NULL;
            return TRUE;
        }

    }
    return FALSE;
}
DWORD
ISRHandler(
    DWORD InstanceIndex
    )
{
    if (InstanceIndex<MAX_ISR16550_INSTANCES && 
            Isr16550Handle[InstanceIndex]!=NULL &&
            Isr16550Handle[InstanceIndex]!=INVALID_ISR16550_HANDLE)
        return Isr16550(Isr16550Handle[InstanceIndex],InstanceIndex);
    else
        return SYSINTR_CHAIN;
}
void DestroyInstance(
    DWORD InstanceIndex 
    )
{
    if (InstanceIndex<MAX_ISR16550_INSTANCES && Isr16550Handle[InstanceIndex]!=NULL )
        Isr16550Handle[InstanceIndex]=NULL;
}
