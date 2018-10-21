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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//------------------------------------------------------------------------------
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
#include <windows.h>
#include <oal.h>
#include <Timer.h>
#include "mpsupport.h"
#include "nkintr.h"
#include "acpica_itf.h"

// APIC ICR-LOW bit positions
#define ICR_VECTOR                                  0       // bit 0-7
#define ICR_DELIVERY_MODE                           8       // bit 8-10
#define ICR_DESTINATION_MODE                        11      // bit 11
#define ICR_DELIVERY_STATUS                         12      // bit 12
#define ICR_LEVEL                                   14      // bit 14
#define ICR_TRIGGER_MODE                            15      // bit 15
#define ICR_DESTINATION_SHORTHAND                   18

// APIC ICR-LOW delivery mode
#define ICR_DELIVERY_MODE_FIXED                     (0 << ICR_DELIVERY_MODE)
#define ICR_DELIVERY_MODE_LOW_PRIO                  (1 << ICR_DELIVERY_MODE)
#define ICR_DELIVERY_MODE_SMI                       (2 << ICR_DELIVERY_MODE)
#define ICR_DELIVERY_MODE_NMI                       (4 << ICR_DELIVERY_MODE)
#define ICR_DELIVERY_MODE_INIT                      (5 << ICR_DELIVERY_MODE)
#define ICR_DELIVERY_MODE_STARTUP                   (6 << ICR_DELIVERY_MODE)

// APIC ICR-LOW destination mode
#define ICR_DESTINATION_MODE_PHYSICAL               (0 << ICR_DELIVERY_MODE)
#define ICR_DESTINATION_MODE_LOGICAL                (1 << ICR_DELIVERY_MODE)

// APIC ICR-LOW delivery status
#define ICR_DELIVERY_STATUS_IDLE                    (0 << ICR_DELIVERY_STATUS)
#define ICR_DELIVERY_STATUS_PENDING                 (1 << ICR_DELIVERY_STATUS)

// APIC ICR-LOW level
#define ICR_LEVEL_DEASSERT                          (0 << ICR_LEVEL)
#define ICR_LEVEL_ASSERT                            (1 << ICR_LEVEL)

// trigger mode
#define ICR_TRIGGER_MODE_EDGE                       (0 << ICR_TRIGGER_MODE)
#define ICR_TRIGGER_MODE_LEVEL                      (1 << ICR_TRIGGER_MODE)

// APIC ICR-LOW destination shorthand
#define ICR_DESTINATION_SPECIFIC_CPU                (0 << ICR_DESTINATION_SHORTHAND)
#define ICR_DESTINATION_SELF                        (1 << ICR_DESTINATION_SHORTHAND)
#define ICR_DESTINATION_ALL_INCLUDE_SELF            (2 << ICR_DESTINATION_SHORTHAND)
#define ICR_DESTINATION_ALL_BUT_SELF                (3 << ICR_DESTINATION_SHORTHAND)

#define IPI_QUERY_PERF_COUNTER                      (OEM_FIRST_IPI_COMMAND + 1)
#define IPI_FLUSH_TLB                               (OEM_FIRST_IPI_COMMAND + 2)

#ifdef DEBUG
#define MP_MSG_ZONE 	TRUE
#else
#define MP_MSG_ZONE 	FALSE
#endif

extern DWORD MpStart_RM_Length;
extern DWORD MpStart_PM_Length;
extern DWORD PMAddr;
void MpStart_PM (void);
void MpStart_RM (void);
DWORD GetCR3 (void);
DWORD GetCR4 (void);

//
// fOalMpEnable is a FIXUPVAR, which is set when IMGMPENABLE is set. MP is NOT enabled by default.
//
const BOOL volatile fOalMpEnable = FALSE;
const BOOL volatile fOalNM10Fix = FALSE;

volatile LONG    OalNumCpus = 1;          // # of CPUs, default 1

const ULONGLONG tmpGDT[] = {
    0,                          // 0x00
    0x00CF9A000000FFFF,         // 0x08: Ring 0 code, Limit = 4G
    0x00CF92000000FFFF,         // 0x10: Ring 0 data, Limit = 4G
};


#define	MP_TYPE_CPU			0
#define	MP_TYPE_BUS			1
#define	MP_TYPE_IOAPIC		2
#define	MP_TYPE_INTSRC		3
#define	MP_TYPE_LINTSRC		4

#define MP_POLARITY_CONFORMS       0
#define MP_POLARITY_ACTIVE_HIGH    1
#define MP_POLARITY_RESERVED       2
#define MP_POLARITY_ACTIVE_LOW     3

#define MP_TRIGGER_CONFORMS        (0)
#define MP_TRIGGER_EDGE            (1<<2)
#define MP_TRIGGER_RESERVED        (2<<2)
#define MP_TRIGGER_LEVEL           (3<<2)

