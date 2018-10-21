//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2013 Intel Corporation All Rights Reserved. 
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


//----------------------------------------------------------------------------------
// File: chw.h Provides interface to XHCI host controller. Main class for XHCI
//----------------------------------------------------------------------------------

#ifndef CHW_H
#define CHW_H

#include <CRegEdit.h>
#include "usb200.h"
#include "hal.h"
#include "datastructures.h"
#include "sync.hpp"
#include "chcd.hpp"
#include "cpipe.h"
#include "ctransfer.h"

// Registry related defines

// XHCI Host Driver registry setting for pipe cache entries
#define XHCI_REG_DesiredPipeCacheSize   (TEXT("HcdPipeCache"))
#define DEFAULT_PIPE_CACHE_SIZE (8)
#define MINIMUM_PIPE_CACHE_SIZE (4)
#define MAXIMUM_PIPE_CACHE_SIZE (120)

class CHW;
class CXhcd;
class CXhcdEventHandler;
class CXhcdRing;

typedef struct _PERIOD_TABLE
{
    UCHAR bPeriod;
    UCHAR bQhIdx;
    UCHAR bInterruptScheduleMask;
} PERIOD_TABLE, *PPERIOD_TABLE;

typedef struct _PIPE_LIST_ELEMENT
{
    CPipe*                      pPipe;
    struct _PIPE_LIST_ELEMENT * pNext;
} PIPE_LIST_ELEMENT, *PPIPE_LIST_ELEMENT;

class CBusyPipeList : public LockObject
{
public:
    CBusyPipeList(DWORD dwFrameSize) : m_pBusyPipeList(NULL)
    {
        m_FrameListSize = dwFrameSize;
    };
    ~CBusyPipeList()
    {
        DeInit();
    };
    BOOL Init();
    VOID DeInit() const;
    BOOL AddToBusyPipeList(IN CPipe * const pPipe, IN const BOOL fHighPriority);
    BOOL RemoveFromBusyPipeList(IN CPipe * const pPipe);
    CPipe* GetPipe(IN UCHAR bSlotId, IN UINT uEpAddress);

private:

    DWORD   m_FrameListSize;

    // CheckForDoneTransfersThread related variables
    PPIPE_LIST_ELEMENT m_pBusyPipeList;
#ifdef DEBUG
    INT              m_debug_numItemsOnBusyPipeList;
#endif // DEBUG
};

// this class is an encapsulation of XHCI hardware registers.
class CHW : public CHcd
{
    friend class CXhcdEventHandler;
    friend class CXhcdRing;

public:

    //
    // Hardware Init/Deinit routines
    //
    CHW(IN const REGISTER portBase,
        IN const DWORD dwSysIntr,
        IN CPhysMem * const pCPhysMem,
        IN LPVOID pvXhcdPddObject,
        IN LPCTSTR lpDeviceRegistry);
    ~CHW();
    virtual BOOL Initialize();
    virtual VOID DeInitialize(BOOL fFreeMemory);

    VOID EnterOperationalState(VOID);

    VOID StopHostController(VOID);

    LPCTSTR GetControllerName(VOID) const
    {
        return m_s_cpszName;
    }

    //
    // Functions to Query frame values
    //
    BOOL GetFrameNumber(OUT LPDWORD lpdwFrameNumber);
    BOOL GetFrameLength(OUT LPUSHORT lpuFrameLength);
    BOOL SetFrameLength(IN HANDLE hEvent, IN USHORT wFrameLength);
    BOOL StopAdjustingFrame(VOID);

    //
    // Root Hub Queries
    //
    BOOL GetPortStatus(IN const UCHAR bPort, OUT USB_HUB_AND_PORT_STATUS& rStatus) const;
    BOOL RootHubFeature(IN const UCHAR bPort,
                        IN const UCHAR bSetOrClearFeature,
                        IN const USHORT wFeature);
    BOOL ResetAndEnablePort(IN const UCHAR bPort) const;
    VOID DisablePort(IN const UCHAR bPort) const;
    VOID CheckPortStatus() const;
    virtual BOOL WaitForPortStatusChange(HANDLE m_hHubChanged) const;  


    // PowerCallback
    VOID PowerMgmtCallback(IN BOOL fOff);


    CRegistryEdit   m_deviceReg;
    UINT            m_uTestCount;

private:

