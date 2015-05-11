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

// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2011 Intel Corporation All Rights Reserved. 
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

/*++

Module Name:
    AtapiSharedIrq.cpp

Abstract:
    Base ATA/ATAPI IRQ sharing support.

Revision History:

--*/

#include <atamain.h>
#include <giisr.h>
#include <atapisharedirq.h>
#include <pcibus.h>

// ----------------------------------------------------------------------------
// Function: CPCISharedIRQ
//     Constructor
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

CPCISharedIRQ::CPCISharedIRQ(
    )
{
    m_hIsr = NULL;
    m_hIst = NULL;
    m_hSharedIRQEvent = NULL;
    m_fTerminated = FALSE;
    m_pIDEPort = NULL;
}

// ----------------------------------------------------------------------------
// Function: CPCISharedIRQ
//     Destructor
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

CPCISharedIRQ::~CPCISharedIRQ(
    )
{
    if (NULL != m_hIsr) {
        FreeIntChainHandler(m_hIsr);
        m_hIsr = NULL;
    }
    if (NULL != m_hIst) {
        m_fTerminated = TRUE;   // direct IST to exit
        SetEvent(m_hSharedIRQEvent);   // set IRQ event
        DWORD dwWaitResult;
        dwWaitResult = WaitForSingleObject(m_hIst, INFINITE); // wait for IST to exit
        if (dwWaitResult != WAIT_OBJECT_0) {
            DEBUGMSG(ZONE_ERROR, (_T(
                "Atapi!CPCISharedIRQ::~CPCISharedIRQ> failed to wait for IST to exit\r\n"
                )));
        }
        CloseHandle(m_hIst);
        m_hIst = NULL;
        m_fTerminated = FALSE;

    }
    if (NULL != m_hSharedIRQEvent) {
        CloseHandle(m_hSharedIRQEvent);
        m_hSharedIRQEvent = NULL;
    }
}

// ----------------------------------------------------------------------------
// Function: HookSharedIrq
//     Initialize IST/ISR
//
// Parameters:
//     pPort
// ----------------------------------------------------------------------------

BOOL
CPCISharedIRQ::HookSharedIrq(
    CDisk *pThis
    )
{
    DWORD dwCleanUp = 0; // track progress for undo on clean up
#ifdef DEBUG
    ASSERT(pThis->m_pPort != NULL);
    DWORD OldSysIntr = pThis->m_pPort->m_dwSysIntr; // track the old SysIntr value
#endif

    // IDE_Init fetches the interrupt/SysIntr assignments for the device from
    // the device's device key; interrupt/SysIntr assignments are completed by
    // the PCI Bus driver

    m_pIDEPort = pThis->m_pPort;

    // if LegaceIRQ == pPort->m_pController->m_pPort[ATA_PRIMARY]->m_dwIrq
    // both controller and driver are configured in Legacy Mode, so no need to do anything else.
    if (m_pIDEPort->m_pController->m_pIdeReg->dwLegacyIRQ == m_pIDEPort->m_pController->m_pPort[ATA_PRIMARY]->m_dwIrq) {
        return TRUE;
    }

    // create an interrupt event

    m_hSharedIRQEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == m_hSharedIRQEvent) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCISharedIRQ::HookSharedIrq> failed to create interrupt event; port %p device %x\r\n"
            ), m_pIDEPort, this));
        goto cleanUp;
    }
    dwCleanUp |= CP_CLNUP_IRQEVENT;

    // initialize interrupt
    
    // Disable the interrupt initialized in CPCIDisk::ConfigPort first
    InterruptDisable(m_pIDEPort->m_dwSysIntr);

    // The SysIntr allocated by CATAController::MapIrqtoSysIntr could be a static mapping, 
    // request our own SYSINTR when needed.
    // if m_pIdeReg->dwSysIntr==0 both Primary and Secondary's SysIntrs are from static mapping.
    // Seconday SYSINTR is always static mapping.
    // 
    if ((m_pIDEPort->m_pController->m_pIdeReg->dwSysIntr==0) || 
        (m_pIDEPort->m_pController->m_pPort[ATA_SECONDARY]==m_pIDEPort)) {
        DWORD dwReturned = 0;

        if (!KernelIoControl(
            IOCTL_HAL_REQUEST_SYSINTR,
            (LPVOID)&m_pIDEPort->m_dwIrq, sizeof(DWORD),
            (LPVOID)&m_pIDEPort->m_dwSysIntr, sizeof(DWORD),
            &dwReturned
        )) {
            DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                "Atapi!CPCISharedIRQ::HookSharedIrq> Failed to map IRQ(%d) to SysIntr for device\r\n"
                ), m_pIDEPort->m_dwSysIntr));
            goto cleanUp;
        }
        dwCleanUp |= CP_CLNUP_INTSYSINTR;
    }

    DEBUGMSG (ZONE_INIT, (_T(
        "Atapi!CPCISharedIRQ::HookSharedIrq> Irq 0x%x was mapped to 0x%x and now remapped to 0x%x, SysIntr in registry: 0x%x\r\n"
        ), m_pIDEPort->m_dwIrq, OldSysIntr, m_pIDEPort->m_dwSysIntr,
        m_pIDEPort->m_pController->m_pIdeReg->dwSysIntr));

    // and then re-associate the m_pIDEPort->m_dwSysIntr to m_hSharedIRQEvent, 
    // m_pIDEPort->m_hIRQEvent will be triggered by IST
    if (!InterruptInitialize(m_pIDEPort->m_dwSysIntr, m_hSharedIRQEvent, NULL, 0)) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCISharedIRQ::HookSharedIrq> failed to initialize interrupt; device %x, sysintr %u\r\n"
            ), this, m_pIDEPort->m_dwSysIntr));
        goto cleanUp;
    }

    // load our ISR

    m_hIsr = LoadIntChainHandler(
        m_pIDEPort->m_pController->m_pIdeReg->pszIsrDll,
        m_pIDEPort->m_pController->m_pIdeReg->pszIsrHandler,
       (BYTE)m_pIDEPort->m_dwIrq);

    if (!m_hIsr) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCISharedIRQ::HookSharedIrq> Failed to load ISR DLL %s; device %x\r\n"
            ), m_pIDEPort->m_pController->m_pIdeReg->pszIsrDll, this));
        goto cleanUp;
    }
    dwCleanUp |= CP_CLNUP_INTCHNHDNLR;

    // setup ISR handler

    GIISR_INFO Info;

    memset(&Info, 0, sizeof(Info));
    // set up ISR handler
    Info.SysIntr = m_pIDEPort->m_dwSysIntr;
    Info.CheckPort = TRUE;
    Info.PortIsIO = TRUE;
    Info.UseMaskReg = FALSE;
    // I/O space, no need to map.
    Info.PortAddr = m_pIDEPort->m_dwBMRStatic + 2;
    Info.PortSize = sizeof(BYTE);
    Info.Mask = 0x04;

    // install ISR handler

    if (!KernelLibIoControl(m_hIsr, IOCTL_GIISR_INFO, &Info, sizeof(Info), NULL, 0, NULL)) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCISharedIRQ::HookSharedIrq> Failed to install ISR handler; device %x\r\n"
            ), this));
        goto cleanUp;
    }

    // create IST

    HANDLE hIst;
    DWORD dwIstId;
    hIst = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IstMain, this, 0, &dwIstId);
    if (NULL == hIst) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCISharedIRQ::HookSharedIrq> Failed to create IST; device %x\r\n"
            ), this));
        goto cleanUp;
    }
    dwCleanUp |= CP_CLNUP_IST;

    // store handle to IST
    m_hIst = hIst;

    // override IST's default priority, if applicable

    DWORD dwIstPriority;
    if (AtaGetRegistryValue(pThis->m_hDevKey, L"IstPriority256", &dwIstPriority)) {
        if (FALSE == ::CeSetThreadPriority(hIst, dwIstPriority)) {
            DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                "Atapi!CPCISharedIRQ::HookSharedIrq> Failed to set IST priority to %d\r\n"
                ), dwIstPriority));
            goto cleanUp;
        }
    }
    return TRUE;

