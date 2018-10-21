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
// File: chw.cpp
//
// Description:
// 
//     Main class for XHCI
//     This file implements the XHCI specific register routines
//----------------------------------------------------------------------------------

#include <windows.h>
#include <nkintr.h>

#include "globals.hpp"
#include "chw.h"
#include "ceventhandler.h"
#include "cring.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

//----------------------------------------------------------------------------------
// Function: Init
//
// Description: Constructor for CBusyPipeList
//
// Parameters: None
//
// Returns: Always TRUE
//----------------------------------------------------------------------------------
BOOL CBusyPipeList::Init()
{
    m_pBusyPipeList = NULL;

#ifdef DEBUG
    m_debug_numItemsOnBusyPipeList = 0;
#endif

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: DeInit
//
// Description: Destructor for CBusyPipeList
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CBusyPipeList::DeInit() const
{
}

//----------------------------------------------------------------------------------
// Function: GetPipe
//
// Description: Get pipe by slot ID and endpoint index
//
// Parameters: bSlotId - device slot id
//
//             uEpAddress - endpoint address
//
// Returns: pointer to pipe
//----------------------------------------------------------------------------------
CPipe* CBusyPipeList::GetPipe(IN UCHAR bSlotId, IN UINT uEpAddress) 
{
    CPipe* pPipe = NULL;

    Lock();

    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = m_pBusyPipeList;

    while(pCurrent != NULL)
    {
        if((pCurrent->pPipe->GetDeviceAddress() == bSlotId) &&
           (pCurrent->pPipe->m_bEndpointAddress == uEpAddress))
        {
            pPipe = pCurrent->pPipe;
            break;
        }

        // this pipe is still busy. Move to next item
        pPrev = pCurrent;
        pCurrent = pPrev->pNext;
    }

    Unlock();
    return pPipe;
}

//----------------------------------------------------------------------------------
// Function: AddToBusyPipeList
//
// Description: Add the pipe indicated by pPipe to our list of busy pipes.
//              This allows us to check for completed transfers after 
//              getting an interrupt, and being signaled via 
//              SignalCheckForDoneTransfers
//
// Parameters: pPipe - pipe to add to busy list
//
//             fHighPriority - if TRUE, add pipe to start of busy list,
//                             else add pipe to end of list.
//
// Returns: TRUE if pPipe successfully added to list, else FALSE
//----------------------------------------------------------------------------------
BOOL CBusyPipeList::AddToBusyPipeList(IN CPipe * const pPipe,
                                        IN const BOOL fHighPriority)
{
    DEBUGMSG(ZONE_PIPE,
        (TEXT("XHCI: +CPipe::AddToBusyPipeList - new pipe(%s) 0x%x, pri %d\n"),
        pPipe->GetPipeType(),
        pPipe,
        fHighPriority));

    PREFAST_DEBUGCHK(pPipe != NULL);
    BOOL fSuccess = FALSE;

    // make sure there nothing on the pipe already(it only gets officially
    // added after this function succeeds).
    Lock();
   
    PPIPE_LIST_ELEMENT pNewBusyElement = new PIPE_LIST_ELEMENT;

    if(pNewBusyElement != NULL)
    {
        pNewBusyElement->pPipe = pPipe;

        if(fHighPriority || m_pBusyPipeList == NULL)
        {
            // add pipe to start of list
            pNewBusyElement->pNext = m_pBusyPipeList;
            m_pBusyPipeList = pNewBusyElement;
        }
        else
        {
            // add pipe to end of list
            PPIPE_LIST_ELEMENT pLastElement = m_pBusyPipeList;

            while(pLastElement->pNext != NULL)
            {
                pLastElement = pLastElement->pNext;
            }

            pNewBusyElement->pNext = NULL;
            pLastElement->pNext = pNewBusyElement;
        }

        fSuccess = TRUE;
    #ifdef DEBUG
        m_debug_numItemsOnBusyPipeList++;
    #endif // DEBUG
    }

    Unlock();
    DEBUGMSG(ZONE_PIPE,
        (TEXT("XHCI: -CPipe::AddToBusyPipeList - \
              new pipe(%s) 0x%x, pri %d, returning BOOL %d\n"),
        pPipe->GetPipeType(),
        pPipe,
        fHighPriority,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: RemoveFromBusyPipeList
//
// Description: Remove this pipe from our busy pipe list. This happens if
//              the pipe is suddenly aborted or closed while a transfer
//              is in progress
//
// Parameters: pPipe - pipe to remove from busy list
//
// Returns: None
//----------------------------------------------------------------------------------
BOOL CBusyPipeList::RemoveFromBusyPipeList(IN CPipe * const pPipe) 
{
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE,
        (TEXT("XHCI: +CPipe::RemoveFromBusyPipeList - pipe(%s) 0x%x\n"),
        pPipe->GetPipeType(),
        pPipe));

    Lock();

    BOOL fRemovedPipe = FALSE;
#ifdef DEBUG
{
    // check m_debug_numItemsOnBusyPipeList
    PPIPE_LIST_ELEMENT pBusy = m_pBusyPipeList;
    INT iCount = 0;

    while(pBusy != NULL)
    {
        DEBUGCHK(pBusy->pPipe != NULL);
        pBusy = pBusy->pNext;
        iCount++;
    }

    DEBUGCHK(m_debug_numItemsOnBusyPipeList == iCount);
}
#endif // DEBUG

    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = m_pBusyPipeList;

    while(pCurrent != NULL)
    {
        if(pCurrent->pPipe == pPipe)
        {
            // Remove item from the linked list
            if(pCurrent == m_pBusyPipeList)
            {
                DEBUGCHK(pPrev == NULL);
                m_pBusyPipeList = m_pBusyPipeList->pNext;
            }
            else
            {
                DEBUGCHK(pPrev != NULL && pPrev->pNext == pCurrent);
                pPrev->pNext = pCurrent->pNext;
            }

            delete pCurrent;
            pCurrent = NULL;
            fRemovedPipe = TRUE;
        #ifdef DEBUG
            DEBUGCHK(--m_debug_numItemsOnBusyPipeList >= 0);
        #endif // DEBUG
            break;
        }
        else
        {
            // Check next item
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }

    Unlock();

    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE && fRemovedPipe,
        (TEXT("XHCI: -CPipe::RemoveFromBusyPipeList, removed pipe(%s) 0x%x\n"),
        pPipe->GetPipeType(),
        pPipe));
    DEBUGMSG(ZONE_PIPE && ZONE_VERBOSE && !fRemovedPipe,
        (TEXT("XHCI: -CPipe::RemoveFromBusyPipeList, \
              pipe(%s) 0x%x was not on busy list\n"),
        pPipe->GetPipeType(),
        pPipe));

    return fRemovedPipe;
}

#define FRAME_LIST_SIZE (0x2000)

const TCHAR CHW::m_s_cpszName[5] = L"XHCD";

//----------------------------------------------------------------------------------
// Function: CHW
//
// Description: Constructor for CHW
//
// Parameters: portBase - Pointer to XHCI controller registers.
//
//             dwSysIntr - Logical value for XHCI interrupt (SYSINTR_xx).
//
//             pCPhysMem - Memory object.
//
//             pvXhcdPddObject - Pointer to PDD specific data for this instance.
//
//             lpDeviceRegistry - Registry key where XHCI configuration settings are stored.
//
// Returns: None
//----------------------------------------------------------------------------------
CHW::CHW(IN const REGISTER portBase,
         IN const DWORD dwSysIntr,
         IN CPhysMem * const pCPhysMem,
         IN LPVOID pvXhcdPddObject,
         IN LPCTSTR lpDeviceRegistry) : m_cBusyPipeList(FRAME_LIST_SIZE),
                                        m_deviceReg(HKEY_LOCAL_MACHINE,lpDeviceRegistry),
                                        m_pXhcdEventHandler(NULL),
                                        m_pXhcdRing(NULL),
                                        m_bNumUsb2Ports(0),
                                        m_bNumUsb3Ports(0),
                                        m_fInitialized(FALSE),
                                        m_pDcbaa(NULL),
                                        m_pScratchpad (NULL),
                                        m_dwTickCountStartTime(0)
{
    INT i;


    m_dwTickCountStartTime = GetTickCount ();
    g_fPowerUpFlag = FALSE;
    g_fPowerResuming = FALSE;
    m_capBase = portBase;
    m_opBase  = 
        portBase +
        HOST_CONTROLLER_LENGTH(READ_REGISTER_ULONG((PULONG)(portBase + USB_XHCD_CAPLENGTH)));
    m_runBase =
        portBase +
        (READ_REGISTER_ULONG((PULONG)(portBase + USB_XHCD_RTSOFF)) & RTS_OFF_MASK);
    m_irSetBase = m_runBase + 0x20;

    m_uHcsParams1 = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS));
    m_uHcsParams2 = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS2));
    m_uHcsParams3 = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS3));
    m_uHccParams  = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCCPARAMS));

    if (HOST_CONTROLLER_64_CONTEXT (m_uHccParams)) {
        m_uiContextSize = CTX_SIZE_64;
    } else {
        m_uiContextSize = CTX_SIZE_32;
    }

    m_bNumOfPort = MAX_PORTS(m_uHcsParams1);
    
    //m_pHcd = pHcd;
    m_pMem = pCPhysMem;
    m_pPddContext = pvXhcdPddObject;
    m_dwFrameListMask = FRAME_LIST_SIZE - 1;

    m_dwSysIntr = dwSysIntr;
    m_hUsbInterruptEvent = NULL;
    m_hUsbInterruptThread = NULL;
    m_fUsbInterruptThreadClosing = FALSE;

    m_fFrameLengthIsBeingAdjusted = FALSE;
    m_fStopAdjustingFrameLength = FALSE;
    m_hAdjustDoneCallbackEvent = NULL;
    m_usNewFrameLength = 0;
    m_dwCapability = 0;
    m_fDoResume=FALSE;
    m_dwBusyIsochPipes = 0;
    m_dwBusyAsyncPipes = 0;

    for(i = 0; i < MAX_SLOTS_NUMBER; i++)
    {
        m_pLogicalDevices [i] = NULL;
    }
    m_erst.pEntries = NULL;

    InitializeCriticalSection( &m_csFrameCounter );
}

//----------------------------------------------------------------------------------
// Function: ~CHW
//
// Description: Destructor for CHW
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CHW::~CHW()
{
    if(m_dwSysIntr)
    {
        KernelIoControl(IOCTL_HAL_DISABLE_WAKE,
                        &m_dwSysIntr,
                        sizeof(m_dwSysIntr),
                        NULL,
                        0,
                        NULL);
    }

    DeInitialize(TRUE);
    DeleteCriticalSection(&m_csFrameCounter);
}

//----------------------------------------------------------------------------------
// Function:
//
// Description: Force Host Controller into halt state.
//
// Parameters: None
//
// Returns: Returns:  0 if successful. Otherwise < 0.
//----------------------------------------------------------------------------------
INT CHW::HaltController() const
{
    UINT uhalted;
    UINT uCmd;
    UINT uMask;

    uMask = ~(INTERRUPT_REQUESTS);

    uhalted = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS)) & STATUS_HALT;
    if(!uhalted)
    {
        uMask &= ~COMMAND_RUN;
    }

    uCmd = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uCmd &= uMask;

    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uCmd);

    return Handshake((UINT)(m_opBase + USB_XHCD_USBSTS),
                        STATUS_HALT,
                        STATUS_HALT,
                        MAX_HALT);
}

//----------------------------------------------------------------------------------
// Function: Handshake
//
// Description: Reading  a register until handshake
//
// Parameters: uAddr - address of register to be read
//
//             uMask - mask of read result
//
//             uDone - expected result 
//
//             iSec - timeout
//
// Returns:  0 if successful. Otherwise < 0.
//----------------------------------------------------------------------------------
INT CHW::Handshake(UINT uAddr, UINT uMask, UINT uDone, INT iSec) const
{
    UINT uResult;
    UINT uCount = iSec;

    if(uCount == 0)
    {
        uCount = 1;
    }

    do
    {
        uResult = READ_REGISTER_ULONG((PULONG)uAddr);

        if(uResult == ~(UINT)0)
        {
            return -XHCD_INVALID;
        }

        uResult &= uMask;

        if(uResult == uDone)
        {
            return XHCD_OK;
        }

        Sleep(1);
        uCount--;
    } while(uCount > 0);

    return -XHCD_TIMEDOUT;
}

