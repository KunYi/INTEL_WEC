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

// Copyright (c) 2002 BSQUARE Corporation.  All rights reserved.
// DO NOT REMOVE --- BEGIN EXTERNALLY DEVELOPED SOURCE CODE ID 40973--- DO NOT REMOVE
//
// Copyright 2008 Atheros Communications, Inc.
//     add one 1 bit interrupt mode handling

//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2014 Intel Corporation All Rights Reserved. 
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

#ifndef _SDHCSLOT_DEFINED
#define _SDHCSLOT_DEFINED

#include <SDCardDDK.h>
#include <SDHCD.h>
#include <ceddk.h>
#include <windot11.h>
#include <bldver.h>
#include "SDHCRegs.h"
#include "Sdhcdma.hpp"
#include "SDHCPowerCtrlBase.hpp"
#include "sd3voltage.h"

#define SDHC_MAX_POWER_SUPPLY_RAMP_UP   250     // SD Phys Layer 6.6
#define NUM_BYTE_FOR_POLLING_MODE 0x800
#define POLLING_MODE_TIMEOUT_DEFAULT    2

#define SDHC_POWER_UP_DELAY_KEY         _T("PowerUpDelay")
#define SDHC_WAKEUP_SOURCES_KEY         _T("DefaultWakeupSources")
#define SDHC_CAN_WAKE_ON_INT_KEY        _T("AllowWakeOnSDIOInterrupts")
#define SDHC_FREQUENCY_KEY              _T("BaseClockFrequency")
#define SDHC_TIMEOUT_FREQUENCY_KEY      _T("TimeoutClockFrequency")
#define SDHC_TIMEOUT_KEY                _T("TimeoutInMS")
#define SDHC_POLLINGMODE_SIZE           _T("PollingModeSize")
#define SDHC_POLLINGMODE_TIMEOUT        _T("PollingModeTiemout")
#define SDHC_DISABLE_DMA_KEY            _T("DisableDMA")
#define SDHC_IDLE1BITIRQ                _T("Idle1BitIRQ")

#define ADJUST_BUS_IDLE                 TRUE
#define ADJUST_BUS_NORMAL               FALSE

#define WORD_NOT_SET                    (WORD) -1

#define SD3_POWERUP_START_DELAY         3

#if (CE_MAJOR_VER > 7)
typedef DOT11_PUBLIC_ADAPTIVE_CTRL SD_ADAPTIVE_CONTROL;
typedef PDOT11_PUBLIC_ADAPTIVE_CTRL PSD_ADAPTIVE_CONTROL;
#endif