    CHW& operator=(CHW&);

    static DWORD CALLBACK UsbInterruptThreadStub(IN PVOID pContext);
    DWORD UsbInterruptThread();

    static DWORD CALLBACK UsbAdjustFrameLengthThreadStub(IN PVOID pContext);
    DWORD UsbAdjustFrameLengthThread();

    VOID UpdateFrameCounter(VOID);
    VOID SuspendHostController();
    VOID ResumeHostController();

    // utility functions
    BOOL ReadUSBHwInfo();

#ifdef DEBUG
    // Query Host Controller for registers, and prints contents
    VOID DumpUSBCMD(VOID);
    VOID DumpUSBSTS(VOID);
    VOID DumpUSBINTR(VOID);
    VOID DumpAllRegisters(VOID);
    VOID DumpPORTSC(IN const USHORT wPort);
    VOID DumpRings(VOID);
    VOID CheckUSBSTS(VOID);
#endif

    REGISTER    m_capBase;
    REGISTER    m_opBase;
    REGISTER    m_runBase;
    REGISTER    m_irSetBase;

    UCHAR       m_bNumOfPort;

    //readonly HC data
    UINT        m_uHcsParams1;
    UINT        m_uHcsParams2;
    UINT        m_uHcsParams3;
    UINT        m_uHccParams;
    UINT        m_uiContextSize;

    INT         m_iPageSize;
    INT         m_iPageShift;

    CXhcdRing*          m_pXhcdRing;
    CXhcdEventHandler*  m_pXhcdEventHandler;

    CBusyPipeList m_cBusyPipeList;

    // internal frame counter variables
    CRITICAL_SECTION m_csFrameCounter;
    DWORD   m_dwFrameListMask;
    DWORD   m_dwFrameCounterHighPart;
    DWORD   m_dwFrameCounterLowPart;

    // interrupt thread variables
    DWORD    m_dwSysIntr;
    HANDLE   m_hUsbInterruptEvent;
    HANDLE   m_hUsbInterruptThread;
    BOOL     m_fUsbInterruptThreadClosing;

    // frame length adjustment variables
    // note - use LONG because we need to use InterlockedTestExchange
    LONG     m_fFrameLengthIsBeingAdjusted;
    LONG     m_fStopAdjustingFrameLength;
    HANDLE   m_hAdjustDoneCallbackEvent;
    USHORT   m_usNewFrameLength;

    DWORD   m_dwCapability;
    BOOL    m_fDoResume;

    BOOL    m_fInitialized;
    UCHAR   m_ucPortChange [MAX_PORT_NUMBER]; //array of dummy port change states

    static const TCHAR m_s_cpszName[5];

public:
    DWORD   SetCapability(DWORD dwCap);
    DWORD   GetCapability()
    {
        return m_dwCapability;
    };

protected:
    CPhysMem *m_pMem;

private:

    // initialization parameters for the IST to support CE resume
    //(resume from fully unpowered controller).
    LPVOID    m_pPddContext;
    BOOL g_fPowerUpFlag ;
    BOOL g_fPowerResuming ;
    DWORD   m_dwBusyIsochPipes;
    DWORD   m_dwBusyAsyncPipes;

    DEVICE_CONTEXT_ARRAY*  m_pDcbaa;
    ERST_TABLE             m_erst;
    SCRATCHPAD*            m_pScratchpad;     // Scratchpad 
    DOORBELL_ARRAY*        m_dba;             // doorbell array

    UCHAR   m_bPortArray[MAX_PORT_NUMBER];  //array of port capabilities
    UCHAR   m_bNumUsb2Ports;                //USB 2.0 ports number
    UCHAR   m_bNumUsb3Ports;                //USB 3.0 ports number

    DWORD   m_dwTickCountStartTime;         //start time in ticks

    // slot enabling and address device helpers
    COMPLETION    m_addrDev;
    UCHAR         m_bSlotId;
    LOGICAL_DEVICE *m_pLogicalDevices[MAX_SLOTS_NUMBER];

    INT Handshake(UINT uAddr, UINT uMask, UINT uDone, INT iSec) const;
    INT ResetHostController() const;
    INT InitializeMemory();
    VOID XhciWrite64(const UINT64 u64Val, UINT uRegs) const;
    UINT64 XhciRead64(UINT uRegs) const;
    VOID RingCommandDoorbell() const;

