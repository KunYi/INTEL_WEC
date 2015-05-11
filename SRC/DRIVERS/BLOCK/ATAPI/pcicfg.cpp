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

#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>
#include <devload.h>
#include <PCIbus.h>
#include <debug.h>
#include <atamain.h>

typedef PPCI_RSRC (*PPCIRSRC_NEW)(
    DWORD Bus,
    DWORD Device,
    DWORD Function,
    DWORD Offset,
    DWORD Base,
    DWORD Size,
    BOOL  Bridge,
    DWORD SecBus,
    BOOL  Placed,
    PPCI_CFG_INFO ConfigInfo
    );

typedef VOID (*PPCIRSRC_ADD)(
    PPCI_RSRC Head,
    PPCI_RSRC Rsrc
    );

//
// PCI class code and revision register
//
typedef
union _PCI_CR {

   struct {
      BYTE  RevisionID;
      BYTE  ProgIF;
      BYTE  SubClassCode;
      BYTE  ClassCode;
   };

   DWORD reg;

} PCI_CR,*PPCI_CR ;

#define PCI_CONFIG_COMMAND_STATUS   (1 << 2)
#define PCI_CONFIG_CLASS_REVISION   (2 << 2)

//
// Function prototypes
//
static BOOL ConfigRsrc(
    PPCI_DEV_INFO pInfo,
    PPCI_RSRC pMemHead,
    PPCI_RSRC pIoHead,
    DWORD *pMemSize,
    DWORD *pIoSize
    );

static BOOL ConfigSize(
    PPCI_DEV_INFO pInfo
    );

static BOOL ConfigInit(
    PPCI_DEV_INFO pInfo
    );

