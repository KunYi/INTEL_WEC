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

    Platform dependent Serial definitions for generic
    controller.

Notes: 
--*/
#ifndef __CSERPDD_H_
#define __CSERPDD_H_
#include <CRegEdit.h>
#include <CMthread.h>
#include <CSync.h>
// Define Registry.
#define SERIAL_RX_BUFFER_SIZE TEXT("RxBufferSize")

#ifndef dim
// Prefer to use _countof() directly, replacing dim()
#pragma deprecated("dim")
#define dim(x) _countof(x)
#endif

//
// This object is describe the PDD interface, So any Serial PDD should has this interface.
//
class CSerialPDD;
class CSerialPDDPowerUpCallback : public CMiniThread  {
public:
    CSerialPDDPowerUpCallback(CSerialPDD * pSerialObj);
    ~CSerialPDDPowerUpCallback();
    BOOL Init() { return (m_hEvent!=NULL); };
    BOOL SignalCallback() { return (m_hEvent!=NULL?SetEvent(m_hEvent):FALSE); };
private:
    CSerialPDDPowerUpCallback&operator=(CSerialPDDPowerUpCallback&){ASSERT(FALSE);}
    virtual DWORD ThreadRun();
    CSerialPDD * const m_pSerialObj;
    HANDLE  m_hEvent;
};

class CSerialPDD : public CRegistryEdit {
public :
//    
// Interface For Initialization
    CSerialPDD(LPCTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj );
    virtual ~CSerialPDD();
    virtual BOOL Init();
    virtual void PostInit() ;
    virtual void  PreDeinit() {;};
    PHWOBJ  GetHwObject() { return m_pHwObj; };
protected:
    PVOID const  m_pMdd;
    PHWOBJ const m_pHwObj;
    HANDLE m_hParent;
//
//Device Operation
public:
    virtual BOOL    Open();
    virtual BOOL    Close();
            BOOL    IsOpen() { return m_lOpenCount!=0; };
    virtual BOOL    Ioctl(DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut);
private:
    LONG    m_lOpenCount;
//
//Power Managment Operation
public:
    virtual BOOL    InitialPower(BOOL bInit);
    virtual BOOL    PowerOff();
    virtual BOOL    PowerOn();
    virtual BOOL    SetDevicePowerState(CEDEVICE_POWER_STATE pwrState);
    virtual void    SerialRegisterBackup()=0;
    virtual void    SerialRegisterRestore()=0;
    virtual CEDEVICE_POWER_STATE    GetDevicePowerState() { return m_PowerState; };
    virtual POWER_CAPABILITIES  GetPowerCapabilities() { return m_PowerCapabilities; };
    BOOL            IsThisPowerManaged() { return m_IsThisPowerManaged; };
    BOOL            IsPowerResumed ( ) { return (InterlockedExchange( &m_IsResumed,0)!=0); };
protected:
    CEDEVICE_POWER_STATE m_PowerState;
    BOOL                m_IsThisPowerManaged;
    POWER_CAPABILITIES  m_PowerCapabilities;
    LONG                m_IsResumed;
    CSerialPDDPowerUpCallback * m_PowerCallbackThread;
    HANDLE              m_PowerHelperHandle;
    HANDLE              m_hPowerLock;
    static CEDEVICE_POWER_STATE SetPowerStateStatic( DWORD, CEDEVICE_POWER_STATE );
//
protected:
    CLockObject      m_HardwareLock;
// 
// Interrupt Interface
public:
    virtual BOOL    InitialEnableInterrupt(BOOL bEnable ) ; // Enable All the interrupt may include Xmit Interrupt.
    virtual BOOL    NotifyPDDInterrupt(INTERRUPT_TYPE interruptType);
    virtual INTERRUPT_TYPE  GetInterruptType();
    virtual BOOL    EventCallback(ULONG fdwEventMask, ULONG fdwModemStatus = 0 );
    virtual BOOL    DataReplaced(PBYTE puData,BOOL isBadData);
protected:
    DWORD       m_dwInterruptFlag;
    CLockObject  m_InterruptLock;
    DWORD      m_dwPriority256;
//
//  
//
//  Special Cancellation
public:
    virtual BOOL    PurgeComm( DWORD fdwAction);
// 
//  Tx Function.
    virtual BOOL    InitXmit(BOOL bInit) = 0;
    virtual void    XmitInterruptHandler(PUCHAR pTxBuffer, ULONG *pBuffLen) = 0;
    virtual void    XmitComChar(UCHAR ComChar) = 0;
    virtual BOOL    EnableXmitInterrupt(BOOL bEnable)= 0;
    virtual BOOL    CancelXmit() = 0 ;
//
//  Rx Function.
    virtual BOOL    InitReceive(BOOL bInit) = 0;
    virtual ULONG   GetRxBufferSize() { return m_ulRxBufferSize; };
    virtual ULONG   ReceiveInterruptHandler(PUCHAR pRxBuffer,ULONG *pBufflen) = 0;
    virtual ULONG   CancelReceive() = 0;
//
//  Modem
    virtual BOOL    InitModem(BOOL bInit) = 0;
    virtual void    ModemInterruptHandler()= 0; // This is Used to Indicate Modem Signal Changes.
    virtual ULONG   GetModemStatus() = 0;
    virtual void    SetDTR(BOOL bSet)= 0;
    virtual void    SetRTS(BOOL bSet)= 0;
    virtual BOOL    IsCTSOff() {  return ((GetModemStatus() & MS_CTS_ON)==0) ; };
    virtual BOOL    IsDSROff() {  return ((GetModemStatus() & MS_DSR_ON)==0) ; };
//  Line Function
    virtual BOOL    InitLine(BOOL bInit) = 0;
    virtual void    LineInterruptHandler() = 0;
    virtual void    SetBreak(BOOL bSet) = 0 ;    
    virtual BOOL    SetBaudRate(ULONG BaudRate,BOOL bIrModule) = 0;
    virtual BOOL    SetByteSize(ULONG ByteSize) = 0;
    virtual BOOL    SetParity(ULONG Parity)= 0;
    virtual BOOL    SetStopBits(ULONG StopBits)= 0;
	
    
	virtual BOOL    SetAutoFlowCtlEnable(BOOL bFlagAFE) =0;  
    virtual BOOL    SetDCB(LPDCB lpDCB);
	virtual BOOL    SetCommTimeouts(LPCOMMTIMEOUTS lpCommTimeouts);
            DCB     GetDCB() { return m_DCB; };
protected:
    DCB             m_DCB;
	DWORD   		m_ForcePIO;
//
//  Configuration
public:
    virtual void    SetDefaultConfiguration(); 
	virtual BOOL    GetDivisorOfRate(ULONG BaudRate,PULONG pulDivisor) =0;
    COMMPROP        GetCommProperties() { return m_CommPorp; };
protected:
    COMMPROP        m_CommPorp;
	COMMTIMEOUTS 	m_CommTimeouts;
	
//
//
//
//  IR Special Handle
public:
    virtual void    SetOutputMode(BOOL UseIR, BOOL Use9Pin) {   
        m_fIREnable = UseIR;
        m_f9PinEnable=Use9Pin;
    }
    virtual void    GetOutputMode(BOOL* pUseIR, BOOL* pUse9Pin) {
        if (pUseIR) *pUseIR = m_fIREnable;
        if (pUse9Pin) *pUse9Pin = m_f9PinEnable;
    }
protected:
    BOOL m_fIREnable;
    BOOL m_f9PinEnable;
    ULONG m_ulRxBufferSize;
// 
// Error Handling
public:
    virtual void    SetReceiveError(ULONG);
    virtual ULONG   GetReceivedError() ;
protected:
    CSerialPDD&operator=(CSerialPDD&){ASSERT(FALSE);}
    ULONG   m_ulCommErrors;
};


