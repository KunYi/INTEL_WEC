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
// Copyright (c) 2002-2012 Intel Corporation All Rights Reserved. 
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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

Abstract:  
   This file implements the NK kernel interfaces for firmware interrupt
  support on the CEPC.
 
Functions:


Notes: 

--*/

#include <windows.h>
#include <oal.h>
#include <nkintr.h>
#include <x86boot.h>

#include "pehdr.h"
#include "romldr.h"
#include "romxip.h"

#ifdef BSP_UEFI
#include "acpitable.h"
#endif

#ifdef BSP_POWER_MANAGEMENT
#include "ioctl_atom.h"
#endif 

#define NOT_FIXEDUP ((DWORD*)-1)

//------------------------------------------------------------------------------
//
// dwOEMDrWatsonSize
//
// Global variable which specify DrWatson buffer size. It can be fixed
// in config.bib via FIXUPVAR.
//
#define DR_WATSON_SIZE_NOT_FIXEDUP  ((DWORD)-1)
DWORD dwOEMDrWatsonSize = DR_WATSON_SIZE_NOT_FIXEDUP;

void  PCIInitBusInfo (void);
DWORD OEMPowerManagerInit(void);
void  InitClock(void);
void  x86InitMemory (void);
void  x86InitPICs(void);
void  x86RebootInit(void);
void  RTCPostInit();
void  QPCPostInit();

extern LPCWSTR g_pPlatformManufacturer;
extern LPCWSTR g_pPlatformName;
static BOOL g_fPostInit;
void OALMpInit (void);

extern ULONG
PCIWriteBusData(
              IN ULONG BusNumber,
              IN ULONG DeviceNumber,
              IN ULONG FunctionNumber,
              IN PVOID Buffer,
              IN ULONG Offset,
              IN ULONG Length
              );
extern ULONG
PCIReadBusData(
              IN ULONG BusNumber,
              IN ULONG DeviceNumber,
              IN ULONG FunctionNumber,
              IN PVOID Buffer,
              IN ULONG Offset,
              IN ULONG Length
              );

void disLegacyUsbSupport(void);
// LPC interface id
#define INTEL_LPC_DEVICEID_ICH2   0x2440
#define INTEL_LPC_DEVICEID_ICH2M  0x244C
#define INTEL_LPC_DEVICEID_ICH4   0x24C0
#define INTEL_LPC_DEVICEID_ICH4M  0x24CC
#define INTEL_LPC_DEVICEID_ICH6ME 0x2641
#define INTEL_LPC_DEVICEID_ICH7   0x27B8
#define INTEL_LPC_DEVICEID_ICH7M  0x27B9
#define INTEL_LPC_DEVICEID_ICH7MBD  0x27BD
#define INTEL_LPC_DEVICEID_ICH8   0x2810
#define INTEL_LPC_DEVICEID_ICH8M  0x2815
#define INTEL_LPC_DEVICEID_ICH8DH 0x2812
#define INTEL_LPC_DEVICEID_ICH8DO 0x2814
#define INTEL_LPC_DEVICEID_ICH8ME 0x2811

#define INTEL_LPC_DEVICEID_US15W  0x8119
#define INTEL_LPC_DEVICEID_ICH7U  0x27B9

#define INTEL_LPC_DEVICEID_ICH9DH 0x2912
#define INTEL_LPC_DEVICEID_ICH9DO 0x2914
#define INTEL_LPC_DEVICEID_ICH9R  0x2916
#define INTEL_LPC_DEVICEID_ICH9ME 0x2917
#define INTEL_LPC_DEVICEID_ICH9   0x2918
#define INTEL_LPC_DEVICEID_ICH9M  0x2919

#define INTEL_LPC_DEVICEID_ICH10  0x3A10
#define INTEL_LPC_DEVICEID_ICH10D 0x3A14

