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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

Abstract:

    Serial PDD for 16550 Common Code.

Notes: 
--*/
#include <windows.h>
#include <ceddk.h>

#include <ddkreg.h>
#include <serhw.h>
#include "hw16550.h"
#include <Serdbg.h>

#include "serpriv.h"
#include "pddisr.h"
#include <nkintr.h>

#include "..\..\DMA\inc\DmaPublic.h"
#define NUM_OF_SHARED_PAGE 1
#define MAX_RX_DMA_SIZE (PAGE_SIZE)

CPdd16550Isr::CPdd16550Isr(LPCTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj)
:   CPdd16550 (lpActivePath, pMdd, pHwObj)
{
    m_pVirtualStaticAddr = NULL;
    m_pIsrInfoVirt = NULL ;
    m_pXmitBuffer = NULL;
    m_pReceiveBuffer = NULL ;
    m_hIsrHandler = NULL ;
    m_dwDeviceID = 0;
}
CPdd16550Isr::~CPdd16550Isr()
{
    if (m_hISTEvent) {
        m_bTerminated=TRUE;
        ThreadStart();
        SetEvent(m_hISTEvent);
        ThreadTerminated(1000);
    };
    if (m_hIsrHandler){
        FreeIntChainHandler(m_hIsrHandler);
    };
    if (m_pIsrInfoVirt) {
        VirtualFree((LPVOID)m_pIsrInfoVirt,0,MEM_RELEASE);
    }

    if (m_DMADeviceHandle)
    {
        CloseHandle(m_DMADeviceHandle);
    }

    if (m_UartDmaTxThreadHandle)
    {
        CloseHandle(m_UartDmaTxThreadHandle);
    }

    if (m_UartDmaRxThreadHandle)
    {
        CloseHandle(m_UartDmaRxThreadHandle);
    }

    if (m_UartDmaTxEvent)
    {
        CloseHandle(m_UartDmaTxEvent);
    }

    if (m_UartDmaRxEvent)
    {
        CloseHandle(m_UartDmaRxEvent);
    }

    if (m_TxDmaCompleteEvent)
    {
        CloseHandle(m_TxDmaCompleteEvent);
    }

    if (m_RxDmaCompleteEvent)
    {
        CloseHandle(m_RxDmaCompleteEvent);
    }

    if (m_TxDmaBufferVirtualAddress)
    {
        HalFreeCommonBuffer(&m_AdapterObject,m_TxDmaBufferLen + 2*sizeof(DWORD),m_TxDmaBufferPhyAddress,m_TxDmaBufferVirtualAddress,FALSE);
    }

    if (m_RxDmaBufferVirtualAddress)
    {
        HalFreeCommonBuffer(&m_AdapterObject,m_RxDmaBufferLen + 2*sizeof(DWORD),m_RxDmaBufferPhyAddress, m_RxDmaBufferVirtualAddress,FALSE);
    }
}

DWORD UartDmaTx(LPVOID pcontext)
{
    return ((CPdd16550Isr*)pcontext)->UartDmaTxThread();
}

DWORD UartDmaRx(LPVOID pcontext)
{
    return ((CPdd16550Isr*)pcontext)->UartDmaRxThread();
}

DWORD CPdd16550Isr::UartDmaTxThread()
{
    while(!IsTerminated())
    {
        DMA_REQUEST_CONTEXT DmaTxRequest;
        BOOL Rtn;
        DWORD BytesHandled;
        if (WaitForSingleObject(m_UartDmaTxEvent,INFINITE) == WAIT_OBJECT_0)
        {
            m_HardwareLock.Lock();

            /* We don't know the driver loading sequence in WEC7, so had to check it before start dma transfer
            must do it after we get m_HardwareLock.Lock();
            */
            if (m_DMADeviceHandle == NULL)
            {
                m_DMADeviceHandle =  CreateFile(
                    DMA2_DEVICE_NAME, 
                    GENERIC_READ|GENERIC_WRITE,       
                    FILE_SHARE_READ|FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL
                    );
                if (m_DMADeviceHandle == NULL
                    || m_DMADeviceHandle == INVALID_HANDLE_VALUE)
                {
                    m_DMADeviceHandle = NULL;
                    m_HardwareLock.Unlock();
                    continue;
                }

            }

            memset(&DmaTxRequest,0,sizeof(DmaTxRequest));
            DmaTxRequest.ChannelRx = (LPSSDMA_CHANNELS)m_DmaChannelRx;
            DmaTxRequest.ChannelTx = (LPSSDMA_CHANNELS)m_DmaChannelTx;

            DmaTxRequest.RequestLineRx = m_DmaRequestLineRx;
            DmaTxRequest.RequestLineTx = m_DmaRequestLineTx;          

            DmaTxRequest.TransactionType = TRANSACTION_TYPE_SINGLE_TX;
            DmaTxRequest.hDmaCompleteEvent = m_TxDmaCompleteEvent;

            DmaTxRequest.PeripheralFIFOPhysicalAddress =
                (ULONG)(m_ioPhysicalBase.LowPart + (TRANSMIT_HOLDING_REGISTER *(m_pIsrInfoVirt->uMultiplier)));

            DmaTxRequest.SysMemPhysAddress = m_TxDmaBufferPhyAddress;
            DmaTxRequest.SysMemPhysAddress.LowPart += m_TxDmaBufferReadIndex;

            DmaTxRequest.TransactionSizeInByte = m_TxDmaBufferWriteIndex - m_TxDmaBufferReadIndex;

            DmaTxRequest.VirtualAdd = m_TxDmaBufferVirtualAddress;

            DWORD TotalTimeout;         
            DWORD Timeout;
            TotalTimeout = m_CommTimeouts.WriteTotalTimeoutMultiplier * DmaTxRequest.TransactionSizeInByte +
                m_CommTimeouts.WriteTotalTimeoutConstant;

            if ( TotalTimeout == 0)
            {
                /* Set a safty value, consider BAUD_300, 50ms for one byte */
                TotalTimeout = 50 * DmaTxRequest.TransactionSizeInByte + 300;
            }
            else
            {
                DWORD DELTA;
                ASSERT(TotalTimeout!=0);
                DELTA = min(100,TotalTimeout/10);
                TotalTimeout = TotalTimeout - DELTA;
            }            
            Timeout = TotalTimeout;
            DmaTxRequest.TotalTimeOutInMs = TotalTimeout;
            DmaTxRequest.IntervalTimeOutInMs = 0; /* No Interval Time Out for TX*/

            *m_TxBytesReturned = (DWORD)-1;
            DmaTxRequest.pBytesReturned = m_TxBytesReturned;

            BytesHandled = 0;

            EnableXmitInterrupt(FALSE);

            m_HardwareLock.Unlock();
            ASSERT(DmaTxRequest.TransactionSizeInByte != 0);
            DEBUGMSG(ZONE_WRITE, (TEXT("UartDmaTxThread : Start Dma Tx for length %d with TotalTimeout %d; Tx Read Index %d\r\n"), DmaTxRequest.TransactionSizeInByte, DmaTxRequest.TotalTimeOutInMs,m_TxDmaBufferReadIndex));
            Rtn = DeviceIoControl(m_DMADeviceHandle,
                (DWORD)IOCTL_LPSSDMA_REQUEST_DMA_UART_TX,
                &DmaTxRequest,
                sizeof(DmaTxRequest),
                NULL,
                0,
                &BytesHandled,
                NULL);

            DWORD WaitRlt;

            WaitRlt = WaitForSingleObject(DmaTxRequest.hDmaCompleteEvent, max(Timeout,(DWORD)(Timeout+1000)));
            DEBUGMSG(ZONE_WRITE, (TEXT("UartDmaTxThread : Dma Tx finished with len %d and Wait Result %d\r\n"), *m_TxBytesReturned,WaitRlt));

            ASSERT(*m_TxBytesReturned == DmaTxRequest.TransactionSizeInByte);

            m_HardwareLock.Lock();
            m_TxDmaBufferReadIndex += *m_TxBytesReturned;
            if (m_TxDmaBufferReadIndex == m_TxDmaBufferWriteIndex)
            {
                m_TxDmaBufferReadIndex =  m_TxDmaBufferWriteIndex = 0;
            }
            if (WaitRlt == WAIT_OBJECT_0)
            {
                EnableXmitInterrupt(TRUE);
            }
            else
            {
                ASSERT(0);
            }
            m_HardwareLock.Unlock();


        }
    }
    return 0;
}

