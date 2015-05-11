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
#ifndef __PDD16550_H_
#define __PDD16550_H_
#include <cserpdd.h>
#include <hw16550.h>
class CReg16550 {
public:
    CReg16550(__in_bcount_opt(dwStride*(SCRATCH_REGISTER+1)) PBYTE pRegAddr, DWORD dwStride);
    virtual ~CReg16550() { ; };
    virtual BOOL    Init() { return (m_pReg!=NULL); } ;
    // We do not virtual Read & Write data because of Performance Concern.
    virtual void    Write_DATA(UINT8 uData) { WRITE_PORT_UCHAR( m_pData, uData); };
    virtual UINT8   Read_Data() { return READ_PORT_UCHAR(m_pData); } ;
    virtual void    Write_IER(UINT8 uData) { WRITE_PORT_UCHAR(m_pIER, uData); };
    virtual UINT8   Read_IER() { return READ_PORT_UCHAR(m_pIER); };
    virtual void    Write_FCR(UINT8 uData) {WRITE_PORT_UCHAR( m_pIIR_FCR, uData);m_FCR = uData;};
    virtual UINT8   Read_FCR() { return m_FCR; };
    virtual UINT8   Read_IIR() { return READ_PORT_UCHAR( m_pIIR_FCR) ;};
    virtual void    Write_LCR(UINT8 uData) { WRITE_PORT_UCHAR(m_pLCR, uData);};
    virtual UINT8   Read_LCR() { return READ_PORT_UCHAR(m_pLCR);};
    virtual void    Write_MCR(UINT8 uData) { WRITE_PORT_UCHAR( m_pMCR, uData);};
    virtual UINT8   Read_MCR() { return READ_PORT_UCHAR(m_pMCR);};
    virtual void    Write_LSR(UINT8 uData) { WRITE_PORT_UCHAR( m_pLSR, uData);};
    virtual UINT8   Read_LSR() { return READ_PORT_UCHAR(m_pLSR);};
    virtual void    Write_MSR(UINT8 uData) { WRITE_PORT_UCHAR( m_pMSR, uData) ; };
    virtual UINT8   Read_MSR() { return READ_PORT_UCHAR( m_pMSR); };
    virtual void    Write_SCR(UINT8 uData) { WRITE_PORT_UCHAR( m_pSRC, uData );};
    virtual UINT8   Read_SCR() { return READ_PORT_UCHAR(m_pSRC); };
	virtual UINT8   Read_TFL() { return READ_PORT_UCHAR(m_pTFL); };
	virtual UINT8   Read_RFL() { return READ_PORT_UCHAR(m_pRFL); };
	virtual void    Write_PRV_CLK(ULONG uData) { WRITE_REGISTER_ULONG( m_pCLK, uData );};
    virtual ULONG   Read_PRV_CLK() { return READ_REGISTER_ULONG(m_pCLK); };


    virtual BOOL    Write_BaudRate(UINT16 uData);
	virtual BOOL    SetAutoFlowCtl(BOOL bEnable);

    virtual void    Backup();
    virtual void    Restore();
#ifdef DEBUG
    virtual void    DumpRegister();
#endif
	
protected:
    PBYTE  const  m_pReg;
    DWORD  const  m_dwStride;
    //BYTE    m_FCR; // Because it is writeonly register.
    BOOL    m_fIsBackedUp;
    PBYTE   m_pData;    
    PBYTE   m_pIER;
    PBYTE   m_pIIR_FCR;
    PBYTE   m_pLCR;
    PBYTE   m_pMCR;
    PBYTE   m_pLSR;
    PBYTE   m_pMSR;
    PBYTE   m_pTFL;
	PBYTE   m_pRFL;
    PBYTE   m_pSRC;
	PULONG  m_pCLK;
    BYTE    m_FCR; 
    BYTE    m_IERWrite;
    BYTE    m_IERRead;

private:
    BYTE    m_IERBackup;
    BYTE    m_LCRBackup;
    BYTE    m_MCRBackup;
    UINT16  m_BaudRate;
    
    CReg16550&operator=(CReg16550&){ASSERT(FALSE);}
};