#define INTEL_LPC_DEVICEID_NM10   0x27bc
#ifdef BSP_UEFI
extern IRQ_TO_PIRQ_TBL     g_Irq2PirqTbl[MAX_PIRQ_NUM];
extern IRQ_INFO_TBL        g_IrqAssignedInfoTbl[MAX_PIRQ_NUM];
extern DWORD g_dwInfoInx;
#endif
//------------------------------------------------------------------------------
//
// x86InitRomChain
//
// Initialize the rom chain
//
static void x86InitRomChain()
{
    // Added for MultiXIP stuff
    static      ROMChain_t  s_pNextRom[MAX_ROM] = {0};
    DWORD       dwRomCount = 0;
    DWORD       dwChainCount = 0;
    DWORD *     pdwCurXIP;
    DWORD       dwNumXIPs;
    PXIPCHAIN_ENTRY pChainEntry = NULL;
    static DWORD *pdwXIPLoc = NOT_FIXEDUP;

#ifdef DEBUG
    TCHAR       szXIPName[XIP_NAMELEN];
    int         i;
#endif

#ifdef DEBUG
    NKOutputDebugString (TEXT("Looking for rom chain\n"));
#endif

    if(pdwXIPLoc == NOT_FIXEDUP)
    {
#ifdef DEBUG
        NKOutputDebugString (TEXT("Rom chain NOT found\n"));
#endif
        return;  // no chain or not fixed up properly
    }

#ifdef DEBUG
    NKOutputDebugString (TEXT("Rom chain found\n"));
#endif


    // set the top bit to mark it as a virtual address
    pdwCurXIP = (DWORD*)(((DWORD)pdwXIPLoc) | 0x80000000);

    // first DWORD is number of XIPs
    dwNumXIPs = (*pdwCurXIP);

    if(dwNumXIPs > MAX_ROM)
    {
        NKOutputDebugString (TEXT("ERROR: Number of XIPs exceeds MAX\n"));
        return;
    }

    pChainEntry = (PXIPCHAIN_ENTRY)(pdwCurXIP + 1);

    while(dwChainCount < dwNumXIPs)
    {
        if ((pChainEntry->usFlags & ROMXIP_OK_TO_LOAD) &&  // flags indicates valid XIP
            *(const DWORD*)(((DWORD)(pChainEntry->pvAddr)) + ROM_SIGNATURE_OFFSET) == ROM_SIGNATURE)
        {
            s_pNextRom[dwRomCount].pTOC = *(ROMHDR **)(((DWORD)(pChainEntry->pvAddr)) + ROM_SIGNATURE_OFFSET + 4);
            s_pNextRom[dwRomCount].pNext = NULL;

#ifdef DEBUG
            NKOutputDebugString ( _T("XIP found: ") );

            for (i = 0; (i< (XIP_NAMELEN-1)) && (pChainEntry->szName[i] != '\0'); ++i)
            {
                szXIPName[i] = (TCHAR)pChainEntry->szName[i];
            }
            szXIPName[i] = TEXT('\0');
            NKOutputDebugString ( szXIPName );

            NKOutputDebugString ( _T("\n") );
#endif

            if (dwRomCount != 0)
            {
                s_pNextRom[dwRomCount-1].pNext = &s_pNextRom[dwRomCount];
            }
            else
            {
                OEMRomChain = s_pNextRom;
            }
            dwRomCount++;
        }
        else
        {
            NKOutputDebugString ( _T("Invalid XIP found\n") );
        }

        ++pChainEntry;
        dwChainCount++;
    }

#ifdef DEBUG
    {
        ROMChain_t         *pchain = OEMRomChain;

        NKOutputDebugString ( _T("chain contents...\n") );
        while(pchain){
            NKOutputDebugString ( _T("found item\n") );
            pchain = pchain->pNext;
        }
    }
#endif
    
}

#ifdef BSP_UEFI