DWORD CPdd16550Isr::UartDmaRxThread()
{
    while(!IsTerminated())
    {
        if (WaitForSingleObject(m_UartDmaRxEvent,INFINITE) == WAIT_OBJECT_0)
        {
            DMA_REQUEST_CONTEXT DmaRxRequest;
            BOOL Rtn;
            DWORD BytesHandled;
            PHW_INDEP_INFO  pSerialHead = (PHW_INDEP_INFO)m_pMdd;
            UINT8       RxHwFifoLevel;
            //m_HardwareLock.Lock();
            RxHwFifoLevel = m_pReg16550->Read_RFL();
            DEBUGMSG(ZONE_READ, (TEXT("UartDmaRxThread :  with rx fifo level %d\r\n"), RxHwFifoLevel));            
            //m_HardwareLock.Unlock();
            if (RxHwFifoLevel)
            {
                m_HardwareLock.Lock();

                /* We don't know the driver loading sequence in WEC7, so had to check it before start dma transfer
                   must do it after we get m_HardwareLock.Lock();
                */
                if (m_DMADeviceHandle == NULL)
                {
                    m_DMADeviceHandle =  CreateFile(
                        DMA2_DEVICE_NAME, 
                        GENERIC_READ|GENERIC_WRITE,       
                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL
                        );
                    if (m_DMADeviceHandle == NULL
                        || m_DMADeviceHandle == INVALID_HANDLE_VALUE)
                    {
                        m_DMADeviceHandle = NULL;
                        m_HardwareLock.Unlock();
                        continue;
                    }

                }


                memset(&DmaRxRequest,0,sizeof(DmaRxRequest));
                DmaRxRequest.ChannelRx = (LPSSDMA_CHANNELS)m_DmaChannelRx;
                DmaRxRequest.ChannelTx = (LPSSDMA_CHANNELS)m_DmaChannelTx;

                DmaRxRequest.RequestLineRx = m_DmaRequestLineRx;
                DmaRxRequest.RequestLineTx = m_DmaRequestLineTx;          

                DmaRxRequest.TransactionType = TRANSACTION_TYPE_SINGLE_RX;
                DmaRxRequest.hDmaCompleteEvent = m_RxDmaCompleteEvent;

                DmaRxRequest.PeripheralFIFOPhysicalAddress =
                    (ULONG)(m_ioPhysicalBase.LowPart + (RECEIVE_BUFFER_REGISTER *(m_pIsrInfoVirt->uMultiplier)));

                ULONG        RoomLeft = 0;
                register DWORD RxWIndex=RxWrite(pSerialHead), RxRIndex=RxRead(pSerialHead);                   
                DmaRxRequest.SysMemPhysAddress.LowPart = m_RxDmaBufferPhyAddress.LowPart + RxWIndex;                
                DmaRxRequest.VirtualAdd = RxBuffWrite(pSerialHead);
                //RoomLeft = RxLength(pSerialHead) - (RxWIndex>=RxRIndex?RxWIndex- RxRIndex : RxLength(pSerialHead) - RxRIndex + RxWIndex);
                if (RxRIndex == 0)
                {
                    // have to leave one byte free.
                    RoomLeft = RxLength(pSerialHead) - RxWIndex - 1;
                }
                else
                {
                    RoomLeft = RxLength(pSerialHead) - RxWIndex;
                }
                if (RxRIndex > RxWIndex) 
                {
                    RoomLeft = RxRIndex - RxWIndex - 1;
                }                
                if (RoomLeft > MAX_RX_DMA_SIZE)
                {
                    RoomLeft = MAX_RX_DMA_SIZE;
                }
                if (pSerialHead->RxRemain)
                {
                    RoomLeft = pSerialHead->RxRemain < RoomLeft ? pSerialHead->RxRemain : RoomLeft;
                    pSerialHead->RxRemain = 0;
                }
                
                /* Compute total time to wait. Take product and add constant.*/
                ULONG           IntervalTimeout;    // The interval timeout                    
                ULONG           TotalTimeout;       // The Total Timeout            
                
                if ( MAXDWORD != m_CommTimeouts.ReadTotalTimeoutMultiplier ) 
                {
                    TotalTimeout = m_CommTimeouts.ReadTotalTimeoutMultiplier * RoomLeft +
                        m_CommTimeouts.ReadTotalTimeoutConstant;                        
                } else {
                    TotalTimeout = m_CommTimeouts.ReadTotalTimeoutConstant;                        
                }
                IntervalTimeout = m_CommTimeouts.ReadIntervalTimeout;
                
                if (IntervalTimeout == MAXDWORD && TotalTimeout)
                {
                    /* good case*/
                    // IntervalTimeout == MAXDWORD means return immediately after TotalTimeout
                    // so if TotalTimeout == 0, means return even if no bytes have been received which handled by mdd
                    RoomLeft = 1;    
                }
                
                if ( TotalTimeout == 0)
                {
                    /* Set a safty value, consider BAUD_300, 50ms for one byte */
                    TotalTimeout = 50 *  RoomLeft + 300;
                }
                
                DmaRxRequest.TransactionSizeInByte = RoomLeft;
                
                DmaRxRequest.IntervalTimeOutInMs = min(IntervalTimeout/2 + 1,100);
                DmaRxRequest.TotalTimeOutInMs = TotalTimeout;

                *m_RxBytesReturned = (DWORD)-1;
                DmaRxRequest.pBytesReturned = m_RxBytesReturned;
                BytesHandled = 0;
                m_HardwareLock.Unlock();
                if (RoomLeft != 0)
                {
                    DEBUGMSG(ZONE_READ, (TEXT("UartDmaRxThread : Start Dma Rx for length %d IntervalTimeOutInMs %d with Total timeout %d\r\n"), DmaRxRequest.TransactionSizeInByte, DmaRxRequest.IntervalTimeOutInMs,TotalTimeout));
                    Rtn = DeviceIoControl(m_DMADeviceHandle,
                        (DWORD)IOCTL_LPSSDMA_REQUEST_DMA_UART_RX,
                        &DmaRxRequest,
                        sizeof(DmaRxRequest),
                        NULL,
                        0,
                        &BytesHandled,
                        NULL);
                    
                    DWORD WaitRlt;
                    
                    WaitRlt = WaitForSingleObject(DmaRxRequest.hDmaCompleteEvent,max(TotalTimeout,TotalTimeout+1000));
                    DEBUGMSG(ZONE_READ, (TEXT("UartDmaRxThread : Dma Rx finished with len %d and Wait Result %d\r\n"), *m_RxBytesReturned,WaitRlt));
                    if(WaitRlt == WAIT_OBJECT_0)
                    {
                        DWORD interrupts = INTR_RX; 
                        m_RxDmaTransfered = *m_RxBytesReturned;
                        if (RxWIndex + m_RxDmaTransfered > m_RxDmaBufferSize)
                        {
                            m_RxWrapped = TRUE;
                            memcpy(m_RxDmaBufferVirtualAddress, m_RxDmaBufferVirtualAddress + m_RxDmaBufferSize, m_RxDmaTransfered - (m_RxDmaBufferSize - RxWIndex));    
                        }        
                        NotifyPDDInterrupt((INTERRUPT_TYPE)interrupts);
                    }
                    else
                    {
                        ASSERT(0);
                    }
                    m_RxBufferFullSleepTime = 1;
                }
                else
                {
                    m_RxBufferFullSleepTime *= 2;
                    if (m_RxBufferFullSleepTime > 1000)
                    {
                        m_RxBufferFullSleepTime = 1000;
                    }
                    DEBUGMSG(ZONE_THREAD, (TEXT("UartDmaRxThread :  ring buffer full with rxRead = 0x%x, rxWrite=0x%x, sleep %d milisecond\r\n"), RxRIndex, RxWIndex, m_RxBufferFullSleepTime));
                    Sleep(m_RxBufferFullSleepTime);
                    EnableRDAInterrupt(TRUE);
                }

            }
            else
            {
                EnableRDAInterrupt(TRUE);
            }

        }
    }
 
    return 0;
}