EXTERN_C DWORD
PCIConfig_ReadDword(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C WORD
PCIConfig_ReadWord(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C BYTE
PCIConfig_ReadByte(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C void
PCIConfig_Write(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset,
    ULONG Value,
    ULONG Size = sizeof(DWORD)
    );



//
// DeviceConfig
//
EXTERN_C DWORD GenericConfig(
      DWORD Command
    , PPCI_DEV_INFO		pInfo
    , PPCI_RSRC			pRsrc1
    , PPCI_RSRC			pRsrc2
    , DWORD				*pMemSize
    , DWORD				*pIoSize
    )
{
    DEBUGMSG(1, (L"ATAPI:PCIConfig!DeviceConfig+(%d)\r\n", Command));

    switch (Command) {
    //
    // This call is used to determine the size and locations for memory and io
    // mapped devices.  However, if we return ERROR_NOT_SUPPORTED, then the PCI
    // bus will do the same thing.
    //
    case PCIBUS_CONFIG_RSRC:
        if (ConfigRsrc(pInfo, pRsrc1, pRsrc2, pMemSize, pIoSize)) {
            return ERROR_SUCCESS;
        } else {
            return ERROR_GEN_FAILURE;
        }
        //return ERROR_NOT_SUPPORTED;

    //
    // This is apparently used to write a value to a configuration register.  I
    // don't know how or why we would use this.
    //
    case PCIBUS_CONFIG_SET:
        return ERROR_NOT_SUPPORTED;

    //
    // This function is used to fill in the PCI_DEV_INFO structure's IO and
    // memory register arrays based on the information in the base address
    // registers.  The reason we need to even have this here instead of using
    // the PCIReadBARs function in the PCI code is for legacy reasons.  An
    // explanation is found in the ConfigSize function.
    //
    case PCIBUS_CONFIG_SIZE:
        if (ConfigSize(pInfo)) {
            return ERROR_SUCCESS;
        } else {
            return ERROR_GEN_FAILURE;
        }

    //
    // This is the only function for which there is no default in the PCI code.
    // It's purpose, as far as I can see, is to allow us to do any final
    // initialization on the host adapter beyond what PCI will do.  This can be
    // device specific if we wanted to setup DMA, to enable or disable channels,
    // etc.  It is a great utility function.
    //
    // ConfigInit currently does nothing, just returns true.  The promise card
    // has a function though, check out the PromiseInit function.
    //
    case PCIBUS_CONFIG_INIT:
        if (ConfigInit(pInfo)) {
            return ERROR_SUCCESS;
        } else {
            return ERROR_GEN_FAILURE;
        }

    default:
        break;
    }

    DEBUGMSG(1, (L"ATAPI:PCIConfig!DeviceConfig-: ERROR: Command %d not recognized\r\n", Command));

    return ERROR_BAD_COMMAND;
}


//
// DeviceConfig for Native PCI IDE controller
//
EXTERN_C DWORD NativeConfig(
      DWORD Command
    , PPCI_DEV_INFO pInfo
    , PPCI_RSRC pRsrc1
    , PPCI_RSRC pRsrc2
    , DWORD *pMemSize
    , DWORD *pIoSize
    )
{
    DEBUGMSG(ZONE_INIT, (L"ATAPI:PCIConfig!NativeConfig+(%d)\r\n", Command));

    switch (Command) {
    case PCIBUS_CONFIG_RSRC:
        {
            DWORD dwLegacyIRQ;
            HKEY hKey;
            UCHAR mode = pInfo->Cfg->ProgIf & 0x0f;
            
            if ((ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, pInfo->RegPath, 0, 0, &hKey)) || (!hKey))
                return ERROR_GEN_FAILURE;

            // fetch legacy irq
            if (!AtaGetRegistryValue(hKey, REG_VAL_IDE_LEGACYIRQ, &dwLegacyIRQ)) {
                dwLegacyIRQ = (DWORD)-1;
            }

            // Bit definitions for ProgIf
            // +-----+------------------------------------------------------------------------+
            // | Bit | Description                                                            |
            // |-----+------------------------------------------------------------------------+
            // |  0  | Determines the mode that the primary IDE channel is operating in.      |
            // |     | Zero corresponds to 'compatibility'(legacy), one means native-PCI mode.|
            // |-----+------------------------------------------------------------------------+
            // |  1  | If this bit is one, the primary channel supports both modes and        |
            // |     | may be set to either mode by writing bit 0.                            |
            // |-----+------------------------------------------------------------------------+
            // |  2  | Determines the mode that the secondary IDE channel is operating in.    |
            // |     | Zero corresponds to 'compatibility'(legacy), one means native-PCI mode.|
            // |-----+------------------------------------------------------------------------+
            // |  3  | If this bit is one, the secondary channel supports both modes and      |
            // |     | may be set to either mode by writing bit 0.                            |
            // |-----+------------------------------------------------------------------------+

            //
            //             |ProgIf
            //             | 0x0A | 0x00 | 0x05 | 0x0F
            // dwLegacyIRQ +------+------+------+------
            //       == -1 | Tran |   X  |   V  |  V
            //             +------+------+------+------
            //       != -1 |   V  |   V  | OverWrt Reg
            //
            //           V = Combination is 100% match
            //           X = Combination is invalid
            //        Tran = Transfer to Native Mode
            // OverWrt Reg = Overwrite the "LegacyIRQ" registry to -1 (native mode)
            //               in CATAController::MapIORegisters
            //
            if (dwLegacyIRQ == (DWORD)-1)
            {
                // dwLegacyIRQ==-1: intends to use native mode.

                // Controler is placed in Legacy mode but supports dual mode,
                // so configured to native mode.
                if (mode == 0x0A)
                {
                    PCI_CR *pCR = (PCI_CR *)&pInfo->Cfg->RevisionID;

                    // Save the command, so PCIBUS can restore it later.
                    pInfo->Command = pInfo->Cfg->Command;

                    // Disable device to let PCIBUS reallocate IRQ.
                    pInfo->Cfg->Command = 0;
                    PCIConfig_Write(pInfo->Bus, pInfo->Device, pInfo->Function, PCI_CONFIG_COMMAND_STATUS, 0xFFFF0000);

                    // Enter native mode.
                    pCR->ProgIF |= 0x05;
                    PCIConfig_Write(pInfo->Bus, pInfo->Device, pInfo->Function, PCI_CONFIG_CLASS_REVISION, pCR->reg);

                    DEBUGMSG(ZONE_WARNING,
                        (L"ATAPI:PCIConfig!NativeConfig: WARN: Controller is placed in legacy mode with %s=-1, "
                        L"reconfigure the controller into native mode\r\n", REG_VAL_IDE_LEGACYIRQ));
                }
                // Placed in Legacy mode but *not* supports dual mode.
                else if (mode == 0x00)
                {
                    DEBUGMSG(ZONE_ERROR, 
                        (L"ATAPI:PCIConfig!NativeConfig-: ERROR: Controller does not support dual mode, "
                        L"set the %s\\%s with proper value\r\n", pInfo->RegPath, REG_VAL_IDE_LEGACYIRQ));
                    return ERROR_GEN_FAILURE;
                }
            }
            RegCloseKey(hKey);
        }
        // Let PCIBUS to do the rest.
        return ERROR_NOT_SUPPORTED;

    case PCIBUS_CONFIG_SET:
        return ERROR_NOT_SUPPORTED;

    case PCIBUS_CONFIG_SIZE:
        // If controller is in legacy mode.
        if ((pInfo->Cfg->ProgIf & 0x05) == 0x00)
            return GenericConfig(Command, pInfo, pRsrc1, pRsrc2, pMemSize, pIoSize);
        else
            return ERROR_NOT_SUPPORTED;

    case PCIBUS_CONFIG_INIT:
        return GenericConfig(Command, pInfo, pRsrc1, pRsrc2, pMemSize, pIoSize);

    default:
        break;
    }

    DEBUGMSG(ZONE_INIT, (L"ATAPI:PCIConfig!NativeConfig-: ERROR: Command %d not recognized\r\n", Command));

    return ERROR_BAD_COMMAND;
}


// Inline functions
EXTERN_C DWORD
PCIConfig_ReadDword(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    )
{
    ULONG RetVal = FALSE;
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;

    HalGetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &RetVal, Offset, sizeof(RetVal));

    return RetVal;
}

// Inline functions
EXTERN_C WORD
PCIConfig_ReadWord(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    )
{
    WORD RetVal = FALSE;
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;
    
    HalGetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &RetVal, Offset, sizeof(RetVal));

    return RetVal;
}