// Memory Mapped IO version.
class CMemReg16550: public CReg16550 {
public:
    CMemReg16550(PBYTE pRegAddr, DWORD dwStride) 
        :CReg16550(pRegAddr,dwStride) 
    {;}
    // We do not virtual Read & Write data because of Performance Concern.
    virtual void    Write_DATA(UINT8 uData) { WRITE_REGISTER_UCHAR( m_pData, uData); };
    virtual UINT8   Read_Data() { return READ_REGISTER_UCHAR(m_pData); } ;
    virtual void    Write_IER(UINT8 uData) { 
        /* special w/a to h/w issue that failed to write IER */
        DWORD maxcount = 5;
        DWORD count = 0;
        UINT8 IER;
        while (count++ < maxcount)
        {
            WRITE_REGISTER_UCHAR(m_pIER, uData); 
            IER = Read_IER();
            m_IERWrite = uData;
            m_IERRead = IER;
            if (IER == uData)
            {
                break;
            }
        }
        ASSERT(count < maxcount);
    };
    virtual UINT8   Read_IER() { return READ_REGISTER_UCHAR(m_pIER); };
    virtual void    Write_FCR(UINT8 uData) { 
        WRITE_REGISTER_UCHAR( m_pIIR_FCR, uData);
        m_FCR = uData; 
        m_FCR &= ~SERIAL_FCR_RCVR_RESET ;
        m_FCR &= ~SERIAL_FCR_TXMT_RESET ; 
    };
    virtual UINT8   Read_IIR() { return READ_REGISTER_UCHAR( m_pIIR_FCR) ;};
    virtual void    Write_LCR(UINT8 uData) { WRITE_REGISTER_UCHAR(m_pLCR, uData);};
    virtual UINT8   Read_LCR() { return READ_REGISTER_UCHAR(m_pLCR);};
    virtual void    Write_MCR(UINT8 uData) { WRITE_REGISTER_UCHAR( m_pMCR, uData);};
    virtual UINT8   Read_MCR() { return READ_REGISTER_UCHAR(m_pMCR);};
    virtual void    Write_LSR(UINT8 uData) { WRITE_REGISTER_UCHAR( m_pLSR, uData);};
    virtual UINT8   Read_LSR() { return READ_REGISTER_UCHAR(m_pLSR);};
    virtual void    Write_MSR(UINT8 uData) { WRITE_REGISTER_UCHAR( m_pMSR, uData) ; };
    virtual UINT8   Read_MSR() { return READ_REGISTER_UCHAR( m_pMSR); };
    virtual UINT8   Read_TFL() { return READ_REGISTER_UCHAR(m_pTFL); };
	virtual UINT8   Read_RFL() { return READ_REGISTER_UCHAR(m_pRFL); };
    virtual void    Write_SCR(UINT8 uData) { WRITE_REGISTER_UCHAR( m_pSRC, uData );};
    virtual UINT8   Read_SCR() { return READ_REGISTER_UCHAR(m_pSRC); };
	virtual void    Write_PRV_CLK(ULONG uData) { WRITE_REGISTER_ULONG( m_pCLK, uData );};
    virtual ULONG   Read_PRV_CLK() { return READ_REGISTER_ULONG(m_pCLK); };

private:
    CMemReg16550&operator=(CMemReg16550&){ASSERT(FALSE);}
};


//#define HIGH_WATER_SIZE     4
typedef struct  __PAIRS {
    ULONG   Key;
    ULONG   AssociatedValue;
} PAIRS, *PPAIRS;