BOOL CPdd16550Isr::InitDMA(DMA_ADAPTER_OBJECT*  pAdapterObject)
{
    m_dwDeviceID = 0;
    if(!GetRegValue(L"DeviceID", (PBYTE)&m_dwDeviceID, sizeof(m_dwDeviceID)))
    {
        goto ErrExit;
    }

    if (m_dwDeviceID != UART0_PCI_VENDOR_ID
        && m_dwDeviceID != UART1_PCI_VENDOR_ID)
    {
        goto ErrExit;
    }

    /*COM1 Req Line 2 ,3*/
    /*COM2 Req Line 4 ,5*/
    m_DmaChannelTx = m_DmaChannelRx = m_DmaRequestLineTx = m_DmaRequestLineRx = (DWORD)(-1);
    /*TODO Make it to registry*/
    if (m_dwDeviceID == UART0_PCI_VENDOR_ID)
    {
        m_DmaChannelTx = 2;
        m_DmaChannelRx = 3;

        m_DmaRequestLineTx = 2;
        m_DmaRequestLineRx = 3;
        
    }

    if (m_dwDeviceID == UART1_PCI_VENDOR_ID)
    {
        m_DmaChannelTx = 4;
        m_DmaChannelRx = 5;

        m_DmaRequestLineTx = 4;
        m_DmaRequestLineRx = 5;

    }

    m_UartDmaTxEvent =  CreateEvent(0, FALSE, FALSE, NULL);
    m_UartDmaRxEvent =  CreateEvent(0, FALSE, FALSE, NULL);
    m_UartDmaTxThreadHandle = CreateThread(NULL,0,UartDmaTx,this,CREATE_SUSPENDED,NULL);
    m_UartDmaRxThreadHandle = CreateThread(NULL,0,UartDmaRx,this,CREATE_SUSPENDED,NULL);

    m_DMADeviceHandle = NULL;
    m_DMADeviceHandle = CreateFile(
        DMA2_DEVICE_NAME, 
        GENERIC_READ|GENERIC_WRITE,       
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if (m_DMADeviceHandle == INVALID_HANDLE_VALUE)
    {
        ASSERT(0);
        goto ErrExit;
    }

    m_TxDmaCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    m_RxDmaCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);

    // Allocate a physical buffer.
    m_TxDmaBufferLen = PAGE_SIZE * 4 - 2*sizeof(DWORD);
    m_TxDmaBufferVirtualAddress = 
        (PBYTE)HalAllocateCommonBuffer(pAdapterObject, m_TxDmaBufferLen + 2*sizeof(DWORD), &m_TxDmaBufferPhyAddress, FALSE);
 
    m_TxDmaBufferReadIndex =  m_TxDmaBufferWriteIndex = 0;

    m_TxBytesReturned = (DWORD*)(m_TxDmaBufferVirtualAddress + m_TxDmaBufferLen);
    *m_TxBytesReturned = 0xCCCCCCCC; /* add guard bytes in case dma write overflow*/
    m_TxBytesReturned++;

    m_RxDmaBufferSize = PAGE_SIZE * 4;
    m_RxDmaBufferLen = m_RxDmaBufferSize + MAX_RX_DMA_SIZE;
    m_RxDmaBufferVirtualAddress = 
        (PBYTE)HalAllocateCommonBuffer(pAdapterObject,  m_RxDmaBufferLen + 2*sizeof(DWORD), &m_RxDmaBufferPhyAddress, FALSE);

    m_RxBytesReturned = (DWORD*)(m_RxDmaBufferVirtualAddress + m_RxDmaBufferLen);
    *m_RxBytesReturned = 0xCCCCCCCC; /* add guard bytes in case dma write overflow*/
    m_RxBytesReturned++;


    ResumeThread(m_UartDmaTxThreadHandle);
    ResumeThread(m_UartDmaRxThreadHandle);

    m_pIsrInfoVirt->TxInDMA = TRUE;
    m_pIsrInfoVirt->RxInDMA = TRUE;
    
    return TRUE;
ErrExit:
    m_pIsrInfoVirt->TxInDMA = FALSE;
    m_pIsrInfoVirt->RxInDMA = FALSE;
    return FALSE;
}