//----------------------------------------------------------------------------------
// Function: Initialize
//
// Description: Reset and Configure the Host Controller with the schedule.
//              This function is only called from the CXhcd::Initialize routine.
//              This function is static
//
// Parameters: None
//
// Returns: TRUE if initialization succeeded, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::Initialize()
{
    DEBUGMSG(ZONE_INIT,(TEXT("%s: +CHW::Initialize\n"),GetControllerName()));

    if (m_pXhcdEventHandler == NULL) 
    {
        m_pXhcdEventHandler = new CXhcdEventHandler(this, m_pMem);
    }
    if (m_pXhcdRing == NULL)
    {
        m_pXhcdRing = new CXhcdRing(this, m_pMem);
    }

    if((m_pXhcdEventHandler == NULL) || (m_pXhcdRing == NULL))
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("-CHW::Initialize - memory allocation arrer\n")));
        ASSERT(FALSE);
        return FALSE;
    }
    
    // set up the frame list area.
    if(m_opBase == 0 || m_cBusyPipeList.Init()==FALSE)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("%s: -CHW::Initialize - \
                  zero Register Base or CPeriodicMgr or CAsyncMgr fails\n"),
                  GetControllerName()));
        ASSERT(FALSE);
        return FALSE;
    }

    // read registry setting for XCHI core ID
    if(!ReadUSBHwInfo())
    {
        return FALSE;
    }
  
    // m_hUsbInterrupt - Auto Reset, and Initial State = non-signaled
    DEBUGCHK(m_hUsbInterruptEvent == NULL);
    TCHAR *cpszHsUsbFnIntrEvent = NULL;

    m_hUsbInterruptEvent = CreateEvent(NULL, FALSE, FALSE, cpszHsUsbFnIntrEvent);

    if(cpszHsUsbFnIntrEvent != NULL)
    {
        delete [] cpszHsUsbFnIntrEvent; 
    }

    if(m_hUsbInterruptEvent == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("%s: -CHW::Initialize. Error creating \
                  USBInterrupt or USBHubInterrupt event\n"),
                  GetControllerName()));

        return FALSE;
    }

    // Just to make sure this is really ours.
    InterruptDisable(m_dwSysIntr);

    if (HaltController() != XHCD_OK)
    {
        return FALSE;
    }

    if (ResetHostController() != XHCD_OK)
    {
        return FALSE;
    }

    if (InitializeMemory() != XHCD_OK)
    {
        return FALSE;
    }

    RunController();

    // Initialize Interrupt. When interrupt id # m_sysIntr is triggered,
    // m_hUsbInterruptEvent will be signaled. Last 2 params must be NULL
    if(!InterruptInitialize(m_dwSysIntr, m_hUsbInterruptEvent, NULL, NULL))
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("%s: -CHW::Initialize. Error on InterruptInitialize\n"),
            GetControllerName()));

        return FALSE;
    }

    // Start up our IST - the parameter passed to the thread
    // is unused for now
    DEBUGCHK(m_hUsbInterruptThread == NULL &&
              m_fUsbInterruptThreadClosing == FALSE);

    if(m_hUsbInterruptThread == NULL)
    {
        m_hUsbInterruptThread = CreateThread(0,
                                            0,
                                            UsbInterruptThreadStub,
                                            this,
                                            0,
                                            NULL);
    }

    if(m_hUsbInterruptThread == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("%s: -CHW::Initialize. Error creating IST\n"),
            GetControllerName()));

        return FALSE;
    }

    CeSetThreadPriority(m_hUsbInterruptThread, g_dwIstThreadPriority);

    if(!m_pXhcdEventHandler->Initialize())
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("%s: -CHW::Initialize. Error XhcdEventHandler initialization\n"),
            GetControllerName()));

        return FALSE;
    }
    
    DEBUGMSG(ZONE_INIT,
        (TEXT("%s: -CHW::Initialize, success!\n"),
        GetControllerName()));

    m_fInitialized = TRUE;

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: DeInitialize
//
// Description: Delete any resources associated with static members
//              This function is only called from the ~CXhcd() routine.
//              This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DeInitialize(BOOL fFreeMemory)
{
    INT i;

    // tell USBInterruptThread that we are closing
    m_fUsbInterruptThreadClosing = TRUE;
    for (i = 0; i < MAX_PORT_NUMBER; i++) 
    {
        m_ucPortChange [i] = 1;
    }
    m_fInitialized = FALSE;

    // Wake up the interrupt thread and give it time to die.
    if(m_hUsbInterruptEvent)
    {
        SetEvent(m_hUsbInterruptEvent);

        if(m_hUsbInterruptThread)
        {
            DWORD dwWaitReturn = WaitForSingleObject(m_hUsbInterruptThread, 1000);

            if(dwWaitReturn != WAIT_OBJECT_0)
            {
                DEBUGCHK(0);
            }

            CloseHandle(m_hUsbInterruptThread);
            m_hUsbInterruptThread = NULL;
        }

        // we have to close our interrupt before closing the event!
        InterruptDisable(m_dwSysIntr);

        CloseHandle(m_hUsbInterruptEvent);
        m_hUsbInterruptEvent = NULL;
    }
    else
    {
        InterruptDisable(m_dwSysIntr);
    }

    // Stop The Controller.

    m_cBusyPipeList.DeInit();

    for(i = 0; i < MAX_SLOTS_NUMBER; i++)
    {
        if (m_pLogicalDevices [i] != NULL) {
            m_pXhcdRing->FreeLogicalDevice(i);
            m_pLogicalDevices [i] = NULL;
        }
    }

    if (m_pXhcdEventHandler)
    {
        m_pXhcdEventHandler->DeInitialize();
        if (fFreeMemory) 
        {
            delete m_pXhcdEventHandler;
            m_pXhcdEventHandler = NULL;

            if (m_erst.pEntries != NULL) {
                m_pMem->FreeMemory((PBYTE)m_erst.pEntries,
                     m_pMem->VaToPa((PBYTE)m_erst.pEntries),
                    CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
                m_erst.pEntries = NULL;
            }
        }
    }
    ScratchpadFree ();

    XhciWrite64 (0,(UINT)(m_opBase + USB_XHCD_DCBAAP));
    
    if (fFreeMemory)
    {
        if (m_pDcbaa != NULL) {
            m_pMem->FreeMemory((PBYTE)m_pDcbaa,
                 m_pMem->VaToPa((PBYTE)m_pDcbaa),
                 CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
            m_pDcbaa = NULL;
        }

        if (m_pXhcdRing != NULL) {
            delete m_pXhcdRing;
            m_pXhcdRing = NULL;
        }
    }

    m_fUsbInterruptThreadClosing = FALSE;
}


//----------------------------------------------------------------------------------
// Function: RunController
//
// Description: 
//
// Parameters: None
//
// Returns: 0
//----------------------------------------------------------------------------------
INT CHW::RunController() const
{
    UINT uTemp;  

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMOD));
    uTemp &= ~INTERRUPT_MASK;
    uTemp |=(UINT)160;
    WRITE_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMOD), uTemp);

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uTemp |=(COMMAND_EIE);
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uTemp);

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));
    WRITE_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN),
                            INTERRUPT_ENABLE(uTemp));

    return 0;
}

//----------------------------------------------------------------------------------
// Function: EnterOperationalState
//
// Description: Signal the host controller to start processing the schedule
//              This function is only called from the CXhcd::Initialize routine.
//              It assumes that CPipe::Initialize and CHW::Initialize
//              have already been called.
//              This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::EnterOperationalState(VOID)
{
    UINT uTemp;
    INT iRet;
    UINT uPortReg;
    UINT uPortId;
    BOOL bPortChanged;
    UINT uStatus;

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uTemp |=(COMMAND_RUN);
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uTemp);

    iRet = Handshake((UINT)(m_opBase + USB_XHCD_USBSTS),
                    STATUS_HALT,
                    0,
                    MAX_HALT);

    if(iRet == -XHCD_TIMEDOUT)
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("Host controller takes too long to start.\n")));
        HaltController();
        return;
    }

    if((uStatus = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS))) &
        STATUS_HALT)
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("Host controller is halted.\n")));
        return;
    }

    bPortChanged = FALSE;

    for(uPortId = 0;  uPortId < MAX_PORT_NUMBER; uPortId++)
    {
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * uPortId));

        if((uPortReg & WARM_RESET_CHANGE) != 0)
        {
            uTemp = SetPortStateToNeutral(uPortReg);    

            WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                BYTES_IN_DWORD * PORT_REGISTER_NUMBER * uPortId),
                uTemp | WARM_RESET_CHANGE);
        }

        if(0 != (uPortReg & PORT_CONNECTION_STATUS))
        {
            bPortChanged = TRUE;
        }
    }

    if(bPortChanged)
    {
        SetEvent(m_pXhcdEventHandler->m_hUsbHubChangeEvent);
    }

    DEBUGMSG(ZONE_INIT,
        (TEXT("%s: -CHW::EnterOperationalState\n"),
        GetControllerName()));
}

//----------------------------------------------------------------------------------
// Function: StopHostController
//
// Description: Signal the host controller to stop processing the schedule
//              This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
// ******************************************************************
VOID CHW::StopHostController(VOID)
{
    Lock();
    HaltController();
    Unlock();
}

//----------------------------------------------------------------------------------
// Function: AddToBusyPipeList
//
// Description: Add the pipe indicated by pPipe to our list of busy pipes.
//              This allows us to check for completed transfers after 
//              getting an interrupt, and being signaled via 
//              SignalCheckForDoneTransfers
//
// Parameters: pPipe - pipe to add to busy list
//
//             fHighPriority - if TRUE, add pipe to start of busy list,
//                             else add pipe to end of list.
//
// Returns: TRUE if pPipe successfully added to list, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::AddToBusyPipeList(IN CPipe * const pPipe, IN const BOOL fHighPriority)
{
    if(m_cBusyPipeList.AddToBusyPipeList(pPipe,fHighPriority))
    {
        USB_ENDPOINT_DESCRIPTOR eptDescr = pPipe->GetEndptDescriptor();
        eptDescr.bmAttributes &= USB_ENDPOINT_TYPE_MASK;

        if(eptDescr.bmAttributes==USB_ENDPOINT_TYPE_ISOCHRONOUS ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT)
        {
            m_dwBusyIsochPipes++;
        }
        else if(eptDescr.bmAttributes==USB_ENDPOINT_TYPE_BULK ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_CONTROL)
        {
            m_dwBusyAsyncPipes++;
        }

        return TRUE;
    }

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: AddEndpoint
//
// Description: Add endpoint to the input control context,
//              initialize endpoint ring and context 
//
// Parameters: bSlotId - slot ID
//
//             peptDescr - endpoint descriptor
//
// Returns: TRUE if endpoint successfully added, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::AddEndpoint(UCHAR bSlotId, USB_ENDPOINT_DESCRIPTOR* peptDescr) const 
{
    return(m_pXhcdRing->AddEndpoint(bSlotId, peptDescr) == XHCD_OK);
}

//----------------------------------------------------------------------------------
// Function: GetCurrentEventSeg
//
// Description: Check if event dma addres is correct and return 
//              corresponding  transfer data
//
// Parameters: bSlotId - device slot id
//
//             bEpIndex - endpoint index 
//
//             pEndpointRingSegment - endpoint transfer ring segment
//
//             pDequeue - dequeue address of event ring
//
//             ppTd - transfer data to be returned
//
//             u64EventDma - dma address of checked event
//
// Returns: TRUE if address found in segment else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::GetCurrentEventSeg(UCHAR bSlotId,
                             UCHAR bEpIndex,
                             RING_SEGMENT* pEndpointRingSegment,
                             TRB* pDequeue,
                             TRANSFER_DATA** ppTd,
                             UINT64 u64EventDma)
{
    CQueuedPipe* pPipe;
    UCHAR    bAddress;

    if((bSlotId == 0) || (bEpIndex >= MAX_EP_NUMBER))
    {
        return FALSE;
    }

    bAddress = m_pXhcdRing->GetEndpointAddressByIndex(bEpIndex);

    pPipe =(CQueuedPipe*) m_cBusyPipeList.GetPipe(bSlotId, bAddress);

    if(pPipe == NULL)
    {
        return NULL;
    }

    return pPipe->GetCurrentEventSeg(pEndpointRingSegment, pDequeue, ppTd, u64EventDma);
}

