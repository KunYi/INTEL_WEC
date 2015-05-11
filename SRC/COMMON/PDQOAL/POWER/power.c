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
//
// Power off/Suspend routines
//

#include <windows.h>
#include <nkintr.h>
#include <oalintr.h>
#include "pc.h"
#include <oal.h>
#include <pci.h>
#include "ioctl_atom.h"
#include "ichID.h"

static UCHAR GetPICInterruptMask(BOOL fMaster);
static BOOL SetPICInterruptMask(BOOL fMaster, UCHAR ucMask);
void x86PowerShutDown(void);

//
// This routine is invoked when the OFF button is pressed. It is responsible
// for any final power off state and putting the cpu into standby.
//
static int CpuSleepX86(void) {

#ifdef NOTDEF
    // NOTE : This bit of code will reboot the system. Some people build this way
    // so that they can do a soft reboot simply by choosing suspend from the run menu.
    __asm {
        cli                     ; Disable Interrupts
        mov     al, 0FEh        ; Reset CPU
        out     064h, al
        jmp     near $          ; Should not get here
    }
#endif

    __asm {
        sti
        hlt
        cli
    }
}


static UCHAR GetPICInterruptMask(BOOL fMaster) {
    UCHAR ucMask;
    UCHAR ucPort = (fMaster)?0x21:0xa1;

    __asm {
        pushfd
        cli
        xor dx, dx
        mov dl, ucPort
        in al, dx
        mov ucMask, al
        sti
        popfd
    }

    return ucMask;
}


static BOOL SetPICInterruptMask(BOOL fMaster, UCHAR ucMask) {
    WORD wPort = (fMaster)?0x21:0xa1;

    __asm {
        pushfd                      ; Save interrupt state
        cli
        mov     dx, wPort
        mov     al, ucMask
        out     dx, al
        sti
        popfd                       ; Restore interrupt state
    }

    return TRUE;
}

static DWORD dwLastWakeupSource=SYSWAKE_UNKNOWN;

static BYTE fInterruptWakeup[SYSINTR_MAXIMUM];
static BYTE fInterruptWakeupMask[SYSINTR_MAXIMUM];

static void SetInterruptMask(PBYTE pMaster,PBYTE pSlave)
{
    DWORD sysIntr;
    *pMaster=*pSlave=0;
    for (sysIntr = 0; sysIntr < SYSINTR_MAXIMUM; sysIntr++) {
        if (fInterruptWakeupMask[sysIntr]) {
             UINT32 count, *pIrqs, i;
             count = 1;
             if (!OALIntrTranslateSysIntr(sysIntr, &count, &pIrqs)) continue;
             for (i = 0; i < count; i++) {
                 if (pIrqs[i] != -1)  {
                    if (pIrqs[i] < 8) 
                        *pMaster|=(1 << pIrqs[i]);
                    else 
                        *pSlave |=(1 << (pIrqs[i] - 8));
                }
             }                 
        }
    }

}

DWORD OEMSetWakeupSource( DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM) {
        fInterruptWakeupMask[dwSources]=1;
        return 1;
    } 
    return 0;
}

DWORD OEMPowerManagerInit(void)
{
#ifdef INIT_WAKE
    // OEMPowerOff used to hardcode the keyboard and mouse to wake the system.
    // Device drivers can now tell the OAL to enable/disable it's wake interrupt.
    // Define INIT_WAKE if you want to use the hardcoded values.
    OEMSetWakeupSource(SYSINTR_KEYBOARD);
    OEMSetWakeupSource(SYSINTR_TOUCH);
#endif
    OEMSetWakeupSource(SYSINTR_RTC_ALARM);
    return 0;
}

DWORD OEMResetWakeupSource( DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM) {
        fInterruptWakeupMask[dwSources]=0;
        return 1;
    } 
    return 0;
}

DWORD OEMGetWakeupSource(void)
{
    return dwLastWakeupSource;
}

void OEMIndicateIntSource(DWORD dwSources)
{
    if (dwSources<SYSINTR_MAXIMUM ) {
        fInterruptWakeup[dwSources]=1;
    }
}

void OEMClearIntSources()
{
    memset(fInterruptWakeup,0,SYSINTR_MAXIMUM);
}

DWORD OEMFindFirstWakeSource()
{
    DWORD dwCount;

    for (dwCount=0;dwCount<SYSINTR_MAXIMUM;dwCount++) {
        if (fInterruptWakeup[dwCount]&& fInterruptWakeupMask[dwCount])
            break;
    }

    return (dwCount == SYSINTR_MAXIMUM) ? SYSWAKE_UNKNOWN : dwCount;
}

void OEMPowerOff(void)
{
    BYTE bOldMaster,bOldSlave;
    BYTE bMaster,bSlave;
    DWORD dwWake;

    INTERRUPTS_OFF();

    // clear our wake source, which is the reason we are exiting this routine
    dwLastWakeupSource = SYSWAKE_UNKNOWN;
        
    // get the current interrupt mask
    bOldMaster=GetPICInterruptMask(TRUE);
    bOldSlave=GetPICInterruptMask(FALSE);

    // configure the new mask and clear wake source globals
    SetInterruptMask(&bMaster,&bSlave);
    SetPICInterruptMask( TRUE, (UCHAR)~(bMaster| (1<<INTR_PIC2)));
    SetPICInterruptMask( FALSE, (UCHAR)~bSlave );

    do {
       INTERRUPTS_ON();
       // wait for an interrupt that we've enabled as a wake source
       CpuSleepX86();
       INTERRUPTS_OFF();
       // determine what woke us up
       dwWake = OEMFindFirstWakeSource();
    } while (dwWake == SYSWAKE_UNKNOWN);
    
    // restore our interrupt mask
    SetPICInterruptMask(TRUE, bOldMaster);
    SetPICInterruptMask(FALSE, bOldSlave);

    dwLastWakeupSource = dwWake;
    
    INTERRUPTS_ON();
}


