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
#include <hw16550.h>
#include <Serdbg.h>

#include "pdd16550.h"
#include <isr16550.h>
#include <nkintr.h>
extern "C" BOOL    TranslateBusAddr(HANDLE hBusAccess,
            INTERFACE_TYPE  InterfaceType,ULONG BusNumber,PHYSICAL_ADDRESS BusAddress,PULONG AddressSpace,PPHYSICAL_ADDRESS TranslatedAddress );

CReg16550::CReg16550(__in_bcount(dwStride*(SCRATCH_REGISTER+1)) PBYTE pRegAddr, DWORD dwStride) 
:   m_pReg (pRegAddr)
,   m_dwStride(dwStride)
{
    m_pData =  pRegAddr + (dwStride * RECEIVE_BUFFER_REGISTER);    
    m_pIER  =  pRegAddr + (dwStride * INTERRUPT_ENABLE_REGISTER);
    m_pIIR_FCR=pRegAddr + (dwStride * INTERRUPT_IDENT_REGISTER);
    m_pLCR  =  pRegAddr + (dwStride * LINE_CONTROL_REGISTER);
    m_pMCR  =  pRegAddr + (dwStride * MODEM_CONTROL_REGISTER);
    m_pLSR  =  pRegAddr + (dwStride * LINE_STATUS_REGISTER);
    m_pMSR  =  pRegAddr + (dwStride * MODEM_STATUS_REGISTER);
    m_pTFL  =  pRegAddr + (dwStride * UART_TX_FIFO_LEVEL_REGISTER);
    m_pRFL  =  pRegAddr + (dwStride * UART_RX_FIFO_LEVEL_REGISTER);
    m_pSRC  =  pRegAddr + (dwStride * SCRATCH_REGISTER);
    m_pCLK =   (PULONG)(pRegAddr + (PRV_CLOCKS));
    m_FCR = 0;
    m_pGeneral = pRegAddr + PRV_GENERAL;
    m_fIsBackedUp = FALSE;
}
void CReg16550::Backup()
{
    m_fIsBackedUp = TRUE;
    m_IERBackup = Read_IER();
    m_LCRBackup = Read_LCR();
    m_MCRBackup = Read_MCR();
    
}
void CReg16550::Restore()
{
    if (m_fIsBackedUp) {
        Write_FCR(m_FCR);
        Write_IER(m_IERBackup);
        Write_LCR(m_LCRBackup);
        Write_MCR( m_MCRBackup);
        Write_BaudRate(m_BaudRate);
        m_fIsBackedUp = FALSE;

    }
}
#ifdef DEBUG
void CReg16550::DumpRegister()
{
    UINT8 byte;
    byte = Read_LSR() ;
    NKDbgPrintfW(TEXT("16550 lsr: \t%2.2X\t"), byte);
    if ( byte & SERIAL_LSR_DR )
        NKDbgPrintfW(TEXT("DataReady "));
    if ( byte & SERIAL_LSR_OE )
        NKDbgPrintfW(TEXT("OverRun "));
    if ( byte & SERIAL_LSR_PE )
        NKDbgPrintfW(TEXT("ParityErr "));
    if ( byte & SERIAL_LSR_FE )
        NKDbgPrintfW(TEXT("FramingErr "));
    if ( byte & SERIAL_LSR_BI )
        NKDbgPrintfW(TEXT("BreakIntpt "));
    if ( byte & SERIAL_LSR_THRE )
        NKDbgPrintfW(TEXT("THREmpty "));
    if ( byte & SERIAL_LSR_TEMT )
        NKDbgPrintfW(TEXT("TXEmpty "));
    if ( byte & SERIAL_LSR_FIFOERR )
        NKDbgPrintfW(TEXT("FIFOErr "));
    NKDbgPrintfW(TEXT("\r\n"));

    byte = Read_Data() ;
    NKDbgPrintfW(TEXT("16550 rbr/thr:\t%2.2X\r\n"), byte);

    byte = Read_IER();
    NKDbgPrintfW(TEXT("16550 IER: \t%2.2X\t"), byte);
    if ( byte & SERIAL_IER_RDA )
        NKDbgPrintfW(TEXT("RXData "));
    if ( byte & SERIAL_IER_THR )
        NKDbgPrintfW(TEXT("TXData "));
    if ( byte & SERIAL_IER_RLS )
        NKDbgPrintfW(TEXT("RXErr "));
    if ( byte & SERIAL_IER_MS )
        NKDbgPrintfW(TEXT("ModemStatus "));
    NKDbgPrintfW(TEXT("\r\n"));

    byte = Read_IIR();
    NKDbgPrintfW(TEXT("16550 iir: \t%2.2X\t"), byte);
    if ((byte & SERIAL_IIR_INT_INVALID) == 0) {
        NKDbgPrintfW(TEXT("IntPending "));
        switch (byte & SERIAL_IIR_INT_MASK) {
        case SERIAL_IIR_RLS :
            NKDbgPrintfW(TEXT("Line Status "));
            break;
        case SERIAL_IIR_RDA : case SERIAL_IIR_CTI :case SERIAL_IIR_CTI_2:
            NKDbgPrintfW(TEXT("RXData "));
            break;        
        case SERIAL_IIR_THRE:
            NKDbgPrintfW(TEXT("TXData "));
            break;
        case SERIAL_IIR_MS :
            NKDbgPrintfW(TEXT("ModemStatus "));
            break;
        default:
            NKDbgPrintfW(TEXT("Unknown "));
            break;
        }
    }
    NKDbgPrintfW(TEXT("\r\n"));

    byte = Read_LCR();
    NKDbgPrintfW(TEXT("16550 lcr: \t%2.2X\t"), byte);

    NKDbgPrintfW(TEXT("%dBPC "), ((byte & 0x03)+5) );

    if ( byte & SERIAL_LCR_DLAB )
        NKDbgPrintfW(TEXT("DLAB "));
    if ( byte & SERIAL_LCR_DLAB )
        NKDbgPrintfW(TEXT("Break "));
    NKDbgPrintfW(TEXT("\r\n"));


    byte = Read_MSR();
    NKDbgPrintfW(TEXT("16550 msr: \t%2.2X\t"), byte);
    if ( byte & SERIAL_MSR_DCTS )
        NKDbgPrintfW(TEXT("DCTS "));
    if ( byte & SERIAL_MSR_DDSR )
        NKDbgPrintfW(TEXT("DDSR "));
    if ( byte & SERIAL_MSR_TERI )
        NKDbgPrintfW(TEXT("TERI "));
    if ( byte & SERIAL_MSR_DDCD )
        NKDbgPrintfW(TEXT("DDCD"));
    if ( byte & SERIAL_MSR_CTS )
        NKDbgPrintfW(TEXT(" CTS"));
    if ( byte & SERIAL_MSR_DSR )
        NKDbgPrintfW(TEXT("DSR "));
    if ( byte & SERIAL_MSR_RI )
        NKDbgPrintfW(TEXT("RI "));
    if ( byte & SERIAL_MSR_DCD )
        NKDbgPrintfW(TEXT("DCD "));
    NKDbgPrintfW(TEXT("\r\n"));

}
#endif
BOOL CReg16550::Write_BaudRate(UINT16 uData)
{
    UINT8   lcr = Read_LCR();
    Write_LCR( lcr | SERIAL_LCR_DLAB);
    Write_DATA(uData & 0xff);
    Write_IER((uData >> 8) & 0xff);
    Write_LCR( lcr );
    m_BaudRate = uData;
    return TRUE;
}
/*
@fn BOOL CReg16550::SetAFE(BOOL bEnable)
@remarks Updates the 5th bit of Modem Control Register
@param bEnable [@ref IN] Flag holding status of Hardware Auto Flow Control
@retval BOOL [@ref OUT]  Contains the result for the concerned 
attempt.The result TRUE for success and FALSE for filure.
*/
//Auto Flow Control is used case when RTS_CONTROL_ENABLE is used
BOOL CReg16550::SetAutoFlowCtl(BOOL bEnable)   
{
    UINT16 mcr = Read_MCR();
    UINT8  general = Read_GenConfig();

    if(TRUE == bEnable )
    {
        //clear PRV_GENERAL bit3 to disable overriding RTS when autoflow control is used
        Write_GenConfig(general & ~GENERAL_SERIAL_MANUAL_RTS);
        DEBUGMSG(ZONE_INIT | ZONE_FUNCTION,
            (TEXT("bEnable = TRUE. Setting 5th bit of Modem control register")));
        Write_MCR((UINT8)(mcr | SERIAL_MCR_AFCE));
        DEBUGMSG(ZONE_INIT | ZONE_FUNCTION,
            (TEXT("Clearing bit 3 of PRV_GENERAL register to disable Manual RTS when AutoFlowCtl is enable")));
        DEBUGMSG(TRUE,(TEXT("SetAutoFlowCtl %0X"),Read_MCR()));
    }
    else
    {
        DEBUGMSG(ZONE_INIT|ZONE_FUNCTION,
            (TEXT("bEnable = FALSE. Clearing 5th bit of Modem control register")));
        Write_MCR((UINT8)(mcr & (~SERIAL_MCR_AFCE)));
        DEBUGMSG(ZONE_INIT | ZONE_FUNCTION,
            (TEXT("Setting bit 3 of PRV_GENERAL register to enable Manual RTS when AutoFlowCtl is disabled")));
        //clear PRV_GENERAL bit3
        Write_GenConfig(general | GENERAL_SERIAL_MANUAL_RTS);
        DEBUGMSG(TRUE,(TEXT("SetAutoFlowCtl - Curr MSR = %0X"),Read_MCR()));
    }
    
    return TRUE;
}

