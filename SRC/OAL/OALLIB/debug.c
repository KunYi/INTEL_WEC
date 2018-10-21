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
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

Abstract:  

Functions:


Notes: 

--*/
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

#undef WINCEMACRO

#include <windows.h>
#include <nkintr.h>
#include <pc_smp.h>
#include <wdm.h>
#include <bootarg.h>
#include <x86boot.h>
#include <oal.h>
#include <pci.h>

enum LS 
{
    LS_TSR_EMPTY       = 0x40,
    LS_THR_EMPTY       = 0x20,
    LS_RX_BREAK        = 0x10,
    LS_RX_FRAMING_ERR  = 0x08,
    LS_RX_PARITY_ERR   = 0x04,
    LS_RX_OVERRUN      = 0x02,
    LS_RX_DATA_READY   = 0x01,

    LS_RX_ERRORS       = ( LS_RX_FRAMING_ERR | LS_RX_PARITY_ERR | LS_RX_OVERRUN )
};

//
// Global variables.
//

PUCHAR IoPortBase;

static X86BootInfo x86Info;
PX86BootInfo g_pX86Info = &x86Info;
UINT8 g_x86uuid[16];

extern LPCSTR  g_oalDeviceNameRoot;
//   14400 = 8
//   16457 = 7 +/-
//   19200 = 6
//   23040 = 5
//   28800 = 4
//   38400 = 3
//   57600 = 2
//  115200 = 1