// Bit 7: IMCRP presence bit
#define IMCRP                   0x80

const FWORDPtr tmpGDTBase = { sizeof (tmpGDT)-1, (LPVOID) tmpGDT };

#define TGDT_R0_CODE            0x08
#define TGDT_R0_DATA            0x10
#define PHYS_TO_VIRT_OFFSET     0x80000000  // RAM offset to convert phys->virt or virt->phys
                                            // (can be found OEMAddressTable)

DWORD tmpSP[2] = { 0, TGDT_R0_CODE };

__declspec (align(4096)) DWORD tmpPD[1024];

#define ATTRIB_RW_NOCACHE  0xfb             // 00011111011b, r/w/uc/4M page

static volatile APIC_REGS *g_pApic;
static DWORD g_dwMasterCpuId;
static DWORD g_CR3;


DWORD OEMMpPerCPUInit (void);
void OEMIpiHandler (DWORD dwCommand, DWORD dwData);
BOOL OEMSendInterProcessorInterrupt (DWORD dwType, DWORD dwTarget);
BOOL OEMMpCpuPowerFunc (DWORD dwProcessor, BOOL fOnOff, DWORD dwHint);

#ifdef APIC
extern ApicGlobalSystemInterrupt g_gsi[MAX_GSI];
DWORD ggsicount;

extern ApicIrqRoutingMap  g_irm[MAX_IRM];      
DWORD girmcount;

MP_BUS BusList[MAX_BUS_ENTRY];
DWORD buscount;
MP_IOAPIC IoApicList[MAX_IOAPIC_ENTRY];
DWORD ioapiccount;
MP_INTSRC IntSrcList[MAX_IA_ENTRY];
DWORD intsrccount;
#endif

// NOTE: calling this function requires either in interrupt/idle context, or holding a
//       spinlock. Otherwise, thread switch can occur and endup running on another cpu.
BOOL IsMasterCPU ()
{
    return (g_pApic->ApicId >> 24) == g_dwMasterCpuId;
}

static DWORD IpiInterruptHandler (void)
{
    g_pApic->EOIReg = 0;
    return SYSINTR_IPI;
}

void
OEMIdleEx (LARGE_INTEGER *pliIdle)
{
    if (!IsMasterCPU () || ((int) (dwReschedTime - CurMSec) > 0)) {

        int nIdle = CurMSec;

        // enter idle
        _asm {
            sti
            hlt
            cli
        }

        nIdle = CurMSec - nIdle;

        if (nIdle > 0) {
            pliIdle->QuadPart += nIdle;
        }
    }
}