//----------------------------------------------------------------------------------
// Function: RemoveFromBusyPipeList
//
// Description: Remove  pipevrom busy pipe list
//
// Parameters: pPipe - pipe to be removed
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::RemoveFromBusyPipeList(IN CPipe * const pPipe)
{ 
    if(m_cBusyPipeList.RemoveFromBusyPipeList(pPipe))
    {
        USB_ENDPOINT_DESCRIPTOR eptDescr = pPipe->GetEndptDescriptor();
        eptDescr.bmAttributes &= USB_ENDPOINT_TYPE_MASK;

        if(eptDescr.bmAttributes==USB_ENDPOINT_TYPE_ISOCHRONOUS ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT)
        {
            if(m_dwBusyIsochPipes)
            {
                m_dwBusyIsochPipes--;
            }
        }
        else if(eptDescr.bmAttributes==USB_ENDPOINT_TYPE_BULK ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_CONTROL)
        {
            if(m_dwBusyAsyncPipes)
            {
                m_dwBusyAsyncPipes--;
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Function: UsbInterruptThreadStub
//
// Description:  stub to start interrupt thread
//
// Parameters: pContext - pointer to pointer to CXhcd object, which is the main entry
//
// Returns: 0 
//----------------------------------------------------------------------------------
DWORD CHW::UsbInterruptThreadStub(IN PVOID pContext)
{
    return((CHW *)pContext)->UsbInterruptThread();
}

//----------------------------------------------------------------------------------
// Function: UsbInterruptThread
//
// Description: Main IST to handle interrupts from the USB host controller
//              This function is private
//
// Parameters: pContext - parameter passed in when starting thread,
//                      (currently unused)
//
// Returns: 0 on thread exit.
//----------------------------------------------------------------------------------
DWORD CHW::UsbInterruptThread()
{
    UINT uTemp;
    UINT uTemp2;
    UINT64 u64Temp;

    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
        (TEXT("%s: +CHW::Entered USBInterruptThread\n"),
        GetControllerName()));

    while(!m_fUsbInterruptThreadClosing)
    {
        WaitForSingleObject(m_hUsbInterruptEvent, INFINITE);

        if(m_fUsbInterruptThreadClosing)
        {
            break;
        }

        uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS));
        uTemp2 = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));

        if(uTemp == BAD_DEVICE_STATUS && uTemp2 == BAD_DEVICE_STATUS)
        {
            InterruptDone(m_dwSysIntr);
            return 0;
        }

        if(!(uTemp & STATUS_INTERRUPT) && !INTERRUPT_PENDING(uTemp2))
        {
            InterruptDone(m_dwSysIntr);
            return 0;
        }

        if(uTemp & STATUS_FATAL)
        {
            HaltController();
            InterruptDone(m_dwSysIntr);
            return 0;
        }

        uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS));
        uTemp |= STATUS_INTERRUPT;
        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS), uTemp);

        uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));
        uTemp |= 0x3;
        WRITE_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN), uTemp);

        uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));

        m_pXhcdEventHandler->HandleEvent();

        u64Temp = XhciRead64((UINT)(m_irSetBase + USB_XHCD_ERDP));

        XhciWrite64(u64Temp | HIGH_BUSY_BYTE,(UINT)(m_irSetBase + USB_XHCD_ERDP));

        InterruptDone(m_dwSysIntr);
    }

    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE,
        (TEXT("%s: -CHW::Leaving USBInterruptThread\n"),
        GetControllerName()));

    return(0);
}

//----------------------------------------------------------------------------------
// Function: UpdateFrameCounter
//
// Description:  update frame counter
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::UpdateFrameCounter(VOID)
{
    EnterCriticalSection(&m_csFrameCounter);

    // Read the frame register contents and copy it to
    // the OUT parameter.
    // Note : This copies only 14 bits

    DWORD dwCurrentFRNUM =
        READ_REGISTER_ULONG((PULONG)(m_runBase + USB_XHCD_MFINDEX)) &
        USB_XHCD_MFINDEX_FRAME_INDEX_MASK;

    DWORD dwCarryBit = m_dwFrameListMask + 1;

    // Overflow
    if((dwCurrentFRNUM & dwCarryBit) !=(m_dwFrameCounterHighPart & dwCarryBit))
    {
        m_dwFrameCounterHighPart += dwCarryBit;
    }

    m_dwFrameCounterLowPart = dwCurrentFRNUM;

    LeaveCriticalSection(&m_csFrameCounter);
}