BOOL CPdd16550Isr::Init()
{
    // IST Setup .
    DDKISRINFO ddi;
    if (GetIsrInfo(&ddi)!=ERROR_SUCCESS) {
        return FALSE;
    }
    if (CPdd16550::Init() &&
                ddi.dwIrq<0xff && ddi.dwIrq!=0 && ddi.szIsrDll[0] != 0 && ddi.szIsrHandler[0] !=0) { // USE IISR according registry.
                
        // Water Marker minimun can be 4 for ISR. 
        if (m_dwWaterMark<4)
            m_dwWaterMark = 4;
        // Get Intr Done RegKey.
        DWORD dwDoneIntrInISR = 0 ;
        if (!GetRegValue(PC_REG_INTRDONEINISR_VAL_NAME, (PBYTE)&dwDoneIntrInISR, PC_REG_INTRDONEINISR_VAL_LEN)) {
            dwDoneIntrInISR = 0;
        }

        // We create Shared Structure First.
        DWORD dwBlockSize=NUM_OF_SHARED_PAGE*PAGE_SIZE;
        PVOID pIsrAddress = VirtualAlloc(NULL,dwBlockSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE|PAGE_NOCACHE);
        m_pIsrInfoVirt = (ISR16550_INFO *)pIsrAddress ;

        DEBUGMSG (ZONE_INIT,(TEXT("SL_InstallSoftwareISR, VirtualAddr=0x%X Kernel Addr=0x%X\r\n"),m_pIsrInfoVirt ,pIsrAddress));
        
        // Translate IO to static.
        DDKWINDOWINFO dwi;
        if ( GetWindowInfo( &dwi)!=ERROR_SUCCESS || 
                dwi.dwNumMemWindows < 1 || 
                dwi.memWindows[0].dwBase == 0 || 
                dwi.memWindows[0].dwLen <  m_dwRegStride * (SCRATCH_REGISTER+1)) 
            return FALSE;
        DWORD dwInterfaceType = MaximumInterfaceType;
        if (m_ActiveReg.IsKeyOpened() && 
                m_ActiveReg.GetRegValue( DEVLOAD_INTERFACETYPE_VALNAME, (PBYTE)&dwInterfaceType,sizeof(DWORD))) {
            dwi.dwInterfaceType = dwInterfaceType;
        }
        PHYSICAL_ADDRESS    ioPhysicalBase = { dwi.memWindows[0].dwBase, 0};
        ULONG               inIoSpace = 0;
        m_ioPhysicalBase = ioPhysicalBase;
        DEBUGMSG (ZONE_INIT,(TEXT("SL_InstallSoftwareISR, memory base add=0x%X\r\n"),ioPhysicalBase));
        if (!TransBusAddrToStatic((INTERFACE_TYPE)dwi.dwInterfaceType, 0, ioPhysicalBase,dwi.memWindows[0].dwLen , &inIoSpace,&m_pIoStaticAddr)) {
            DEBUGMSG(ZONE_ERROR,(TEXT("CPdd16550Isr::Init!TransBusAddrToStatic(%x,%x) call failed.\r\n"), 
                    dwi.memWindows[0].dwBase,dwi.memWindows[0].dwLen));
            return FALSE;
        }
        DEBUGMSG (ZONE_INIT,(TEXT("SL_InstallSoftwareISR, virtual  add=0x%X\r\n"),m_pIoStaticAddr));

        // Intialize the Structure and then load IISR.
        if (m_pIsrInfoVirt) {
            DWORD dwRcvBlockSize;
            DWORD dwXmitBlockSize;
            m_pIsrInfoVirt->pBlockPhysAddr=pIsrAddress;
            m_pIsrInfoVirt->dwBlockSize=dwBlockSize;
            m_pIsrInfoVirt->dwIntrDoneInISR = dwDoneIntrInISR;
            m_pIsrInfoVirt->dwReceiveHWWaterMaker=GetWaterMark();
            m_pIsrInfoVirt->InBufferOffset=sizeof(ISR16550_INFO);
            dwRcvBlockSize=dwBlockSize - sizeof(ISR16550_INFO);
            dwRcvBlockSize = dwRcvBlockSize * 2 / 3; // Harf for xmitting harf for Receiving. receive buffer is double size of send
            dwRcvBlockSize = (dwRcvBlockSize/sizeof(DWORD))*sizeof(DWORD); // Make DWORD alignment.
            m_pIsrInfoVirt->OutBufferOffset=sizeof(ISR16550_INFO)+dwRcvBlockSize;
            // Initial Receive 
            m_pReceiveBuffer=(PRcvDataBuffer)((PBYTE)m_pIsrInfoVirt+m_pIsrInfoVirt->InBufferOffset);
            m_pReceiveBuffer->dwBufferSize=(dwRcvBlockSize-sizeof(RcvDataBuffer))/sizeof(IIR_EVENT);
            m_pReceiveBuffer->dwWaterMark=m_pReceiveBuffer->dwBufferSize/2;
            m_pReceiveBuffer->dwFIFO_In = m_pReceiveBuffer->dwFIFO_Out=0;
            // Inital Xmit Buffer.
            m_pXmitBuffer=(PXmitDataBuffer)((PBYTE)m_pIsrInfoVirt+m_pIsrInfoVirt->OutBufferOffset);
            ASSERT(m_pIsrInfoVirt->OutBufferOffset+sizeof(XmitDataBuffer)<dwBlockSize);
            dwXmitBlockSize =dwBlockSize- m_pIsrInfoVirt->OutBufferOffset;
            m_pXmitBuffer->dwBufferSize=dwXmitBlockSize-sizeof(XmitDataBuffer);
            m_pXmitBuffer->dwWaterMark=m_pXmitBuffer->dwBufferSize/2;
            m_pXmitBuffer->dwFIFO_In= m_pXmitBuffer->dwFIFO_Out=0;
            //Set Hardware Info.
            m_pIsrInfoVirt->SysIntr=m_dwSysIntr;
            m_pIsrInfoVirt->pIoAddress = (PBYTE)m_pIoStaticAddr;
            m_pIsrInfoVirt->lIoSpace = inIoSpace;
            m_pIsrInfoVirt->uMultiplier= m_dwRegStride;
            //m_bMoreXmitData=FALSE;
            m_pIsrInfoVirt->bIntrBypass=FALSE;
            // Install The ISR.
            DEBUGMSG (ZONE_INIT,(TEXT("SL_InstallSoftwareISR, SysIntr=0x%X,Irq=0x%X,ioAddr==0x%X \r\n"),
                m_pIsrInfoVirt->SysIntr,ddi.dwIrq,m_pIsrInfoVirt->pIoAddress));
            
            m_hIsrHandler = LoadIntChainHandler(ddi.szIsrDll, ddi.szIsrHandler, (BYTE)ddi.dwIrq);
            if (m_hIsrHandler == NULL) {
                DEBUGMSG(ZONE_ERROR, (TEXT("SL_InstallSoftwareISR: LoadIntChainHandler(%s, %s, %d) failed\r\n"),
                    ddi.szIsrDll, ddi.szIsrHandler,ddi.dwIrq));
                return FALSE;;
            }
            if (!KernelLibIoControl(m_hIsrHandler, IOCTL_ISR16550_INFO, pIsrAddress, dwBlockSize, NULL, 0, NULL)) {
                DEBUGMSG(ZONE_ERROR,(TEXT("SL_InstallSoftwareISR: KernelLibIoControl call failed.\r\n")));
                KernelLibIoControl(m_hIsrHandler, IOCTL_ISR16550_UNLOAD, (LPVOID)&m_pIsrInfoVirt, sizeof(ISR16550_INFO), NULL, 0, NULL);
                return FALSE;
            }
            

            m_dwDeviceID = 0;
            if(!GetRegValue(L"DeviceID", (PBYTE)&m_dwDeviceID, sizeof(m_dwDeviceID)))
            {
                ASSERT(0);
            }
            

            m_pIsrInfoVirt->ISRLockFailure = 0;
            m_pIsrInfoVirt->ISRLock = 0;
            m_CTSOFFCount = 0;
            m_ForcePIO = 0;
            if (!GetRegValue(PC_REG_FORCEPIO_VAL_NAME,(PBYTE)&m_ForcePIO,sizeof(DWORD))) {
                m_ForcePIO = 0;
            }
            if (m_ForcePIO)
            {
                m_pIsrInfoVirt->TxInDMA = FALSE;
                m_pIsrInfoVirt->RxInDMA = FALSE;
            }else
            {
                ASSERT(dwInterfaceType != MaximumInterfaceType);
                m_AdapterObject.ObjectSize = sizeof(m_AdapterObject);
                m_AdapterObject.InterfaceType = (INTERFACE_TYPE)dwInterfaceType;
                m_AdapterObject.BusNumber = 0;
                InitDMA(&m_AdapterObject);
            }
            return TRUE;
        }
        else {
            DEBUGMSG (ZONE_ERROR,(TEXT("SL_InstallSoftwareISR, Cano not alloc Phys Buffer size=0x%X - ERROR\r\n"),dwBlockSize ));
        }
    }
    return FALSE;
}