void MPCacheRangeFlush (LPVOID pAddr, DWORD dwLength, DWORD dwFlags)
{
    if ((dwFlags & CACHE_SYNC_FLUSH_TLB)  && !(dwFlags & CSF_CURR_CPU_ONLY)) {
        NKSendInterProcessorInterrupt (IPI_TYPE_ALL_BUT_SELF, 0, IPI_FLUSH_TLB, 0);
    }
    OEMCacheRangeFlush (pAddr, dwLength, dwFlags);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
ULONG
PCIReadBusData(
              IN ULONG BusNumber,
              IN ULONG DeviceNumber,
              IN ULONG FunctionNumber,
              OUT PVOID Buffer,
              IN ULONG Offset,
              IN ULONG Length
              );
ULONG
PCIWriteBusData(
              IN ULONG BusNumber,
              IN ULONG DeviceNumber,
              IN ULONG FunctionNumber,
              IN PVOID Buffer,
              IN ULONG Offset,
              IN ULONG Length
              );

void
OEMKdEnableTimer(
    IN BOOL fEnable
    );

//---------------------------------------------------------------------------------------------
// CPUSupportsAPIC - Check if the CPU has APIC support
//
// return FALSE if the CPU does not have APIC support
//
static BOOL CPUSupportsAPIC()
{
    const DWORD APIC_FLAG = 1<<9;

    BOOL HasAPIC = FALSE;
    DWORD FeatureFlags = 0, ProcID = 0;

    __asm {
        mov eax, 01h
        cpuid
        mov ProcID, eax
        mov FeatureFlags, edx
    }

    HasAPIC = FeatureFlags & APIC_FLAG;

    return HasAPIC;
}

#pragma warning(push)
#pragma warning(disable:4152) // nonstandard extension, function/data pointer conversion in expression
//---------------------------------------------------------------------------------------------
// OEMInitAllCPUs - start all CPUs running, all AP should jump to pfnContinue upon completing
//                  initialization.
//
// return TRUE if there are more than one CPUs, FALSE otherwise.
//
// Assumptions:
//      code is running on BSP (BootStrapProcessor), CR3 of BSP has page directory setup.
//---------------------------------------------------------------------------------------------
BOOL OEMStartAllCPUs (PLONG pnCpus, FARPROC pfnContinue)
{
    PMPStartParam pSipiPage = (PMPStartParam) SIPI_PAGE_PHYS_ADDR;
    DWORD  dwTmpCR3;

    OALMSG(OAL_FUNC, (L"+OEMStartAllCPUs\r\n"));

    // create a static mapping to API registers
    g_pApic = (volatile APIC_REGS *) NKCreateStaticMapping (APIC_REGS_PHYS_BASE>>8, sizeof (APIC_REGS));

    // setup a temporary page directory for booting up other cpus. the temp page directory
    // is exactly the same as the current one, except that it has an uncached identity
    // mapping for the 1st 4M.
    //
    //
    g_CR3 = GetCR3 ();
    memcpy (tmpPD, (LPVOID) (g_CR3 + PHYS_TO_VIRT_OFFSET), sizeof (tmpPD));
    tmpPD[0] = ATTRIB_RW_NOCACHE;   // map 0 -> 0 (4M), r/w, uncached
    dwTmpCR3 = (DWORD) &tmpPD[0] - PHYS_TO_VIRT_OFFSET;

    // switch to the temporary page directory we created.
    _asm {
        mov     eax, dwTmpCR3
        mov     cr3, eax
    }

    /*
    SIPI Page will be setup to the following

    +========================================================+ <- SIPI vecotr page (physical at (SIPI_PAGE_PHYS_ADDR))
    | Jmp instruction to start of MpStart_RM copy below      |
    +--------------------------------------------------------+
    |                                                        |
    | Remainder of MPStartParam fields                       |
    |                                                        |
    +--------------------------------------------------------+ <-- copy of MpStart_RM (target of the 1st jump instruction)
    |                                                        |
    | 16-bit real-mode code from MpStart_RM (mpstart.asm)    |
    |                                                        |
    | Enters 32-bit protected mode (no paging) and performs  |
    | a far jump to MpStart_PM.                              |
    |                                                        |
    +--------------------------------------------------------+ <-- copy of MpStart_PM
    |                                                        |
    | 32-bit protected-mode code from MpStart_PM.            |
    |                                                        |
    | Enable paging, jump to MpPagingEnabled.                |
    |                                                        |
    +--------------------------------------------------------+
    */


    // Copy Real-Mode Startup code to the startup page
    memcpy( pSipiPage, MpStart_RM, MpStart_RM_Length);

    // Copy Protected-Mode startup code to follow the startup page
    memcpy ((LPBYTE) pSipiPage + MpStart_RM_Length, MpStart_PM, MpStart_PM_Length);

    // fixup parameters
    pSipiPage->PageDirPhysAddr = dwTmpCR3;

    pSipiPage->Cr4Value        = GetCR4 ();

    // fix up the jump destination to enter protected mode (PMAddr, see mpstart.asm)
    *(LPDWORD) ((DWORD)pSipiPage + (DWORD)&PMAddr - (DWORD)MpStart_RM) = SIPI_PAGE_PHYS_ADDR + MpStart_RM_Length;

    // setup tmp GDT (map only ring-0)
    memcpy (&pSipiPage->tmpGDT[0], tmpGDT, sizeof (tmpGDT));

    // fixup value of tmpGDTBase to be used for lgdt
    pSipiPage->tmpGDTBase.Size = sizeof (pSipiPage->tmpGDT) - 1;
    pSipiPage->tmpGDTBase.Base = (LPVOID) (SIPI_PAGE_PHYS_ADDR + offsetof (MPStartParam, tmpGDT));

    // set continuation address on the temp stack
    tmpSP[0] = (DWORD) pfnContinue;

    g_dwMasterCpuId = (g_pApic->ApicId >> 24);

    // send INIT to all-but-self
    g_pApic->IcrLow = ICR_DESTINATION_ALL_BUT_SELF
                    | ICR_TRIGGER_MODE_EDGE
                    | ICR_LEVEL_ASSERT
                    | ICR_DELIVERY_MODE_INIT;

    // send SIPI to all-but-self
    g_pApic->IcrLow = ICR_DESTINATION_ALL_BUT_SELF
                    | ICR_TRIGGER_MODE_EDGE
                    | ICR_LEVEL_ASSERT
                    | ICR_DELIVERY_MODE_STARTUP
                    | SIPI_VECTOR;

    DEBUGMSG (1, (L"Send another SIPI\r\n"));

    IoDelay (8000); // enough delay to get CPUs ready

    // send SIPI to all-but-self
    g_pApic->IcrLow = ICR_DESTINATION_ALL_BUT_SELF
                    | ICR_TRIGGER_MODE_EDGE
                    | ICR_LEVEL_ASSERT
                    | ICR_DELIVERY_MODE_STARTUP
                    | SIPI_VECTOR;

    DEBUGMSG (1, (L"Done sending another SIPI\r\n"));


    // enough delay to allow other CPUs finish initialization
    IoDelay (2000);

    // switch back to the original CR3.
    _asm {
        mov     eax, g_CR3
        mov     cr3, eax
    }
    
    //Workaround for mobile CPU NM10 Platform
    if(fOalNM10Fix)
        OalNumCpus = 0x1;

    // setup IPI vector if more than 1 CPU
    if (OalNumCpus > 1) {
        g_pOemGlobal->pfnMpPerCPUInit   = OEMMpPerCPUInit;
        g_pOemGlobal->pfnIpiHandler     = OEMIpiHandler;
        g_pOemGlobal->pfnSendIpi        = OEMSendInterProcessorInterrupt;
        g_pOemGlobal->pfnMpCpuPowerFunc = OEMMpCpuPowerFunc;

        g_pOemGlobal->pfnIdleEx           = OEMIdleEx;
        g_pOemGlobal->pfnCacheRangeFlush  = MPCacheRangeFlush;
        g_pOemGlobal->pfnKdEnableTimer    = OEMKdEnableTimer;

        HookIpi (IPI_VECTOR, (FARPROC) IpiInterruptHandler);

    }
    else {
        g_pOemGlobal->fMPEnable         = FALSE;
    }
    // update the number of CPUs
    *pnCpus = OalNumCpus;


    OALMSG(OAL_FUNC, (L"-OEMStartAllCPUs, OalNumCpus = %d\r\n", OalNumCpus));

    return (OalNumCpus > 1);
}
#pragma warning(pop)
#define APIC_INT_ENABLE_BIT             0x100

//---------------------------------------------------------------------------------------------
// OEMMpPerCPUInit - 1st call from kernel after AP started, running on the AP. OEM can perform
//                   per-CPU intialization. Typically it's enabling inter-processor interrupts,
//                   and probably setup local timer/perf-counters (without enabling interrupts)
//
// return value - hardware CPU id.
//---------------------------------------------------------------------------------------------
DWORD OEMMpPerCPUInit (void)
{
    DWORD dwHardwareCpuID = (g_pApic->ApicId >> 24);    // bit 31-24 is the local APIC id

    // enable inter-CPU interrupts
    g_pApic->SpurIntVecReg = APIC_INT_ENABLE_BIT | 0xf; // 0xf - what's in the BSP apic, not sure if we need to
                                                        //       set it to the same value.
    return dwHardwareCpuID;
}

//---------------------------------------------------------------------------------------------
// OEMIpiHandler - This is to handle platform specific IPI, sent by calling
//                 g_pNKGlobal->pfnSendIpi from platform code. Normally nop.
//
//---------------------------------------------------------------------------------------------

typedef VOID (*PFNIPICCALLBACK)(DWORD);

void OEMIpiHandler (DWORD dwCommand, DWORD dwData)
{
    switch (dwCommand)
    {
    case IPI_QUERY_PERF_COUNTER:
        {
            LARGE_INTEGER *pCounter = (LARGE_INTEGER*)dwData;
            pCounter->QuadPart = GetTimeStampCounter();
        }
        break;
    case IPI_FLUSH_TLB:
        _asm {
            mov eax, cr3
            mov cr3, eax
        }
        break;
    case IPI_TEST_CALL_FUNCTION_PTR:
        {
            DWORD dwCurProcNum = GetCurrentProcessorNumber();
            PFNIPICCALLBACK  pfnIPICallBack =(PFNIPICCALLBACK)dwData;
            (pfnIPICallBack)(dwCurProcNum);
        }
        break;
    default:
        ;
    }
}

//---------------------------------------------------------------------------------------------
// OEMSendInterProcessorInterrupt - Send an Inter-Processor interrupt.
//
// dwType can be IPI_TYPE_ALL_BUT_SELF, IPI_TYPE_ALL_INCLUDE_SELF, and IPI_TYPE_SPECIFIC_CPU.
// when IPI_TYPE_SPECIFIC_CPU is specified, dwTarget will contain an hardware CPU id (returned
// from OEMMpPerCPUInit).
//
//---------------------------------------------------------------------------------------------
BOOL OEMSendInterProcessorInterrupt (DWORD dwType, DWORD dwTarget)
{
    DWORD dwIcrCommand = IPI_VECTOR
                       | ICR_DESTINATION_MODE_PHYSICAL
                       | ICR_LEVEL_ASSERT
                       | ICR_TRIGGER_MODE_EDGE
                       | ICR_DELIVERY_MODE_FIXED;

    switch (dwType) {
    case IPI_TYPE_ALL_BUT_SELF:
        dwIcrCommand |= ICR_DESTINATION_ALL_BUT_SELF;
        break;
    case IPI_TYPE_ALL_INCLUDE_SELF:
        dwIcrCommand |= ICR_DESTINATION_ALL_INCLUDE_SELF;
        break;
    case IPI_TYPE_SPECIFIC_CPU:
        dwIcrCommand |= ICR_DESTINATION_SPECIFIC_CPU;
        g_pApic->IcrHigh = (dwTarget << 24);
        break;
    default:
        DEBUGCHK (0);
        return FALSE;
    }

    g_pApic->IcrLow = dwIcrCommand;
    return TRUE;
}

//---------------------------------------------------------------------------------------------
// OEMMpCpuPowerFunc - Power on/off a CPU for power saving.
//---------------------------------------------------------------------------------------------
BOOL OEMMpCpuPowerFunc (DWORD dwProcessor, BOOL fOnOff, DWORD dwHint)
{
    UNREFERENCED_PARAMETER(dwHint);

    // not supported on x86, simulate power operations by always returning TRUE
    //Issue IPI to specific core to break from idle state
    return fOnOff ? OEMSendInterProcessorInterrupt(IPI_TYPE_SPECIFIC_CPU, dwProcessor) : TRUE;
}


void OALMpInit (void)
{
    OALMSG(MP_MSG_ZONE, (L"+OALMpInit\r\n"));

    if (fOalMpEnable)
    {
        // MP enabled image, query the CPU has APIC support before we enable
        if (CPUSupportsAPIC())
        {
            g_pOemGlobal->fMPEnable         = TRUE;
            g_pOemGlobal->pfnStartAllCpus   = OEMStartAllCPUs;
        }
        else
        {
            OALMSG(MP_MSG_ZONE, (L"MP Support was disabled as APIC was not detected.\r\n"));
        }
    }
    OALMSG(MP_MSG_ZONE, (L"-OALMpInit\r\n"));
}


//
// function to initialize MP after paging is enabled.
// NOTE: WE DO NOT HAVE A STACK AT THIS POINT. DO NOT CALL TO C FUNCTIONS IN THIS CODE.
//
// This function jumps to pfnNkContine to continue CPU intialization in kernel.
//
void __declspec(naked) MpPagingEnabled (void)
{
    _asm {
            wbinvd                              // clear out the cache

            // increment the number of CPUs (MUST use interlocked operation,
            // for all CPUs are updating the same variable here)
            mov     eax, 1
            lock xadd dword ptr [OalNumCpus], eax

            // reload GDT to use kernel virtual address instead of the identitiy mapped address
            lgdt    [tmpGDTBase]


            // reload the real CR3
            mov     eax, g_CR3
            mov     cr3, eax

            // jump to kernel continuation function to finish startup, with the new GDT
            // NOTE: tmpSP is shared between all secondary processors,
            //       *DO NOT PUSH ANYTHING ONTO IT*
            //
            mov     esp, offset [tmpSP]
            retf

            cli
            hlt
    }
}

//------------------------------------------------------------------------------
//
//  Function:  
//  MpScanFps
//
//  Description: 
//  Scans Physical Memory from 'start' for 'length' to find the Floating Point Structure
//
//  Parameters:
//  dwphysStart    [in]        Start address of the phsical memory location
//  dwLength       [in]        Length of memory to be scan
//
//  Returns:
//  NULL             - fail to find the MP FPS
//  virtual address  - The virtual address of the MP FPS
//
static const MP_FPS* MpScanFps(DWORD dwphysStart, DWORD dwLength)
{
    const char szSig[] = "_MP_";
    const char* psVirtStart = (const char *)(OALPAtoCA(dwphysStart));
    DWORD dwOffset = 0;
    // interate though the memory range looking for the _MP_ sig on 16byte boundries
    for (dwOffset = 0; dwOffset < dwLength; dwOffset += 16) 
    {
        if (0 == strncmp(psVirtStart + dwOffset, szSig, _countof(szSig) - 1)) 
		{
            OALMSG(MP_MSG_ZONE, (TEXT(" MP FPS found")));
            return (const MP_FPS*)(psVirtStart + dwOffset);
        }
    }

    return NULL;
}


//------------------------------------------------------------------------------
//
//  Function:  
//  MpVerifyFpsCheckSum
//
//  Description: 
//  Verify the MP FPS checksum
//
//  Parameters:
//  pbData         [in]        Start address of the phsical memory location
//  dwLength       [in]        Length of memory to be scan
//
//  Returns:
//  TRUE            - The Mp FPS is valid
//  FALSE           - Not a valid MP FPS
//
static BOOL MpVerifyFpsCheckSum (const BYTE* pbData, DWORD dwLength)
{
    BYTE bSum = 0;
    DWORD count = 0;

    if(NULL == pbData)
        return FALSE;

    for (count = 0; count < dwLength; ++count)
            bSum += *pbData++;
    
    OALMSG(bSum != 0, (TEXT(" Invalid checksum found on Mp Floating Point Struct")));

    return bSum == 0;
}


//------------------------------------------------------------------------------
//
//  Function:  
//  MPGetFpsBaseAddr
//
//  Description: 
//  Get the MP FPS base address
//
//  Parameters:
//  None
//
//  Returns:
//  NULL             - fail to find the MP FPS
//  virtual address  - The virtual address of the MP FPS
//
static const MP_FPS* MPGetFpsBaseAddr()
{
    DWORD dwLength = 0;
    // check first 1k
    const MP_FPS* pMpFps = MpScanFps(0, 0x400);
    if (!pMpFps)
	{
        // check the next memory range, as defined in the Intel MP spec
        pMpFps = MpScanFps(0xE0000, 0xfffff-0xe0000);
	}
    if (pMpFps)
    {
        // we found the Mp_Fps, make sure it's valid
        if(pMpFps->Length == 1)
        {
             //According to Intel MP Spec, Length 1 mean 16 BYTE data block
             dwLength = 16;
        }
        else
             return NULL;

        if (MpVerifyFpsCheckSum((const BYTE*)(pMpFps), dwLength))
            return pMpFps;
		else
            return NULL;
    }

    return NULL;
}

//------------------------------------------------------------------------------
//
//  Function:  
//  GetMPFpsInfo
// 
//  Description: 
//  To retrieve the information under Mp Floating Point Structure
//
//  Parameters:
//  Type      [in]        Type of information to be retrieved
//  pdwData   [out]       Contains the MP FPS information
//
//  Returns:
//  FALSE - indicate fail or un-supported feature
//  TRUE  - indicate suscess or supported feature
//
BOOL GetMPFpsInfo(MP_FPS_TYPE Type, DWORD *pdwData)
{
    BOOL fRetVal = FALSE;
    static BOOL fScanFps = TRUE;
    static const MP_FPS *pMpFpsAddr;

    if(NULL == pdwData)
        return FALSE;
    //Scan the MP FPS once only, then save it to database
    if( fScanFps )
    {
        pMpFpsAddr = MPGetFpsBaseAddr();
        if(NULL == pMpFpsAddr)
	        return FALSE;

        fScanFps = FALSE;
    }

    switch ( Type )
    {
        //return the MP version
        case MP_FPS_VERSION: 
            *pdwData = (DWORD) (pMpFpsAddr->Spec);
            fRetVal = TRUE;
            break;
        //return the MP Config Table Phy Addr
        case MP_CONFIG_TABLE_PHY_ADDR: 
            *pdwData = (DWORD) (pMpFpsAddr->PhysAddrPtr);
            fRetVal = TRUE;
            break;

        //To indicate the presense of IMCR bit
        case MP_IMCRP: 
			if( pMpFpsAddr->Feature2 & IMCRP )
                fRetVal = TRUE;
			else
				fRetVal = FALSE;
            break;

        default:
            OALMSG (MP_MSG_ZONE,(TEXT("GetSMPInfo: unknown code %x\n"),Type));
            fRetVal = FALSE;

    }

    return fRetVal;
}

#ifdef APIC
#ifndef ACPI_CA

// init the interrupte assignment entry
void MpGetIaEntry(MP_INTSRC *m)
{
	int i;

	OALMSG(MP_MSG_ZONE, (TEXT("Int: type %d, pol %d, trig %d, bus %02x, IRQ %02x, APIC ID %x, APIC INT %02x\n"),
		m->type, m->irqflag & 3, (m->irqflag >> 2) & 7, m->srcbus,
		m->srcbusirq, m->dstapic, m->dstirq));

	for (i = 0; i < MAX_IA_ENTRY; i++) {
		if (IntSrcList[i].type != MP_TYPE_INTSRC){
			break;
			}
	}

	if(i >= MAX_IA_ENTRY){
		return ;
		}

	memcpy(&IntSrcList[i], m, sizeof(*m));
	intsrccount++;
	OALMSG (MP_MSG_ZONE,(TEXT("MpGetIaEntry:intsrccount %d\n"),intsrccount));

}

static void MpGetIoapicEntry(MP_IOAPIC *m)
{

	int i;

	for (i = 0; i < MAX_IOAPIC_ENTRY; i++){
		if (IoApicList[i].type != MP_TYPE_IOAPIC){
			break;
			}
		}
	if(i >= MAX_IOAPIC_ENTRY){
		return;
		}
	
	memcpy(&IoApicList[i],m,sizeof(MP_IOAPIC));
	ioapiccount++;
}


int MpInitApicGlobalSystemInterruptInfo()
{
	int i = 0;
	for (i = 0; i < MAX_GSI; ++i)  {
        memset(&g_gsi[i], 0, sizeof(ApicGlobalSystemInterrupt));
        g_gsi[i].irq = 0xFF;
		g_gsi[i].ioApicID = -1;                         
		g_gsi[i].ioApicPAddr = -1;                      
		g_gsi[i].ioApicVAddr = (BYTE volatile *)-1;      
    	}

	// the first 16 ISA interrupts are active high and edge triggered
	for (i = 0; i < 16; ++i)	{
		g_gsi[i].irq = i;
		g_gsi[i].trigger = EDGE_TRIGGERED;
		g_gsi[i].polarity = ACTIVE_HIGH;
		g_gsi[i].ioApicID = -1;                    
		g_gsi[i].ioApicPAddr = -1;                      
		g_gsi[i].ioApicVAddr = (BYTE volatile *)-1;     
		g_gsi[i].ioApicInput = i;                        
	}

    return TRUE;
}


int	MpGetApicInterruptOverrideInfo(){

	int i;
	for (i=0;i<MAX_IA_ENTRY;i++) {

		MP_INTSRC *intsrc =&IntSrcList[i];
		
		if(intsrc->type==0){
			return TRUE;
			}

		if(intsrc->dstirq>15){
			return TRUE;
			}

		if(intsrc->srcbusirq !=intsrc->dstirq ||
			( intsrc->irqflag !=0 && (intsrc->irqflag != (g_gsi[intsrc->dstirq].polarity|g_gsi[intsrc->dstirq].trigger<<2)))){

			if (intsrc->dstirq != intsrc->srcbusirq){
				g_gsi[intsrc->dstirq].irq = 0xFF; 
			}

			g_gsi[intsrc->dstirq].irq = intsrc->dstirq;

			// polarity
			if ( ( intsrc->irqflag & MP_POLARITY_MASK) == MP_POLARITY_CONFORMS){
				if ( intsrc->srcbus == 0 ){
					g_gsi[intsrc->dstirq].polarity = ACTIVE_HIGH;
				}
			}
			else if ( (intsrc->irqflag & MP_POLARITY_MASK ) == MP_POLARITY_ACTIVE_HIGH ){
				g_gsi[intsrc->dstirq].polarity = ACTIVE_HIGH;
			}
			else if ( (intsrc->irqflag & MP_POLARITY_MASK) == MP_POLARITY_ACTIVE_LOW ){
				g_gsi[intsrc->dstirq].polarity = ACTIVE_LOW;
			}
			else if ( (intsrc->irqflag &MP_POLARITY_MASK) == MP_POLARITY_RESERVED ){
				// invalid (reserved)
			}

			// trigger mode
			if (( intsrc->irqflag & MP_TRIGGER_MASK) == MP_TRIGGER_CONFORMS ){
				if ( intsrc->srcbus == 0 /*ISA*/ ){
					g_gsi[intsrc->dstirq].trigger= EDGE_TRIGGERED;
				}
			}
			else if ( (intsrc->irqflag & MP_TRIGGER_MASK) == MP_TRIGGER_EDGE ){
				g_gsi[intsrc->dstirq].trigger = EDGE_TRIGGERED;
			}
			else if ( (intsrc->irqflag & MP_TRIGGER_MASK) == MP_TRIGGER_LEVEL )
				g_gsi[intsrc->dstirq].trigger = LEVEL_TRIGGERED;
		}else if ( (intsrc->irqflag & MP_TRIGGER_MASK) == MP_TRIGGER_RESERVED ){

			// invalid (reserved)
		}

	}

	return TRUE;
}

int 	MpGetIoApicInfo(){

 
	BYTE volatile * volatile        ioapicvaddr = NULL;
	DWORD                           ver = 0;
	BYTE                            maxRedirEntries = 0;
	BYTE                            input = 0;
	BYTE							i;
	
	for (i=0;i<ioapiccount;i++){

		MP_IOAPIC *pioapic = &IoApicList[i];
		if(pioapic->type==0){
			return TRUE;
			}


	    	OALMSG(MP_MSG_ZONE,(L"GetIoApicInfo() Id %d Address 0x%08X ", 
			                             pioapic->apicid,
										 pioapic->apicaddr));

		// access IOAPIC via an indirect addressing scheme using APIC_IND and APIC_DAT registers
		ioapicvaddr = (BYTE volatile *)AcpiOsMapMemory(pioapic->apicaddr, APIC_CHIP_SIZE);
		if (ioapicvaddr == NULL){
		    OALMSG (MP_MSG_ZONE, (L"Failed to memory map IOAPIC"));
			return FALSE;
		}

		*(ioapicvaddr + APIC_IND) = APIC_IND_VER;           // access to APIC_IND_VER
		ver = *(DWORD volatile *)(ioapicvaddr + APIC_DAT);  // read APIC_IND_VER data
		maxRedirEntries = (BYTE)((ver & 0x00FF0000) >> 16);

		for (input = 0; input <= maxRedirEntries; ++input){
		
			g_gsi[input].ioApicID =pioapic->apicid;
			g_gsi[input].ioApicInput = input;
			g_gsi[input].ioApicPAddr = pioapic->apicaddr;
			
			// for PCI interrupts ( > 15) set to LEVEL_TRIGGERED & ACTIVE_LOW
			if (input > 15)	{
				g_gsi[input].irq = input;
				g_gsi[input].polarity = ACTIVE_LOW;
				g_gsi[input].trigger = LEVEL_TRIGGERED;
			}
		}
		AcpiOsUnmapMemory((void*)ioapicvaddr, APIC_CHIP_SIZE); 
	}
	return TRUE;
}

BOOL MpGlobalSystemInterruptInfo()
{
    
	MpInitApicGlobalSystemInterruptInfo();
	MpGetApicInterruptOverrideInfo();
	MpGetIoApicInfo();

	return TRUE;
}

void x86InitMPTable(){

	static const MP_FPS *pMpFpsAddr;
	static const MP_TABLE *pMpTable;
	DWORD  length=0;
	UCHAR *mpentry=NULL;
	ULONG mptablesize=0;

	OALMSG (MP_MSG_ZONE, (TEXT(" -----------+++ x86InitMP +++---------------")));

	ggsicount=0;
	girmcount=0;
	buscount=0;
	ioapiccount=0;
	intsrccount=0;
	memset(IoApicList,0,sizeof(MP_IOAPIC));

	//Get the mptable floating point struct address.
	pMpFpsAddr = MPGetFpsBaseAddr();
	if(NULL == pMpFpsAddr){
	    return ;
		}

	OALMSG (MP_MSG_ZONE, (TEXT("spec version:%d"),pMpFpsAddr->Spec));
	OALMSG (MP_MSG_ZONE, (TEXT("mptable header addr:0x%x"),pMpFpsAddr->PhysAddrPtr));

	//Get the mptable address
	pMpTable = (MP_TABLE *)AcpiOsMapMemory(pMpFpsAddr->PhysAddrPtr,sizeof(MP_TABLE));

	//Get the mptable length
	mptablesize = pMpTable->Length;
	AcpiOsUnmapMemory(pMpTable, sizeof(MP_TABLE));
	OALMSG (MP_MSG_ZONE, (TEXT("mptable mptablesize:%d"),mptablesize));

	//Get the whole mptable
	pMpTable = (MP_TABLE *)AcpiOsMapMemory(pMpFpsAddr->PhysAddrPtr,mptablesize);
	length = sizeof(MP_TABLE);
	mpentry =((UCHAR *)pMpTable) + length;


	while (length < pMpTable->Length) {
		switch (*mpentry) {
		case MP_TYPE_CPU:
			mpentry += sizeof(MP_CPU);
			length += sizeof(MP_CPU);
			break;
		case MP_TYPE_BUS:
			mpentry += sizeof(MP_BUS);
			length += sizeof(MP_BUS);
			break;
		case MP_TYPE_IOAPIC:
			OALMSG (MP_MSG_ZONE, (TEXT("mptable ENTRY_IOAPIC")));
			MpGetIoapicEntry((MP_IOAPIC *)mpentry);
			mpentry += sizeof(MP_IOAPIC);
			length += sizeof(MP_IOAPIC);
			break;
		case MP_TYPE_INTSRC:
			OALMSG (MP_MSG_ZONE, (TEXT("mptable ENTRY_INTSRC")));
			MpGetIaEntry(( MP_INTSRC *)mpentry);
			mpentry += sizeof(MP_INTSRC);
			length += sizeof(MP_INTSRC);
			break;
		case MP_TYPE_LINTSRC:
			mpentry += sizeof(MP_LINTSRC);
			length += sizeof(MP_LINTSRC);
			break;
		default:
			length = pMpTable->Length;
			break;
		}

	}
	AcpiOsUnmapMemory(pMpTable, mptablesize);
	OALMSG (MP_MSG_ZONE, (TEXT(" -----------+++ end x86InitMP +++---------------")));
}

void x86UnInitMPTable(){
}
#endif //acpica
#endif //APIC