void x86SetLPCRegister()
{
    DWORD data1, data2;
    int i;
    
    data1 = 0;
    data2 = 0;
    
    for(i = 0; i < 4; i++)
    {
        data1 |= (g_Irq2PirqTbl[i].bIRQX << (i*8));
    }
        
#if 0
    // PIRQE-H is not routed to 8259, so we can not set value,
    // if we to set value, the PS2 devic will work failed.
    for(i = 4; i < 8; i++)
        data2 |= g_Irq2PirqTbl[i].bIRQX << i*8;
           
    PCIWriteBusData(0,31,0,&data2,0x68,4);
    PCIReadBusData(0,31,0,&data2,0x68,4);
#else
#ifdef ENABLE_NM10
    //PIRQE-H routing, enable IRQ5 for NM10 Ethernet
    data2 = 0x80808005;
#else
	data2 = 0x80808080;
#endif
    PCIWriteBusData(0,31,0,&data2,0x68,4);
    PCIReadBusData(0,31,0,&data2,0x68,4);
#endif
    
    PCIWriteBusData(0,31,0,&data1,0x60,4);
    PCIReadBusData (0,31,0,&data1,0x60,4);
    OALMSG(OAL_LOG_INFO,(TEXT("INTA-D Routing @ 60 = %x\r\n"),data1));
    OALMSG(OAL_LOG_INFO,(TEXT("INTE-H Routing @ 68 = %x\r\n"),data2)); 
           
    //Setting up the 8259 pair for PCI level triggering
    data1 = (UCHAR) READ_PORT_UCHAR ((PUCHAR) 0x4D0);
    data2 = READ_PORT_UCHAR ((PUCHAR) 0x4D1);
    
    for(i = 0; i < (int) g_dwInfoInx; i ++)
    {
        OALMSG(OAL_LOG_INFO, (L"g_IrqAssignedInfoTbl[%d], IRQ %d\r\n", i, g_IrqAssignedInfoTbl[i].bIRQX));

        if( (g_IrqAssignedInfoTbl[i].bIRQX >= 3) &&
            (8 > g_IrqAssignedInfoTbl[i].bIRQX) )
        {
            data1 |= (1 << g_IrqAssignedInfoTbl[i].bIRQX);
        }
        else if( (g_IrqAssignedInfoTbl[i].bIRQX >= 9) && 
            (16 > g_IrqAssignedInfoTbl[i].bIRQX)) 
        {
            data2 |= (1 << (g_IrqAssignedInfoTbl[i].bIRQX - 8));
        }
    }
    
    WRITE_PORT_UCHAR ((PUCHAR) 0x4D0, (UCHAR) data1);
    data1 = (UCHAR) READ_PORT_UCHAR ((PUCHAR) 0x4D0);
    WRITE_PORT_UCHAR ((PUCHAR) 0x4D1, (UCHAR) data2);
    data2 = (UCHAR) READ_PORT_UCHAR ((PUCHAR) 0x4D1);
    OALMSG(OAL_LOG_INFO,(TEXT("PIC intr trigger master[%x] & slave[%x]\r\n"), data1, data2 ));
}
#endif // BSP_UEFI

//------------------------------------------------------------------------------
//
// OEMInit
//
// OEMInit is called by the kernel after it has performed minimal
// initialization. Interrupts are disabled and the kernel is not
// ready to handle exceptions. The only kernel service available
// to this function is HookInterrupt. This should be used to
// install ISR's for all the hardware interrupts to be handled by
// the firmware. Note that ISR's must be installed for any interrupt
// that is to be routed to a device driver - otherwise the
// InterruptInitialize call from the driver will fail.
//
void OEMInit()
{   
    // Set up the debug zones according to the fix-up variable initialOALLogZones
    OALLogSetZones(initialOALLogZones);

    OALMSG(OAL_FUNC, (L"+OEMInit\r\n"));

    // initialize interrupts
    OALIntrInit ();

    // initialize PIC
    x86InitPICs();

#ifdef BSP_UEFI
    // Find ACPI table for proliferate PCI routing later
    x86InitACPITables();
    x86ParseDsdtTables();
#endif

    // Initialize PCI bus information
    PCIInitBusInfo ();

    // starts KITL (will be a no-op if KITLDLL doesn't exist)
    KITLIoctl (IOCTL_KITL_STARTUP, NULL, 0, NULL, 0, NULL);
    
#ifdef DEBUG

    // Instead of calling OEMWriteDebugString directly, call through exported
    // function pointer.  This will allow these messages to be seen if debug
    // message output is redirected to Ethernet or the parallel port.  Otherwise,
    // lpWriteDebugStringFunc == OEMWriteDebugString.    
    NKOutputDebugString (TEXT("ATOM Firmware Init\r\n"));

#endif

    // sets the global platform manufacturer name and platform name
    g_oalIoCtlPlatformManufacturer = g_pPlatformManufacturer;
    g_oalIoCtlPlatformName = g_pPlatformName;

    OEMPowerManagerInit();
    
    // initialize clock
    InitClock();

    // initialize memory (detect extra ram, MTRR/PAT etc.)
    x86InitMemory ();

    // Reserve 128kB memory for Watson Dumps
    dwNKDrWatsonSize = 0;
    if (dwOEMDrWatsonSize != DR_WATSON_SIZE_NOT_FIXEDUP) 
    {
        dwNKDrWatsonSize = dwOEMDrWatsonSize;
    }

    x86RebootInit();
    x86InitRomChain();

    // initialize UHCI/EHCI controller to overcome Legacy USB support
    disLegacyUsbSupport();
    
#ifdef BSP_UEFI
    x86SetLPCRegister();
#endif

#ifdef DEBUG
    {
        DWORD data;
        PCIReadBusData(0,31,0,&data,0x60,4);
        DEBUGMSG(1,(TEXT("INTA-D Routing @ 60 = %x\r\n"),data));
        PCIReadBusData(0,31,0,&data,0x68,4);
        DEBUGMSG(1,(TEXT("INTE-H Routing @ 68 = %x\r\n"),data));
        DEBUGMSG(1,(TEXT("4D0-1 1:%x 0:%x\r\n"),READ_PORT_UCHAR((PUCHAR)0x4d1),READ_PORT_UCHAR((PUCHAR)0x4d0) ));
    }
#endif // DEBUG

    // Override default (100 milliseconds) to 10 ms.
    g_pOemGlobal->dwDefaultThreadQuantum = 10; // 10 milliseconds

    OALMpInit ();

#ifdef DEBUG
    NKOutputDebugString (TEXT("Firmware Init Done.\r\n"));
#endif

    OALMSG(OAL_FUNC, (L"-OEMInit\r\n"));
}