//----------------------------------------------------------------------------------
// Function: GetFrameNumber
//
// Description: Return the current frame number
//              See also comment in UpdateFrameCounter
//
// Parameters: lpdwFrameNumber - pointer to the frame number
//
// Returns: 32 bit current frame number
//----------------------------------------------------------------------------------
BOOL CHW::GetFrameNumber(OUT LPDWORD lpdwFrameNumber)
{
    EnterCriticalSection(&m_csFrameCounter);

    // This algorithm is right out of the Win98 uhcd.c code
    UpdateFrameCounter();
    DWORD dwFrame = m_dwFrameCounterHighPart +(m_dwFrameCounterLowPart & m_dwFrameListMask);
        
    LeaveCriticalSection(&m_csFrameCounter);

    *lpdwFrameNumber = dwFrame;
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: GetFrameLength
//
// Description: get frame length
//
// Parameters: lpuFrameLength - pointer to the frame lenght number
//
// Returns: TRUE
//----------------------------------------------------------------------------------
BOOL CHW::GetFrameLength(OUT LPUSHORT lpuFrameLength)
{
    *lpuFrameLength = 60000;
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: SetFrameLength
//
// Description: set frame length
//
// Parameters:  noyt used
//
// Returns: FALSE
//----------------------------------------------------------------------------------
BOOL CHW::SetFrameLength(IN HANDLE , IN USHORT )
{
    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: StopAdjustingFrame
//
// Description: Stop modifying the host controller frame length
//
// Parameters: None
//
// Returns: FALSE
//----------------------------------------------------------------------------------
BOOL CHW::StopAdjustingFrame(VOID)
{
    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: WaitForCompletion
//
// Description: wai until the command is completed
//
// Parameters: pComplete - pointer to the completion flag structure 
//
//             uTimeout - wait timeout
//
// Returns: waited time if command is completed, 0 - if timeout is expired
//----------------------------------------------------------------------------------
UINT CHW::WaitForCompletion(COMPLETION* pComplete, UINT uTimeout) const
{
    UINT i;

    i = uTimeout;

    while((pComplete->iStatus != 1) &&(i > 0))
    {
        i--;
        Sleep(1);
    }
    
    return i;
}

//----------------------------------------------------------------------------------
// Function: EnableSlot
//
// Description: Query a USB device for slot id. Caller is responsible for buffer
//              alloc/delete
//
// Parameters: pbSlotId - pointer to slot ID
//
// Returns: device address if slot is enabled, else FALSE
//----------------------------------------------------------------------------------
PVOID CHW::EnableSlot(UCHAR* pbSlotId)
{
    INT iRet;

    iRet = m_pXhcdRing->IssueSlotControl(TRANSFER_BLOCK_ENABLE_SLOT, 0);

    if(iRet)
    {
        return NULL;
    }

    m_pXhcdRing->InitCompletion(&m_addrDev);
    RingCommandDoorbell();

    if(WaitForCompletion(&m_addrDev, USB_CTRL_SET_TIMEOUT) <= 0)
    {
        m_pXhcdRing->MoveDequeue(m_pXhcdRing->m_pCmdRing, FALSE);
        return NULL;
    }

    if(!m_bSlotId) {
        return NULL;
    }

    if(AllocateLogicalDevice(m_bSlotId) != XHCD_OK)
    {
        if(!m_pXhcdRing->IssueSlotControl(TRANSFER_BLOCK_DISABLE_SLOT, *pbSlotId))
        {
            RingCommandDoorbell();
        }

        return NULL;
    }

    *pbSlotId = m_bSlotId;

    if (m_bSlotId < MAX_SLOTS_NUMBER) 
    {
        return (PVOID) m_pLogicalDevices [m_bSlotId];
    } 
    else
    {
        return NULL;
    }
} 

//----------------------------------------------------------------------------------
// Function: AllocateLogicalDevice
//
// Description: allocate data for logical device
//
// Parameters: bSlotId - device slot ID
//
// Returns: XHCD_OK if data are allocated successfully else < 0
//----------------------------------------------------------------------------------
INT CHW::AllocateLogicalDevice(UCHAR bSlotId)
{
    LOGICAL_DEVICE *pDev;
    INT i;

    if(bSlotId == 0 || m_pLogicalDevices [bSlotId])
    {
        return -XHCD_FAIL;
    }

    m_pLogicalDevices [bSlotId] =
        (LOGICAL_DEVICE*)malloc(sizeof(LOGICAL_DEVICE));
    if(!m_pLogicalDevices[bSlotId])
    {
        return -XHCD_FAIL;
    }

    pDev = m_pLogicalDevices[bSlotId];

    pDev->pOutContext = m_pXhcdRing->AllocateContainerContext(TYPE_DEVICE);
    if(!pDev->pOutContext)
    {
        goto cleanUp;
    }

    pDev->pInContext = m_pXhcdRing->AllocateContainerContext(TYPE_INPUT);
    if(!pDev->pInContext)
    {
        goto cleanUp;
    }

    for(i = 0; i < MAX_EP_NUMBER; i++)
    {
        pDev->eps[i].uEpState = 0;
        pDev->eps[i].pStoppedTd = NULL;
        pDev->eps[i].pRing = NULL;
        pDev->eps[i].pNewRing = NULL;
        pDev->eps[i].bAbortCounter = 0;
    }

    pDev->eps[0].pRing = m_pXhcdRing->AllocateRing(TRUE);
    if(!pDev->eps[0].pRing)
    {
        goto cleanUp;
    }

    pDev->bDeviceSpeed = 0;
    
    m_pXhcdRing->InitCompletion(&pDev->cmdCompletion);
    InitListHead(&pDev->cmdList);

    m_pDcbaa->u64DevContextPtrs[bSlotId] = (UINT64)pDev->pOutContext->u64Dma;

    return XHCD_OK;
cleanUp:
    m_pXhcdRing->FreeLogicalDevice(bSlotId);
    return -XHCD_FAIL;
}

//----------------------------------------------------------------------------------
// Function: ChangeMaxPacketSize
//
// Description: Send an Evaluate Context command to the controller to update Context. 
//
// Parameters: bSlotId - slot ID
//
//             bMaxPktSize - device Max Packet Size identified from GET_DESCRIPTOR.
//
// Returns: XHCD_OK if command passed correctly, else < 0
//----------------------------------------------------------------------------------
 INT CHW::ChangeMaxPacketSize(UCHAR bSlotId, UINT bMaxPktSize)
{
    INT iRet = 0;
    LOGICAL_DEVICE *pVirtDev = NULL;
    ENDPOINT_CONTEXT_DATA *pEp0Context;
    INPUT_CONTROL_CONTEXT *pCtrlContext;
    COMPLETION *pCmdCompletion;

    if (bSlotId != 0)
    {
        pVirtDev = m_pLogicalDevices [bSlotId];
    }

    if(!pVirtDev)
    {
        return -XHCD_INVALID;
    }
    pCmdCompletion = &pVirtDev->cmdCompletion;
    pCtrlContext = m_pXhcdRing->GetInputControlContext(pVirtDev->pInContext);
    if(pCtrlContext == NULL)
    {
        return -XHCD_INVALID;
    }
    pEp0Context = GetEndpointContext(pVirtDev->pInContext, 0);
    pEp0Context->dwMaxPacketSize = bMaxPktSize;
    pCtrlContext->uAddFlags = ENDPOINT_ZERO;
    iRet = m_pXhcdRing->IssueEvaluateContext(pVirtDev->pInContext->u64Dma, bSlotId);
    if(iRet)
    {
        return iRet;
    }

    m_pXhcdRing->InitCompletion(pCmdCompletion);
    RingCommandDoorbell();
    
    if(WaitForCompletion(pCmdCompletion, USB_CTRL_SET_TIMEOUT) <= 0)
    {
        m_pXhcdRing->MoveDequeue(m_pXhcdRing->m_pCmdRing, FALSE);
        return -XHCD_TIMEDOUT;
    }

    switch(pVirtDev->uCmdStatus)
    {
    case STATUS_CONTEXT_STATE:
    case STATUS_BAD_SPLIT:
        iRet = -XHCD_INVALID;
        break;

    case STATUS_TRANSFER_ERROR:
        iRet = -XHCD_TRANSFER_ERROR;
        break;

    case STATUS_DB_SUCCESS:
        break;

    case STATUS_INVALID:
        iRet = -XHCD_INVALID;
        break;

    default:
        iRet = -XHCD_INVALID;
        break;
    }

    if(iRet)
    {
        return iRet;
    }
    pCtrlContext->uAddFlags = 0;

    return XHCD_OK;
}
//----------------------------------------------------------------------------------
// Function: AddressDevice
//
// Description: Send an Address Device command to the controller. 
//
// Parameters: bSlotId - slot ID
//
//             uPortId - port ID
//
//             bSpeed - device speed
//
// Returns: XHCD_OK if command passed correctly, else < 0
//----------------------------------------------------------------------------------
INT CHW::AddressDevice(UCHAR bSlotId,
                       UINT uPortId,
                       UCHAR bSpeed, UCHAR bRootHubPort, UCHAR bHubSID, UINT uDevRoute) 
{
    LOGICAL_DEVICE *pVirtDev = NULL;
    INT iRet = 0;
    INPUT_CONTROL_CONTEXT *pCtrlContext;

    if (bSlotId != 0)
    {
        pVirtDev = m_pLogicalDevices [bSlotId];
    }

    if(!pVirtDev)
    {
        return -XHCD_INVALID;
    }

    iRet = m_pXhcdRing->InitializeLogicalDevice(bSlotId, bSpeed, uPortId, bRootHubPort, bHubSID, uDevRoute);
    if(iRet)
    {
        return iRet;
    }
        
    iRet = m_pXhcdRing->IssueAddressDevice(pVirtDev->pInContext->u64Dma, bSlotId);
    if(iRet)
    {
        return iRet;
    }

    m_pXhcdRing->InitCompletion(&m_addrDev);
    RingCommandDoorbell();
    
    if(WaitForCompletion(&m_addrDev, USB_CTRL_SET_TIMEOUT) <= 0)
    {
        m_pXhcdRing->MoveDequeue(m_pXhcdRing->m_pCmdRing, FALSE);
        return -XHCD_TIMEDOUT;
    }

    switch(pVirtDev->uCmdStatus)
    {
    case STATUS_CONTEXT_STATE:
    case STATUS_BAD_SPLIT:
        iRet = -XHCD_INVALID;
        break;

    case STATUS_TRANSFER_ERROR:
        iRet = -XHCD_TRANSFER_ERROR;
        break;

    case STATUS_DB_SUCCESS:
        break;

    case STATUS_INVALID:
        iRet = -XHCD_INVALID;
        break;

    default:
        iRet = -XHCD_INVALID;
        break;
    }

    if(iRet)
    {
        return iRet;
    }

    pCtrlContext = m_pXhcdRing->GetInputControlContext(pVirtDev->pInContext);
    if (pCtrlContext == NULL)
    {
        return -XHCD_INVALID;
    }
    pCtrlContext->uAddFlags = 0;
    pCtrlContext->uDropFlags = 0;

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: AllocateCommand
//
// Description: allocate data for command structure
//
// Parameters: fAllocateCompletion - initialize command completion status
//
// Returns: pointer to the command data
//----------------------------------------------------------------------------------
COMMAND* CHW::AllocateCommand(BOOL fAllocateCompletion) const
{
    COMMAND *pCommand;

    pCommand = (COMMAND *) malloc(sizeof(*pCommand));
    if(!pCommand)
    {
        return NULL;
    }

    if(fAllocateCompletion)
    {
        m_pXhcdRing->InitCompletion(&pCommand->completion);
    }

    pCommand->uStatus = 0;

    pCommand->cmdList.pStruct = NULL;
    pCommand->cmdList.pLinkFwd = NULL;
    pCommand->cmdList.pLinkBack = NULL;

    return pCommand;
}

//----------------------------------------------------------------------------------
// Function: ResetDevice
//
// Description: Send a Reset Device command to the controller.
//
// Parameters: bSlotId - device slot id
//
// Returns: XHCD_OK if command passed correctly, < 0 - elsewhere
//----------------------------------------------------------------------------------
INT CHW::ResetDevice(UCHAR bSlotId) const
{
    INT iRet, i;
    LOGICAL_DEVICE *pVirtDev;
    COMMAND *pResetDeviceCmd;
    SLOT_CONTEXT_DATA *pSlotContext;


    pVirtDev = m_pLogicalDevices[bSlotId];
    if(!pVirtDev)
    {
        return -XHCD_INVALID;
    }

    pSlotContext = m_pXhcdRing->GetSlotContext(pVirtDev->pOutContext);
    if(pSlotContext == NULL ||
       pSlotContext->dwSlotState == DISABLED_SLOT)
    {
        return -XHCD_FAIL;
    }

    pResetDeviceCmd = AllocateCommand(TRUE);
    if(!pResetDeviceCmd)
    {
        return -XHCD_NOMEMORY;
    }

    pResetDeviceCmd->pCommandTrb = m_pXhcdRing->m_pCmdRing->pEnqueue;

    if((pResetDeviceCmd->pCommandTrb->linkTrb.uControl & TRANSFER_BLOCK_TYPE_BITMASK)
            == TRANSFER_BLOCK_TYPE(TRANSFER_BLOCK_LINK))
    {
        pResetDeviceCmd->pCommandTrb =
            m_pXhcdRing->m_pCmdRing->pRingSegment->pTrbs;
    }

    UsbListLink(&pVirtDev->cmdList,
        (VOID*)pResetDeviceCmd,
        &pResetDeviceCmd->cmdList);

    iRet = m_pXhcdRing->IssueResetDevice(bSlotId);
    if(iRet)
    {
        UsbListUnlink(&pVirtDev->cmdList, &pResetDeviceCmd->cmdList);
        goto cleanUp;
    }

    RingCommandDoorbell();

    if(WaitForCompletion(&pResetDeviceCmd->completion,
                                USB_CTRL_SET_TIMEOUT) <= 0)
    {
        m_pXhcdRing->MoveDequeue(m_pXhcdRing->m_pCmdRing, FALSE);
        UsbListUnlink(&pVirtDev->cmdList, &pResetDeviceCmd->cmdList);
        iRet = -XHCD_TIMEDOUT;
        goto cleanUp;
    }

    iRet = pResetDeviceCmd->uStatus;
    switch(iRet)
    {
    case STATUS_BAD_SPLIT:
    case STATUS_CONTEXT_STATE:
        iRet = 0;
        goto cleanUp;
    case STATUS_DB_SUCCESS:
        break;
    default:
        iRet = -XHCD_INVALID;
        goto cleanUp;
    }

    for(i = 1; i < MAX_EP_NUMBER; i++)
    {
        if(!pVirtDev->eps[i].pRing)
        {
            continue;
        }
        m_pXhcdRing->FreeEndpointRing(pVirtDev, i);
    }

    iRet = 0;

cleanUp:
    m_pXhcdRing->FreeCommand(pResetDeviceCmd);
    return iRet;
}

//----------------------------------------------------------------------------------
// Function: GetPortStatus
//
// Description: This function will return the current root hub port
//          status in a non-hardware specific format
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
//             rStatus - reference to USB_HUB_AND_PORT_STATUS to get the
//                       status
//
// Returns: TRUE
//----------------------------------------------------------------------------------
BOOL CHW::GetPortStatus(IN const UCHAR bPort,
                         OUT USB_HUB_AND_PORT_STATUS& rStatus) const
{
    memset(&rStatus, 0, sizeof(USB_HUB_AND_PORT_STATUS));
    if (!m_fInitialized) 
    {
        rStatus.change.port.usConnectStatusChange = m_ucPortChange [bPort - 1];
    }
    else if(bPort > 0)
    {
        // request refers to a root hub port
        UINT uPortReg = 0;
        UINT uPortSpeed = 0;
        
        // Read the PORTSC contents 
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

        if(uPortReg & STATE_POWER)
        { 
            // Now fill in the USB_HUB_AND_PORT_STATUS structure
            //portSC.bit.usConnectStatusChange;
            rStatus.change.port30.usConnectStatusChange =
                ((uPortReg & CONNECTION_STATUS_CHANGE) != 0);
            //portSC.bit.usPortEnableChange;
            //rStatus.change.port30.usPortEnableChange =
            //    ((uPortReg & PORT_ENABLE_CHANGE) != 0);
            //portSC.bit.usOverCurrentChange;
            rStatus.change.port30.usOverCurrentChange =
                ((uPortReg & OVER_CURRENT_CHANGE) != 0);
            rStatus.change.port30.usResetChange =
                ((uPortReg & RESET_CHANGE) != 0);
            rStatus.change.port30.usWarmResetChange =
                ((uPortReg & WARM_RESET_CHANGE) != 0);
            rStatus.change.port30.usPortLinkStateChange =
                ((uPortReg & RESET_CLEAR) != 0);
            rStatus.change.port30.usPortConfigError =
                ((uPortReg & PORT_CONFIG_ERROR_CHANGE) != 0);
            // for root hub, we don't set any of these change bits:
            DEBUGCHK(rStatus.change.port.usSuspendChange == 0);

            uPortSpeed = DEVICE_SPEED(uPortReg);

            rStatus.status.port30.usDeviceSpeed = uPortSpeed;

            //portSC.bit.ConnectStatus;
            rStatus.status.port30.usPortConnected =
                ((uPortReg & PORT_CONNECTION_STATUS) != 0);
            //portSC.bit.Enabled;
            rStatus.status.port30.usPortEnabled =
                ((uPortReg & STATUS_PE) != 0);
            //portSC.bit.OverCurrentActive ;
            rStatus.status.port30.usPortOverCurrent =
                ((uPortReg & STATUS_OC) != 0);
            // root hub ports are always powered
            rStatus.status.port30.usPortPower = 1;
            //portSC.bit.Reset;
            rStatus.status.port30.usPortReset =
                ((uPortReg & STATUS_RESET) != 0);
            //portSC.bit.Suspend
            rStatus.status.port30.usPortLinkState =
                ((uPortReg & STATE_MASK) >> STATE_OFFSET);
            rStatus.status.port.usPortSuspended =
                ((uPortReg & STATE_MASK) == DEVICE_U3);
        }
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: CheckPortStatus
//
// Description: This function chack port status of all ports and reset
//              disconnennected port if they are not in the dosconnected states
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::CheckPortStatus() const
{
    // request refers to a root hub port
    UINT uPortReg = 0;
    UCHAR bPort;
    UINT uTemp;

    for (bPort = 0; bPort < MAX_PORT_NUMBER; bPort++) 
    {
      
        // Read the PORTSC contents 
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * bPort));

        if (!(uPortReg & STATUS_CONNECT) && (uPortReg & STATE_POWER) &&
            ((uPortReg & STATE_MASK) != DEVICE_RXDETECT)) 
        {
            uTemp = SetPortStateToNeutral(uPortReg);
            uTemp |= STATUS_RESET;
            WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                BYTES_IN_DWORD * PORT_REGISTER_NUMBER * bPort),
                uTemp);

            Sleep(PORT_RESET_TIMEOUT);

            if (m_bPortArray [bPort] == USB30_MAJOR_REVISION) {
                SetLinkState (bPort + 1, DEVICE_RXDETECT);
            }

            uPortReg =
                 READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                BYTES_IN_DWORD * PORT_REGISTER_NUMBER * bPort));

        }
    }
}


//----------------------------------------------------------------------------------
// Function: UsbXhcdClearPortChangeBits
//
// Description: This routine clears all port change bits
//
// Parameters: wPortId - port number
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::UsbXhcdClearPortChangeBits(USHORT wPortId) const
{
    UINT uPortReg = 0;
    UINT uTemp;

    uPortReg =
        READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
        BYTES_IN_DWORD * PORT_REGISTER_NUMBER * wPortId));

    if((uPortReg & RESET_CHANGE) != 0)
    {
        // Clear the port reset status change 
        uTemp = SetPortStateToNeutral(uPortReg);    
        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * wPortId),
            uTemp | RESET_CHANGE);
    }

    if((uPortReg & WARM_RESET_CHANGE) != 0)
    {
        // Clear the port warm reset status change 
        uTemp = SetPortStateToNeutral(uPortReg);    
        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * wPortId),
            uTemp | WARM_RESET_CHANGE);
    }

    if((uPortReg & RESET_CLEAR) != 0)
    {
        // Clear the port reset status change 
        uTemp = SetPortStateToNeutral(uPortReg);    
        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * wPortId),
            uTemp | RESET_CLEAR);
    }
    
    return;
}