void CPdd16550Isr::PostInit()
{
    CPdd16550::PostInit();
    // if Rx DMA is enabled override the mdd Rx buffer with DMA buffer
    if (m_pIsrInfoVirt->RxInDMA)
    {
        PHW_INDEP_INFO  pSerialHead = (PHW_INDEP_INFO)m_pMdd;
        pSerialHead->RxBufferInfo.RxCharBuffer = m_RxDmaBufferVirtualAddress;
        pSerialHead->RxBufferInfo.Length = m_RxDmaBufferSize;        
           DEBUGMSG(ZONE_INIT, (TEXT("buffer %x %d "),m_RxDmaBufferVirtualAddress,m_RxDmaBufferSize));

    }
   
}

inline BOOL CPdd16550Isr::PeekIIRData(PBYTE pbInt,PBYTE pbData)
{
    ASSERT(m_pReceiveBuffer!=NULL);
    if (GetRcvDataSize(m_pReceiveBuffer)>0) {
        if (pbInt)
            *pbInt=m_pReceiveBuffer->bBuffer[m_pReceiveBuffer->dwFIFO_Out].bIntFlag;
        if (pbData) 
            *pbData=m_pReceiveBuffer->bBuffer[m_pReceiveBuffer->dwFIFO_Out].bIntData;
        return TRUE;
    }
    else
        return FALSE;
}
inline BOOL CPdd16550Isr::ReadIIRData(PBYTE pbInt,PBYTE pbData)
{
    m_HardwareLock.Lock();    
    BOOL bReturn = PeekIIRData(pbInt,pbData);
    if (bReturn)
        m_pReceiveBuffer->dwFIFO_Out=IncRcvIndex(m_pReceiveBuffer,m_pReceiveBuffer->dwFIFO_Out);
    m_HardwareLock.Unlock();
    return bReturn;
}
inline BOOL CPdd16550Isr::WriteFIFOData(BYTE bData)
{
    if ( GetXmitAvailableBuffer(m_pXmitBuffer)>2) {
        m_pXmitBuffer->bBuffer[m_pXmitBuffer->dwFIFO_In]=bData;
        m_pXmitBuffer->dwFIFO_In=IncXmitIndex(m_pXmitBuffer,m_pXmitBuffer->dwFIFO_In);
        return TRUE;
    }
    else
        return FALSE;
}