typedef class CSDHCSlotBase {
    friend class CSDHCSlotBaseDMA;
    friend class CSDHCSlotBaseSDMA;
    friend class CSDHCSlotBase32BitADMA2;
    friend class CSDHCPowerCtrlBase;
public:
    // Constructor - only initializes the member data. True initialization
    // occurs in Init().
    CSDHCSlotBase();
    virtual ~CSDHCSlotBase();

    // Perform basic initialization including initializing the hardware
    // so that the capabilities register can be read.
    virtual BOOL Init(DWORD dwSlot, volatile BYTE *pbRegisters,
        PSDCARD_HC_CONTEXT pHCDContext, DWORD dwSysIntr, HANDLE hBusAccess,
        INTERFACE_TYPE interfaceType, DWORD dwBusNumber, CReg *pregDevice);

    // Second stage of hardware initialization. Complete slot configuration
    // and enable interrupts.
    virtual SD_API_STATUS Start();
    // Signals card removal disables the slot.
    virtual SD_API_STATUS Stop();

    // Process a slot option call.
    virtual SD_API_STATUS SlotOptionHandler(SD_SLOT_OPTION_CODE sdOption, 
        PVOID pData, DWORD cbData);

    // Get this slot's power state.
    virtual CEDEVICE_POWER_STATE GetPowerState() { return m_cpsCurrent; }

    // What power state is required upon power up?
    virtual CEDEVICE_POWER_STATE GetPowerUpRequirement() { return m_cpsAtPowerDown; }

    // Called when the device is suspending.
    virtual VOID PowerDown();
    // Called when the device is resuming.
    virtual VOID PowerUp();

    // Start this bus request.
    virtual SD_API_STATUS BusRequestHandler(PSD_BUS_REQUEST pRequest);

    // Returns TRUE if the interrupt routine needs servicing, say at
    // initialization to see if a card is present.
    virtual BOOL NeedsServicing() { return m_fCheckSlot; }

    // Handle a slot interrupt. Also called when NeedsServicing() returns TRUE.
    virtual VOID HandleInterrupt();

    // Called by the controller to get the controller interrupt register.
    inline WORD ReadControllerInterrupts() 
    {
        m_ControllerInterrupts = ReadWord(SDHC_SLOT_INT_STATUS);
        return m_ControllerInterrupts;
    }

    //Factory Method for Creating Power Control
    SD_API_STATUS CreatePowerCtrl(SDCARD_DEVICE_TYPE device0Type, PTCHAR loadPath);
    // Place the slot into the desired power state.
    virtual VOID    SetHardwarePowerState(CEDEVICE_POWER_STATE cpsNew);
    //gets the local adaptive control from the slot
    virtual SD_API_STATUS GetLocalAdaptiveControl(PSD_ADAPTIVE_CONTROL pAdaptiveControl);

    BOOL                    IsPowerManaged () {return m_fIsPowerManaged;};
    BOOL                    SleepsWithPower () {return m_fSleepsWithPower;};
    BYTE                    WakeupControl () {return m_bWakeupControl;};

    CEDEVICE_POWER_STATE    CurrentPowerState() {return m_cpsCurrent;};

    VOID                    ControlCardPowerOff(
                                BOOL *pbfKeepPower, 
                                BOOL bResetOnSuspendResume, 
                                BOOL bPwrControlOnSuspendResume);

    VOID                    ControlCardPowerOn(
                                BOOL bResetOnSuspendResume,
                                BOOL bPwrControlOnSuspendResume);

    VOID        RestoreInterrupts(BOOL UseNewRestoreMethod);

protected:
    CSDHCSlotBase(const CSDHCSlotBase&); 
    CSDHCSlotBase& operator=(CSDHCSlotBase&);

    virtual SD_API_STATUS SubmitBusRequestHandler(PSD_BUS_REQUEST pRequest );

    // What is this slot's maximum clock rate?
    virtual DWORD DetermineMaxClockRate();

    // What is this slot's maximum block length?
    virtual DWORD DetermineMaxBlockLen();

    // What should this slot use for timeout control?
    virtual DWORD DetermineTimeoutControl();

    // What are the default wakeup sources?
    virtual DWORD DetermineWakeupSources();
    
    // Set the slot voltage.
    virtual VOID SetVoltage(DWORD dwVddSetting);
    // Set the bus width and clock rate.
    virtual VOID SetInterface(PSD_CARD_INTERFACE_EX pInterface);

    // Set this slot's power state.
    virtual VOID SetPowerState(CEDEVICE_POWER_STATE cpsNew);

    // Get the capabilities register.
    virtual SSDHC_CAPABILITIES GetCapabilities()
    {
        SSDHC_CAPABILITIES pCaps = {0};
        if (m_pCaps != NULL)
        {
            m_pCaps->dw = ReadDword(SDHC_CAPABILITIES);
            return *m_pCaps;
        }
        return pCaps;
    }

    virtual SSDHC_VERSION GetSDHCVersion()
    {
        SSDHC_VERSION pVersion = {0};
        if (m_pVersion != NULL)
        {
            m_pVersion->uw = ReadWord(SDHC_HOST_CONTROLLER_VER);
            return *m_pVersion;
        }
        return pVersion;
    }

    // Fill in the slot info structure.
    virtual SD_API_STATUS InitSlotInfo(PSDCARD_HC_SLOT_INFO pSlotInfo);

    // Get the desired Vdd window.
    virtual DWORD GetDesiredVddWindow();

    // Get the max Vdd window.
    virtual DWORD GetMaxVddWindow();

    // Is the card write-protected?
    virtual BOOL IsWriteProtected() 
    {
        m_IsWriteProtected = ReadDword(SDHC_PRESENT_STATE);
        return ((m_IsWriteProtected & STATE_WRITE_PROTECT) == 0);
    }

    // Enable/disable SDIO card interrupts.
    virtual VOID EnableSDIOInterrupts(BOOL fEnable);

    // How much extra time in ms for initial clocks is needed upon
    // insertion of a card for the power supply ramp up?
    virtual DWORD GetPowerSupplyRampUpMs() 
    {
        if (m_dwPowerSupplyRampUpMs == -1)
        {
            m_dwPowerSupplyRampUpMs = m_pregDevice->ValueDW(SDHC_POWER_UP_DELAY_KEY,
                SDHC_MAX_POWER_SUPPLY_RAMP_UP);
        }
        return m_dwPowerSupplyRampUpMs;
    }
    
    // Register access routines. These are not virtual so that we get
    // good inline read/write perf.
    template <class T>
    inline VOID WriteReg   (DWORD dwOffset, T tValue) {
        CheckRegOffset(dwOffset, sizeof(T));
        volatile T *ptRegister = (volatile T *) (m_pbRegisters + dwOffset);
        *ptRegister = tValue;
    }
    template <class T>
    inline T ReadReg       (DWORD dwOffset) {
        CheckRegOffset(dwOffset, sizeof(T));
        volatile T *ptRegister = (volatile T *) (m_pbRegisters + dwOffset);
        return *ptRegister;
    }

    inline BYTE  ReadByte  (DWORD dwOffset) {
        return ReadReg<BYTE>(dwOffset);
    }
    inline VOID  WriteByte (DWORD dwOffset, BYTE bValue) {
        WriteReg(dwOffset, bValue);
    }

    inline WORD  ReadWord  (DWORD dwOffset) {
        return ReadReg<WORD>(dwOffset);
    }
    inline VOID  WriteWord (DWORD dwOffset, WORD wValue) {
        WriteReg(dwOffset, wValue);
    }

    inline DWORD ReadDword (DWORD dwOffset) {
        return ReadReg<DWORD>(dwOffset);
    }
    inline VOID  WriteDword(DWORD dwOffset, DWORD dwValue) {
        WriteReg(dwOffset, dwValue);
    }


    // Interrupt handling methods
    virtual VOID HandleRemoval(BOOL fCancelRequest);
    virtual VOID HandleInsertion();
    virtual BOOL HandleCommandComplete();
    virtual VOID HandleErrors();
    virtual VOID HandleTransferDone();
    virtual VOID HandleReadReady();
    virtual VOID HandleWriteReady();
    virtual SD_API_STATUS HandleSuspend(BOOL fCancelRequest);
    virtual SD_API_STATUS HandleResume();
    virtual VOID HandleSlotReset(BOOL fCancelRequest);
    virtual PVOID SlotAllocDMABuffer(ULONG Length,PPHYSICAL_ADDRESS  LogicalAddress,BOOLEAN CacheEnabled );
    virtual BOOL SlotFreeDMABuffer( ULONG Length,PHYSICAL_ADDRESS  LogicalAddress,PVOID VirtualAddr,BOOLEAN CacheEnabled );


    // Allocates a physical buffer for DMA.
    virtual PVOID AllocPhysBuffer(size_t cb, PDWORD pdwPhysAddr);

    // Frees the physical buffer.
    virtual VOID FreePhysBuffer(PVOID pv);

    // Performs the actual enabling/disabling of SDIO card interrupts.
    virtual VOID DoEnableSDIOInterrupts(BOOL fEnable);

    // Perform the desired reset and wait for completion. Returns FALSE
    // if there is a timeout.
    virtual BOOL SoftwareReset(BYTE bResetBits);

    // Keep reading the register using (*pfnReadReg)(dwRegOffset) until
    // value & tMask == tWaitForEqual.
    template<class T>
    BOOL WaitForReg(
        T (CSDHCSlotBase::*pfnReadReg)(DWORD),
        DWORD dwRegOffset,
        T tMask,
        T tWaitForEqual,
        DWORD dwTimeout = 1000        
        );

    // Turn the LED on or off.
    virtual VOID EnableLED(BOOL fEnable);

    // Calls SDHCDIndicateSlotStateChange.
    virtual VOID IndicateSlotStateChange(SD_SLOT_EVENT sdEvent);

    // Calls SDHCDGetAndLockCurrentRequest.
    virtual PSD_BUS_REQUEST GetAndLockCurrentRequest();

    virtual VOID UnlockCurrentRequest(PSD_BUS_REQUEST pRequest);

    // Calls SDHCDPowerUpDown.
    virtual VOID PowerUpDown(BOOL fPowerUp, BOOL fKeepPower);

    // Calls SDHCDIndicateBusRequestComplete.
    virtual VOID IndicateBusRequestComplete(PSD_BUS_REQUEST pRequest, SD_API_STATUS status);

    //Updates the HC Owned Request
    virtual VOID UpdateCurrentHCOwned(PSD_BUS_REQUEST pRequest);

    //Checks the hardware
    virtual SD_API_STATUS CheckHardware(PSD_CARD_STATUS pCardStatus);

    //sets the adaptive control of the upper driver
    virtual SD_API_STATUS UpcallSetAdaptiveControl(PSD_ADAPTIVE_CONTROL pAdaptiveControl);

    //gets the adaptive control from the upper driver
    virtual SD_API_STATUS UpcallGetAdaptiveControl(PSD_ADAPTIVE_CONTROL pAdaptiveControl);

    //frees all the outstanding requests
    virtual SD_API_STATUS FreeAllOutStandingRequests();

    //synchronous slot event
    virtual SD_API_STATUS SynchronousSlotStateChange(SD_SLOT_EVENT Event);

    //Added to follow the flow diagram in SDHC 2.0 Spec
    virtual SD_API_STATUS HandleResetDevice();

    // Finds the closest rate that is *pdwRate or lower. Stores the
    // actual rate in *pdwRate.
    virtual VOID SetClockRate(PDWORD pdwRate);

    // Turn on the SD clock according to the clock divisor found 
    // in SetClockRate().
    virtual VOID SDClockOn();

    // Turn off the SD clock.
    virtual VOID SDClockOff();

    // Determine the Vdd windows from the capabilities register.
    virtual DWORD DetermineVddWindows();

    // Set an interrupt event.
    virtual VOID SetInterruptEvent() { ::SetInterruptEvent(m_dwSysIntr); }

    virtual BOOL DetermineCommandPolling();
    virtual BOOL PollingForCommandComplete();

    // Handle Idle bus case
    virtual VOID IdleBusAdjustment(BOOL Idle);

    virtual VOID InitCard();

    virtual BOOL CheckHardwareStatus();

    // 1.8 signaling switch
    SD_API_STATUS DoHWVoltageSwitch();

#ifdef DEBUG
    // Print out the standard host register set.
    virtual VOID DumpRegisters();

    // Validate the member data.
    virtual VOID Validate();

    // Verify that the desired register accesses are properly aligned.
    VOID CheckRegOffset(DWORD dwOffset, DWORD dwRegSize) {
        DEBUGCHK( (dwOffset % dwRegSize) == 0);
        DEBUGCHK(dwOffset < sizeof(SSDHC_REGISTERS));
        DEBUGCHK( (dwOffset + dwRegSize) <= sizeof(SSDHC_REGISTERS));
    }
#else
    // These routines do nothing in non-debug builds.
    inline VOID DumpRegisters() {}
    inline VOID Validate() {}
    inline VOID CheckRegOffset(DWORD /*dwOffset*/, DWORD /*dwRegSize*/) {}
#endif

private:
    VOID        CancelRequest();
    VOID        WaitForRequestComplete();
    VOID        EnableWakeUpSources();
    HANDLE      m_hRequestCompletedEvent;


protected:
    BOOL                    m_fUhsIsEnabled;
    CReg                   *m_pregDevice;           // pointer to device registry key
    CSDHCSlotBaseDMA       *m_SlotDma;              // DMA object
    CSDHCPowerCtrlBase     *m_pSDHCPowerCtrlBase;   // Slot power control object
    DWORD                   m_dwSlot;               // physical slot number
    volatile BYTE          *m_pbRegisters;          // memory-mapped registers

    PSDCARD_HC_CONTEXT      m_pHCDContext;          // host context
    DWORD                   m_dwSysIntr;            // system interrupt
    HANDLE                  m_hBusAccess;           // bus parent
    INTERFACE_TYPE          m_interfaceType;        // interface of the controller
    DWORD                   m_dwBusNumber;          // bus number of the controller

    DWORD                   m_dwVddWindows;         // supported VDD windows
    DWORD                   m_dwMaxClockRate;       // maximum clock rate
    DWORD                   m_dwTimeoutControl;     // timeout control value
    DWORD                   m_dwMaxBlockLen;        // maximum block length

    WORD                    m_wRegClockControl;     // register value of Clock Control
    WORD                    m_wIntSignals;          // saved int signals for powerup
    CEDEVICE_POWER_STATE    m_cpsCurrent;           // current power state
    CEDEVICE_POWER_STATE    m_cpsAtPowerDown;       // power state at PowerDown()

    DWORD                   m_dwDefaultWakeupControl;   // wakeup source list 
    BYTE                    m_bWakeupControl;           // current wakeup interrupts
    UCHAR                   m_OldVoltage;               // voltage at power down
    SSDHC_VERSION           *m_pVersion;                // version
    SSDHC_CAPABILITIES      *m_pCaps;                   // capabilities
    WORD                    m_ControllerInterrupts;     // controller interrupts
    DWORD                   m_IsWriteProtected;         // write protected flag
    SD_CARD_RCA             m_CardRCA;                  // card relative address

#ifdef DEBUG
    DWORD                   m_dwReadyInts;          // number of Read/WriteReady interrupts that have occurred
#endif DEBUG

    BOOL                    m_fCommandCompleteOccurred;     // has the Command Complete occurred for the current transfer?

    PSD_BUS_REQUEST         m_pCurrentRequest; // Current Processing Request.
    BOOL                    m_fCurrentRequestFastPath;
    SD_API_STATUS           m_FastPathStatus;
    DWORD                   m_dwFastPathTimeoutTicks;

    DWORD                   m_dwPollingModeSize;
    DWORD                   m_dwPowerSupplyRampUpMs;

    BOOL                    m_fSleepsWithPower : 1;         // keep power in PowerDown()?
    BOOL                    m_fPowerUpDisabledInts : 1;     // did PowerUp disable SDIO card interrupts?
    BOOL                    m_fIsPowerManaged : 1;          // is the power manager handling us?
    BOOL                    m_fSDIOInterruptsEnabled : 1;   // are SDIO card interrupts enabled?
    BOOL                    m_fCardPresent : 1;             // is a card present
    BOOL                    m_fAutoCMD12Success : 1;        // AutoCMD12 success
    BOOL                    m_fCheckSlot : 1;               // does HandleInterrupt() need to be run?
    BOOL                    m_fCanWakeOnSDIOInterrupts : 1; // can wake on SDIO interrupts
    BOOL                    m_f4BitMode : 1;                // 4 bit bus mode?
    BOOL                    m_fFakeCardRemoval : 1;         // should we simulate card removal?
    BOOL                    m_fCommandPolling: 1; 
    BOOL                    m_fDisableDMA:1;                // Disable The DMA
    BOOL                    m_Idle1BitIRQ : 1;              // move to 1-bit mode when idle and expecting IRQ
    BOOL                    m_fSuspended: 1;

} *PCSDHCSlotBase;