//------------------------------------------------------------------------------
//
// IsAfterPostInit
//
// TRUE once the kernel has performed a more full featured init. Critical Sections can now be used
//
BOOL IsAfterPostInit() 
{ 
    return g_fPostInit; 
}

//------------------------------------------------------------------------------
//
// x86IoCtlPostInit
//
// Called once the kernel has performed a more full featured init. Critical Sections can now be used
//
BOOL x86IoCtlPostInit(
                      UINT32 code, 
                      __in_bcount(nInBufSize) const void * lpInBuf, 
                      UINT32 nInBufSize, 
                      __out_bcount(nOutBufSize) const void * lpOutBuf, 
                      UINT32 nOutBufSize, 
                      __out UINT32 * lpBytesReturned
                      ) 
{
    UNREFERENCED_PARAMETER(code);
    UNREFERENCED_PARAMETER(lpInBuf);
    UNREFERENCED_PARAMETER(nInBufSize);
    UNREFERENCED_PARAMETER(lpOutBuf);
    UNREFERENCED_PARAMETER(nOutBufSize);

    if (lpBytesReturned) 
    {
        *lpBytesReturned = 0;
    }

    RTCPostInit();
    QPCPostInit();

    g_fPostInit = TRUE;

    return TRUE;
}


static void millisecondDelay(int iDelay)
{
    ULONG dwCount = (ULONG)(iDelay * 1500);
    ULONG dwIndex;
    UCHAR temp;

    for (dwIndex = 0; dwIndex < dwCount; dwIndex++)
    {
        temp = INPORT8((PUCHAR)0x80);
    }
}

#ifdef BSP_POWER_MANAGEMENT
USHORT g_usDeviceId; //global device id to be used in oal
#endif 

void disLegacyUsbSupport(void)
{
    USHORT deviceId;
    ULONG  pmBase;
    ULONG  data;
    ULONG  usb1Addr, usb2Addr, usb3Addr;
    ULONG  usb4Addr, usb5Addr, usb6Addr;

    /* Begin Disable NATIVE PCI of Cantiga SATA controller */
    PCIReadBusData(0x0, 0x1f, 0x2, &deviceId,0x02, 2); //should be 2928 //
    
    OALMSG(OAL_LOG_INFO,(TEXT("IDE deviceId = 0x%x\r\n"),deviceId));
    if( deviceId == 0x2928){
        data = 0x8A;
        PCIWriteBusData(0x0, 0x1f, 0x2, &(PVOID)data, 0x9, 1);
    }
    /* End   Disble NATIVE PCI of Cantiga SATA controller */
    

    // Access to Device ID for LPC device on Bus-0, Dev-31, Func-0 
    // Refer to Chipset Spec Update for specific values
    PCIReadBusData(0x0, 0x1f, 0x0, &deviceId, 0x02, 2);

    
#ifdef BSP_POWER_MANAGEMENT
    g_usDeviceId = deviceId;
#endif

    // Obtain the ACPI IO Base Address from LPC's PMBase register at offset-0x40
    PCIReadBusData(0x0, 0x1f, 0x0, &pmBase, 0x40, 4);
    pmBase &= 0xffffff80;

    OALMSG(OAL_LOG_INFO,(TEXT("disLegacyUsbSupport LPC deviceId=0x%x PMBase=0x%x \r\n"),deviceId, pmBase));

    if(deviceId == INTEL_LPC_DEVICEID_ICH6ME)
    {
        // 915
        data = 0x01;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6B, 1);
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6F, 1);

        data = 0x0000;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x70, 2);

        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfff9fff7;
        OUTPORT32((PULONG)(pmBase + 0x30), data);


        // Get USB1/2/3 base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);

        millisecondDelay(1);

        // Disable SMI generation by clearing USB controller's register C0 bit 4 (USBSMIEN), enable
        // interrupt generation by setting the USB controller's register C0 bit 13 (USBPIRQEN), and
        // disable SMI sources.
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC1, 1);
    }    
    else if((deviceId == INTEL_LPC_DEVICEID_ICH7U) ||
            (deviceId == INTEL_LPC_DEVICEID_ICH7)||
            (deviceId == INTEL_LPC_DEVICEID_ICH7M))
    {
#ifdef BSP_POWER_MANAGEMENT
        data = 0x01;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6B, 1);
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6F, 1);
        data = 0x0000;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x70, 2);