CSerialPDD * CreateSerialObject(LPTSTR lpActivePath, PVOID pMdd,PHWOBJ pHwObj, DWORD DeviceArrayIndex);
void DeleteSerialObject(CSerialPDD * pSerialObj);
// Default Constants
#ifndef DEFAULT_CE_THREAD_PRIORITY
#define DEFAULT_CE_THREAD_PRIORITY 104
#endif
#define PC_REG_DEVINDEX_VAL_NAME TEXT("DeviceArrayIndex") 
#define PC_REG_DEVINDEX_VAL_LEN  sizeof( DWORD )
#define PC_REG_REGSTRIDE_VAL_NAME TEXT("RegStride")
#define PC_REG_REGSTRIDE_VAL_LEN  sizeof( DWORD )
#define PC_REG_SERIALPRIORITY_VAL_NAME TEXT("Priority256")
#define PC_REG_SERIALPRIORITY_VAL_LEN sizeof(DWORD)
#define PC_REG_SERIALWATERMARK_VAL_NAME TEXT("WaterMarker")
#define PC_REG_SERIALWATERMARKER_VAL_LEN sizeof(DWORD)


//
// Settable baud rates in the provider.
//


#define BAUD_3600        ((ULONG)0x00080000)
#define BAUD_312500      ((ULONG)0x00100000)
#define BAUD_230400		 ((ULONG)0x00200000)
#define BAUD_460800		 ((ULONG)0x00400000)
#define BAUD_921600		 ((ULONG)0x00800000)
#define BAUD_1000000     ((ULONG)0x01000000)
#define BAUD_1500000	 ((ULONG)0x02000000)
#define BAUD_2000000     ((ULONG)0x04000000)
#define BAUD_3000000	 ((ULONG)0x08000000)
#define BAUD_4000000     ((ULONG)0x20000000)

#endif