#define CB_DMA_BUFFER 0x20000 // 128KB buffer
#define CB_DMA_PAGE   0x1000  // we program DMA for 4KB pages


#define TRANSFER_IS_WRITE(pRequest)        (SD_WRITE == (pRequest)->TransferClass)
#define TRANSFER_IS_READ(pRequest)         (SD_READ == (pRequest)->TransferClass)
#define TRANSFER_IS_COMMAND_ONLY(pRequest) (SD_COMMAND == (pRequest)->TransferClass)      

#define TRANSFER_SIZE(pRequest)            ((pRequest)->BlockSize * (pRequest)->NumBlocks)


#define SDHC_DEFAULT_TIMEOUT                2000 // 2 seconds


// Is this request an SDIO abort (CMD52, Function 0, I/O Abort Reg)?
inline
BOOL
TransferIsSDIOAbort(
                    PSD_BUS_REQUEST pRequest
                    )
{
    PREFAST_DEBUGCHK(pRequest);

    BOOL fRet = FALSE;
    
    if (pRequest->CommandCode == SD_CMD_IO_RW_DIRECT) {
        if (IO_RW_DIRECT_ARG_FUNC(pRequest->CommandArgument) == 0) {
            if (IO_RW_DIRECT_ARG_ADDR(pRequest->CommandArgument) == SD_IO_REG_IO_ABORT) {
                fRet = TRUE;
            }
        }
    }

    return fRet;
}

#endif // _SDHCSLOT_DEFINED

// DO NOT REMOVE --- END EXTERNALLY DEVELOPED SOURCE CODE ID --- DO NOT REMOVE