    VOID SetHostControllerEventDequeue() const;
    INT SetupPortArrays();
    VOID DeterminePortTypes(volatile UINT* puAddr, UCHAR bMajorRevision);
    INT ScratchpadAlloc();
    void ScratchpadFree ();

    UINT SetPortStateToNeutral(UINT uState) const;
    VOID SetLinkState(UCHAR bPortId, UINT uLinkState) const;
    VOID UsbXhcdClearPortChangeBits(USHORT wPortId) const;
    INT AllocateLogicalDevice(UCHAR bSlotd);
    INT ResetDevice(UCHAR bSlotId) const;
    COMMAND* AllocateCommand(BOOL fAllocateCompletion) const;


    INT RunController() const;
    PVOID EnableSlot(UCHAR* pbSlotId);

    DWORD GetStartTime () const
    {
        return m_dwTickCountStartTime;
    };

    INT AddressDevice(UCHAR bSlotId, UINT uPortId, UCHAR bSpeed, UCHAR bRootHubPort, UCHAR bHubSID, UINT uDevRoute);
    UINT WaitForCompletion(COMPLETION* pComplete, UINT uTimeout) const;

    VOID InitListHead(PLIST_HEAD ptr) const;
    VOID UsbListLink(PLIST_HEAD pHead, VOID* pStruct, PLINK pLink) const;
    VOID UsbListUnlink(PLIST_HEAD pHead, PLINK pLink) const;
    BOOL UsbListEmpty(PLIST_HEAD pList) const;

    BOOL GetCurrentEventSeg(UCHAR bSlotId,
                            UCHAR bEpIndex,
                            RING_SEGMENT* pEndpointRingSegment,
                            TRB* pDequeue,
                            TRANSFER_DATA** ppTd,
                            UINT64 u64EventDma);

public:

    INT PrepareTransfer(UCHAR bSlotId,
                        UCHAR uEpAddress,
                        TRANSFER_DATA** ppTd,
                        UINT uTransferLength,
                        UCHAR bEndpointType) const;

    BOOL IsTrbInSement(RING_SEGMENT *pStartSeg,
                                TRB *pDequeueTrb,
                                TRB *pTrb,
                                UINT64 u64EventDma) const;

    INT ChangeMaxPacketSize(UCHAR bSlotId, UINT bMaxPktSize);
    BOOL AddEndpoint(UCHAR bSlotId, USB_ENDPOINT_DESCRIPTOR* peptDescr) const;
    BOOL DoConfigureEndpoint(UCHAR bSlotId, USB_DEVICE_DESCRIPTOR* pDevDesc, USB_HUB_DESCRIPTOR* pHubDesc) const;
    BOOL FreeLogicalDevice(UCHAR bSlotId) const;
    BOOL ResetEndpoint(UCHAR bSlotId, UINT uEpAddress, UCHAR bEndpointType) const;
    BOOL DropEndpoint(UCHAR bSlotId, UINT uEpAddress) const;
    VOID PutTransferToRing(RING *pRing, BOOL fConsumer, UINT uField1, UINT uField2, UINT uField3, UINT uField4) const;
    VOID RingTransferDoorbell(UCHAR bSlotId,
                              UCHAR bEpIndex,
                              INT iStartCycle,
                              NORMAL_TRB *pStartTrb) const;
    ENDPOINT_CONTEXT_DATA* GetEndpointContext(CONTAINER_CONTEXT *pContext,
                                              UCHAR bEpIndex) const;
    
    BOOL GetPowerUpFlag() const
    {
        return g_fPowerUpFlag;
    };
    BOOL GetPowerResumingFlag() const
    {
        return g_fPowerResuming;
    };
    CPhysMem * GetPhysMem() const
    {
        return m_pMem;
    };
    UCHAR GetNumberOfPort() const
    {
        return m_bNumOfPort;
    };


    //Bridge To its Instance.
    BOOL AddToBusyPipeList(IN CPipe * const pPipe, IN const BOOL fHighPriority);
    VOID RemoveFromBusyPipeList(IN CPipe * const pPipe);

    INT HaltController() const;
    INT CleanupHaltedEndpoint(UCHAR bSlotId, UCHAR bEpAddress) const;
    BOOL CountAbortTransfers(UCHAR bSlotId, UCHAR bEpAddress);

};

#endif /* CHW_H */