//-----------------------------------------------------------------------------
//
//  OEMWriteDebugLED
//
//
//
void OEMWriteDebugLED(
                      WORD wIndex, 
                      DWORD dwPattern
                      )
{
    UNREFERENCED_PARAMETER(wIndex);
    UNREFERENCED_PARAMETER(dwPattern);
    // no LED
}
#if 0
static X86KITLINFO kitlinfo = {
    &x86Info,                           // PX86BootInfo pX86Info;
    
    (FARPROC) PCIReadBARs,              // FARPROC pfnPCIReadBARs;
    (FARPROC) PCIInitInfo,              // FARPROC pfnPCIInitInfo;
    (FARPROC) PCIReadBusData,           // FARPROC pfnPCIReadBusData;
    (FARPROC) PCIWriteBusData,          // FARPROC pfnPCIWriteBusData;
    (FARPROC) PCIGetBusDataByOffset,    // FARPROC pfnPCIGetBusDataByOffset;
    (FARPROC) PCIReg,                   // FARPROC pfnPCIReg;
};
#endif
//-----------------------------------------------------------------------------
//
//  InitBootInfo
//
//  
//
void InitBootInfo(
                  __in const BOOT_ARGS * const pBootArgs
                  )
{
    // Hard set to COM2, 38400
    //pBootArgs->ucComPort = 2;
    //pBootArgs->ucBaudDivisor = 0x0002;

    if (BOOT_ARG_VERSION_SIG != pBootArgs->dwVersionSig) 
    {
        // nothing passed from bootloader, set default
        BSPInitDfltBootInfo (&x86Info);
    } 
    else 
    {
        x86Info.dwKitlIP            = pBootArgs->EdbgAddr.dwIP;
        x86Info.dwKitlBaseAddr      = pBootArgs->dwEdbgBaseAddr;
        x86Info.dwKitlDebugZone     = pBootArgs->dwEdbgDebugZone;
        x86Info.KitlTransport       = pBootArgs->KitlTransport;
        x86Info.ucKitlAdapterType   = pBootArgs->ucEdbgAdapterType;
        x86Info.ucComPort           = pBootArgs->ucComPort;
        x86Info.ucBaudDivisor       = pBootArgs->ucBaudDivisor;
        x86Info.ucKitlIrq           = pBootArgs->ucEdbgIRQ;
        x86Info.dwRebootAddr        = (BOOTARG_SIG == pBootArgs->dwEBootFlag)? pBootArgs->dwEBootAddr : g_pNKGlobal->dwStartupAddr;
        x86Info.cxDisplayScreen     = pBootArgs->cxDisplayScreen;
        x86Info.cyDisplayScreen     = pBootArgs->cyDisplayScreen;
        x86Info.bppScreen           = pBootArgs->bppScreen;
        x86Info.ucPCIConfigType     = pBootArgs->ucPCIConfigType;
        x86Info.NANDBootFlags       = pBootArgs->NANDBootFlags;     // Boot flags related to NAND support.
        x86Info.NANDBusNumber       = pBootArgs->NANDBusNumber;     // NAND controller PCI bus number.
        x86Info.NANDSlotNumber      = pBootArgs->NANDSlotNumber;    // NAND controller PCI slot number.
        x86Info.fStaticIP           = pBootArgs->EdbgFlags & EDBG_FLAGS_STATIC_IP;
        memcpy (&x86Info.wMac[0], &pBootArgs->EdbgAddr.wMAC[0], sizeof (x86Info.wMac));
        memcpy (&x86Info.szDeviceName[0], &pBootArgs->szDeviceNameRoot[0], sizeof (x86Info.szDeviceName));
        if (pBootArgs->ucLoaderFlags & LDRFL_KITL_DISABLE_VMINI)
        {
            x86Info.fKitlVMINI = FALSE;
        }
        else
        {
            x86Info.fKitlVMINI = TRUE;
        }

        if (pBootArgs->ucLoaderFlags & LDRFL_FORMAT_STORE)
        {
            x86Info.fFormatUserStore = TRUE;
        }
        else
        {
            x86Info.fFormatUserStore = FALSE;
        }

        if (pBootArgs->MajorVersion >= 1 && pBootArgs->MinorVersion >= 1)
        {
            x86Info.RamTop = pBootArgs->RamTop;
        }
        else
        {
            x86Info.RamTop.QuadPart = 0;
        }
    }

    g_pOemGlobal->pKitlInfo = &x86Info; // &kitlinfo;

    // if name root not specified, use BSP specific name root.
    // NOTE: device name can change when KITL started.
    if (!x86Info.szDeviceName[0]) 
    {
        StringCchCopyA ((CHAR*)x86Info.szDeviceName, KITL_MAX_DEV_NAMELEN, g_oalDeviceNameRoot);
    } 
    else 
    {
        g_oalDeviceNameRoot = (LPCSTR) pBootArgs->szDeviceNameRoot;
    }

    // initialize fields that are required to be non-zero
    if (!x86Info.ucBaudDivisor) 
    {
        x86Info.ucBaudDivisor = 3;  // default to 38400
    }
    if (!x86Info.dwRebootAddr) 
    {
        x86Info.dwRebootAddr = g_pNKGlobal->dwStartupAddr;      // no bootloader, set to startup
    }
    if (!x86Info.ucPCIConfigType) 
    {
        x86Info.ucPCIConfigType = 1;                // default PCI config type 1
    }

    // set up our UUID based upon the MAC address of the ethernet card
    // note that this is not really universally unique, but we return it
    // for test purposes

    // Microsoft test manufacturer ID
    g_x86uuid[0] = (UCHAR)0x00;
    g_x86uuid[1] = (UCHAR)0x30;
    g_x86uuid[2] = (UCHAR)0xBD;
    g_x86uuid[3] = (UCHAR)0x2D;
    g_x86uuid[4] = (UCHAR)0x73;
    g_x86uuid[5] = (UCHAR)0x32;

    // Next 16-bits: Version/variant - Version 1.8 format (48/16/64) from Windows Mobile docs
    g_x86uuid[6] = (UCHAR)1;
    g_x86uuid[7] = (UCHAR)8;

    // Last 64-bits: Unique ID
    g_x86uuid[8]  = (UCHAR) ((x86Info.wMac[0] >> 0) & 0xFF);
    g_x86uuid[9]  = (UCHAR) ((x86Info.wMac[0] >> 8) & 0xFF);
    g_x86uuid[10] = (UCHAR) ((x86Info.wMac[1] >> 0) & 0xFF);
    g_x86uuid[11] = (UCHAR) ((x86Info.wMac[1] >> 8) & 0xFF);
    g_x86uuid[12] = (UCHAR) ((x86Info.wMac[2] >> 0) & 0xFF);
    g_x86uuid[13] = (UCHAR) ((x86Info.wMac[2] >> 8) & 0xFF);
    g_x86uuid[14] = 0;
    g_x86uuid[15] = 0;
}