//----------------------------------------------------------------------------------
// Function: RootHubFeature
//
// Description: This function clears all the status change bits associated with
//              the specified root hub port.
//              Assume that caller has already verified the parameters from a USB
//              perspective. The HC hardware may only support a subset of that
//              (which is indeed the case for XHCI).
//
// Parameters: bPort - 0 for the hub itself, otherwise the hub port number
//
//             bSetOrClearFeature - TRUE - set feature, FALSE - clear
//
//             wFeature - feature number
//
// Returns: TRUE if the requested operation is valid, FALSE otherwise.
//----------------------------------------------------------------------------------
BOOL CHW::RootHubFeature(IN const UCHAR bPort,
                          IN const UCHAR bSetOrClearFeature,
                          IN const USHORT wFeature)
{
    if(bPort == 0)
    {
        // request is to Hub but...
        // xhci has no way to tweak features for the root hub.
        return FALSE;
    }

    UINT uPortReg = 0;
    UINT uTemp = 0;

    // Read the PORTSC contents 
    uPortReg =
        READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
        BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));
    
    if(uPortReg & STATE_POWER)
    {
        uTemp = SetPortStateToNeutral(uPortReg);

        if(bSetOrClearFeature == USB_REQUEST_SET_FEATURE)
        {
            switch(wFeature)
            {
            case USB_HUB_FEATURE_PORT_RESET:
                uTemp = uTemp | STATUS_RESET;
                break;

            case USB_HUB_FEATURE_PORT_SUSPEND:
                break;

            case USB_HUB_FEATURE_PORT_POWER:
                uTemp |= STATE_POWER;
                break;

            default:
                return FALSE;
            }
        }
        else
        {
            switch(wFeature)
            {
            case USB_HUB_FEATURE_PORT_ENABLE:
                break;

            case USB_HUB_FEATURE_PORT_SUSPEND:
                break;

            case USB_HUB_FEATURE_C_PORT_CONNECTION: 
            if (m_fInitialized) {
                // USB 3.0 don't need reset
                if(m_bPortArray [bPort - 1] != USB30_MAJOR_REVISION)
                {
                    if(!(uPortReg & STATUS_CONNECT) ||
                        !(uPortReg & STATUS_PE) || 
                        (((uPortReg & STATE_MASK) != DEVICE_U0) &&
                        ((uPortReg & STATE_MASK) != DEVICE_U3)))
                    {
                        uTemp = SetPortStateToNeutral(uPortReg);
                        uTemp = uTemp | STATUS_RESET;
                        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                           BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)),
                           uTemp);

                        // The reset signal should be driven atleast for 50ms
                        // from the Root hub.
                        // Additional 10ms is given here for software delays.
                        Sleep(PORT_RESET_TIMEOUT);

                        // check if the port is reset; if not return failure and break
                        uPortReg =
                            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

                        if(uPortReg & STATUS_RESET)
                        {
                            DEBUGMSG(ZONE_ERROR,
                                (TEXT("SetPortFeature - Unable to reset Port\n")));
                            return FALSE;
                         }

                         uTemp = SetPortStateToNeutral(uPortReg);
                         uTemp = uTemp | RESET_CHANGE;
                    }
                }
                else
                {
                    UsbXhcdClearPortChangeBits(bPort - 1);
                }
                uTemp |= CONNECTION_STATUS_CHANGE;
            } else {
                m_ucPortChange [bPort - 1] = 0;
            }
            break;

            case USB_HUB_FEATURE_C_PORT_ENABLE:
                uTemp |= PORT_ENABLE_CHANGE;
                break;

            case USB_HUB_FEATURE_C_PORT_OVER_CURRENT:
                uTemp |= OVER_CURRENT_CHANGE;
                break;

            case USB_HUB_FEATURE_C_PORT_RESET:
                uTemp |= RESET_CHANGE;
                break;          

            case USB_HUB_FEATURE_C_BH_PORT_RESET:
                uTemp |= WARM_RESET_CHANGE;
                break;  
            case USB_HUB_FEATURE_C_PORT_LINK_STATE:
                uTemp |= RESET_CLEAR;
                break;
            case USB_HUB_FEATURE_C_PORT_SUSPEND:
            case USB_HUB_FEATURE_PORT_POWER:
            default:
                return FALSE;
            }
        }

        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)),
            uTemp);
        Sleep(PORT_RESET_TIMEOUT);
        uPortReg =  READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

//----------------------------------------------------------------------------------
// Function: ResetAndEnablePort
//
// Description: reset/enable device on the given port so that when this
//              function completes, the device is listening on address 0
//              This function takes approx 60 ms to complete, and assumes
//              that the caller is handling any critical section issues
//              so that two different ports(i.e. root hub or otherwise)
//              are not reset at the same time. 
//
// Parameters: bPort - root hub port # to reset/enable
//
// Returns: TRUE if port reset and enabled, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::ResetAndEnablePort(IN const UCHAR bPort) const
{
    UINT uPortReg = 0;
    UINT uTemp;

    // Read the PORTSC contents 
    uPortReg =
        READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
        BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

    if(uPortReg & STATE_POWER)
    {
        uTemp = SetPortStateToNeutral(uPortReg);
        uTemp = uTemp | STATUS_RESET;
        WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)),
            uTemp);

        // The reset signal should be driven atleast for 50ms
        // from the Root hub.
        // Additional 10ms is given here for software delays.
        Sleep(PORT_RESET_TIMEOUT);

        //check if the port is reset; if not return failure and break
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

        if(uPortReg & STATUS_RESET)
        {
            DEBUGMSG(ZONE_ERROR,
                (TEXT("ResetAndEnablePort - Unable to reset Port\n")));
            return FALSE;
        }

        DEBUGMSG(ZONE_INIT,
            (TEXT("%s: Root hub, after reset & enable, port %d portsc = 0x%04x\n"),
            GetControllerName(),
            bPort,
            uPortReg));
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: DisablePort
//
// Description: disable the given root hub port.
//              This function will take about 10ms to complete
//
// Parameters: bPort - port number to disable
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DisablePort(IN const UCHAR bPort) const
{
    UINT uPortReg = 0;
    UINT uTemp;

    if ((bPort > 0) && (bPort <= MAX_PORT_NUMBER)) 
    {
        // Read the PORTSC contents 
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

        if((uPortReg & STATE_POWER) &&
           ((uPortReg & STATE_MASK) != DEVICE_RXDETECT))
        {
            uTemp = SetPortStateToNeutral(uPortReg);
            uTemp |= STATUS_PE;
            WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)),
                uTemp);

            // disable port can take some time to act, because
            // a USB request may have been in progress on the port.
            Sleep(PORT_RESET_TIMEOUT);

            if (m_bPortArray [bPort - 1] == USB30_MAJOR_REVISION) {
                SetLinkState (bPort, DEVICE_RXDETECT);
            }

            // check if the port is disbled; if not return failure and break
             uPortReg =
                 READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
                 BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPort - 1)));

            if(uPortReg & STATUS_PE)
            {
            }
        }
    }

    CheckPortStatus();
}

//----------------------------------------------------------------------------------
// Function: WaitForPortStatusChange
//
// Description: Wait for one of the ports status change
//
// Parameters: m_hHubChanged - event for hub status change thread
//
// Returns: TRUE if port status changed, FALSE if event handle is closing.

//----------------------------------------------------------------------------------
BOOL CHW::WaitForPortStatusChange(HANDLE m_hHubChanged) const
{
    if (m_fUsbInterruptThreadClosing || (m_pXhcdEventHandler == NULL)) {
        return FALSE;
    }
    return m_pXhcdEventHandler->WaitForPortStatusChange(m_hHubChanged);
}

//----------------------------------------------------------------------------------
// Function: PowerMgmtCallback
//
// Description: System power handler - called when device goes into/out of
//              suspend. This needs to be implemented for HCDI
//
// Parameters:  fOff - if TRUE indicates that we're entering suspend,
//                     else signifies resume
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::PowerMgmtCallback(IN BOOL fOff)
{
    if(fOff)
    {
        if((GetCapability() & HCD_SUSPEND_RESUME) != 0)
        {
            m_fDoResume=TRUE;
            SuspendHostController();
        }
        else
        {
            m_fDoResume=FALSE;
            StopHostController();
        }
    }
    else
    {   // resuming...
        g_fPowerUpFlag = TRUE;
        if(m_fDoResume)
        {
            ResumeHostController();
        }
        if(!g_fPowerResuming)
        {
            // can't use member data while 'this' is invalid
            SetInterruptEvent(m_dwSysIntr);
        }
    }
    return;
}

//----------------------------------------------------------------------------------
// Function: SetLinkState
//
// Description: Set port link state. this is used to power manage the port.
//
// Parameters: bPortId - port number
//
//             uLinkState - link state to be set
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::SetLinkState(UCHAR bPortId, UINT uLinkState) const
{
    UINT uTemp;

    uTemp =
        READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
        BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPortId - 1)));
    uTemp = SetPortStateToNeutral(uTemp);
    uTemp &= ~STATE_MASK;
    uTemp |= LINK_STATE_STROBE | uLinkState;
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
        BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(bPortId - 1)),
        uTemp);
}

//----------------------------------------------------------------------------------
// Function: SuspendHostController
//
// Description: Suspend host controller.
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::SuspendHostController()
{
    UINT uPortReg = 0;
    UINT uTemp;

    // We can suspend each port.
    for(UINT uPort = 1; uPort <= m_bNumOfPort; uPort ++)
    {
        // Read the PORTSC contents 
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(uPort - 1)));

        if(uPortReg & STATE_POWER)
        {
            if((uPortReg & STATE_MASK) != DEVICE_U3)
            {
                // Resume the port to U3 state 
                SetLinkState(uPort, DEVICE_U3);
                Sleep(10);
            }
        }
    }
    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uTemp &= ~(COMMAND_RUN);
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uTemp);
}