class CPdd16550 : public CSerialPDD, public CMiniThread  {
public:
    CPdd16550 (LPCTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj);
    virtual ~CPdd16550();
    virtual BOOL Init();
    virtual void PostInit();
    virtual BOOL MapHardware();
    virtual BOOL CreateHardwareAccess();
//  Power Manager Required Function.
    virtual void    SerialRegisterBackup() { m_pReg16550->Backup(); };
    virtual void    SerialRegisterRestore() { m_pReg16550->Restore(); };

// Implement CPddSerial Function.
// Interrupt
    virtual BOOL    InitialEnableInterrupt(BOOL bEnable ) ; // Enable All the interrupt may include Xmit Interrupt.
private:
    virtual DWORD ThreadRun();   // IST
//  Tx Function.
public:
    virtual BOOL    WaitForTransmitterEmpty(DWORD dwTicks);
    virtual BOOL    InitXmit(BOOL bInit);
    virtual void    XmitInterruptHandler(PUCHAR pTxBuffer, ULONG *pBuffLen);
    virtual void    XmitComChar(UCHAR ComChar);
    virtual BOOL    EnableXmitInterrupt(BOOL bEnable);
    virtual BOOL    CancelXmit();
    virtual DWORD   GetWriteableSize();
protected:
    BOOL    m_XmitFifoEnable;
    HANDLE  m_XmitFlushDone;
    BOOL    m_fXmitFlowOff;

//
//  Rx Function.
public:
    virtual BOOL    InitReceive(BOOL bInit);
    virtual ULONG   ReceiveInterruptHandler(PUCHAR pRxBuffer,ULONG *pBufflen);
    virtual ULONG   CancelReceive();
    virtual DWORD   GetWaterMark();
    virtual BYTE    GetWaterMarkBit();
protected:
    BOOL    m_bReceivedCanceled;
    DWORD   m_dwWaterMark;
//
//  Modem
public:
    virtual BOOL    InitModem(BOOL bInit);
    virtual void    ModemInterruptHandler();
    virtual ULONG   GetModemStatus();
    virtual void    SetDTR(BOOL bSet);
    virtual void    SetRTS(BOOL bSet);
//
// Line Function.
    virtual BOOL    InitLine(BOOL bInit) ;
    virtual void    LineInterruptHandler() { GetLineStatus();};
    virtual void    SetBreak(BOOL bSet) ;
    virtual BOOL    SetBaudRate(ULONG BaudRate,BOOL bIrModule) ;
    virtual BOOL    SetByteSize(ULONG ByteSize);
    virtual BOOL    SetParity(ULONG Parity);
    virtual BOOL    SetStopBits(ULONG StopBits);
    virtual BOOL    GetDivisorOfRate(ULONG BaudRate,PULONG pulDivisor);
	virtual BOOL    SetAutoFlowCtlEnable(BOOL bFlagAFE);
//
// Line Internal Function
    BYTE            GetLineStatus();

protected:
    CReg16550 *  m_pReg16550;
    PVOID       m_pRegVirtualAddr;
    BOOL        m_bIsIo;
    DWORD       m_dwRegStride;
    
    CRegistryEdit m_ActiveReg;
//  Interrupt Handler
    DWORD       m_dwSysIntr;
    HANDLE      m_hISTEvent;
// Optional Parameter
    DWORD m_dwDevIndex;
    BOOL  flowStopped;
	DWORD dwFlagFifo;

};
// Constants
#define IER_NORMAL_INTS (SERIAL_IER_RDA | SERIAL_IER_RLS | SERIAL_IER_MS)
//
// Private space bits
//
#define PRV_RESETS_NOT_FUNC_RST           0x00000001
#define PRV_RESETS_NOT_APB_RST            0x00000002

#define PRV_CLOCKS_CLK_EN                0x00000001
#define PRV_CLOCKS_UPDATE                0x80000000
#define PRV_CLOCKS_M_VAL_SHIFT           1
#define PRV_CLOCKS_M_VAL_MASK            0x7FFF
//set it for default baudrate 115200
#define PRV_CLOCKS_M_VAL_DEFAULT        ((USHORT)602)
#define PRV_CLOCKS_M_VAL_HS_DEFAULT     ((USHORT)16383)
#define PRV_CLOCKS_N_VAL_SHIFT          16
#define PRV_CLOCKS_N_VAL_MASK            0x7FFF
#define PRV_CLOCKS_N_VAL_DEFAULT        ((USHORT)32767)
#define PRV_CLOCKS_N_VAL_HS_DEFAULT     ((USHORT)27776)
#define PRV_CLOCKS_SOURCE_DEFAULT       1843200
#define PRV_CLOCKS_SOURCE_HS_DEFAULT    58978800

#define SERIAL_MCR_AFCE 0x22

#endif