PUCHAR GetMCS9901REG(int index);
void TimbWriteByte(BYTE ucChar);
//-----------------------------------------------------------------------------
//
//  OEMInitDebugSerial
//
//  
//
void OEMInitDebugSerial(void)
{
    // Locate bootargs (this is the first opportunity the OAL has to initialize this global).
    //
    InitBootInfo ((const BOOT_ARGS *) ((ULONG)(*(PBYTE *)BOOT_ARG_PTR_LOCATION) | 0x80000000));

#ifdef ENABLE_SERIALDEBUG_MOSCHIP
    IoPortBase = (PUCHAR) GetMCS9901REG(g_pX86Info->ucComPort);
#else
    switch ( g_pX86Info->ucComPort ) 
    {
    case 1:
        IoPortBase = (PUCHAR)COM1_BASE;
        break;

    case 2:
        IoPortBase = (PUCHAR)COM2_BASE;
        break;

    case 3:
        IoPortBase = (PUCHAR)COM3_BASE;
        break;

    case 4:
        IoPortBase = (PUCHAR)COM4_BASE;
        break;

    default:
        IoPortBase = 0;
        break;
    }
#endif

    if ( IoPortBase ) 
    {
        WRITE_PORT_UCHAR(IoPortBase+comLineControl, 0x80);   // Access Baud Divisor
        WRITE_PORT_UCHAR(IoPortBase+comDivisorLow, g_pX86Info->ucBaudDivisor); 
        WRITE_PORT_UCHAR(IoPortBase+comDivisorHigh, 0x00);
        WRITE_PORT_UCHAR(IoPortBase+comFIFOControl, 0x01);   // Enable FIFO if present
        WRITE_PORT_UCHAR(IoPortBase+comLineControl, 0x03);   // 8 bit, no parity
        WRITE_PORT_UCHAR(IoPortBase+comIntEnable, 0x00);     // No interrupts, polled
        WRITE_PORT_UCHAR(IoPortBase+comModemControl, 0x03);  // Assert DTR, RTS
        OEMWriteDebugString(TEXT("Debug Serial Init\r\n"));
    }

    // Turn on ether debug zones here if you need to debug edbg.
    // KITLSetDebug(0xffff);
}

//-----------------------------------------------------------------------------
//
//  OEMWriteDebugByte
//
//  
//
void OEMWriteDebugByte(
                       BYTE ucChar
                       )
{
    if ( IoPortBase ) 
    {
        while ( !(READ_PORT_UCHAR(IoPortBase+comLineStatus) & LS_THR_EMPTY) )
        {
            ;
        }

        WRITE_PORT_UCHAR(IoPortBase+comTxBuffer, ucChar);
    }
}

//-----------------------------------------------------------------------------
//
//  OEMReadDebugByte
//
//  
//
int OEMReadDebugByte()
{
    unsigned char   ucStatus;
    unsigned char   ucChar;

    if ( IoPortBase ) 
    {
        ucStatus = READ_PORT_UCHAR(IoPortBase+comLineStatus);

        if ( ucStatus & LS_RX_DATA_READY ) 
        {
            ucChar = READ_PORT_UCHAR(IoPortBase+comRxBuffer);

            if ( ucStatus & LS_RX_ERRORS ) 
            {
                return (OEM_DEBUG_COM_ERROR);
            } 
            else 
            {
                return (ucChar);
            }

        }
    }

    return (OEM_DEBUG_READ_NODATA);
}


//-----------------------------------------------------------------------------
//
//  OEMClearDebugCommError
//
//  
//
void OEMClearDebugCommError()
{
    if ( IoPortBase ) 
    {
    }
}

