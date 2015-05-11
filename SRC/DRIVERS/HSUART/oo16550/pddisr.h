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

    Platform dependent Serial definitions for 16550  controller.

Notes: 
--*/
#ifndef __PDDISR_H_
#define __PDDISR_H_
#include <Pdd16550.h>
#include <isr16550.h>
class CPdd16550Isr: public CPdd16550 {
public:
    CPdd16550Isr(LPCTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj);
    virtual ~CPdd16550Isr();
    virtual BOOL Init();
	virtual void PostInit();
    BOOL InitDMA(DMA_ADAPTER_OBJECT*  pAdapterObject);
//
// IST
private:
    virtual DWORD ThreadRun();   // IST
protected:
    inline BOOL PeekIIRData(PBYTE pbInt, PBYTE pbData);
    inline BOOL ReadIIRData(PBYTE pbInt, PBYTE pbData);
    inline BOOL WriteFIFOData(BYTE bData);
public:
//

// Xmit Function.
    virtual BOOL    InitXmit(BOOL bInit);
    virtual void    XmitInterruptHandler(PUCHAR pTxBuffer, ULONG *pBuffLen);
    virtual void    XmitComChar(UCHAR ComChar);
    virtual BOOL    EnableXmitInterrupt(BOOL bEnable);
    virtual BOOL    CancelXmit();
//
private:
    void    XmitInterruptHandlerDMA(PUCHAR pTxBuffer, ULONG *pBuffLen);

//  Rx Function.
public:
    virtual BOOL    InitReceive(BOOL bInit);
    virtual ULONG   ReceiveInterruptHandler(PUCHAR pRxBuffer,ULONG *pBufflen);
    virtual ULONG   CancelReceive();
    virtual BOOL    EnableRDAInterrupt(BOOL bEnable);
//
//Line Function
    virtual void    LineInterruptHandler();
//
//Modem Function
    virtual void    ModemInterruptHandler();

protected:
    // now hardware specific fields Duplicated Info
    PVOID       m_pIoStaticAddr;
    PVOID       m_pVirtualStaticAddr;// Static Address.

    volatile ISR16550_INFO     * m_pIsrInfoVirt;
    volatile XmitDataBuffer    * m_pXmitBuffer;
    volatile RcvDataBuffer     * m_pReceiveBuffer;
    HANDLE          m_hIsrHandler;
//    BOOL            bMoreXmitData;// For software xmit buffering
//    DWORD           dwIsrIndex; // For test only


private:
    DWORD   m_dwDeviceID;
    //DWORD   m_ForcePIO;

    HANDLE  m_DMADeviceHandle;
    HANDLE  m_UartDmaTxThreadHandle;
    HANDLE  m_UartDmaRxThreadHandle;
    HANDLE  m_UartDmaTxEvent;
    HANDLE  m_UartDmaRxEvent;

    DWORD   m_DmaChannelRx;
    DWORD   m_DmaChannelTx;
    DWORD   m_DmaRequestLineRx;
    DWORD   m_DmaRequestLineTx;

    HANDLE  m_TxDmaCompleteEvent;
    HANDLE  m_RxDmaCompleteEvent;

    DWORD*  m_TxBytesReturned;
    DWORD*  m_RxBytesReturned;

    PBYTE  m_TxDmaBufferVirtualAddress;
    PBYTE  m_RxDmaBufferVirtualAddress;

    DWORD  m_TxDmaBufferLen;
    DWORD  m_RxDmaBufferLen;

    DWORD  m_TxDmaBufferReadIndex;
    DWORD  m_TxDmaBufferWriteIndex;

    PHYSICAL_ADDRESS   m_TxDmaBufferPhyAddress;
    PHYSICAL_ADDRESS   m_RxDmaBufferPhyAddress;

    PHYSICAL_ADDRESS   m_ioPhysicalBase;
	DWORD m_RxDmaBufferSize;
	DWORD m_RxDmaTransfered;
	BOOL  m_RxWrapped;
	DWORD m_RxBufferFullSleepTime;
	DWORD m_RxWaitTimeout;
    
    DWORD m_CTSOFFCount;
    DMA_ADAPTER_OBJECT m_AdapterObject;
public:
    DWORD UartDmaTxThread(); 
    DWORD UartDmaRxThread(); 
};
#define PC_REG_INTRDONEINISR_VAL_NAME TEXT("IntrDoneInISR") 
#define PC_REG_INTRDONEINISR_VAL_LEN  sizeof( DWORD )
#define PC_REG_FORCEPIO_VAL_NAME TEXT("ForcePIO")
#endif