DWORD CPdd16550Isr::ThreadRun()
{
    DEBUGMSG(ZONE_THREAD, (TEXT("CPdd16550isr::ThreadRun IST Started.\r\n")));
    while ( m_hISTEvent!=NULL && !IsTerminated()) {
        if (WaitForSingleObject( m_hISTEvent,INFINITE)==WAIT_OBJECT_0) {
            UCHAR bInt,bData;
            if (m_pIsrInfoVirt && m_pIsrInfoVirt->bStartRxDma)
            {
                //m_HardwareLock.Lock();
                m_pIsrInfoVirt->bStartRxDma = FALSE;
                SetEvent(m_UartDmaRxEvent);    
                //m_HardwareLock.unLock();
            }
            while (!IsTerminated() && PeekIIRData(&bInt,&bData)) {
                DEBUGMSG(ZONE_THREAD,
                  (TEXT("CPdd16550isr::ThreadRun IIR=%x, bData=%x\r\n"),bInt,bData));
                DWORD interrupts=0;
                switch (  bInt & SERIAL_IIR_INT_MASK ) {
                case SERIAL_IIR_RLS:
                    interrupts |= INTR_LINE;
                    break;
                case SERIAL_IIR_CTI: case SERIAL_IIR_CTI_2: case SERIAL_IIR_RDA:
                    interrupts |= INTR_RX;
                    break;
                case SERIAL_IIR_THRE:
                    interrupts |= INTR_TX;
                    break;
                case SERIAL_IIR_MS :
                    interrupts |= INTR_MODEM;
                    break;
                default:
                    DEBUGMSG(ZONE_ERROR,(TEXT("CPdd16550isr::ThreadRun: Wrong Interrupt Event IIR=%x, bData=%x\r\n"),bInt,bData));
                    ReadIIRData(NULL,NULL);
                    ASSERT(FALSE);
                }
                NotifyPDDInterrupt((INTERRUPT_TYPE)interrupts);
            }
            // ISR did call Interrupt done, So no need to calling it here.
            if (m_pIsrInfoVirt==0 || m_pIsrInfoVirt->dwIntrDoneInISR == 0 )
                InterruptDone(m_dwSysIntr);
        }
        else
            ASSERT(FALSE);
    }
    return 1;

}
// Receive
BOOL  CPdd16550Isr::InitReceive(BOOL bInit)
{
    if (m_pIsrInfoVirt->RxInDMA)
    {
        m_HardwareLock.Lock();    
        if (bInit) {         
            m_pReg16550->Write_FCR((m_pReg16550->Read_FCR() & ~SERIAL_IIR_FIFOS_ENABLED) | SERIAL_FCR_RCVR_RESET | SERIAL_FCR_ENABLE);
            m_pReg16550->Write_IER(m_pReg16550->Read_IER() | SERIAL_IER_RDA);
            m_pReg16550->Read_LSR(); // Clean Line Interrupt.
        }
        else {
            m_pReg16550->Write_IER(m_pReg16550->Read_IER() & ~SERIAL_IER_RDA);
        }
        m_HardwareLock.Unlock();
    }
    else
    {
        return CPdd16550::InitReceive(bInit);
    }
    return TRUE;
}