CPdd16550::CPdd16550 (LPCTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj )
:   CSerialPDD(lpActivePath,pMdd, pHwObj)
,   m_ActiveReg(HKEY_LOCAL_MACHINE,lpActivePath)
,   CMiniThread (0, TRUE)   
{
    m_pReg16550 = NULL;
    m_dwSysIntr = (DWORD)SYSINTR_UNDEFINED;
    m_hISTEvent = NULL;
    m_dwDevIndex = 0;
    m_pRegVirtualAddr = NULL;
    m_bIsIo = TRUE;
    m_XmitFlushDone =  CreateEvent(0, FALSE, FALSE, NULL);
    m_XmitFifoEnable = TRUE;
    m_dwWaterMark = 8 ;
    m_fXmitFlowOff = FALSE;
}
CPdd16550::~CPdd16550()
{
    if (m_pReg16550)
        InitModem(FALSE);
    if (m_hISTEvent) {
        m_bTerminated=TRUE;
        ThreadStart();
        SetEvent(m_hISTEvent);
        ThreadTerminated(1000);
        if (m_dwSysIntr != SYSINTR_UNDEFINED)
            InterruptDisable(m_dwSysIntr );         
        CloseHandle(m_hISTEvent);
    };
    if (m_pReg16550)
        delete m_pReg16550;
    if (m_XmitFlushDone)
        CloseHandle(m_XmitFlushDone);
    if (m_pRegVirtualAddr != NULL && m_bIsIo != TRUE) {
        MmUnmapIoSpace((PVOID)m_pRegVirtualAddr,0UL);
    }
}
BOOL CPdd16550::Init()
{
    if ( CSerialPDD::Init() && IsKeyOpened() && m_XmitFlushDone!=NULL) { 
        // IST Setup .
        DDKISRINFO ddi;
        if (GetIsrInfo(&ddi)!=ERROR_SUCCESS) {
            return FALSE;
        }
        m_dwSysIntr = ddi.dwSysintr;
        if (m_dwSysIntr !=  SYSINTR_UNDEFINED && m_dwSysIntr!=0 ) 
            m_hISTEvent= CreateEvent(0,FALSE,FALSE,NULL);
        
        if (m_hISTEvent!=NULL) {
            if (!InterruptInitialize(m_dwSysIntr,m_hISTEvent,0,0)) {
                m_dwSysIntr = (DWORD)SYSINTR_UNDEFINED ;
                return FALSE;
            }
        }
        else
            return FALSE;
        
        // Get Device Index.
        if (!GetRegValue(PC_REG_DEVINDEX_VAL_NAME, (PBYTE)&m_dwDevIndex, PC_REG_DEVINDEX_VAL_LEN)) {
            m_dwDevIndex = 0;
        }
        if (!GetRegValue(PC_REG_SERIALWATERMARK_VAL_NAME,(PBYTE)&m_dwWaterMark,sizeof(DWORD))) {
            m_dwWaterMark = 8;
        }
        if (!MapHardware() || !CreateHardwareAccess()) {
            return FALSE;
        }
        
        return TRUE;        
    }
    return FALSE;
}
BOOL CPdd16550::MapHardware() 
{
    if (m_pRegVirtualAddr !=NULL)
        return TRUE;

    m_dwRegStride = 1;
    if (!GetRegValue(PC_REG_REGSTRIDE_VAL_NAME,(PBYTE)&m_dwRegStride, PC_REG_REGSTRIDE_VAL_LEN)) {
        m_dwRegStride = 1;
    }

    // Get IO Window From Registry
    DDKWINDOWINFO dwi;
    if ( GetWindowInfo( &dwi)!=ERROR_SUCCESS || 
            dwi.dwNumMemWindows < 1 || 
            dwi.memWindows[0].dwBase == 0 || 
            dwi.memWindows[0].dwLen <  m_dwRegStride * (SCRATCH_REGISTER+1)) 
        return FALSE;
    DWORD dwInterfaceType;
    if (m_ActiveReg.IsKeyOpened() && 
            m_ActiveReg.GetRegValue( DEVLOAD_INTERFACETYPE_VALNAME, (PBYTE)&dwInterfaceType,sizeof(DWORD))) {
        dwi.dwInterfaceType = dwInterfaceType;
    }

    // Translate to System Address.
    DEBUGMSG (TRUE,(TEXT("PDD16550 - dwi.memWindows[0].dwBase:%0x \r\n"),dwi.memWindows[0].dwBase));
    PHYSICAL_ADDRESS    ioPhysicalBase = { dwi.memWindows[0].dwBase, 0};
    ULONG               inIoSpace = 0;
    if (TranslateBusAddr(m_hParent,(INTERFACE_TYPE)dwi.dwInterfaceType,dwi.dwBusNumber, ioPhysicalBase,&inIoSpace,&ioPhysicalBase)) {
        if (inIoSpace) {
            m_bIsIo = TRUE;
            m_pRegVirtualAddr = (PVOID)ioPhysicalBase.LowPart;
        }
        else {
            // Map it if it is Memory Mapped IO.
            m_pRegVirtualAddr = MmMapIoSpace( ioPhysicalBase, dwi.memWindows[0].dwLen,FALSE);
            DEBUGMSG (TRUE,(TEXT("PDD16550 - Virtual Add space:%0x \r\n"),m_pRegVirtualAddr));
            m_bIsIo = FALSE;
        }
    }
    return (m_pRegVirtualAddr!=NULL);
}
BOOL CPdd16550::CreateHardwareAccess()
{
    /*if (m_pReg16550)
        return TRUE;*/
    if (m_pRegVirtualAddr!=NULL) {
        m_pReg16550 =(m_bIsIo? new CReg16550((PBYTE)m_pRegVirtualAddr,m_dwRegStride) : new CMemReg16550 ((PBYTE)m_pRegVirtualAddr,m_dwRegStride));
        if (m_pReg16550 && !m_pReg16550->Init()) { // FALSE.
            delete m_pReg16550 ;
            m_pReg16550 = NULL;
        }
            
    }
    return (m_pReg16550!=NULL);
}
#define MAX_RETRY 0x1000
void CPdd16550::PostInit()
{
    DWORD dwCount=0;
    m_HardwareLock.Lock();
    UCHAR uData;
    while (((uData =m_pReg16550->Read_IIR()) & SERIAL_IIR_INT_INVALID )==0 && dwCount <MAX_RETRY) { // Interrupt.
        InitReceive(TRUE);
        InitModem(TRUE);
        InitLine(TRUE);
        dwCount++;
    }
    ASSERT((uData  & SERIAL_IIR_INT_INVALID) == SERIAL_IIR_INT_INVALID);
    // IST Start to Run.
    m_HardwareLock.Unlock();
    CSerialPDD::PostInit();
    CeSetPriority(m_dwPriority256);
#ifdef DEBUG
    if ( ZONE_INIT )
        m_pReg16550->DumpRegister();
#endif
    ThreadStart();  // Start IST.
}
DWORD CPdd16550::ThreadRun()
{
    while ( m_hISTEvent!=NULL && !IsTerminated()) {
        if (WaitForSingleObject( m_hISTEvent,INFINITE)==WAIT_OBJECT_0) {
            UCHAR bData;
            while (!IsTerminated() && 
                    ((bData = m_pReg16550->Read_IIR()) & SERIAL_IIR_INT_INVALID)==0) {
                DEBUGMSG(ZONE_THREAD,
                  (TEXT("CPdd16550::ThreadRun IIR=%x\r\n"),bData));
                DWORD interrupts=0;
                switch ( bData & SERIAL_IIR_INT_MASK ) {
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
                    ASSERT(FALSE);
                }
                NotifyPDDInterrupt((INTERRUPT_TYPE)interrupts);
            }
            if (m_dwSysIntr!= SYSINTR_UNDEFINED)
                InterruptDone(m_dwSysIntr);
        }
        else
            ASSERT(FALSE);
    }
    return 1;
}
BOOL CPdd16550::InitialEnableInterrupt(BOOL bEnable )
{
    m_HardwareLock.Lock();
    CSerialPDD::InitialEnableInterrupt(bEnable );
    if (bEnable) 
        m_pReg16550->Write_IER(IER_NORMAL_INTS);
    m_HardwareLock.Unlock();
    return TRUE;
}
BOOL    CPdd16550::WaitForTransmitterEmpty(DWORD dwTicks)
{
    DWORD dwStartTicks = GetTickCount();
    while (GetTickCount()- dwStartTicks < dwTicks) {
        if ((m_pReg16550->Read_LSR( ) & SERIAL_LSR_TEMT) == 0 ) 
            Sleep(1);
        else
            return TRUE;
    }
    return FALSE;
}