#endif        
        // 945...
        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfffffff7; // disable LEGACY_USB_EN
        OUTPORT32((PULONG)(pmBase + 0x30), data);
#ifdef BSP_POWER_MANAGEMENT
        // Get USB0/1/2/3 base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x3, &usb4Addr, 0x20, 4);
        usb4Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);

        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);

        data = INPORT16((PUSHORT)(usb4Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb4Addr + 0), data);
#endif
    }
    else if((deviceId == INTEL_LPC_DEVICEID_ICH8M))
    {
        OALMSG(OAL_LOG_INFO,(TEXT("+ disLegacyUsbSupport for ICH8M  \r\n")));

        // For Intel Atom Processor N450, D410 & D510 with the ICH 82801HM Platform         
        // The following logics are required to ensure WEC7 OS booted on such platform.
        //

        // ICH8M has 2x EHCI controllers: Dev-29/Func-7  & Dev-26/Func-7
        // -----------------
        // For Dev-29/Func-7
        // -----------------
        // 0x68 - LEG_EXT_CAP register[7:0] = capability ID (RO) 
        // data = 0x01;
        // PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6B, 1);

        // 0x6F - LEG_EXT_CS register[31:24]
        // Write-1-to-Clear [31:29] 
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6F, 1);

        // Disable all SMI sources 
        // 0x6C-0x6D - LEG_EXT_CS register[0:15]
        // 0x70-0x71 - SPECIAL_SMI register[0:15]
        data = 0x0000;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x70, 2);

        // -----------------
        // For Dev-26/Func-7
        // -----------------
        // 0x68 - LEG_EXT_CAP register[7:0] = capability ID (RO) 
        // data = 0x01;
        // PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6B, 1);

        // 0x6F - LEG_EXT_CS register[31:24]
        // Write-1-to-Clear [31:29] 
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6F, 1);

        // Disable all SMI sources 
        // 0x6C-0x6D - LEG_EXT_CS register[0:15]
        // 0x70-0x71 - SPECIAL_SMI register[0:15]
        data = 0x0000;
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x70, 2);


         // Disable GBL_SMI_EN bit of SMI_EN register
        // Note: This is working fine with LNP Fab-C
        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfffffff7; // disable LEGACY_USB_EN
        OUTPORT32((PULONG)(pmBase + 0x30), data);


        // Get Dev-29:F0-F2 base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);

        // Get Dev-26:F0-F1 base addresses
        PCIReadBusData(0x0, 0x1a, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);

        millisecondDelay(1);

        // Clear the USB_LEGKEY for Dev-29:F0-F2
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC1, 1);

        // Clear the USB_LEGKEY for Dev-26:F0-F1
        data = 0x00;
        PCIWriteBusData(0x0, 0x1a, 0x0, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1a, 0x0, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1a, 0x1, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1a, 0x1, &(PVOID)data, 0xC1, 1);

        millisecondDelay(1);

        OALMSG(OAL_LOG_INFO,(TEXT("- disLegacyUsbSupport for ICH8M  \r\n")));

    }
    else if((deviceId == INTEL_LPC_DEVICEID_NM10))
    {
        RETAILMSG(1,(TEXT("+ disLegacyUsbSupport for NM10 \r\n")));

        // For Intel Atom Processor with the NM10 Platform         
        // The following logics are required to ensure WEC7 OS booted on such platform.
        //

        // NM10 has 1x EHCI controllers: Dev-29/Func-7
        // -----------------
        // For Dev-29/Func-7
        // -----------------
        // 0x68 - LEG_EXT_CAP register[7:0] = capability ID (RO) 
        // data = 0x01;
        // PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6B, 1);

        // 0x6F - LEG_EXT_CS register[31:24]
        // Write-1-to-Clear [31:29] 
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6F, 1);

        // Disable all SMI sources 
        // 0x6C-0x6D - LEG_EXT_CS register[0:15]
        // 0x70-0x71 - SPECIAL_SMI register[0:15]
        data = 0x0000;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x70, 2);


        // Disable GBL_SMI_EN bit of SMI_EN register

        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfffffff7; // disable LEGACY_USB_EN
        OUTPORT32((PULONG)(pmBase + 0x30), data);


        // Get Dev-29:F0-F2 base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;
		PCIReadBusData(0x0, 0x1d, 0x3, &usb4Addr, 0x20, 4);
        usb4Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);
        data = INPORT16((PUSHORT)(usb4Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb4Addr + 0), data);

        millisecondDelay(1);

        // Clear the USB_LEGKEY for Dev-29:F0-F2
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x3, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x3, &(PVOID)data, 0xC1, 1);

        millisecondDelay(1);

        RETAILMSG(1,(TEXT("- disLegacyUsbSupport for NM10  \r\n")));

    }
    else if(deviceId == INTEL_LPC_DEVICEID_US15W)
    {
        // Get USB1/2/3 base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
    }
    else if(deviceId == INTEL_LPC_DEVICEID_ICH9ME)
    {
        data = 0x01;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6B, 1);
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6F, 1);

        data = 0x0000;
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1d, 0x7, &(PVOID)data, 0x70, 2);

        data = 0x01;
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6B, 1);
        data = 0xe0;
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6F, 1);

        data = 0x0000;
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x6C, 2);
        PCIWriteBusData(0x0, 0x1a, 0x7, &(PVOID)data, 0x70, 2);