// Inline functions
EXTERN_C BYTE
PCIConfig_ReadByte(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    )
{
    BYTE  RetVal = FALSE;
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;
    
    HalGetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &RetVal, Offset, sizeof(RetVal));

    return RetVal;
}

EXTERN_C void
PCIConfig_Write(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset,
    ULONG Value,
    ULONG Size
    )
{
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;

    HalSetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &Value, Offset, Size);
}



//
// ConfigRsrc
//
static BOOL ConfigRsrc(
    PPCI_DEV_INFO pInfo,
    PPCI_RSRC pMemHead,
    PPCI_RSRC pIoHead,
    DWORD *pMemSize,
    DWORD *pIoSize
    )
{
    DWORD NumberOfRegs;
    ULONG Offset;
    ULONG i;
    ULONG BaseAddress;
    ULONG Size;
    ULONG Type;
    DWORD Reg;
    BOOL SizeFound;
    DWORD Bus = pInfo->Bus;
    DWORD Device = pInfo->Device;
    DWORD Function = pInfo->Function;
    PPCI_COMMON_CONFIG pCfg = pInfo->Cfg;

    HINSTANCE    hPciBusInstance = NULL;
    PPCIRSRC_NEW pfnPCIRsrc_New = NULL;
    PPCIRSRC_ADD pfnPCIRsrc_Add = NULL;
    BOOL         fResult = FALSE;

    hPciBusInstance = LoadLibrary(L"PCIBUS.DLL");
    if (NULL == hPciBusInstance) {
        goto exit;
    }
    pfnPCIRsrc_New = (PPCIRSRC_NEW)GetProcAddress(hPciBusInstance, L"PCIRsrc_New");
    if (pfnPCIRsrc_New == NULL) {
        goto exit;
    }
    pfnPCIRsrc_Add = (PPCIRSRC_ADD)GetProcAddress(hPciBusInstance, L"PCIRsrc_Add");
    if (pfnPCIRsrc_Add == NULL) {
        goto exit;
    }

    DEBUGMSG(ZONE_PCI | ZONE_INIT, (L"ATAPI:PCIConfig!ConfigRsrc+(Bus %d, Device %d, Function %d)\r\n",
        Bus, Device, Function));

    //
    // NOTE::Do we really need to determine the header type?  If we've gotten this
    // far we are working on a controller, not a pci-to-pci bridge or a cardbus.
    // Was this code just pulled from the PCI code where this same thing is done?
    //

    // Determine number of BARs to examine from header type
    switch (pCfg->HeaderType & ~PCI_MULTIFUNCTION) {
    case PCI_DEVICE_TYPE:
        NumberOfRegs = PCI_TYPE0_ADDRESSES;
        break;

    case PCI_BRIDGE_TYPE:
        NumberOfRegs = PCI_TYPE1_ADDRESSES;
        break;

    case PCI_CARDBUS_TYPE:
        NumberOfRegs = PCI_TYPE2_ADDRESSES;
        break;

    default:
        goto exit;
    }

    for (i = 0, Offset = 0x10; i < NumberOfRegs; i++, Offset += 4) {
        // Get base address register value
        Reg = pCfg->u.type0.BaseAddresses[i];
        Type = Reg & PCI_ADDRESS_IO_SPACE;

        //
        // This process is called sizing a base address register.  It will tell us
        // how much address space a device requires.
        //
        PCIConfig_Write(Bus,Device,Function,Offset,0xFFFFFFFF);
        BaseAddress = PCIConfig_ReadDword(Bus,Device,Function,Offset);
        PCIConfig_Write(Bus, Device, Function, Offset, Reg);

        if (Type) {
            // IO space
            // Re-adjust BaseAddress if upper 16-bits are 0 (allowable in PCI 2.2 spec)
            if (((BaseAddress & PCI_ADDRESS_IO_ADDRESS_MASK) != 0) && ((BaseAddress & 0xFFFF0000) == 0)) {
                BaseAddress |= 0xFFFF0000;
            }

            Size = ~(BaseAddress & PCI_ADDRESS_IO_ADDRESS_MASK);
            Reg &= PCI_ADDRESS_IO_ADDRESS_MASK;
        } else {
            // Memory space
            if ((Reg & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_20BIT) {
                // PCI 2.2 spec no longer supports this type of memory addressing
                DEBUGMSG(ZONE_ERROR, (L"ATAPI:PCIConfig!ConfigRsrc: 20-bit addressing not supported\r\n"));
                goto exit;
            }

            // Re-adjust BaseAddress if upper 16-bits are 0 (allowed by PCI 2.2 spec)
            if (((BaseAddress & PCI_ADDRESS_MEMORY_ADDRESS_MASK) != 0) && ((BaseAddress & 0xFFFF0000) == 0)) {
                BaseAddress |= 0xFFFF0000;
            }

            Size = ~(BaseAddress & PCI_ADDRESS_MEMORY_ADDRESS_MASK);
            Reg &= PCI_ADDRESS_MEMORY_ADDRESS_MASK;
        }

        // Check that the register has a valid format; it should have consecutive high 1's and consecutive low 0's
        SizeFound = (BaseAddress != 0) && (BaseAddress != 0xFFFFFFFF) && (((Size + 1) & Size) == 0);

        Size +=1;

        if (SizeFound) {
            PPCI_RSRC Rsrc = (*pfnPCIRsrc_New)(Bus, Device, Function, Offset, Reg, Size, FALSE, 0, FALSE, pInfo->ConfigInfo);

            if (!Rsrc) {
                DEBUGMSG(ZONE_PCI | ZONE_ERROR, (L"ATAPI:PCIConfig!ConfigRsrc: Failed local alloc of Rsrc\r\n"));
                goto exit;
            }

            if (Type == PCI_ADDRESS_IO_SPACE) {
                *pIoSize += Size;
                (*pfnPCIRsrc_Add)(pIoHead, Rsrc);
            } else {
                *pMemSize += Size;
                (*pfnPCIRsrc_Add)(pMemHead, Rsrc);
            }

            DEBUGMSG(ZONE_PCI | ZONE_INIT, (L"ATAPI:PCIConfig!ConfigRsrc: BAR(%d/%d/%d): Offset 0x%x, Type %s, Size 0x%X\r\n",
                Bus, Device, Function, Offset, (Type == PCI_ADDRESS_IO_SPACE) ? TEXT("I/O") : TEXT("Memory"), Size));
        } else {
            // Some devices have invalid BARs before valid ones (even though the spec says you can't).  Skip invalid BARs.
            continue;
        }

        // check for 64 bit device (memory only)
        if ((Type == PCI_ADDRESS_MEMORY_SPACE) && ((Reg & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT)) {
            // 64 bit device - BAR is twice as wide - zero out high part
            Offset += 4;
            i++;
            PCIConfig_Write(Bus, Device, Function, Offset, 0x0);
        }
    }

    //
    // Add resource for expansion ROM, offset 0x30
    //
    Offset = 0x30;
    PCIConfig_Write(Bus, Device, Function, Offset ,0xFFFFFFFF);
    BaseAddress = PCIConfig_ReadDword(Bus, Device, Function, Offset);

    // Memory space
    Size = ~(BaseAddress & 0xFFFFFFF0);
    Reg &= 0xFFFFFFF0;

    // Check that the register has a valid format; it should have consecutive high 1's and consecutive low 0's
    SizeFound = (BaseAddress != 0) && (BaseAddress != 0xFFFFFFFF) && (((Size + 1) & Size) == 0);

    Size +=1;

    if (SizeFound) {
        PPCI_RSRC Rsrc = (*pfnPCIRsrc_New)(Bus, Device, Function, Offset, Reg, Size, FALSE, 0, FALSE, pInfo->ConfigInfo);

        if (!Rsrc) {
            DEBUGMSG(ZONE_PCI | ZONE_ERROR, (L"ATAPI:PCIConfig!ConfigRsrc: Failed local alloc of Rsrc\r\n"));
            goto exit;
        }

        *pMemSize += Size;
        (*pfnPCIRsrc_Add)(pMemHead, Rsrc);


        DEBUGMSG(ZONE_PCI | ZONE_INIT, (L"ATAPI:PCIConfig!ConfigRsrc: ROM(%d/%d/%d): Offset 0x%x, Type Memory, Size 0x%X\r\n",
            Bus, Device, Function, Offset, Size));
    }

    fResult = TRUE;

exit:;

    if (NULL != hPciBusInstance) {
        FreeLibrary(hPciBusInstance);
    }

    DEBUGMSG(ZONE_PCI | ZONE_INIT, (L"ATAPI:PCIConfig!ConfigRsrc-\r\n", Bus, Device, Function));

    return fResult;
}


//
// ConfigSize
//
static BOOL ConfigSize(
    PPCI_DEV_INFO pInfo
    )
{
    DWORD Offset;
    DWORD i;
    DWORD BaseAddress;
    DWORD Size = 0;
    DWORD Reg;
    DWORD IoIndex = 0;
    DWORD MemIndex = 0;

    DEBUGMSG(ZONE_INIT | ZONE_PCI, (L"ATAPI:PCIConfig!ConfigSize+(Bus %d, Device %d, Function %d)\r\n",
        pInfo->Bus, pInfo->Device, pInfo->Function));

    if ((pInfo->Cfg->HeaderType & ~PCI_MULTIFUNCTION) != PCI_DEVICE_TYPE) {
        return FALSE;
    }

    for (i = 0, Offset = 0x10; i < PCI_TYPE0_ADDRESSES; i++, Offset += 4) {
        // Get base address register value
        Reg = pInfo->Cfg->u.type0.BaseAddresses[i];

        //
        // When using compatibility mode any value in the base address registers
        // for the command and control registers should be ignored and we should
        // use the following:
        //
        //  0. Command block for channel x: 0x1F0
        //  1. Control register for channel x: 0x3F5
        //  2. Command block for channel y: 0x170
        //  3. Control register for channel y: 0x376
        //
        // Note that we are currently using 0x171 and 0x375 for channel y.  I'm
        // not sure why this is right now but it seems to be working.
        //
        if (i <= 3) {
            switch(i) {
                case 0:
                    if (!Reg || !(pInfo->Cfg->ProgIf & 0x01))
                        Reg = 0x1F1;
                    Size = 0x8;
                    break;
                case 1:
                    if (!Reg || !(pInfo->Cfg->ProgIf & 0x01))
                        Reg = 0x3F5;
                    Size = 0x4;
                    break;
                case 2:
                    if (!Reg || !(pInfo->Cfg->ProgIf & 0x04))
                        Reg = 0x171;
                    Size = 0x8;
                    break;
                case 3:
                    if (!Reg || !(pInfo->Cfg->ProgIf & 0x04))
                        Reg = 0x375;
                    Size = 0x4;
                    break;
            }

            pInfo->IoLen.Reg[IoIndex] = Size;
            pInfo->IoLen.Num++;
            pInfo->IoBase.Reg[IoIndex++] = Reg & PCI_ADDRESS_IO_ADDRESS_MASK;
            pInfo->IoBase.Num++;
        } else {
            // Get size info
            PCIConfig_Write(pInfo->Bus, pInfo->Device, pInfo->Function, Offset, 0xFFFFFFFF);
            BaseAddress = PCIConfig_ReadDword(pInfo->Bus, pInfo->Device, pInfo->Function, Offset);
            PCIConfig_Write(pInfo->Bus, pInfo->Device, pInfo->Function, Offset, Reg);

            if (Reg & PCI_ADDRESS_IO_SPACE) {
                // IO space
                // Re-adjust BaseAddress if upper 16-bits are 0 (this is allowable in PCI 2.2 spec)
                if (((BaseAddress & PCI_ADDRESS_IO_ADDRESS_MASK) != 0) && ((BaseAddress & 0xFFFF0000) == 0)) {
                    BaseAddress |= 0xFFFF0000;
                }

                Size = ~(BaseAddress & PCI_ADDRESS_IO_ADDRESS_MASK);

                if ((BaseAddress != 0) && (BaseAddress != 0xFFFFFFFF) && (((Size + 1) & Size) == 0)) {
                    // BAR has valid format (consecutive high 1's and consecutive low 0's)
                    pInfo->IoLen.Reg[IoIndex] = Size + 1;
                    pInfo->IoLen.Num++;
                    pInfo->IoBase.Reg[IoIndex++] = Reg & PCI_ADDRESS_IO_ADDRESS_MASK;
                    pInfo->IoBase.Num++;
                } else {
                    // BAR invalid => skip to next one
                    continue;
                }
            } else {
                // Memory space
                if ((Reg & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_20BIT) {
                    // PCI 2.2 spec no longer supports this type of memory addressing
                    DEBUGMSG(ZONE_ERROR, (L"ATAPI:PCIConfig!ConfigSize: 20-bit addressing not supported\r\n"));
                    return FALSE;
                }

                // Re-adjust BaseAddress if upper 16-bits are 0 (this is allowable in PCI 2.2 spec)
                if (((BaseAddress & PCI_ADDRESS_MEMORY_ADDRESS_MASK) != 0) && ((BaseAddress & 0xFFFF0000) == 0)) {
                    BaseAddress |= 0xFFFF0000;
                }

                Size = ~(BaseAddress & PCI_ADDRESS_MEMORY_ADDRESS_MASK);

                if ((BaseAddress != 0) && (BaseAddress != 0xFFFFFFFF) && (((Size + 1) & Size) == 0)) {
                    // BAR has valid format (consecutive high 1's and consecutive low 0's)
                    pInfo->MemLen.Reg[MemIndex] = Size + 1;
                    pInfo->MemLen.Num++;
                    pInfo->MemBase.Reg[MemIndex++] = Reg & PCI_ADDRESS_MEMORY_ADDRESS_MASK;
                    pInfo->MemBase.Num++;
                } else {
                    // BAR invalid => skip to next one
                    continue;
                }

                if ((Reg & PCI_ADDRESS_MEMORY_TYPE_MASK) == PCI_TYPE_64BIT) {
                    // 64 bit device - BAR is twice as wide, skip upper 32-bits
                    Offset += 4;
                    i++;
                }
            }
        }
    }

    DEBUGMSG(ZONE_INIT | ZONE_PCI, (L"ATAPI:PCIConfig!ConfigSize-\r\n"));

    return TRUE;
}


//
// ConfigInit
//
static BOOL ConfigInit(
    PPCI_DEV_INFO    pInfo
    )
{
    //
    // This is the only function for which there is no default in the PCI code.
    // Its purpose is to allow us to do any final initialization on the host adapter beyond what PCI will do.
    // This can be device specific if we wanted to setup DMA, to enable or disable channels, etc.
    //
    // ConfigInit currently does nothing, just returns true.
    // We use the default value in the IDE Timing, DMA Control, DMA Timing and IDE I/O Configuration which set by BIOS.
    //
    return TRUE;
}