BOOL    CPdd16550::InitXmit(BOOL bInit)
{
    m_HardwareLock.Lock();    
    if (bInit) { 
        m_pReg16550->Write_FCR(m_pReg16550->Read_FCR() | SERIAL_FCR_TXMT_RESET | SERIAL_FCR_ENABLE);
        m_XmitFifoEnable = TRUE;
    }
    else 
        WaitForTransmitterEmpty(100);
    
    m_HardwareLock.Unlock();
    return TRUE;
}
DWORD   CPdd16550::GetWriteableSize()
{
    DWORD dwWriteSize = 0;
    if (GetLineStatus() & SERIAL_LSR_THRE )
        dwWriteSize = (m_XmitFifoEnable?256:1);
    return dwWriteSize;
}
void    CPdd16550::XmitInterruptHandler(PUCHAR pTxBuffer, ULONG *pBuffLen)
{
    PREFAST_DEBUGCHK(pBuffLen!=NULL);
    m_HardwareLock.Lock();    
    m_fXmitFlowOff = FALSE;    
    if (*pBuffLen == 0) {
        if (GetDCB().fRtsControl == RTS_CONTROL_TOGGLE ) { 
            // If it is TOGGLE, we have to wait Transmit Buffer Empty before return
            // because mdd will clear RTS
            WaitForTransmitterEmpty(100);
        }
        EnableXmitInterrupt(FALSE);
    }
    else {
        DEBUGCHK(pTxBuffer);
        PulseEvent(m_XmitFlushDone);
        DWORD dwDataAvaiable = *pBuffLen;
        *pBuffLen = 0;
        if ((m_DCB.fOutxCtsFlow && IsCTSOff())) { //in flow off
            DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::XmitInterruptHandler! Flow Off, Data Discard.\r\n")));
            EnableXmitInterrupt(FALSE);
            m_fXmitFlowOff = TRUE;
        }
        else  {
            DWORD dwWriteSize = GetWriteableSize();
            for (DWORD dwByteWrite=0; dwByteWrite<dwWriteSize && dwDataAvaiable!=0;dwByteWrite++) {
                __try {
                    m_pReg16550->Write_DATA(*pTxBuffer);
                }
                __except( EXCEPTION_EXECUTE_HANDLER ) { // Make sure it end normally.
                    dwDataAvaiable = 1 ;
                    dwByteWrite = dwWriteSize-1;
                }
                pTxBuffer ++;
                dwDataAvaiable--;
            }
            DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::XmitInterruptHandler! Write %d byte to FIFO\r\n"),dwByteWrite));
            *pBuffLen = dwByteWrite;
            EnableXmitInterrupt(TRUE);        
        }
    }
    m_HardwareLock.Unlock();
}
void    CPdd16550::XmitComChar(UCHAR ComChar)
{
    // This function has to poll until the Data can be sent out.
    BOOL bDone = FALSE;
    do {
        m_HardwareLock.Lock(); 
        if ( GetLineStatus() & SERIAL_LSR_THRE ) {  // If Empty.
            m_pReg16550->Write_DATA(ComChar);
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
BOOL    CPdd16550::EnableXmitInterrupt(BOOL fEnable)
{
    m_HardwareLock.Lock();
    if (fEnable)
        m_pReg16550->Write_IER ( m_pReg16550->Read_IER() | SERIAL_IER_THR);
    else
         m_pReg16550->Write_IER ( m_pReg16550->Read_IER() & ~SERIAL_IER_THR);
    m_HardwareLock.Unlock();
    return TRUE;
        
}
BOOL  CPdd16550::CancelXmit()
{
    m_HardwareLock.Lock();
    m_pReg16550->Write_FCR(m_pReg16550->Read_FCR() | SERIAL_FCR_TXMT_RESET | SERIAL_FCR_ENABLE);
    m_HardwareLock.Unlock();
    return FALSE;
    
}
static PAIRS s_HighWaterPairs[] = {
    {SERIAL_1_BYTE_HIGH_WATER, 0},
    {SERIAL_16_BYTE_HIGH_WATER, 16},
    {SERIAL_32_BYTE_HIGH_WATER, 32},
    {SERIAL_62_BYTE_HIGH_WATER, 62}
};

BYTE  CPdd16550::GetWaterMarkBit()
{
    BYTE bReturnKey = (BYTE)s_HighWaterPairs[0].Key;
    for (DWORD dwIndex=_countof(s_HighWaterPairs)-1;dwIndex!=0; dwIndex --) {
        if (m_dwWaterMark>=s_HighWaterPairs[dwIndex].AssociatedValue) {
            bReturnKey = (BYTE)s_HighWaterPairs[dwIndex].Key;
            break;
        }
    }
    return bReturnKey;
}
DWORD   CPdd16550::GetWaterMark()
{
    BYTE bReturnValue = (BYTE)s_HighWaterPairs[0].AssociatedValue;
    for (DWORD dwIndex=_countof(s_HighWaterPairs)-1;dwIndex!=0; dwIndex --) {
        if (m_dwWaterMark>=s_HighWaterPairs[dwIndex].AssociatedValue) {
            bReturnValue = (BYTE)s_HighWaterPairs[dwIndex].AssociatedValue;
            break;
        }
    }
    return bReturnValue;
}

// Receive
BOOL    CPdd16550::InitReceive(BOOL bInit)
{
    m_HardwareLock.Lock();    
    if (bInit) {         
        BYTE uWarterMarkBit = GetWaterMarkBit();
        m_pReg16550->Write_FCR((m_pReg16550->Read_FCR() & ~SERIAL_IIR_FIFOS_ENABLED) | SERIAL_FCR_RCVR_RESET | SERIAL_FCR_ENABLE | (uWarterMarkBit & SERIAL_IIR_FIFOS_ENABLED ));
        m_pReg16550->Write_IER(m_pReg16550->Read_IER() | SERIAL_IER_RDA);
        m_pReg16550->Read_LSR(); // Clean Line Interrupt.
    }
    else {
        m_pReg16550->Write_IER(m_pReg16550->Read_IER() & ~SERIAL_IER_RDA);
    }
    m_HardwareLock.Unlock();
    return TRUE;
}
ULONG   CPdd16550::ReceiveInterruptHandler(PUCHAR pRxBuffer,ULONG *pBufflen)
{
    DWORD dwBytesDropped = 0;
    if (pRxBuffer && pBufflen ) {
        DWORD dwBytesStored = 0 ;
        DWORD dwRoomLeft = *pBufflen;
        m_bReceivedCanceled = FALSE;
        m_HardwareLock.Lock();
        
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
        if (m_bReceivedCanceled)
            dwBytesStored = 0;
        
        m_HardwareLock.Unlock();
        *pBufflen = dwBytesStored;
    }
    else {
        ASSERT(FALSE);
    }
    return dwBytesDropped;
}
ULONG   CPdd16550::CancelReceive()
{
    m_bReceivedCanceled = TRUE;
    m_HardwareLock.Lock();    
    m_pReg16550->Write_FCR(m_pReg16550->Read_FCR() | SERIAL_FCR_RCVR_RESET );
    m_pReg16550->Write_IER(m_pReg16550->Read_IER() | SERIAL_IER_RDA);
    m_HardwareLock.Unlock();
    return 0;
}
BOOL    CPdd16550::InitModem(BOOL bInit)
{
    m_HardwareLock.Lock();    
    if (bInit) {
        //m_pReg16550->Write_MCR(SERIAL_MCR_IRQ_ENABLE);
        m_pReg16550->Write_FCR( m_pReg16550->Read_FCR() | SERIAL_FCR_ENABLE);
        //m_pReg16550->Write_IER( m_pReg16550->Read_IER() | SERIAL_IER_MS);
        m_pReg16550->Write_IER( m_pReg16550->Read_IER());
        m_pReg16550->Read_MSR(); // Clean the Interrupt First.
    }
    else {
        m_pReg16550->Write_MCR(0);
        m_pReg16550->Write_IER( m_pReg16550->Read_IER() & ~SERIAL_IER_MS);
    }
    m_HardwareLock.Unlock();
    return TRUE;
}
ULONG   CPdd16550::GetModemStatus()
{
    m_HardwareLock.Lock();    
    ULONG ulReturn =0 ;
    ULONG Events = 0;
    UINT8 ubModemStatus = m_pReg16550->Read_MSR();
    m_HardwareLock.Unlock();

    // Event Notification.
    if (ubModemStatus & SERIAL_MSR_DCTS)
        Events |= EV_CTS;
    if ( ubModemStatus  & SERIAL_MSR_DDSR )
        Events |= EV_DSR;
    if ( ubModemStatus  & SERIAL_MSR_TERI )
        Events |= EV_RING;
    if ( ubModemStatus  & SERIAL_MSR_DDCD )
        Events |= EV_RLSD;

    // Report Modem Status;
    if ( ubModemStatus & SERIAL_MSR_CTS )
        ulReturn |= MS_CTS_ON;
    if ( ubModemStatus & SERIAL_MSR_DSR )
         ulReturn |= MS_DSR_ON;
    if ( ubModemStatus & SERIAL_MSR_RI )
         ulReturn  |= MS_RING_ON;
    if ( ubModemStatus & SERIAL_MSR_DCD )
         ulReturn  |= MS_RLSD_ON;

    if (Events!=0)
        EventCallback(Events,ulReturn);
    
    return ulReturn;
}
void   CPdd16550::ModemInterruptHandler()
{ 
    GetModemStatus();
    if (m_fXmitFlowOff && !((m_DCB.fOutxCtsFlow && IsCTSOff()))) { // in flow on
        DEBUGMSG(ZONE_THREAD|ZONE_WRITE,(TEXT("CPdd16550::ModemInterruptHandler! Flow On, Retriggering Xmit\r\n")));
        m_InterruptLock.Lock();
        m_dwInterruptFlag |= INTR_TX ; // Re-trigger xmitting.
        m_InterruptLock.Unlock();
    };
};

void    CPdd16550::SetDTR(BOOL bSet)
{
    m_HardwareLock.Lock();
    BYTE bData = m_pReg16550->Read_MCR();
    if (bSet) {
        bData |= SERIAL_MCR_DTR;
    }
    else
        bData &= ~SERIAL_MCR_DTR;
    m_pReg16550->Write_MCR(bData);
    m_HardwareLock.Unlock();
}
void    CPdd16550::SetRTS(BOOL bSet)
{
    m_HardwareLock.Lock();
    BYTE bData = m_pReg16550->Read_MCR();
    if (bSet) {
        bData |= SERIAL_MCR_RTS;
    }
    else
        bData &= ~SERIAL_MCR_RTS;
    m_pReg16550->Write_MCR(bData);
    DEBUGMSG(TRUE, (TEXT("In SetRTS - MSR: 0x%X\n"), bData));
    m_HardwareLock.Unlock();

}
BOOL CPdd16550::InitLine(BOOL bInit)
{
    m_HardwareLock.Lock();
    if  (bInit) {
        m_pReg16550->Write_IER( m_pReg16550->Read_IER() | SERIAL_IER_RLS);
        m_pReg16550->Write_LCR( SERIAL_8_DATA | SERIAL_1_STOP | SERIAL_NONE_PARITY);
    }
    else {
        m_pReg16550->Write_IER( m_pReg16550->Read_IER() & ~SERIAL_IER_RLS);
    }
    m_HardwareLock.Unlock();
    return TRUE;
}
BYTE CPdd16550::GetLineStatus()
{
    m_HardwareLock.Lock();
    BYTE bData = m_pReg16550->Read_LSR();
    m_HardwareLock.Unlock();  
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
    if (bData & SERIAL_LSR_BI) {
        ulError |=  CE_BREAK;
    }
    if (ulError)
        SetReceiveError(ulError);
    if (bData & SERIAL_LSR_BI) {
         EventCallback(EV_BREAK);
    }
    return bData;
        
}
void    CPdd16550::SetBreak(BOOL bSet)
{
    m_HardwareLock.Lock();
    BYTE bData = m_pReg16550->Read_LCR();
    if (bSet)
        bData |= SERIAL_LCR_BREAK;
    else
        bData &= ~SERIAL_LCR_BREAK;
    m_pReg16550->Write_LCR(bData);
    m_HardwareLock.Unlock();      
}
BOOL    CPdd16550::SetBaudRate(ULONG BaudRate,BOOL /*bIrModule*/)
{
    ULONG ulDivisor;
    if (GetDivisorOfRate(BaudRate,&ulDivisor)) {
        m_HardwareLock.Lock();
        if (m_dwSysIntr!=SYSINTR_UNDEFINED)
            InterruptMask(m_dwSysIntr,TRUE);
        __try {
            m_pReg16550->Write_BaudRate((UINT16)ulDivisor);
        }__except( EXCEPTION_EXECUTE_HANDLER ) {
        };
        if (m_dwSysIntr!=SYSINTR_UNDEFINED)
            InterruptMask(m_dwSysIntr,FALSE);
        m_HardwareLock.Unlock();      
        return TRUE;
    }
    else
        return FALSE;
}
BOOL    CPdd16550::SetByteSize(ULONG ByteSize)
{
    BOOL bRet = TRUE;
    m_HardwareLock.Lock();
    UCHAR bData = m_pReg16550->Read_LCR() & ~SERIAL_DATA_MASK;;
    switch ( ByteSize ) {
    case 5:
        bData |= SERIAL_5_DATA;
        break;
    case 6:
        bData |= SERIAL_6_DATA;
        break;
    case 7:
        bData |= SERIAL_7_DATA;
        break;
    case 8:
        bData |= SERIAL_8_DATA;
        break;
    default:
        bRet = FALSE;
        break;
    }
    if (bRet) {
        m_pReg16550->Write_LCR(bData);
    }
    m_HardwareLock.Unlock();
    return bRet;
}
BOOL    CPdd16550::SetParity(ULONG Parity)
{
    BOOL bRet = TRUE;
    UCHAR bData = m_pReg16550->Read_LCR() & ~SERIAL_PARITY_MASK;
    switch ( Parity ) {
    case ODDPARITY:
        bData |= SERIAL_ODD_PARITY;
        break;
    case EVENPARITY:
        bData |= SERIAL_EVEN_PARITY;
        break;
    case MARKPARITY:
        bData |= SERIAL_MARK_PARITY;
        break;
    case SPACEPARITY:
        bData |= SERIAL_SPACE_PARITY;
        break;
    case NOPARITY:
        bData |= SERIAL_NONE_PARITY;
        break;
    default:
        bRet = FALSE;
        break;
    }
    if (bRet) {
        m_pReg16550->Write_LCR(bData);
    }
    return bRet;
}
BOOL    CPdd16550::SetStopBits(ULONG StopBits)
{
    BOOL bRet = TRUE;
    UCHAR bData = m_pReg16550->Read_LCR() & ~SERIAL_STOP_MASK;

    switch ( StopBits ) {
    case ONESTOPBIT :
        bData |= SERIAL_1_STOP ;
        break;
    case ONE5STOPBITS :
        bData |= SERIAL_1_5_STOP ;
        break;
    case TWOSTOPBITS :
        bData |= SERIAL_2_STOP ;
        break;
    default:
        bRet = FALSE;
        break;
    }
    if (bRet) {
        m_pReg16550->Write_LCR(bData);
    }
    return bRet;
}

// Default Rate to Divisor Converter.
BOOL CPdd16550::GetDivisorOfRate(ULONG BaudRate,PULONG pulDivisor)
{
    BOOL status   = TRUE;
    DWORD ClockRate;
    DWORD m = 6912;
    DWORD n = 15625;
    ULONG reg;

        /*
         * For baud rates 1000000, 2000000 and 4000000 the dividers must be
         * adjusted.
         */
        if (BaudRate == 1000000 || BaudRate == 2000000 || BaudRate == 4000000) 
        {
                m = 64;
                n = 100;

          ClockRate = 64000000;
        } 
        else if (BaudRate == 3000000)
        {
                m = 48;
                n = 100;

               ClockRate = 48000000;
        }
        else 
        {
               ClockRate = 44236800;
        }

        DEBUGMSG (TRUE,(TEXT("+GetDivisorOfRate,ClockRate: %X\r\n"), ClockRate));

    /* Reset the clock */
    reg =(m << PRV_CLOCKS_M_VAL_SHIFT) | (n << PRV_CLOCKS_N_VAL_SHIFT);
    m_pReg16550->Write_PRV_CLK(reg);
    reg |= PRV_CLOCKS_CLK_EN | PRV_CLOCKS_UPDATE;
    m_pReg16550->Write_PRV_CLK(reg);


        //
        // Divisor =   SourceClock
        //           ---------------
        //            16 * Baudrate
        //
        // * add 1/2 denominator to numerator to effectively round 
        //   and prevent dropout
        //

        *pulDivisor = (USHORT)((ClockRate + (8 * BaudRate)) /
            (16 * BaudRate));



    if (*pulDivisor == -1) {
      status = FALSE;
   }

    return status;
}


BOOL CPdd16550::SetAutoFlowCtlEnable(BOOL bFlagAFE)  
{
    BOOL bRetVal = TRUE;
    DEBUGMSG(ZONE_INIT | ZONE_FUNCTION,
        (TEXT("Inside function CPdd16550::SetAutoFlowEnable. Calling CReg16550::SetAutoFlowCtl")));
        bRetVal = m_pReg16550->SetAutoFlowCtl(bFlagAFE);
    return bRetVal;
}