//----------------------------------------------------------------------------------
// Function: ResumeHostController
//
// Description: Resume the suspended host controller
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::ResumeHostController()
{
    UINT uPortReg = 0;
    UINT uTemp;

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uTemp |=(COMMAND_RUN);
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uTemp);

     for(UCHAR uPort =1; uPort <= m_bNumOfPort; uPort ++)
     {
        // Read the PORTSC contents 
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(uPort - 1)));

        if(uPortReg & STATE_POWER)
        {
            if((uPortReg & STATE_MASK) != DEVICE_U0)
            {
                // Resume the port
                if (DEVICE_SUPERSPEED(uPortReg)) {
                    SetLinkState(uPort, DEVICE_U0);
                } else {
                    SetLinkState(uPort, DEVICE_RESUME);
                    Sleep(20);
                    SetLinkState(uPort, DEVICE_U0);
                }
                Sleep(10);
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Function: SetCapability
//
// Description: Set Capability.
//
// Parameters: dwCap - Bitmap for Capabliity to be set
//
// Returns: Bitmap for Capabliity.
//----------------------------------------------------------------------------------
DWORD CHW::SetCapability(DWORD dwCap)
{
    DEBUGMSG(ZONE_INIT,
        (TEXT("%s: CHW::SetCapability(%08x) currently=%08x\n"),
        GetControllerName(),
        dwCap,
        m_dwCapability));
    m_dwCapability |= dwCap; 

    if((m_dwCapability & HCD_SUSPEND_RESUME) != 0)
    {
        KernelIoControl(IOCTL_HAL_ENABLE_WAKE,
                        &m_dwSysIntr,
                        sizeof(m_dwSysIntr),
                        NULL,
                        0,
                        NULL);
    }

    return m_dwCapability;
};

//----------------------------------------------------------------------------------
// Function: ReadUSBHwInfo
//
// Description: Read HW information from registry.
//
// Parameters: none
//
// Returns: TRUE
//----------------------------------------------------------------------------------
BOOL CHW::ReadUSBHwInfo()
{
    //
    // If pipe cache value is not retrievable from Registry, default will be used.
    // If value from Registry is ZERO, pipe cache wil be disabled.
    // Too small or too big values will be clipped at min or max.
    //
    m_dwOpenedPipeCacheSize = DEFAULT_PIPE_CACHE_SIZE;
    if(m_deviceReg.IsKeyOpened())
    {
        if(!m_deviceReg.GetRegValue(XHCI_REG_DesiredPipeCacheSize,
                                    (LPBYTE)&m_dwOpenedPipeCacheSize,
                                    sizeof(m_dwOpenedPipeCacheSize)))
        {
            m_dwOpenedPipeCacheSize = DEFAULT_PIPE_CACHE_SIZE;
        }
    }
    if(m_dwOpenedPipeCacheSize)
    {
        if(m_dwOpenedPipeCacheSize < MINIMUM_PIPE_CACHE_SIZE)
        {
            m_dwOpenedPipeCacheSize = MINIMUM_PIPE_CACHE_SIZE;
            DEBUGMSG(ZONE_WARNING,
                (TEXT("%s: CHW::Initialize - \
                      Pipe Cache size less than minimum, adjusted to %u!\n"),
                      GetControllerName(),
                      MINIMUM_PIPE_CACHE_SIZE));
        }
        if(m_dwOpenedPipeCacheSize > MAXIMUM_PIPE_CACHE_SIZE)
        {
            m_dwOpenedPipeCacheSize = MAXIMUM_PIPE_CACHE_SIZE;
            DEBUGMSG(ZONE_WARNING,
                (TEXT("%s: CHW::Initialize - \
                      Pipe Cache size less than maximum, adjusted to %u!\n"),
                      GetControllerName(),
                      MAXIMUM_PIPE_CACHE_SIZE));
        }
    }
    else
    {
        DEBUGMSG(ZONE_WARNING,
            (TEXT("%s: CHW::Initialize - \
                  Pipe Cache size set to ZERO, pipe cache will not be used!\n"),
                  GetControllerName()));
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: ResetHostController
//
// Description: Reset host controller.
//
// Parameters: none
//
// Returns: XHCD_OK if reset successfull, < 0 otherwise.
//----------------------------------------------------------------------------------
INT CHW::ResetHostController() const
{
    UINT uCommand;
    UINT uState;
    INT iRet;

    uState = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS));
    if((uState & STATUS_HALT) == 0)
    {
        return -XHCD_FAIL;
    }

    uCommand = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    uCommand |= COMMAND_RESET;
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD), uCommand);

    iRet = Handshake((UINT)(m_opBase + USB_XHCD_USBCMD),
                    COMMAND_RESET,
                    0,
                    MAX_RESET);

    if(iRet == -XHCD_TIMEDOUT)
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("Host took too long to reset.\n")));
        return iRet;
    }

    return Handshake((UINT)(m_opBase + USB_XHCD_USBSTS),
                    STATUS_CNR,
                    0,
                    MAX_RESET);
}

//----------------------------------------------------------------------------------
// Function: XhciRead64
//
// Description: Read 64 bit register.
//
// Parameters: uRegs - register address
//
// Returns: Register 64-bit value.
//----------------------------------------------------------------------------------
UINT64 CHW::XhciRead64(UINT uRegs) const
{
    UINT64 u64ValLo = READ_REGISTER_ULONG((PULONG) uRegs);
    UINT64 u64ValHi = READ_REGISTER_ULONG((PULONG)(uRegs + 4));
    return u64ValLo +(u64ValHi << BITS_IN_DWORD);
}

//----------------------------------------------------------------------------------
// Function: XhciWrite64
//
// Description: Write 64 bit register.
//
// Parameters: u64Val - Register 64-bit value. 
//
//             uRegs - register address
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::XhciWrite64(const UINT64 u64Val, UINT uRegs) const
{
    UINT u64ValLo = u64Val & 0xFFFFFFFF;
    UINT u64ValHi =(u64Val & 0xFFFFFFFF00000000LL) >> BITS_IN_DWORD;

    WRITE_REGISTER_ULONG((PULONG) uRegs, u64ValLo);
    WRITE_REGISTER_ULONG((PULONG)(uRegs + 4), u64ValHi);
}

//----------------------------------------------------------------------------------
// Function: SetHostControllerEventDequeue
//
// Description: Set event ring dequeue pointer to pEventRing->pDequeue position
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::SetHostControllerEventDequeue() const
{
    UINT64 uTemp;
    UINT64 u64Deq;
    RING* pEventRing = m_pXhcdRing->m_pEventRing;

    u64Deq = m_pXhcdRing->TransferVirtualToPhysical(pEventRing->pRingSegment,
                                        pEventRing->pDequeue);

    if (u64Deq != NULL) 
    {
        uTemp = XhciRead64((UINT)(m_irSetBase + USB_XHCD_ERDP));
        uTemp &= MASK_POINTER;

        uTemp &= ~HIGH_BUSY_BYTE;

        XhciWrite64((u64Deq &(UINT64) ~MASK_POINTER) | uTemp,
                (UINT)(m_irSetBase + USB_XHCD_ERDP));
    }
}

//----------------------------------------------------------------------------------
// Function: ScratchpadAlloc
//
// Description:  Allocate memory for scratchpad.
//
// Parameters: none
//
// Returns: XHCD_OK if memory allocated successfully, else < 0
//----------------------------------------------------------------------------------
INT CHW::ScratchpadAlloc() 
{
    INT i;
    INT iNumSp = MAX_SCRATCHPAD_NUMBER(m_uHcsParams2);

    if(!iNumSp)
    {
        return 0;
    }

    m_pScratchpad =
        (SCRATCHPAD*) malloc(sizeof(SCRATCHPAD));
    if(!m_pScratchpad)
    {
        goto cleanUp;
    }

    if(!m_pMem->AllocateMemory(DEBUG_PARAM(TEXT("Sp array"))(iNumSp * sizeof(UINT64)),
                                (PUCHAR*)&m_pScratchpad->pu64SpArray,
                                CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK,
                                m_iPageSize))
    {
        goto cleanUp;
    }

    m_pScratchpad->u64SpDma =
        (UINT64)(m_pMem->VaToPa((PUCHAR) m_pScratchpad->pu64SpArray));
    if(!m_pScratchpad->u64SpDma)
    {
        goto cleanUp2;
    }

    m_pScratchpad->ppSpBuffers =(VOID**)malloc(iNumSp * sizeof(VOID *));
    if(!m_pScratchpad->ppSpBuffers)
    {
        goto cleanUp3;
    }
    memset (m_pScratchpad->ppSpBuffers, 0, iNumSp * sizeof(VOID *));

    m_pDcbaa->u64DevContextPtrs[0] = m_pScratchpad->u64SpDma;
    for(i = 0; i < iNumSp; i++)
    {
        VOID *pBuf = NULL;
        UINT uDma = 0;
        
        if (m_iPageSize != 0) {
            pBuf = AllocPhysMem(m_iPageSize,
                            PAGE_READWRITE | PAGE_NOCACHE,
                            m_iPageSize,
                            0,
                            (PULONG) &uDma);
        }
        if(pBuf == NULL)
        {
            goto cleanUp4;
        }

        m_pScratchpad->pu64SpArray[i] = uDma;
        m_pScratchpad->ppSpBuffers[i] = pBuf;
    }

    return XHCD_OK;

 cleanUp4:
    iNumSp--; 
    for(i = iNumSp; i >= 0; i--) 
    {
        FreePhysMem(m_pScratchpad->ppSpBuffers[i]);
    }
    free(m_pScratchpad->ppSpBuffers);

 cleanUp3:
    m_pMem->FreeMemory((PBYTE) m_pScratchpad->pu64SpArray,
                        m_pMem->VaToPa((PBYTE)m_pScratchpad->pu64SpArray),
                        CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);

 cleanUp2:
     free(m_pScratchpad);
     m_pScratchpad = NULL;

 cleanUp:
    return -XHCD_NOMEMORY;
}

//----------------------------------------------------------------------------------
// Function: ScratchpadFree
//
// Description:  Free memory allocated for the scratchpad.
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
void CHW::ScratchpadFree () 
{
    INT i;
    INT iNumSp = MAX_SCRATCHPAD_NUMBER(m_uHcsParams2);

    if(!iNumSp)
    {
        return;
    }

    if (m_pScratchpad)
    {
        if (m_pScratchpad->ppSpBuffers)
        {
            iNumSp--;
            for(i = iNumSp; i >= 0; i--)
            {
                if (m_pScratchpad->ppSpBuffers[i])
                {
                    FreePhysMem(m_pScratchpad->ppSpBuffers[i]);
                }
            }
            free(m_pScratchpad->ppSpBuffers);
            m_pMem->FreeMemory((PBYTE) m_pScratchpad->pu64SpArray,
                        m_pMem->VaToPa((PBYTE)m_pScratchpad->pu64SpArray),
                        CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
        }
        free(m_pScratchpad);
        m_pScratchpad = NULL;
    }
}


//----------------------------------------------------------------------------------
// Function: InitializeMemory
//
// Description: Initialize host controller memory.
//
// Parameters: none
//
// Returns: XHCD_OK - success, < 0 - fail.
//----------------------------------------------------------------------------------
INT CHW::InitializeMemory()
{
    UINT uVal, uVal2;
    UINT64 u64Val;
    RING_SEGMENT *pSeg;
    UINT uPageSize;
    INT i;
    RING* pCmdRing;
    RING* pEventRing;

    uPageSize = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PAGESIZE));

    for(i = 0; i < BITS_IN_WORD; i++)
    {
        if(uPageSize & 0x1) 
        {
            break;
        }
        uPageSize = uPageSize >> 1;
    }
    if(i >= BITS_IN_WORD)
    {
        return -XHCD_NOMEMORY;
    }
    
    m_iPageShift = XHCI_PAGE_SHIFT;
    m_iPageSize = 1 << m_iPageShift;

    uVal =
        MAX_SLOTS(READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS)));

    uVal2 = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_CONFIG));
    uVal |=(uVal2 & ~SLOT_MASK);
    WRITE_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_CONFIG), uVal);

    if (m_pDcbaa == NULL) 
    {
        if(!m_pMem->AllocateMemory(DEBUG_PARAM(TEXT("Device context array"))
                                   sizeof(*m_pDcbaa),
                                   (PUCHAR*)&m_pDcbaa,
                                   CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK,
                                   m_iPageSize))
        {
            return -XHCD_NOMEMORY;
        }
    }

    memset(m_pDcbaa, 0, sizeof(*m_pDcbaa));
    m_pDcbaa->u64Dma =(UINT64) m_pMem->VaToPa((PUCHAR)m_pDcbaa);    

    XhciWrite64(m_pDcbaa->u64Dma,(UINT)(m_opBase + USB_XHCD_DCBAAP));

    if(m_pXhcdRing->InitializeMemory() != XHCD_OK)
    {
        return -XHCD_NOMEMORY;
    }

    pCmdRing = m_pXhcdRing->m_pCmdRing;
    pEventRing = m_pXhcdRing->m_pEventRing;

    u64Val = XhciRead64((UINT)(m_opBase + USB_XHCD_CRCR));
    u64Val =(u64Val &(UINT64) COMMAND_RING_RESERVED_BITS) |
            (pCmdRing->pRingSegment->u64Dma &(UINT64) ~COMMAND_RING_RESERVED_BITS) |
            pCmdRing->uCycleState;

    XhciWrite64(u64Val,(UINT)(m_opBase + USB_XHCD_CRCR));

    uVal = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_DBOFF));
    uVal &= DB_OFF_MASK;

    m_dba =(DOORBELL_ARRAY*)(m_capBase + uVal);

    if(m_erst.pEntries == NULL)
    {
        if(!m_pMem->AllocateMemory(DEBUG_PARAM(TEXT("Erst")) sizeof(struct ERST_TABLE_ENTRY) * ERST_NUM_SEGS,
                                (PUCHAR*)&m_erst.pEntries,
                                CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK,
                                0))
        {
            return -XHCD_NOMEMORY;
        }

        if(!m_erst.pEntries)
        {
            return -XHCD_NOMEMORY;
        }
    }

    memset(m_erst.pEntries, 0, sizeof(struct ERST_TABLE_ENTRY)*ERST_NUM_SEGS);
    m_erst.uNumEntries = ERST_NUM_SEGS;
    m_erst.u64ErstDmaAddr =(UINT64)(m_pMem->VaToPa((PUCHAR)m_erst.pEntries));

    struct ERST_TABLE_ENTRY *pEntry = &m_erst.pEntries[0];
    pSeg = pEventRing->pRingSegment;
    pEntry->u64SegAddr = pSeg->u64Dma;
    pEntry->uSegSize = BLOCKS_IN_SEGMENT;
    pEntry->uRsvd = 0;

    uVal = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTSZ));
    uVal &=(UINT)MASK_SIZE;
    uVal |= ERST_NUM_SEGS;
    WRITE_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTSZ), uVal);

    u64Val = XhciRead64((UINT)(m_irSetBase + USB_XHCD_ERSTBA));
    u64Val &= MASK_POINTER;
    u64Val |=(m_erst.u64ErstDmaAddr &(UINT64) ~MASK_POINTER);

    XhciWrite64(u64Val,(UINT)(m_irSetBase + USB_XHCD_ERSTBA));

    SetHostControllerEventDequeue();

    if(ScratchpadAlloc())
    {
        return -XHCD_NOMEMORY;
    }

    if(SetupPortArrays())
    {
        return -XHCD_NOMEMORY;
    }

    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: RingCommandDoorbell
