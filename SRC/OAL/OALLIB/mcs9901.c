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
#include <x86kitl.h>
#include <pci.h>

//----------------------------------------------
//MCS9901
#define VENDOR_ID    (0x9710)
#define DEVICE_ID    (0x9901)

BYTE GetMaxBusNumber()
{
    PCI_SLOT_NUMBER     slotNumber;
    PCI_COMMON_CONFIG   pciConfig;
    int                 bus, device, function;
    int                 length;
    BYTE                bMaxBus = 0;

    //
    // Scan bus 0 for bridges. They'll tell us the number of buses
    //
    bus = 0;
    
    for (device = 0; device < PCI_MAX_DEVICES; device++) 
    {
        slotNumber.u.bits.DeviceNumber = device;

        for (function = 0; function < PCI_MAX_FUNCTION; function++) 
        {
            slotNumber.u.bits.FunctionNumber = function;

            length = PCIGetBusDataByOffset(bus, slotNumber.u.AsULONG, &pciConfig, 0, sizeof(pciConfig));

            if (length == 0 || pciConfig.VendorID == 0xFFFF) break;

            if (pciConfig.BaseClass == PCI_CLASS_BRIDGE_DEV && pciConfig.SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI) 
            {
                if (pciConfig.u.type1.SubordinateBusNumber > bMaxBus) 
                {
                    bMaxBus = pciConfig.u.type1.SubordinateBusNumber;
                }
            }

            if (function == 0 && !(pciConfig.HeaderType & 0x80)) break;
            
        }
        if (length == 0) break;
    }

    return bMaxBus;
}

#define LEN_CSR_INTMEM    0x20000    // Internal registers and memories

PUCHAR GetMCS9901REG(int index)
{
    int    bus, device, function, length = 0, maxbus=0;
    PCI_COMMON_CONFIG   pciConfig;
    USHORT vid = VENDOR_ID;
    USHORT did = DEVICE_ID;
    UINT32 dwIoBase = 0x0000;

    // init PCI config mechanism
    PCIInitConfigMechanism (1);

    maxbus = (int) GetMaxBusNumber();

    RETAILMSG(1, (TEXT("PCI Device Configurations (%d PCI bus(es) present)...\r\n"), maxbus + 1));
    
    function = index -1 ;
    
    if(function <0 || function >=4) function = 0;
    
    for (bus = 0; bus <= maxbus; bus++) 
    {
        for (device = 0; device < PCI_MAX_DEVICES; device++) 
        {
            // read PCI config data
            length = PCIReadBusData ( bus, device, function, &pciConfig, 0, sizeof(pciConfig));

            if (length == 0 || (pciConfig.VendorID == 0xFFFF)) break;

            if(pciConfig.VendorID == vid && pciConfig.DeviceID == did)
            {
                dwIoBase = pciConfig.u.type0.BaseAddresses[0] & 0xFFFFFFFC;
                RETAILMSG(1,(TEXT("Find MCS9901 Chip , COM%d address = 0x%x\r\n"),index,dwIoBase));
            }
        }
    }

    return (PUCHAR)dwIoBase;
}