cleanUp:
    if (dwCleanUp & CP_CLNUP_INTCHNHDNLR) {
        FreeIntChainHandler(m_hIsr);
        m_hIsr = NULL;
    }
    if (dwCleanUp & CP_CLNUP_INTSYSINTR) {
        DWORD dwReturned = 0;
        KernelIoControl(
            IOCTL_HAL_RELEASE_SYSINTR,
            (LPVOID)&m_pIDEPort->m_dwSysIntr, sizeof(DWORD),
            NULL, 0,
            &dwReturned
        );
    }
    if (dwCleanUp & CP_CLNUP_IST) {
        m_fTerminated = TRUE;   // direct IST to exit
        SetEvent(m_hSharedIRQEvent);   // set IRQ event
        DWORD dwWaitResult;
        dwWaitResult = WaitForSingleObject(m_hIst, INFINITE); // wait for IST to exit
        if (dwWaitResult != WAIT_OBJECT_0) {
            DEBUGMSG(ZONE_ERROR, (_T(
                "Atapi!CPCISharedIRQ::HookSharedIrq> failed to wait for IST to exit\r\n"
                )));
        }
        CloseHandle(m_hIst);
        m_hIst = NULL;
        m_fTerminated = FALSE;  // reset IST directive

    }
    if (dwCleanUp & CP_CLNUP_IRQEVENT) {
        CloseHandle(m_hSharedIRQEvent);
        m_hSharedIRQEvent = NULL;
    }
    return FALSE;
}

// ----------------------------------------------------------------------------
// Function: IstMain
//     Interrupt service thread routine
//
// Parameters:
//     pThis - Pointer to this CPCISharedIRQ instance;
// ----------------------------------------------------------------------------
DWORD
CPCISharedIRQ::IstMain(
    CPCISharedIRQ* pThis
    )
{
    BYTE bStatus, bStatus2;
    CPort *pPort;  // this CPCISharedIRQ instance's associated CPort instance

    pPort = pThis->m_pIDEPort;

    while(TRUE) {
        WaitForSingleObject(pThis->m_hSharedIRQEvent, INFINITE);

        // are we being directed to exit?
        if (pThis->m_fTerminated) {
            break;
        }

        // read ATA_REG_STATUS to release Atapi IRQ signal.
        bStatus = READ_PORT_UCHAR((LPBYTE)(pPort->m_dwRegBase + ATA_REG_STATUS));
        
        // Clear the BusMater IRQ register.
        bStatus2 = READ_PORT_UCHAR((LPBYTE)(pPort->m_dwBMR + 2));
        if ( bStatus2 & 0x04 )
        {
            bIsBMIRQ = 1;
            // clear Interrupt but not Error.
            bStatus2 &= ~0x02;
            bStatus2 |= 0x04;
            WRITE_PORT_UCHAR((LPBYTE)(pPort->m_dwBMR + 2), bStatus2);
        }
        // trigger the m_hIRQEvent.
        SetEvent(pPort->m_hIRQEvent);
        
        InterruptDone(pPort->m_dwSysIntr);
    }
    return 0;
}