//
// Description: Ring command doorbell.
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::RingCommandDoorbell() const
{
    UINT uTemp;

    uTemp = READ_REGISTER_ULONG((PULONG)(&m_dba->uDoorbell[0])) & DB_MASK;
    WRITE_REGISTER_ULONG((PULONG)(&m_dba->uDoorbell[0]), uTemp | DB_TARGET_HOST);
    READ_REGISTER_ULONG((PULONG)(&m_dba->uDoorbell[0]));
}

//----------------------------------------------------------------------------------
// Function: SetupPortArrays
//
// Description: Calculate and identify port types by extended capability.
//
// Parameters: none
//
// Returns: XHCD_OK
//----------------------------------------------------------------------------------
INT CHW::SetupPortArrays() 
{
    volatile UINT* puAddr;
    UINT uOffset;
    UINT i;

    uOffset = HOST_CONTROLLER_EXTENDED_CAPABILITIES(m_uHccParams);

    for(i = 0; i < m_bNumOfPort; i++)
    {
        m_bPortArray[i] = 0;
    }

    puAddr =(UINT*)m_capBase;
    puAddr += uOffset;

    while(1)
    {
        UINT uCapId;

        uCapId = READ_REGISTER_ULONG((PULONG)(puAddr));

        if(EXTENDED_CAPABILITIES_ID(uCapId) == EXTENDED_CAPABILITIES_PROTOCOL)
        {
            DeterminePortTypes (puAddr, EXTENDED_PORT_MAJOR(uCapId));
        }

        uOffset = EXTENDED_CAPABILITIES_NEXT(uCapId);

        if(!uOffset ||(m_bNumUsb2Ports + m_bNumUsb3Ports) == m_bNumOfPort)
        {
            break;
        }

        puAddr += uOffset;
    }
    
    return XHCD_OK;
}

//----------------------------------------------------------------------------------
// Function: InitListHead
//
// Description: Free list.
//
// Parameters: pPtr - pointer to the list head
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::InitListHead(PLIST_HEAD pPtr) const
{
    pPtr->pLink = NULL;
}

//----------------------------------------------------------------------------------
// Function: UsbListEmpty
//
// Description: Check if kthe list is empty.
//
// Parameters: pList - pointer to the list head
//
// Returns: TRUE if list is empty else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::UsbListEmpty(PLIST_HEAD pList) const
{
    return pList->pLink == NULL;
}

//----------------------------------------------------------------------------------
// Function: UsbListLink
//
// Description: Add structure to the list.
//
// Parameters: pHead - list head 
//
//             pStruct - ptr to base of structure to be linked 
//
//             pLink -  ptr to LINK structure to be linked
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::UsbListLink(PLIST_HEAD pHead,    
                      VOID* pStruct,      
                      PLINK pLink) const       
{
    PLINK pTail;

    pLink->pStruct = pStruct;

    if (pHead->pLink == NULL) {
        pHead->pLink = pLink;
    } else {
        // Add the entry to the tail of the list. 
        for(pTail = pHead->pLink; pTail->pLinkFwd != NULL;
            pTail = pTail->pLinkFwd)
        {
            ;
        }

        pLink->pLinkFwd = NULL;
        pLink->pLinkBack = pTail;

        pTail->pLinkFwd = pLink;
    }
}

//----------------------------------------------------------------------------------
// Function: UsbListUnlink
//
// Description: Unlink structure from the list.
//
// Parameters: pHead - list head 
//
//             pLink -  ptr to LINK structure to be unlinked
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::UsbListUnlink(PLIST_HEAD pHead,    
                        PLINK pLink) const
{
    if (pLink == pHead->pLink) 
    {
        pHead->pLink = pHead->pLink->pLinkFwd;
    } else {
        if(pLink->pLinkBack)
        {
            pLink->pLinkBack->pLinkFwd = pLink->pLinkFwd;
        }

        if(pLink->pLinkFwd)
        {
            pLink->pLinkFwd->pLinkBack = pLink->pLinkBack;
        }

        pLink->pLinkBack = pLink->pLinkFwd = NULL;
    }
}

//----------------------------------------------------------------------------------
// Function: DeterminePortTypes
//
// Description: Determine port type by extended capability.
//
// Parameters: puAddr - extended capability address
//
//             bMajorRevision  - major revision
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::DeterminePortTypes(volatile UINT* puAddr, UCHAR bMajorRevision)
{
    UINT uTemp, uPortOffset, uPortCount;
    UINT i;

    if(bMajorRevision > USB30_MAJOR_REVISION)
    {
        return;
    }

    uTemp = READ_REGISTER_ULONG((PULONG)(puAddr + 2));
    uPortOffset = EXTENDED_PORT_OFF(uTemp);
    uPortCount = EXTENDED_PORT_COUNT(uTemp);

    if(uPortOffset == 0 ||(uPortOffset + uPortCount - 1) > m_bNumOfPort)
    {
        return;
    }

    uPortOffset--;
    for(i = 0; i < uPortCount; i++)
    {
        if(m_bPortArray[uPortOffset + i] != 0)
        {
            if(m_bPortArray[uPortOffset + i] < bMajorRevision)
            {
                m_bNumUsb3Ports++;
                m_bNumUsb2Ports--;
                m_bPortArray[uPortOffset + i] = bMajorRevision;
            }
            continue;
        }

        m_bPortArray[uPortOffset + i] = bMajorRevision;

        if(bMajorRevision == USB30_MAJOR_REVISION)
        {
            m_bNumUsb3Ports++;
        }
        else
        {
            m_bNumUsb2Ports++;
        }
    }
}

//----------------------------------------------------------------------------------
// Function: SetPortStateToNeutral
//
// Description: Get neutral port state.
//
// Parameters: uState - port state.
//
// Returns: neutral port state.
//----------------------------------------------------------------------------------
UINT CHW::SetPortStateToNeutral(UINT uState) const
{
    return(uState & PORT_READ_ONLY) |(uState & PORT_READ_WRITE);
}

//----------------------------------------------------------------------------------
// Function: PrepareTransfer
//
// Description: This method prepares one transfer for activation
//
// Parameters: bSlotId - device slot ID
//
//             uEpAddress - endpoint address
//
//             ppTd - pointer to transfer data
//
//             uTransferLength - transfer buffer length
//
//             bEndpointType -  endpoint type
//
// Returns: XHCD_OK if success, else error code
//----------------------------------------------------------------------------------
INT CHW::PrepareTransfer(UCHAR bSlotId,
                        UCHAR uEpAddress,
                        TRANSFER_DATA** ppTd,
                        UINT uTransferLength,
                        UCHAR bEndpointType) const
{
    if (m_fInitialized) {
      return m_pXhcdRing->PrepareTransfer(bSlotId,
                                        uEpAddress,
                                        ppTd,
                                        uTransferLength,
                                        bEndpointType);
    } else {
        return -XHCD_FAIL;
    }
}

//----------------------------------------------------------------------------------
// Function: IsTrbInSement
//
// Description: Check if event dma address is correspondent to
//              the transfer data.
//
// Parameters: pStartSeg - endpoint transfer ring segment
//
//             pDequeueTrb - dequeue address of event ring
//
//             pTrb - transfer data to be checked
//
//             u64EventDma - dma address of checked event
//
// Returns: TRUE if dma address is correspondent to
//          the transfer data, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::IsTrbInSement(RING_SEGMENT *pStartSeg,
                        TRB *pDequeueTrb,
                        TRB *pTrb,
                        UINT64 u64EventDma) const
{
    return m_pXhcdRing->IsTrbInSement(pStartSeg, pDequeueTrb, pTrb, u64EventDma);
}

//----------------------------------------------------------------------------------
// Function: DoConfigureEndpoint
//
// Description: Modify context, send configure endpoint command, reset context and
//              set up new endpoint ring.
//
// Parameters: bSlotId - device slot ID
//
// Returns: Returns XHCD_OK if configure endpoint command passed, < 0 otherwise.
//----------------------------------------------------------------------------------
BOOL CHW::DoConfigureEndpoint(UCHAR bSlotId, USB_DEVICE_DESCRIPTOR* pDevDesc, USB_HUB_DESCRIPTOR* pHubDesc) const
{
    return(m_pXhcdRing->DoConfigureEndpoint(bSlotId, pDevDesc, pHubDesc) == XHCD_OK);
}

//----------------------------------------------------------------------------------
// Function: FreeLogicalDevice
//
// Description: Free memory allocated for the logical device.
//
// Parameters: bSlotId - device slot ID
//
// Returns: none
//----------------------------------------------------------------------------------
BOOL CHW::FreeLogicalDevice(UCHAR bSlotId) const
{
    LOGICAL_DEVICE *pVirtDev;
    COMPLETION *pCmdCompletion;
    UINT uState;
    INT i;
    BOOL bRet = TRUE;

    pVirtDev = m_pLogicalDevices [bSlotId];

    if(pVirtDev == NULL) {
        return FALSE;
    }

    for(i = 0; i < MAX_EP_NUMBER; i++) {
        pVirtDev->eps[i].uEpState &= ~ENDPOINT_HALT_PENDING;
    }

    uState = READ_REGISTER_ULONG((PULONG)(m_opBase +  USB_XHCD_USBSTS));
    if(uState == BAD_DEVICE_STATUS)
    {
        m_pXhcdRing->FreeLogicalDevice(bSlotId);
        return FALSE;
    }

    if(m_pXhcdRing->IssueSlotControl(TRANSFER_BLOCK_DISABLE_SLOT, bSlotId))
    {
        return FALSE;
    }

    pCmdCompletion = &pVirtDev->cmdCompletion;
    if (pCmdCompletion == NULL)
    {
        return FALSE;
    }

    m_pXhcdRing->InitCompletion(pCmdCompletion);
    RingCommandDoorbell();

    if(WaitForCompletion(pCmdCompletion, USB_CTRL_SET_TIMEOUT) <= 0)
    {
        m_pXhcdRing->MoveDequeue(m_pXhcdRing->m_pCmdRing, FALSE);
        bRet = FALSE;
    }

    m_pXhcdRing->FreeLogicalDevice(bSlotId);



    return bRet;
}