#ifdef BSP_POWER_MANAGEMENT
        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfffffff7; // disable LEGACY_USB_EN
        OUTPORT32((PULONG)(pmBase + 0x30), data);
#endif
        // Get USB UHCI base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x3, &usb4Addr, 0x20, 4);
        usb4Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x0, &usb5Addr, 0x20, 4);
        usb5Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x1, &usb6Addr, 0x20, 4);
        usb6Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);
        data = INPORT16((PUSHORT)(usb4Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb4Addr + 0), data);

        data = INPORT16((PUSHORT)(usb5Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb5Addr + 0), data);
        data = INPORT16((PUSHORT)(usb6Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb6Addr + 0), data);
    }
    else if((deviceId == INTEL_LPC_DEVICEID_ICH10)||
            (deviceId == INTEL_LPC_DEVICEID_ICH10D))
    {
        //G45
        data = INPORT32((PULONG)(pmBase + 0x30));
        data &= 0xfffffff7; // disable LEGACY_USB_EN
        OUTPORT32((PULONG)(pmBase + 0x30), data);

        // Get USB UHCI base addresses
        PCIReadBusData(0x0, 0x1d, 0x0, &usb1Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x1, &usb2Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1d, 0x2, &usb3Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x0, &usb4Addr, 0x20, 4);
        usb1Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x1, &usb5Addr, 0x20, 4);
        usb2Addr &= 0xffffffe0;
        PCIReadBusData(0x0, 0x1a, 0x2, &usb6Addr, 0x20, 4);
        usb3Addr &= 0xffffffe0;

        //  Clear the run/stop bit (bit 0) and cf flag (bit 6) in each USB CMD register.
        data = INPORT16((PUSHORT)(usb1Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb1Addr + 0), data);
        data = INPORT16((PUSHORT)(usb2Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb2Addr + 0), data);
        data = INPORT16((PUSHORT)(usb3Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb3Addr + 0), data);
        data = INPORT16((PUSHORT)(usb4Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb4Addr + 0), data);
        data = INPORT16((PUSHORT)(usb5Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb5Addr + 0), data);
        data = INPORT16((PUSHORT)(usb6Addr + 0));
        data &= 0xffbe;
        OUTPORT16((PUSHORT)(usb6Addr + 0), data);

        millisecondDelay(1);

        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x0, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x1, &(PVOID)data, 0xC1, 1);
        data = 0x00;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC0, 1);
        data = 0xff;
        PCIWriteBusData(0x0, 0x1d, 0x2, &(PVOID)data, 0xC1, 1);
    }
    else 
    {
        // do nothing
    }
}