BOOL x86PowerIoctl (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
    BOOL rc = FALSE;
    switch (code) {
    case IOCTL_HAL_ENABLE_WAKE:
    case IOCTL_HAL_DISABLE_WAKE:
        if (lpBytesReturned) {
            *lpBytesReturned = 0;
        }
        if (lpInBuf && nInBufSize == sizeof(DWORD)) {
            DWORD dwReturn =
                ((code==IOCTL_HAL_ENABLE_WAKE)
                ? OEMSetWakeupSource   (*(PDWORD)lpInBuf)
                : OEMResetWakeupSource (*(PDWORD)lpInBuf));
            if (dwReturn)
                rc = TRUE;
        } else {
            NKSetLastError (ERROR_INVALID_PARAMETER);
        }
        break;
    case IOCTL_HAL_GET_WAKE_SOURCE:
        if (lpBytesReturned) {
            *lpBytesReturned = sizeof(DWORD);
        }
        if (!lpOutBuf && nOutBufSize > 0) {
            NKSetLastError (ERROR_INVALID_PARAMETER);
        } else if (!lpOutBuf || nOutBufSize < sizeof(DWORD)) {
            NKSetLastError (ERROR_INSUFFICIENT_BUFFER);
        } else {
            *(PDWORD)lpOutBuf = OEMGetWakeupSource();
            rc = TRUE;
        }
        break;
    case IOCTL_HAL_PRESUSPEND:
        if (lpBytesReturned) {
            *lpBytesReturned = 0;
        }
        OEMClearIntSources();
        rc = TRUE;
        break;
    case IOCTL_HAL_POWERSHUTDOWN:
        RETAILMSG(1, (TEXT("SHUTDOWN")));
        x86PowerShutDown();
        rc = TRUE;
        break;        
    default:
        NKSetLastError (ERROR_INVALID_PARAMETER);
        break;
    }
    return rc;
}


//------------------------------------------------------------------------------
// Function: x86IoCtlGetPowerDisposition
//
// IOCTL_HAL_GET_POWER_DISPOSITION IOCTL Handler
//

BOOL x86IoCtlGetPowerDisposition(UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer, UINT32 outSize, UINT32 *pOutSize)
{
    if ((!pOutBuffer) || (outSize<sizeof(DWORD)))
    {
        OALMSG(TRUE, (L"***Error: Incorrect use of IOCTL_HAL_GET_POWER_DISPOSITION.\r\n"));
        return FALSE;
    }

    /* see pkfuncs.h for meaning of this value */
    *((DWORD *)pOutBuffer) = (DWORD)POWER_DISPOSITION_SUSPENDRESUME_MANUAL;
    if (pOutSize)
        *pOutSize = sizeof(DWORD);
    
    return(TRUE);
}

void x86PowerShutDown(void)
{
    USHORT usDeviceId = 0;
    ULONG  ulPmBase = 0;
    ULONG  ulData = 0;
    
    // Obtain the LPC Device ID 
    PCIReadBusData(INTEL_LPC_BUSNUM, INTEL_LPC_DEVICENUM, INTEL_LPC_FUNCNUM, &usDeviceId,INTEL_LPC_OFFSET,INTEL_LPC_LENGTH);

    if (usDeviceId == INTEL_LPC_DEVICEID_E600)
    {
        // Obtain ACPI Base Address from LPC PCI header 
        PCIReadBusData(INTEL_LPC_BUSNUM, INTEL_LPC_DEVICENUM, INTEL_LPC_FUNCNUM, &ulPmBase, ACPI_E600_PMBASE_OFFSET,ACPI_PMBASE_LENGTH);

        // ACPI Base Address bit[15:4]
        ulPmBase &= ACPI_E600_BASE_MASK;
    } 
    else if ((usDeviceId == INTEL_LPC_DEVICEID_ICH8M)||(usDeviceId == INTEL_LPC_DEVICEID_NM10)||(usDeviceId == INTEL_LPC_DEVICEID_VLV2))
    {
        // Obtain ACPI IO Base Address from LPC PCI header 
        PCIReadBusData(INTEL_LPC_BUSNUM, INTEL_LPC_DEVICENUM, INTEL_LPC_FUNCNUM, &ulPmBase, ACPI_ICH8M_PMBASE_OFFSET, ACPI_PMBASE_LENGTH);

        // ACPI Base Address bit[15:7]
        ulPmBase &= ACPI_ICH8M_BASE_MASK;

        //Set SMI control to 0, in order to enable sleep state
        ulData = INPORT32((PULONG)(ulPmBase + ACPI_ICH8M_SMI_CTRL_OFFSET));
        ulData &= ACPI_ICH8M_SMI_CTRL; // 
        OUTPORT32((PULONG)(ulPmBase + ACPI_ICH8M_SMI_CTRL_OFFSET), ulData);
    } 
 
    //Trigger Power shut-down (S5) through ACPI Register
    ulData = INPORT32((PULONG)(ulPmBase + ACPI_PM1_CTRL_OFFSET));
    ulData |= ACPI_SLEEP_ENABLE; // 
    OUTPORT32((PULONG)(ulPmBase + ACPI_PM1_CTRL_OFFSET), ulData);

    
    ulData = INPORT32((PULONG)(ulPmBase + ACPI_PM1_CTRL_OFFSET));
    ulData |= ACPI_SET_S5; // 
    OUTPORT32((PULONG)(ulPmBase + ACPI_PM1_CTRL_OFFSET), ulData); 

    
}