//----------------------------------------------------------------------------------
// Function: ResetEndpoint
//
// Description: Issue reset endpoint command and cleanup endpoint ring.
//
// Parameters: bSlotId - device slot ID
//
//             uEpAddress - endpoint address
//
//             bEndpointType -  endpoint type
//
// Returns: TRUE if success, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::ResetEndpoint(UCHAR bSlotId, UINT uEpAddress, UCHAR bEndpointType) const
{
    return(m_pXhcdRing->ResetEndpoint(bSlotId,
                                        uEpAddress,
                                        bEndpointType) == XHCD_OK);
}

//----------------------------------------------------------------------------------
// Function: CleanupHaltedEndpoint
//
// Description: Reset halted endpoint and set new dequeue state for
//              stalled ring.
//
// Parameters: bSlotId - device slot ID
//
//             bEpAddress - endpoint address
//
// Returns: XHCD_OK if endpoint successfully initialized, else <0
//----------------------------------------------------------------------------------
INT CHW::CleanupHaltedEndpoint(UCHAR bSlotId, UCHAR bEpAddress) const
{
    return m_pXhcdRing->CleanupHaltedEndpoint(bSlotId,
                                               bEpAddress,
                                               NULL,
                                               NULL);
}

//----------------------------------------------------------------------------------
// Function: CountAbortTransfers
//
// Description: Count aborted transfers and reset driver if aborts counter > 5
//
// Parameters: bSlotId - device slot id
//
//             bEpAddress - endpoint address
//
// Returns: TRUE if driver is reseted, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::CountAbortTransfers(UCHAR bSlotId, UCHAR bEpAddress)
{
    
    if (m_pXhcdRing->CountAbortTransfers(bSlotId, bEpAddress)) 
    {
        StopHostController();
        DeInitialize(FALSE);

        //Reset the HostController Hardware
        if (HaltController () != XHCD_OK)
        {
            return FALSE;
        }

        if (ResetHostController() != XHCD_OK)
        {
            return FALSE;
        }

        //Initialize the host controller
        if (Initialize()) {
            EnterOperationalState();
        } 
        else
        {
            return FALSE;
        }
        return TRUE;
    } else {
        return FALSE;
    }
}


//----------------------------------------------------------------------------------
// Function: DropEndpoint
//
// Description: Drop endpoint from the input control context,
//              reset slot context
//
// Parameters: bSlotId - slot ID
//
//             uEpAddress - endpoint address
//
// Returns: TRUE if endpoint successfully added, else FALSE
//----------------------------------------------------------------------------------
BOOL CHW::DropEndpoint(UCHAR bSlotId, UINT uEpAddress) const
{
    if (m_fInitialized) {
        return(m_pXhcdRing->DropEndpoint(bSlotId, uEpAddress) == XHCD_OK);
    } else {
        return FALSE;
    }
}

//----------------------------------------------------------------------------------
// Function: PutTransferToRing
//
// Description: Put TRB to the ring and increment enqueue pointer
//
// Parameters: pRing - pointer to the ring
//
//             fConsumer - is this ring of consumer 
//
//             uField1..4 - fields of TRB
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::PutTransferToRing(RING *pRing, BOOL fConsumer, UINT uField1, UINT uField2, UINT uField3, UINT uField4) const
{
    m_pXhcdRing->PutTransferToRing(pRing, fConsumer, uField1, uField2, uField3, uField4);
}

//----------------------------------------------------------------------------------
// Function: RingTransferDoorbell
//
// Description: Ring endpoint for the transfer.
//
// Parameters: bSlotId - device slot ID
//
//             bEpIndex -  endpoint index
//
//             iStartCycle - cycle state
//
//             pStartTrb - first TRB of transfer
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CHW::RingTransferDoorbell(UCHAR bSlotId,
                            UCHAR bEpIndex,
                            INT iStartCycle,
                            NORMAL_TRB *pStartTrb) const
{
    m_pXhcdRing->RingTransferDoorbell(bSlotId, bEpIndex, iStartCycle, pStartTrb);
}

//----------------------------------------------------------------------------------
// Function: GetEndpointContext
//
// Description: get pointer to the endpoint context data
//
// Parameters: pContext - pointer to the context container
//
//             bEpIndex -endpoint index
//
// Returns: pointer to the endpoint context data
//----------------------------------------------------------------------------------
 ENDPOINT_CONTEXT_DATA* CHW::GetEndpointContext(CONTAINER_CONTEXT *pContext,
                                    UCHAR bEpIndex) const
{
    return m_pXhcdRing->GetEndpointContext(pContext, bEpIndex);
}


#ifdef DEBUG
//----------------------------------------------------------------------------------
// Function: DumpRings
//
// Description: Queries Host Controller for contents of Rings registers, and prints
//              them to DEBUG output. Bit definitions are in XHCI spec
//              This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DumpRings(VOID)
{
    UINT uTemp;
    UINT* pPtr;
    RING* pEventRing;
    RING* pCmdRing;

    DEBUGMSG(ZONE_REGISTERS,(TEXT("Event regs:\n")));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTSZ));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTSZ = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTBA));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTBA = %x\n"), uTemp));

    pPtr =(UINT*)m_pMem->PaToVa((ULONG) uTemp);

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERDP));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERDP = %x\n"), uTemp));

    DEBUGMSG(ZONE_REGISTERS,
        (TEXT("virtual ERDP = %x\n"),
        m_pMem->PaToVa((ULONG) uTemp)));

    pEventRing = m_pXhcdRing->m_pEventRing;
    if(pEventRing != NULL)
    {
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("Event ring:\n")));
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("dequeue %x\n"),
            pEventRing->pDequeue));
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("dma pDequeue %x\n"),
            m_pMem->VaToPa((PBYTE) pEventRing->pDequeue)));
    }

    DEBUGMSG(ZONE_REGISTERS,(TEXT("ptr = %x\n"), pPtr));

    if(pPtr != NULL)
    {
        DEBUGMSG(ZONE_REGISTERS,(TEXT("event ring segment %x\n"), *pPtr));
    }

    pCmdRing = m_pXhcdRing->m_pCmdRing;
    if(pEventRing != NULL)
    {
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("Command ring:\n")));
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("dequeue %x\n"),
            pCmdRing->pDequeue));
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("dma dequeue %x\n"),
            m_pMem->VaToPa((PBYTE) pCmdRing->pDequeue)));
    }
}

//----------------------------------------------------------------------------------
// Function: DumpUSBCMD
//
// Description: Queries Host Controller for contents of USBCMD, and prints
//              them to DEBUG output. Bit definitions are in XHCI spec 
//              used in DEBUG mode only. This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DumpUSBCMD(VOID)
{
    UINT uTemp;

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("USBCMD = %x\n"), uTemp));
}

//----------------------------------------------------------------------------------
// Function: DumpUSBSTS
//
// Description: Queries Host Controller for contents of USBSTS, and prints
//              them to DEBUG output. Bit definitions are in XHCI spec
//              used in DEBUG mode only. This function is static
//
// Parameters:
//
// Returns:
//----------------------------------------------------------------------------------
VOID CHW::DumpUSBSTS(VOID)
{
    UINT uTemp;

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("USBSTS = %x\n"), uTemp));
}

//----------------------------------------------------------------------------------
// Function:
//
// Description: Queries Host Controller for contents of USBINTR, and prints
//              them to DEBUG output. Bit definitions are in XHCI spec 
//              used in DEBUG mode only. This function is static
//
// Parameters:
//
// Returns:
//----------------------------------------------------------------------------------
VOID CHW::DumpUSBINTR(VOID)
{
    UINT uTemp;
    UINT uPortReg = 0;

    DEBUGMSG(ZONE_REGISTERS,(TEXT("interrupt regs:\n")));
    uPortReg = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("iman = %x\n"), uPortReg));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMOD));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("IMOD = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTSZ));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTSZ = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTBA));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTBA = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERDP));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERDP = %x\n"), uTemp));

    DEBUGMSG(ZONE_REGISTERS,
        (TEXT("\tCHW - USB INTERRUPT REGISTER(USBINTR) = 0x%X. Dump:\n"),
        uPortReg));
}

//----------------------------------------------------------------------------------
// Function:
//
// Description: Queries Host Controller for contents of PORTSC #port, and prints
//              them to DEBUG output. Bit definitions are in XHCI spec
//              used in DEBUG mode only. This function is static
//
// Parameters: wPort - the port number to read. 
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DumpPORTSC(IN const USHORT wPort)
{
    DEBUGCHK(wPort >=  1 && wPort <=  m_bNumOfPort);
    if(wPort >=  1 && wPort <=  m_bNumOfPort)
    {
        UINT uPortSC =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER *(wPort - 1)));
        DEBUGMSG(ZONE_REGISTERS,
            (TEXT("\tCHW - PORT STATUS AND CONTROL REGISTER(PORTSC%d) = \
                 0x%X. Dump:\n"),
                 wPort,
                 uPortSC ));
    }
}

//----------------------------------------------------------------------------------
// Function: DumpAllRegisters
//
// Description: Queries Host Controller for all registers, and prints
//          them to DEBUG output. Register definitions are in XHCI spec
//          used in DEBUG mode only. This function is static
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
VOID CHW::DumpAllRegisters(VOID)
{
    UINT uTemp;
    UCHAR i;
    UINT uPortReg = 0;

    DEBUGMSG(ZONE_REGISTERS,(TEXT("host controller capability regs:\n")));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_CAPLENGTH));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("CAPLENGTH = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCIVERSION));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("HCIVERSION = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("HCSPARAMS = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS2));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("HCSPARAMS2 = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCSPARAMS3));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("HCSPARAMS3 = %x\n"), uTemp));
    
    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_HCCPARAMS));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("HCCPARAMS = %x\n"), uTemp));
    
    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_DBOFF));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("DBOFF = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_capBase + USB_XHCD_RTSOFF));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("RTSOFF = %x\n"), uTemp));

    DEBUGMSG(ZONE_REGISTERS,(TEXT("operational regs:\n")));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBCMD));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("USBCMD = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_USBSTS));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("USBSTS = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PAGESIZE));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("PAGESIZE = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_DNCTLR));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("DNCTRL = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_CRCR));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("CRCR = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_DCBAAP));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("DCBAAP = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_CONFIG));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("CONFIG = %x\n"), uTemp));

    DEBUGMSG(ZONE_REGISTERS,(TEXT("port status regs:\n")));

    for(i = 0; i < 8; i++)
    {
        uPortReg =
            READ_REGISTER_ULONG((PULONG)(m_opBase + USB_XHCD_PORT_BASE +
            BYTES_IN_DWORD * PORT_REGISTER_NUMBER * i));
        DEBUGMSG(ZONE_REGISTERS,(TEXT("PORTSC [%d] = %x\n"), i, uPortReg));
    }

    DEBUGMSG(ZONE_REGISTERS,(TEXT("runtime regs:\n")));
    uTemp =
        READ_REGISTER_ULONG((PULONG)(m_runBase + USB_XHCD_MFINDEX)) &
        USB_XHCD_MFINDEX_FRAME_INDEX_MASK;
    DEBUGMSG(ZONE_REGISTERS,(TEXT("MFINDEX = %x\n"), uTemp));

    DEBUGMSG(ZONE_REGISTERS,(TEXT("interrupt regs:\n")));
    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMAN));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("iman = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_IMOD));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("IMOD = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTSZ));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTSZ = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERSTBA));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERSTBA = %x\n"), uTemp));

    uTemp = READ_REGISTER_ULONG((PULONG)(m_irSetBase + USB_XHCD_ERDP));
    DEBUGMSG(ZONE_REGISTERS,(TEXT("ERDP = %x\n"), uTemp));
}

#endif