ULONG   CPdd16550Isr::ReceiveInterruptHandler(PUCHAR pRxBuffer,ULONG *pBufflen)
{
    DWORD dwBytesDropped = 0;
    if (pRxBuffer && pBufflen ) {
        DWORD dwBytesStored = 0 ;
        DWORD dwRoomLeft = *pBufflen;
        m_bReceivedCanceled = FALSE;
        if (m_pIsrInfoVirt->RxInDMA)
        {
            dwBytesStored = m_RxDmaTransfered;
            m_RxDmaTransfered = 0;
            EnableRDAInterrupt(TRUE);
        }
        else
        {
            m_bReceivedCanceled = FALSE;
            m_HardwareLock.Lock();
            BYTE ubIIR;

            if (PeekIIRData(&ubIIR,NULL)) {
                ubIIR &= SERIAL_IIR_INT_MASK;
                if (ubIIR == SERIAL_IIR_CTI ||  ubIIR == SERIAL_IIR_CTI_2 || ubIIR ==SERIAL_IIR_RDA ) { // Receive Data.
                    ReadIIRData(NULL, NULL);
                    while (dwRoomLeft && !m_bReceivedCanceled) {
                        UCHAR uLineStatus = GetLineStatus();
                        if ((uLineStatus & SERIAL_LSR_DR)!=0 ) {
                        UCHAR uData = m_pReg16550->Read_Data();
                            if (DataReplaced(&uData,(uLineStatus & SERIAL_LSR_PE)!=0)) {
                                *pRxBuffer++ = uData;
                                dwRoomLeft--;
                                dwBytesStored++;
                            }
                        }
                        else
                            break;
                    }
               }
            }
            m_HardwareLock.Unlock();
            EnableRDAInterrupt(TRUE);
        }

        if (m_bReceivedCanceled)
            dwBytesStored = 0;

        *pBufflen = dwBytesStored;
    }
    else {
        ASSERT(FALSE);
    }
    return dwBytesDropped;
}
ULONG   CPdd16550Isr::CancelReceive()
{
    m_HardwareLock.Lock();
    // Cancel anything in software fifo.
    InterruptMask(m_dwSysIntr,TRUE);
    // We have to Mask interrupt because ISR also touching this variable.
    m_pReceiveBuffer->dwFIFO_In = m_pReceiveBuffer->dwFIFO_Out; // Data Gone

    m_bReceivedCanceled = TRUE;
    m_pReg16550->Write_FCR(m_pReg16550->Read_FCR() | SERIAL_FCR_RCVR_RESET);
    m_pReg16550->Write_IER(m_pReg16550->Read_IER() | SERIAL_IER_RDA);

    InterruptMask(m_dwSysIntr,FALSE);
    m_HardwareLock.Unlock();
    return 0;
}
BOOL   CPdd16550Isr::EnableRDAInterrupt(BOOL bEnable)
{
    m_HardwareLock.Lock();

    if (m_dwSysIntr!=SYSINTR_UNDEFINED)
        InterruptMask(m_dwSysIntr,TRUE);
    
    if (bEnable)
        m_pReg16550->Write_IER(m_pReg16550->Read_IER() | SERIAL_IER_RDA);
    else
        m_pReg16550->Write_IER(m_pReg16550->Read_IER() & ~SERIAL_IER_RDA);

    if (m_dwSysIntr!=SYSINTR_UNDEFINED)
        InterruptMask(m_dwSysIntr,FALSE);

    m_HardwareLock.Unlock();
    return TRUE;
}

BOOL CPdd16550Isr::InitXmit(BOOL bInit)
{
    m_HardwareLock.Lock();    
    if (bInit) {
        UINT8 FCR;
        FCR = m_pReg16550->Read_FCR() & ~SERIAL_FCR_TXE_TRIGGER_MASK;
        if (m_pIsrInfoVirt->TxInDMA)
        {
            m_pReg16550->Write_FCR( FCR | SERIAL_FCR_TXMT_RESET | SERIAL_FCR_ENABLE | SERIAL_FCR_TET_HALF_MASK);
        }else
        {
            m_pReg16550->Write_FCR( FCR | SERIAL_FCR_TXMT_RESET | SERIAL_FCR_ENABLE);
        }
        m_XmitFifoEnable = TRUE;
    }
    else 
        WaitForTransmitterEmpty(100);
    
    m_HardwareLock.Unlock();
    return TRUE;
}

void CPdd16550Isr::XmitInterruptHandlerDMA(PUCHAR pTxBuffer, ULONG *pBuffLen)
{
    DEBUGCHK(pBuffLen!=NULL);
    BYTE bInt;

    m_HardwareLock.Lock();    
    while (PeekIIRData(&bInt,NULL) && (bInt&SERIAL_IIR_INT_MASK) == SERIAL_IIR_THRE ) { // Dump Xmit Empty Signal in Input Queue
        ReadIIRData(NULL,NULL);
    }
    if (*pBuffLen == 0) {
        EnableXmitInterrupt(FALSE);
    }else
    {
        DEBUGCHK(pTxBuffer);
        DWORD dwDataAvaiable = *pBuffLen;
        *pBuffLen = 0;

        if ((m_DCB.fOutxCtsFlow && IsCTSOff())) { // We are Follow off
            DEBUGMSG(ZONE_ERROR,(TEXT("CPdd16550::XmitInterruptHandlerDMA! Flow Off, Data may Discard.\r\n")));
            
            if (m_CTSOFFCount > 10 )
            {
                EnableXmitInterrupt(FALSE);
            }else
            {
                /* try to wait some time for CTS*/
                Sleep(100); 
                EnableXmitInterrupt(TRUE);
            }
            m_CTSOFFCount++;
        }else if (dwDataAvaiable){
            DWORD dwByteWrite=0;
            dwByteWrite = min(dwDataAvaiable,m_TxDmaBufferLen - m_TxDmaBufferWriteIndex);
            m_CTSOFFCount = 0;
            memcpy(m_TxDmaBufferVirtualAddress + m_TxDmaBufferWriteIndex,
                pTxBuffer,
                dwByteWrite);
            
            m_TxDmaBufferWriteIndex += dwByteWrite;
            SetEvent(m_UartDmaTxEvent);
            *pBuffLen = dwByteWrite;
            DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::XmitInterruptHandler! Write %d byte to FIFO\r\n"),dwByteWrite));
        }
    }
    m_HardwareLock.Unlock();
}
void    CPdd16550Isr::XmitInterruptHandler(PUCHAR pTxBuffer, ULONG *pBuffLen)
{
    DEBUGCHK(pBuffLen!=NULL);
    BYTE bInt;

    if (m_pIsrInfoVirt->TxInDMA)
    {
        return XmitInterruptHandlerDMA(pTxBuffer,pBuffLen);
    }

    m_HardwareLock.Lock();    
    while (PeekIIRData(&bInt,NULL) && (bInt&SERIAL_IIR_INT_MASK) == SERIAL_IIR_THRE ) { // Dump Xmit Empty Signal in Input Queue
        ReadIIRData(NULL,NULL);
    }
    
    if (*pBuffLen == 0) {
        if (GetXmitDataSize(m_pXmitBuffer)== 0 ) {
            EnableXmitInterrupt(FALSE);
        }
        if (GetDCB().fRtsControl == RTS_CONTROL_TOGGLE ) { 
            // If it is TOGGLE, we have to wait Transmit Buffer Empty before return
            // because mdd will clear RTS
            WaitForTransmitterEmpty(100);
        }
        
    }
    else {
        DEBUGCHK(pTxBuffer);
        PulseEvent(m_XmitFlushDone);
        DWORD dwDataAvaiable = *pBuffLen;
        *pBuffLen = 0;

        if (!(SERIAL_MCR_AFCE & m_pReg16550->Read_MCR()) && ((m_DCB.fOutxCtsFlow && IsCTSOff()) ||(m_DCB.fOutxDsrFlow && IsDSROff()))) { // We are Follow off
            DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::XmitInterruptHandler! Flow Off, Data Discard.=== CTSOFF %d\r\n"), m_CTSOFFCount));

            if (m_CTSOFFCount > 10 )
            {
                EnableXmitInterrupt(FALSE);
            }else
            {
                /* try to wait some time for CTS*/
                Sleep(100); 
                EnableXmitInterrupt(TRUE);
            }
            m_CTSOFFCount++;
        } else {
            DWORD dwByteWrite = 0 ;
            DWORD dwWriteSize = (SERIAL_FIFO_DEPTH - m_pReg16550->Read_TFL());

            while((dwByteWrite<dwWriteSize) && (dwDataAvaiable>0)) {
                m_pReg16550->Write_DATA(*pTxBuffer);
                pTxBuffer ++;
                dwByteWrite++;
                dwDataAvaiable--;
            }

            EnableXmitInterrupt(TRUE);        

            *pBuffLen = dwByteWrite;
            DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::XmitInterruptHandler! Write %d byte to FIFO\r\n"),dwByteWrite));
        }
    }
    m_HardwareLock.Unlock();
    
}
void CPdd16550Isr::XmitComChar(UCHAR ComChar)
{
    // This function has to pull until the Data can be sent out.
    BOOL bDone = FALSE;
    do {
        m_HardwareLock.Lock(); 
        if ( WriteFIFOData(ComChar) ) {  // If Empty.
            bDone = TRUE;
        }
        else {
            EnableXmitInterrupt(TRUE);
        }
        m_HardwareLock.Unlock();
        if (!bDone)
           WaitForSingleObject(m_XmitFlushDone, (ULONG)1000); 
    }
    while (!bDone);

}
BOOL   CPdd16550Isr::EnableXmitInterrupt(BOOL bEnable)
{
    m_HardwareLock.Lock();

    if (m_dwSysIntr!=SYSINTR_UNDEFINED)
        InterruptMask(m_dwSysIntr,TRUE);
    
    if (bEnable)
        m_pReg16550->Write_IER ( m_pReg16550->Read_IER() | SERIAL_IER_THR);
    else
         m_pReg16550->Write_IER ( m_pReg16550->Read_IER() & ~SERIAL_IER_THR);

    if (m_dwSysIntr!=SYSINTR_UNDEFINED)
        InterruptMask(m_dwSysIntr,FALSE);

    m_HardwareLock.Unlock();
    return TRUE;
}

BOOL  CPdd16550Isr::CancelXmit()
{
    m_HardwareLock.Lock();
    // Cancel anything in software fifo.
    InterruptMask(m_dwSysIntr,TRUE);
    
    // We have to Mask interrupt because ISR also touching this variable.
    m_pXmitBuffer->dwFIFO_In = m_pXmitBuffer->dwFIFO_Out; // Data Gone
    
    if (m_pIsrInfoVirt->TxInDMA)
    {
        m_TxDmaBufferReadIndex = m_TxDmaBufferWriteIndex = 0;
    }

    m_pReg16550->Write_FCR(m_pReg16550->Read_FCR() | SERIAL_FCR_TXMT_RESET | SERIAL_FCR_ENABLE);
    m_pReg16550->Write_IER ( m_pReg16550->Read_IER() & ~SERIAL_IER_THR);

    InterruptMask(m_dwSysIntr,FALSE);
    m_HardwareLock.Unlock();
    return 0;
}
//
//Line Function
void  CPdd16550Isr::LineInterruptHandler()
{
    BYTE bInt,bData;
    DEBUGCHK(PeekIIRData(&bInt,NULL) && (bInt&SERIAL_IIR_INT_MASK) == SERIAL_IIR_RLS);
    if (ReadIIRData(&bInt, &bData) && ((bInt&SERIAL_IIR_INT_MASK)== SERIAL_IIR_RLS)) {
        ULONG ulError = 0;
        if (bData & SERIAL_LSR_OE ) {
            ulError |=  CE_OVERRUN;
        }
        if (bData & SERIAL_LSR_PE) {
            ulError |= CE_RXPARITY;
        }
        if (bData & SERIAL_LSR_FE) {
            ulError |=  CE_FRAME;
        }
        if (ulError)
            SetReceiveError(ulError);
        if (bData & SERIAL_LSR_BI) {
             EventCallback(EV_BREAK);
        }
    }
}
//
//Modem Function
 void    CPdd16550Isr::ModemInterruptHandler()
{
    BYTE bInt,ubModemStatus;
    DEBUGCHK(PeekIIRData(&bInt,NULL) && (bInt & SERIAL_IIR_INT_MASK) == SERIAL_IIR_MS);
    if (ReadIIRData(&bInt, &ubModemStatus) && ((bInt & SERIAL_IIR_INT_MASK)== SERIAL_IIR_MS)) {
    // Event Notification.
        ULONG Events = 0;
        if (ubModemStatus & SERIAL_MSR_DCTS)
            Events |= EV_CTS;
        if ( ubModemStatus  & SERIAL_MSR_DDSR )
            Events |= EV_DSR;
        if ( ubModemStatus  & SERIAL_MSR_TERI )
            Events |= EV_RING;
        if ( ubModemStatus  & SERIAL_MSR_DDCD )
            Events |= EV_RLSD;
        if (Events!=0)
            EventCallback(Events, (ubModemStatus & SERIAL_MSR_DCD)!=0?MS_RLSD_ON:0);
    }
}

