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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//
// -----------------------------------------------------------------------------

#include <windows.h>
#include <oal.h>
#include <nkintr.h>
#include <x86boot.h>
#include <x86kitl.h>
#include <oal_intr.h>
#include <PCIReg.h>
#include "pci.h"
#include "acpitable.h"

//----------------------------------------------------------------------------------
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

#pragma pack(1)

typedef struct {
    BYTE PCIBusNumber;
    BYTE PCIDeviceNumber;// In upper 5bits

    BYTE INTA_LinkValue;
    unsigned short INTA_IrqBitMap;

    BYTE INTB_LinkValue;
    unsigned short INTB_IrqBitMap;

    BYTE INTC_LinkValue;
    unsigned short INTC_IrqBitMap;

    BYTE INTD_LinkValue;
    unsigned short INTD_IrqBitMap;

    BYTE SlotNumber;
    BYTE Reserved;
} RoutingOptionTable;


enum { MAX_DEVICE = 0x20 };

typedef struct {
    unsigned short BufferSize;
    BYTE * pDataBuffer;
    DWORD DS;
    RoutingOptionTable routingOptionTable[MAX_DEVICE];
} IRQRountingOptionsBuffer;

typedef struct _irqLink {
    BYTE linkValue;
    BYTE bus;
    BYTE device;
    BYTE func;
    struct _irqLink* pNext;
} irqLink, *pIrqLink;


// Disable DEBUG print by #undef
#undef BSP_UEFI_DEBUG

#ifdef BSP_UEFI_DEBUG
#define OAL_UEFI_DBG   1
#else
#define OAL_UEFI_DBG   0
#endif

#define INVALID_IRQ   0xFF
// By default, debug print is turned off
#undef BSP_PCI_ROUTING_OPTION_TABLE_DEBUG

typedef enum {
    NONE  = 0,
    PIRQA = 1,
    PIRQB = 2,
    PIRQC = 3,
    PIRQD = 4, 
    PIRQE = 5,
    PIRQF = 6,
    PIRQG = 7,
    PIRQH = 8 
};

// An array to store each PIRQ assigned IRQ number
IRQ_TO_PIRQ_TBL     g_Irq2PirqTbl[MAX_PIRQ_NUM];
DWORD g_dwInfoInx = 0;

// An array to store an IRQ has been assigned counter, this is used to avoid 
// onw IRQ be assign too much 
IRQ_INFO_TBL        g_IrqAssignedInfoTbl[MAX_PIRQ_NUM];

RoutingOptionTable g_RoutingOptionTable[PCI_MAX_BUS * PCI_MAX_DEVICES];
BridgeRecordTable g_BridgeRecordTable[PCI_MAX_BUS];
DWORD g_dwRecordInx = 0;

#pragma pack()

extern AML_PIRQ_IRQ_TBL g_PirqTbl[MAX_PIRQ_NUM];

enum { NUM_IRQS = 0x10 };
static pIrqLink irqToLinkValue[NUM_IRQS];

// limit the pool to 1 page in size
enum { IRQ_LINK_POOL_SIZE = ((VM_PAGE_SIZE / (sizeof(irqLink))) - NUM_IRQS) };
// Simple allocation pool for the irqToLinkValue table
static irqLink irqLinkPool[IRQ_LINK_POOL_SIZE];

//------------------------------------------------------------------------------
//
//  Function:  AllocFreeIrqLink
//
//  allocates an unused link from the allocation pool.  Note that there is no way to dealloc as it is
//  expected that this table will only need to be set up once.
//
static pIrqLink AllocFreeIrqLink()
{
    static unsigned int irqLinkPoolCounter = 0;
    if(irqLinkPoolCounter >= (IRQ_LINK_POOL_SIZE - 1))
    {
        // We've run out of memory in our pool
        OALMSG(OAL_ERROR, (L"irqLinkPoolCounter (%d) exceeded allocation pool size!\r\n", irqLinkPoolCounter));
        return 0;
    }
    else
    {
        // Allocate the next free block
        irqLinkPoolCounter++;
        return &(irqLinkPool[irqLinkPoolCounter]);
    }
}

//------------------------------------------------------------------------------
//
//  Function:  AddIrqLink
//
//  allocates an irqLink at inIrqLink and fills the value with the link number
//
static BOOL AddIrqLink(
                       __inout pIrqLink* inIrqLink,
                       DWORD inIrq,
                       BYTE inLinkNumber,
                       BYTE inBus,
                       BYTE inDevice,
                       BYTE inFunc
                       )
{
    *inIrqLink = AllocFreeIrqLink();
    if(!(*inIrqLink))
    {
        OALMSG(OAL_ERROR, (L"Failed allocation for IrqToLink, link %x will not be associated with Irq %d\r\n", inLinkNumber, inIrq));
    }
    else
    {
        // Allocation successful, update the entry with the new value
        (*inIrqLink)->linkValue = inLinkNumber;
        (*inIrqLink)->bus = inBus;
        (*inIrqLink)->device = inDevice;
        (*inIrqLink)->func = inFunc;
        OALMSG(OAL_PCI,(L"AddIrqLink: LinkNumber=%x,bus=%d,device=%d,func=%d associated with irq=%d\r\n",inLinkNumber,inBus,inDevice,inFunc,inIrq));
        return TRUE;
    }
    return FALSE;
}

static IRQRountingOptionsBuffer irqRoutingOptionBuffer;
static WORD wBestPCIIrq=0;


//------------------------------------------------------------------------------
//
//  Function:  DumpRoutingOption
//
//
//
static void DumpRoutingOption(
                              __in const IRQRountingOptionsBuffer * const pBuffer,
                              WORD wExClusive
                              )
{
    OALMSG(OAL_PCI,(L"DumpRoutingOption with PCI Exclusive Irq Bit (wExClusive)  =%x \r\n",wExClusive));
    if (pBuffer)
    {
        const RoutingOptionTable * pRoute = (pBuffer->routingOptionTable);
        DWORD dwCurPos=0;
        OALMSG(OAL_PCI,(L"DumpRoutingOption BufferSize = %d @ address %x \r\n", pBuffer->BufferSize,pBuffer->pDataBuffer));
        while (pRoute && dwCurPos + sizeof(RoutingOptionTable)<=pBuffer->BufferSize)
        {
            OALMSG(OAL_PCI,(L"DumpRouting for Bus=%d ,Device=%d SlotNumber=%d\r\n",
                pRoute->PCIBusNumber,(pRoute->PCIDeviceNumber)>>3,pRoute->SlotNumber));
            OALMSG(OAL_PCI,(L"     INTA_LinkValue=%x,INTA_IrqBitMap=%x\r\n",pRoute->INTA_LinkValue,pRoute->INTA_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTB_LinkValue=%x,INTB_IrqBitMap=%x\r\n",pRoute->INTB_LinkValue,pRoute->INTB_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTC_LinkValue=%x,INTC_IrqBitMap=%x\r\n",pRoute->INTC_LinkValue,pRoute->INTC_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTD_LinkValue=%x,INTC_IrqBitMap=%x\r\n",pRoute->INTD_LinkValue,pRoute->INTD_IrqBitMap));
            dwCurPos +=sizeof(RoutingOptionTable);
            pRoute ++;
        }
    }
}

//------------------------------------------------------------------------------
//
//  Function:  ScanConfiguredIrq
//
//
//
static void ScanConfiguredIrq(
                              __in const IRQRountingOptionsBuffer *pBuffer
                              )
{
    DWORD Irq;
    DWORD Pin;
#ifdef BSP_UEFI_DEBUG
       UINT  i;
#endif // BSP_UEFI_DEBUG

    memset(irqToLinkValue, 0, sizeof(irqToLinkValue));
    memset(irqLinkPool, 0, sizeof(irqLinkPool));

    if (pBuffer)
    {
        const RoutingOptionTable * pRoute = (const RoutingOptionTable * )(pBuffer->pDataBuffer);
        DWORD dwCurPos=0;

        OALMSG(OAL_PCI,(L"ScanConfigureIrq: BufferSize = %d @ address %x \r\n", pBuffer->BufferSize,pBuffer->pDataBuffer));
        while (pRoute && dwCurPos + sizeof(RoutingOptionTable)<=pBuffer->BufferSize)
        {
            BOOL isMultiFunc = FALSE;
            DWORD dwFunc;
            OALMSG(OAL_PCI,(L"ScanConfigureIrq: for Bus=%d ,Device=%d SlotNumber=%d\r\n",
                pRoute->PCIBusNumber,(pRoute->PCIDeviceNumber)>>3,pRoute->SlotNumber));
            OALMSG(OAL_PCI,(L"     INTA_LinkValue=%x,INTA_IrqBitMap=%x\r\n",pRoute->INTA_LinkValue,pRoute->INTA_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTB_LinkValue=%x,INTB_IrqBitMap=%x\r\n",pRoute->INTB_LinkValue,pRoute->INTB_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTC_LinkValue=%x,INTC_IrqBitMap=%x\r\n",pRoute->INTC_LinkValue,pRoute->INTC_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTD_LinkValue=%x,INTD_IrqBitMap=%x\r\n",pRoute->INTD_LinkValue,pRoute->INTD_IrqBitMap));
            for (dwFunc=0;dwFunc<8;dwFunc++)
            {
                PCI_COMMON_CONFIG pciConfig;
                DWORD dwLength;
                pciConfig.VendorID = 0xFFFF;
                pciConfig.HeaderType = 0;

                dwLength=PCIReadBusData(pRoute->PCIBusNumber,(pRoute->PCIDeviceNumber)>>3,dwFunc,
                          &pciConfig,0,sizeof(pciConfig) - sizeof(pciConfig.DeviceSpecific));

                if (dwFunc==0)
                {
                    isMultiFunc = (pciConfig.HeaderType & PCI_MULTIFUNCTION)?TRUE:FALSE;
                }

                if (dwLength != (sizeof(pciConfig) - sizeof(pciConfig.DeviceSpecific)) ||
                        (pciConfig.DeviceID == PCI_INVALID_DEVICEID) || (pciConfig.VendorID == PCI_INVALID_VENDORID) || (pciConfig.VendorID == 0))
                {
                    if (dwFunc == 0)
                    {
                        // If not a multi-function device, continue to next device
                        if (!isMultiFunc)
                        {
                            OALMSG(OAL_PCI,(L"PCI is not multi function, so go next device!!!!\r\n"));
                            break;
                        }
                    }
                    continue;
                }
#ifdef BSP_UEFI_DEBUG              
                if((pciConfig.HeaderType & ~PCI_MULTIFUNCTION) == 0) 
                {
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.HeaderType == 0!!!\r\n"));
                    OALMSG(OAL_UEFI_DBG,(L"VendorID      =  0x%X\r\n", pciConfig.VendorID));
                    OALMSG(OAL_UEFI_DBG,(L"DeviceID      =  0x%X\r\n", pciConfig.DeviceID));
                    OALMSG(OAL_UEFI_DBG,(L"Command       =  0x%X\r\n", pciConfig.Command));
                    OALMSG(OAL_UEFI_DBG,(L"Status        =  0x%X\r\n", pciConfig.Status));
                    OALMSG(OAL_UEFI_DBG,(L"RevisionID    =  0x%X\r\n", pciConfig.RevisionID));
                    OALMSG(OAL_UEFI_DBG,(L"ProgIf        =  0x%X\r\n", pciConfig.ProgIf));
                    OALMSG(OAL_UEFI_DBG,(L"SubClass      =  0x%X\r\n", pciConfig.SubClass));
                    OALMSG(OAL_UEFI_DBG,(L"BaseClass     =  0x%X\r\n", pciConfig.BaseClass));
                    OALMSG(OAL_UEFI_DBG,(L"CacheLineSize =  0x%X\r\n", pciConfig.CacheLineSize));
                    OALMSG(OAL_UEFI_DBG,(L"LatencyTimer  =  0x%X\r\n", pciConfig.LatencyTimer));
                    OALMSG(OAL_UEFI_DBG,(L"HeaderType    =  0x%X\r\n", pciConfig.HeaderType));
                    OALMSG(OAL_UEFI_DBG,(L"BIST          =  0x%X\r\n", pciConfig.BIST));
                    for(i = 0; i < PCI_TYPE0_ADDRESSES; i++)
                    {
                        OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.BaseAddresses[%d]   =  0x%X\r\n", i, pciConfig.u.type0.BaseAddresses[i]));
                    }

                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.CIS                     =  0x%X\r\n", pciConfig.u.type0.CIS));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.SubVendorID             =  0x%X\r\n", pciConfig.u.type0.SubVendorID));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.SubSystemID             =  0x%X\r\n", pciConfig.u.type0.SubSystemID));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.ROMBaseAddress          =  0x%X\r\n", pciConfig.u.type0.ROMBaseAddress));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.InterruptLine           =  0x%X\r\n", pciConfig.u.type0.InterruptLine));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.InterruptPin            =  0x%X\r\n", pciConfig.u.type0.InterruptPin));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.MinimumGrant            =  0x%X\r\n", pciConfig.u.type0.MinimumGrant));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type0.MaximumLatency          =  0x%X\r\n", pciConfig.u.type0.MaximumLatency));
                }
                else if((pciConfig.HeaderType & ~PCI_MULTIFUNCTION) == 0x01) 
                {
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.HeaderType == 0x01!!!\r\n"));         
                    OALMSG(OAL_UEFI_DBG,(L"VendorID      =  0x%X\r\n", pciConfig.VendorID));
                    OALMSG(OAL_UEFI_DBG,(L"DeviceID      =  0x%X\r\n", pciConfig.DeviceID));
                    OALMSG(OAL_UEFI_DBG,(L"Command       =  0x%X\r\n", pciConfig.Command));
                    OALMSG(OAL_UEFI_DBG,(L"Status        =  0x%X\r\n", pciConfig.Status));
                    OALMSG(OAL_UEFI_DBG,(L"RevisionID    =  0x%X\r\n", pciConfig.RevisionID));
                    OALMSG(OAL_UEFI_DBG,(L"ProgIf        =  0x%X\r\n", pciConfig.ProgIf));
                    OALMSG(OAL_UEFI_DBG,(L"SubClass      =  0x%X\r\n", pciConfig.SubClass));
                    OALMSG(OAL_UEFI_DBG,(L"BaseClass     =  0x%X\r\n", pciConfig.BaseClass));
                    OALMSG(OAL_UEFI_DBG,(L"CacheLineSize =  0x%X\r\n", pciConfig.CacheLineSize));
                    OALMSG(OAL_UEFI_DBG,(L"LatencyTimer  =  0x%X\r\n", pciConfig.LatencyTimer));
                    OALMSG(OAL_UEFI_DBG,(L"HeaderType    =  0x%X\r\n", pciConfig.HeaderType));
                    OALMSG(OAL_UEFI_DBG,(L"BIST          =  0x%X\r\n", pciConfig.BIST));
                    
                    for(i = 0; i < PCI_TYPE1_ADDRESSES; i++)
                    {
                        OALMSG(1,(L"pciConfig.u.type1.BaseAddresses[%d]           =  0x%X\r\n", i, pciConfig.u.type1.BaseAddresses[i]));
                    } 

                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.PrimaryBusNumber                =  0x%X\r\n", pciConfig.u.type1.PrimaryBusNumber));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.SecondaryBusNumber              =  0x%X\r\n", pciConfig.u.type1.SecondaryBusNumber));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.SubordinateBusNumber            =  0x%X\r\n", pciConfig.u.type1.SubordinateBusNumber));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.SecondaryLatencyTimer           =  0x%X\r\n", pciConfig.u.type1.SecondaryLatencyTimer));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.IOBase                          =  0x%X\r\n", pciConfig.u.type1.IOBase));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.IOLimit                         =  0x%X\r\n", pciConfig.u.type1.IOLimit));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.SecondaryStatus                 =  0x%X\r\n", pciConfig.u.type1.SecondaryStatus));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.MemoryBase                      =  0x%X\r\n", pciConfig.u.type1.MemoryBase));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.MemoryLimit                     =  0x%X\r\n", pciConfig.u.type1.MemoryLimit));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.PrefetchableMemoryBase          =  0x%X\r\n", pciConfig.u.type1.PrefetchableMemoryBase));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.PrefetchableMemoryLimit         =  0x%X\r\n", pciConfig.u.type1.PrefetchableMemoryLimit));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.PrefetchableMemoryBaseUpper32   =  0x%X\r\n", pciConfig.u.type1.PrefetchableMemoryBaseUpper32));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.IOBaseUpper                     =  0x%X\r\n", pciConfig.u.type1.IOBaseUpper));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.IOLimitUpper                    =  0x%X\r\n", pciConfig.u.type1.IOLimitUpper));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.Reserved2                       =  0x%X\r\n", pciConfig.u.type1.Reserved2));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.ExpansionROMBase                =  0x%X\r\n", pciConfig.u.type1.ExpansionROMBase));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.InterruptLine                   =  0x%X\r\n", pciConfig.u.type1.InterruptLine));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.InterruptPin                    =  0x%X\r\n", pciConfig.u.type1.InterruptPin));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type1.BridgeControl                   =  0x%X\r\n", pciConfig.u.type1.BridgeControl));       
                }
                else if((pciConfig.HeaderType & ~PCI_MULTIFUNCTION) == 0x02) 
                {
                    OALMSG(OAL_UEFI_DBG,(L"VendorID      =  0x%X\r\n", pciConfig.VendorID));
                    OALMSG(OAL_UEFI_DBG,(L"DeviceID      =  0x%X\r\n", pciConfig.DeviceID));
                    OALMSG(OAL_UEFI_DBG,(L"Command       =  0x%X\r\n", pciConfig.Command));
                    OALMSG(OAL_UEFI_DBG,(L"Status        =  0x%X\r\n", pciConfig.Status));
                    OALMSG(OAL_UEFI_DBG,(L"RevisionID    =  0x%X\r\n", pciConfig.RevisionID));
                    OALMSG(OAL_UEFI_DBG,(L"ProgIf        =  0x%X\r\n", pciConfig.ProgIf));
                    OALMSG(OAL_UEFI_DBG,(L"SubClass      =  0x%X\r\n", pciConfig.SubClass));
                    OALMSG(OAL_UEFI_DBG,(L"BaseClass     =  0x%X\r\n", pciConfig.BaseClass));
                    OALMSG(OAL_UEFI_DBG,(L"CacheLineSize =  0x%X\r\n", pciConfig.CacheLineSize));
                    OALMSG(OAL_UEFI_DBG,(L"LatencyTimer  =  0x%X\r\n", pciConfig.LatencyTimer));
                    OALMSG(OAL_UEFI_DBG,(L"HeaderType    =  0x%X\r\n", pciConfig.HeaderType));
                    OALMSG(OAL_UEFI_DBG,(L"BIST          =  0x%X\r\n", pciConfig.BIST));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.BaseAddresses        =  0x%X\r\n", pciConfig.u.type2.BaseAddress)); 
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.CapabilitiesPtr      =  0x%X\r\n", pciConfig.u.type2.CapabilitiesPtr));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.SecondaryStatus      =  0x%X\r\n", pciConfig.u.type2.SecondaryStatus));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.PrimaryBusNumber     =  0x%X\r\n", pciConfig.u.type2.PrimaryBusNumber ));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.CardbusBusNumber     =  0x%X\r\n", pciConfig.u.type2.CardbusBusNumber));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.SubordinateBusNumber =  0x%X\r\n", pciConfig.u.type2.SubordinateBusNumber));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.MemoryBase0          =  0x%X\r\n", pciConfig.u.type2.MemoryBase0));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.MemoryLimit0         =  0x%X\r\n", pciConfig.u.type2.MemoryLimit0));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.MemoryBase1          =  0x%X\r\n", pciConfig.u.type2.MemoryLimit1));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.MemoryLimit1         =  0x%X\r\n", pciConfig.u.type1.MemoryBase));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOBase0_LO           =  0x%X\r\n", pciConfig.u.type2.IOBase0_LO));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOBase0_HI           =  0x%X\r\n", pciConfig.u.type2.IOBase0_HI));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOLimit0_LO          =  0x%X\r\n", pciConfig.u.type2.IOLimit0_LO));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOLimit0_HI          =  0x%X\r\n", pciConfig.u.type2.IOLimit0_HI));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOBase1_LO           =  0x%X\r\n", pciConfig.u.type2.IOBase1_LO));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOBase1_HI           =  0x%X\r\n", pciConfig.u.type2.IOBase1_HI));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOLimit1_LO          =  0x%X\r\n", pciConfig.u.type2.IOLimit1_LO));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IOLimit1_HI          =  0x%X\r\n", pciConfig.u.type2.IOLimit1_HI));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.InterruptLine        =  0x%X\r\n", pciConfig.u.type2.InterruptLine));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.InterruptPin         =  0x%X\r\n", pciConfig.u.type2.InterruptPin));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.BridgeControl        =  0x%X\r\n", pciConfig.u.type2.BridgeControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.SubVendorID          =  0x%X\r\n", pciConfig.u.type2.SubVendorID));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.SubSystemID          =  0x%X\r\n", pciConfig.u.type2.SubSystemID));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.LegacyBaseAddress    =  0x%X\r\n", pciConfig.u.type2.LegacyBaseAddress));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.SystemControl        =  0x%X\r\n", pciConfig.u.type2.SystemControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.MultiMediaControl    =  0x%X\r\n", pciConfig.u.type2.MultiMediaControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.GeneralStatus        =  0x%X\r\n", pciConfig.u.type2.GeneralStatus));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.GPIO0Control         =  0x%X\r\n", pciConfig.u.type2.GPIO0Control));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.GPIO1Control         =  0x%X\r\n", pciConfig.u.type2.GPIO1Control));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.GPIO2Control         =  0x%X\r\n", pciConfig.u.type2.GPIO2Control));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.GPIO3Control         =  0x%X\r\n", pciConfig.u.type2.GPIO3Control));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.IRQMuxRouting        =  0x%X\r\n", pciConfig.u.type2.IRQMuxRouting));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.RetryStatus          =  0x%X\r\n", pciConfig.u.type2.CardControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.CardControl          =  0x%X\r\n", pciConfig.u.type1.BridgeControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.DeviceControl        =  0x%X\r\n", pciConfig.u.type2.DeviceControl));
                    OALMSG(OAL_UEFI_DBG,(L"pciConfig.u.type2.Diagnostic           =  0x%X\r\n", pciConfig.u.type2.Diagnostic));
                }
#endif //BSQR_DEBUG

                Irq=(DWORD)-1;
                Pin=0;
                switch( pciConfig.HeaderType & ~PCI_MULTIFUNCTION)
                {
                    case PCI_DEVICE_TYPE: // Devices
                        Irq = pciConfig.u.type0.InterruptLine;
                        Pin = pciConfig.u.type0.InterruptPin;
                        break;
                    case PCI_BRIDGE_TYPE: // PCI-PCI bridge
                        Irq = pciConfig.u.type1.InterruptLine;
                        Pin = pciConfig.u.type1.InterruptPin;
                        break;

                    case PCI_CARDBUS_TYPE: // PCI-Cardbus bridge
                        Irq = pciConfig.u.type2.InterruptLine;
                        Pin = pciConfig.u.type2.InterruptPin;
                        break;
                }

                if (Irq<=0xf)
                {
                    BYTE bLinkNumber=0;
                    switch (Pin)
                    {
                        case 1:
                            bLinkNumber=pRoute->INTA_LinkValue;
                            break;
                        case 2:
                            bLinkNumber=pRoute->INTB_LinkValue;
                            break;
                        case 3:
                            bLinkNumber=pRoute->INTC_LinkValue;
                            break;
                        case 4:
                            bLinkNumber=pRoute->INTD_LinkValue;
                            break;
                    }

                    if (bLinkNumber!=0)
                    {
                        if (irqToLinkValue[Irq])
                        {
                            // Already some links associated with this irq

                            // Search the list for a duplicate entry
                            pIrqLink currentIrqToLink = irqToLinkValue[Irq];
                            BOOL matchFound = FALSE;
                            do
                            {
                                if(currentIrqToLink->linkValue == bLinkNumber &&
                                   currentIrqToLink->bus == pRoute->PCIBusNumber &&
                                   currentIrqToLink->device == ((pRoute->PCIDeviceNumber)>>3) &&
                                   currentIrqToLink->func == dwFunc)
                                {
                                    matchFound = TRUE;
                                }
                                if(currentIrqToLink->pNext)
                                {
                                    currentIrqToLink = currentIrqToLink->pNext;
                                }
                                else
                                {
                                    break;
                                }
                            } while (!matchFound);

                            if(!matchFound)
                            {
                                // No association for this link with this irq, so add a new entry to pNext
                                AddIrqLink(&(currentIrqToLink->pNext), Irq, bLinkNumber, pRoute->PCIBusNumber, ((pRoute->PCIDeviceNumber)>>3), (BYTE)dwFunc);
                            }
                        }
                        else
                        {
                            // No links associated with this irq, add a new entry to the list
                            AddIrqLink(&(irqToLinkValue[Irq]), Irq, bLinkNumber, pRoute->PCIBusNumber, ((pRoute->PCIDeviceNumber)>>3), (BYTE)dwFunc);
                        }
                    }
                }

                if (!isMultiFunc)
                {
#ifdef ENABLE_NM10
					//continue scan the next device
					continue;
#else
				    //Not multi-function card.
                    break;
#endif
                }
            }
            dwCurPos +=sizeof(RoutingOptionTable);
            pRoute ++;
        }
    }
}


//------------------------------------------------------------------------------
//
//  Function:  AssingDevicePirq
//
// This function is to assign PIRQ for a device, the rule is follow the
// PCI to PCI Bridge specification
//
static void AssingDevicePirq(
                             BYTE   bBusNum,
                             DWORD  dwDevNum,
                             DWORD  dwFunc,
                             BYTE   bType,
                             BYTE   bLinkX,
                             __inout PRT_TABLE*  pPirqLinkTable
                            )
{

    
    BYTE ParentBus = 0;
    BYTE bPirq = PIRQA;
    BYTE DevStartPirq = 0;
    BOOL bFoundParent = FALSE; 
    UINT i;
    PCI_COMMON_CONFIG pciConfig;
    pciConfig.VendorID = 0xFFFF;
    pciConfig.HeaderType = 0;

    OALMSG(OAL_PCI,(L"+AssingDevicePirq(): Bus%d Dev%d Func%d\r\n",
        bBusNum, dwDevNum, dwFunc));

    PCIReadBusData(bBusNum, dwDevNum, dwFunc,
        &pciConfig,0,sizeof(pciConfig) - sizeof(pciConfig.DeviceSpecific));

    ParentBus = pciConfig.u.type1.PrimaryBusNumber;

    // Search Primary bus number
    for(i = 0; i < g_dwRecordInx; i++) 
    {
        if(g_BridgeRecordTable[i].PrimaryBusNumber == ParentBus) 
        {
            DevStartPirq = g_BridgeRecordTable[i].BridgeDevINTA_StartLinkValue;
            OALMSG(OAL_PCI,(L"AssingDevicePirq(): Found in ParentBus%d Dev%d " 
            L"of PriBusNum Bus%d, and its INTA start PIRQ = %d\r\n",
            g_BridgeRecordTable[i].PCIBusNumber, g_BridgeRecordTable[i].PCIDeviceNumber,
            g_BridgeRecordTable[i].PrimaryBusNumber, DevStartPirq));
            bFoundParent = TRUE;
            break;
        }
    }

    // if it is failed to search Primary bus number, we should to find type 1
    // of SecondaryBusNumber or type2 of CardbusBusNumber to see if device is
    // under thoese Bus.
    if(!bFoundParent) 
    {
        for(i = 0; i < g_dwRecordInx; i++) 
        {
            if(bType == PCI_BRIDGE_TYPE) 
            {
                if(g_BridgeRecordTable[i].SecondaryBusNumber == ParentBus) 
                {
                    DevStartPirq = g_BridgeRecordTable[i].BridgeDevINTA_StartLinkValue;
                    OALMSG(OAL_PCI,(L"AssingDevicePirq(): Found in ParentBus%d "
                    L"Dev%d of SecBusNum Bus%d, and its PIRQ = %d\r\n",
                    g_BridgeRecordTable[i].PCIBusNumber, g_BridgeRecordTable[i].PCIDeviceNumber,
                    g_BridgeRecordTable[i].PrimaryBusNumber, DevStartPirq));
                    bFoundParent = TRUE;
                    break;
                }
            }
            else if(bType == PCI_CARDBUS_TYPE) 
            {
                if(g_BridgeRecordTable[i].CardbusBusNumber == ParentBus) 
                {
                    DevStartPirq = g_BridgeRecordTable[i].BridgeDevINTA_StartLinkValue;
                    OALMSG(OAL_PCI,(L"AssingDevicePirq(): Found in ParentBus%d "
                    L"Dev%d of CardBusNum Bus%d, and its PIRQ = %d\r\n",
                    g_BridgeRecordTable[i].PCIBusNumber, g_BridgeRecordTable[i].PCIDeviceNumber,
                    g_BridgeRecordTable[i].PrimaryBusNumber, DevStartPirq));
                    bFoundParent = TRUE;
                    break;
                }
            }
        }
    }

    // if DevStartPirq == PIRQA , it means this bus Link sequence is INTA-PIRQA, INTB-PIRQB, INTC-PIRQC, INTD-PIRQD.
    // if DevStartPirq == PIRQB , it means this bus Link sequence is INTA-PIRQB, INTB-PIRQC, INTC-PIRQD, INTD-PIRQA.
    // if DevStartPirq == PIRQC , it means this bus Link sequence is INTA-PIRQC, INTB-PIRQD, INTC-PIRQA, INTD-PIRQB.
    // if DevStartPirq == PIRQD , it means this bus Link sequence is INTA-PIRQD, INTB-PIRQA, INTC-PIRQB, INTD-PIRQC.
    if(DevStartPirq && bFoundParent) 
    {
        if(DevStartPirq == PIRQA) 
        {
            switch( dwDevNum%4 ) 
            { 
                case 0: //4n
                    if(bLinkX == 1) bPirq = PIRQA;
                    if(bLinkX == 2) bPirq = PIRQB;
                    if(bLinkX == 3) bPirq = PIRQC;
                    if(bLinkX == 4) bPirq = PIRQD;
                    break;
                 case 1: //4n+1
                    if(bLinkX == 1) bPirq = PIRQB;
                    if(bLinkX == 2) bPirq = PIRQC;
                    if(bLinkX == 3) bPirq = PIRQD;
                    if(bLinkX == 4) bPirq = PIRQA;
                    break;
                case 2: //4n+2
                    if(bLinkX == 1) bPirq = PIRQC;
                    if(bLinkX == 2) bPirq = PIRQD;
                    if(bLinkX == 3) bPirq = PIRQA;
                    if(bLinkX == 4) bPirq = PIRQB;
                    break;
                case 3: //4n+2
                    if(bLinkX == 1) bPirq = PIRQD;
                    if(bLinkX == 2) bPirq = PIRQA;
                    if(bLinkX == 3) bPirq = PIRQB;
                    if(bLinkX == 4) bPirq = PIRQC;
                    break;
            }
        }
        else if(DevStartPirq == PIRQB) 
        {
            switch(dwDevNum%4) 
            {
                case 0: //4n
                    if(bLinkX == 1) bPirq = PIRQB;
                    if(bLinkX == 2) bPirq = PIRQC;
                    if(bLinkX == 3) bPirq = PIRQD;
                    if(bLinkX == 4) bPirq = PIRQA;
                    break;
                case 1: //4n+1
                    if(bLinkX == 1) bPirq = PIRQC;
                    if(bLinkX == 2) bPirq = PIRQD;
                    if(bLinkX == 3) bPirq = PIRQA;
                    if(bLinkX == 4) bPirq = PIRQB;
                    break;
                case 2: //4n+2
                    if(bLinkX == 1) bPirq = PIRQD;
                    if(bLinkX == 2) bPirq = PIRQA;
                    if(bLinkX == 3) bPirq = PIRQB;
                    if(bLinkX == 4) bPirq = PIRQC;
                    break;
                case 3: //4n+3
                    if(bLinkX == 1) bPirq = PIRQA;
                    if(bLinkX == 2) bPirq = PIRQB;
                    if(bLinkX == 3) bPirq = PIRQC;
                    if(bLinkX == 4) bPirq = PIRQD;
                    break;
            }
        }
        else if(DevStartPirq == PIRQC) 
        {
             switch(dwDevNum%4) 
             {
                case 0: //4n
                    if(bLinkX == 1) bPirq = PIRQC;
                    if(bLinkX == 2) bPirq = PIRQD;
                    if(bLinkX == 3) bPirq = PIRQA;
                    if(bLinkX == 4) bPirq = PIRQB;
                    break;
                case 1: //4n+1
                    if(bLinkX == 1) bPirq = PIRQD;
                    if(bLinkX == 2) bPirq = PIRQA;
                    if(bLinkX == 3) bPirq = PIRQB;
                    if(bLinkX == 4) bPirq = PIRQC;
                    break;
                case 2: //4n+2
                    if(bLinkX == 1) bPirq = PIRQA;
                    if(bLinkX == 2) bPirq = PIRQB;
                    if(bLinkX == 3) bPirq = PIRQC;
                    if(bLinkX == 4) bPirq = PIRQD;
                    break;
                case 3: //4n+3
                    if(bLinkX == 1) bPirq = PIRQB;
                    if(bLinkX == 2) bPirq = PIRQC;
                    if(bLinkX == 3) bPirq = PIRQD;
                    if(bLinkX == 4) bPirq = PIRQA;
                    break;
            }
        }
        else if(DevStartPirq == PIRQD)
        {
            switch(dwDevNum%4) 
            {
                case 0: //4n
                    if(bLinkX == 1) bPirq = PIRQD;
                    if(bLinkX == 2) bPirq = PIRQA;
                    if(bLinkX == 3) bPirq = PIRQB;
                    if(bLinkX == 4) bPirq = PIRQC;
                    break;
                case 1: //4n+1
                    if(bLinkX == 1) bPirq = PIRQA;
                    if(bLinkX == 2) bPirq = PIRQB;
                    if(bLinkX == 3) bPirq = PIRQC;
                    if(bLinkX == 4) bPirq = PIRQD;
                    break;
                case 2: //4n+2
                    if(bLinkX == 1) bPirq = PIRQB;
                    if(bLinkX == 2) bPirq = PIRQC;
                    if(bLinkX == 3) bPirq = PIRQD;
                    if(bLinkX == 4) bPirq = PIRQA;
                    break;
                case 3: //4n+3
                    if(bLinkX == 1) bPirq = PIRQC;
                    if(bLinkX == 2) bPirq = PIRQD;
                    if(bLinkX == 3) bPirq = PIRQA;
                    if(bLinkX == 4) bPirq = PIRQB;
                    break;
            }
        }
        pPirqLinkTable->DevAssignedLinkValue = bPirq;
        if(bLinkX == 1) pPirqLinkTable->BridgeDevINTA_StartLinkValue = bPirq;
        if(bLinkX == 2) pPirqLinkTable->BridgeDevINTB_StartLinkValue = bPirq;
        if(bLinkX == 3) pPirqLinkTable->BridgeDevINTC_StartLinkValue = bPirq;
        if(bLinkX == 4) pPirqLinkTable->BridgeDevINTD_StartLinkValue = bPirq;
    }


    OALMSG(OAL_INFO && OAL_PCI ,(L"-AssingDevicePirq(): dBus%d Dev%d Func%d Returned PIRQ = %d\r\n",
        bBusNum, dwDevNum, dwFunc, bPirq));
}

//------------------------------------------------------------------------------
//
//  Function:  GetPciConfiguredBusNumber
//
//
//
static BYTE GetPciConfiguredBusNumber()
{
    PCI_SLOT_NUMBER     slotNumber;
    PCI_COMMON_CONFIG   pciConfig;
    DWORD               dwBus = 0, dwDev, dwFunc;
    UINT                uLength;
    BYTE                bMaxBus = 0;


    for (dwDev = 0; dwDev < PCI_MAX_DEVICES; dwDev++) 
    {
        slotNumber.u.bits.DeviceNumber = dwDev;
        for (dwFunc = 0; dwFunc < PCI_MAX_FUNCTION; dwFunc++) 
        {
            slotNumber.u.bits.FunctionNumber = dwFunc;
            uLength = PCIGetBusDataByOffset(dwBus, slotNumber.u.AsULONG, &pciConfig, 0, sizeof(pciConfig));

            if (uLength == 0 || pciConfig.VendorID == 0xFFFF)
            {
                break;
            }

            if (pciConfig.BaseClass == PCI_CLASS_BRIDGE_DEV &&
                pciConfig.SubClass == PCI_SUBCLASS_BR_PCI_TO_PCI)
            {
                if (pciConfig.u.type1.SubordinateBusNumber > bMaxBus) 
                {
                    bMaxBus = pciConfig.u.type1.SubordinateBusNumber;
                }
            }

            if (dwFunc == 0 && !(pciConfig.HeaderType & 0x80))
            {
                break;
            }
        }
        if (uLength == 0)
        {
            break;
        }
    }

    return bMaxBus;
}

//------------------------------------------------------------------------------
//
//  Function:  FindIrqForPirqx
//
//
//
static BYTE FindIrqForPirqx(BYTE bPirq)
{
    BYTE bRtnIrq = 0xFF;

    if ( bPirq > 0 && bPirq < MAX_PIRQ_NUM)
    {
        if(g_Irq2PirqTbl[bPirq-1].bPIRQX == bPirq)
        {
            bRtnIrq = g_Irq2PirqTbl[bPirq-1].bIRQX;
        }
    }
    OALMSG(OAL_UEFI_DBG,(L"FindIrqForPirqx: returned PIRQ%d of IRQ number = %d\r\n", bPirq, bRtnIrq));
    return bRtnIrq;
}

//------------------------------------------------------------------------------
//
//  Function:  CreatePciRoutingOptionTable
//
// This function is to create a PCI routing Option Table by enumerate devices that
// attached on PCI bus except sub-function, during enumerate device, this function
// also to record PCI bridge information, assign IRQ and set Command regiaster as 0x07
// to each device if need.
//
static BOOL CreatePciRoutingOptionTable(
                                        BYTE maxbus,
                                        __inout RoutingOptionTable* pRoute,
                                        __inout DWORD* pdwIndex
                                       )
{
    BOOL  bRC = FALSE;
    DWORD dwCurIndex = 0;
    DWORD dwRtnindex = 0;
    BYTE  bBusNumber = 0;
    BYTE  bIntPinNum = 0;
    BYTE  bIrqNum, bType;
    PRT_TABLE PirqLinkTable;
    PRT_TABLE *pPirqLinkTable = NULL;
    DWORD dwDev, dwFunc;
    BYTE bIntLine = 0;
    
    if (pRoute)
    {
        OALMSG(OAL_INFO && OAL_PCI,(L"CreatePciRoutingOptionTable: maxbus = %d\r\n", maxbus));
        for (bBusNumber; bBusNumber <= maxbus; bBusNumber++)
        {
            for (dwDev = 0; dwDev < PCI_MAX_DEVICES; dwDev++) 
            {
                 BOOL isMultiFunc = FALSE;
                 bIrqNum = 0;
                 pPirqLinkTable = &PirqLinkTable;
                 memset(pPirqLinkTable, 0, sizeof(PRT_TABLE));
                 bType = 0;
#ifndef ENABLE_NM10

                 dwCurIndex = dwRtnindex;
#endif
                 for (dwFunc = 0; dwFunc < PCI_MAX_FUNCTION; dwFunc++) 
                 {
                    DWORD dwLength;
                    PCI_COMMON_CONFIG pciConfig;
                    pciConfig.VendorID = 0xFFFF;
                    pciConfig.HeaderType = 0;
                    bIntPinNum = 0;

                    dwLength=PCIReadBusData(bBusNumber, dwDev, dwFunc,
                        &pciConfig,0,sizeof(pciConfig) - sizeof(pciConfig.DeviceSpecific));

                    if (dwLength == 0 || pciConfig.VendorID == 0xFFFF)
                    {
#ifdef ENABLE_NM10
						continue;
#else
                        break;
#endif
                    }
#ifndef ENABLE_NM10

                    if(!isMultiFunc)
                    {
                        bRC = TRUE;
                        dwRtnindex++;
                    }
#endif        
                    isMultiFunc = (pciConfig.HeaderType & PCI_MULTIFUNCTION) ? TRUE : FALSE;

                    pRoute[dwCurIndex].PCIBusNumber = bBusNumber;
                    pRoute[dwCurIndex].PCIDeviceNumber = (BYTE) dwDev << 3;
                    pRoute[dwCurIndex].SlotNumber = 0;
                    pRoute[dwCurIndex].Reserved = 0;

                    // To get device type and its IntPin to decide need to
                    // process IRQ assign or not
                    switch( pciConfig.HeaderType & ~PCI_MULTIFUNCTION)
                    {
                        case PCI_DEVICE_TYPE: // Devices
                            bType = PCI_DEVICE_TYPE;
                            bIntPinNum = pciConfig.u.type0.InterruptPin;
                            bIntLine = pciConfig.u.type0.InterruptLine;

                            break;
                        case PCI_BRIDGE_TYPE: // PCI-PCI bridge
                            bType = PCI_BRIDGE_TYPE;
                            bIntPinNum = pciConfig.u.type1.InterruptPin;
                            bIntLine = pciConfig.u.type1.InterruptLine;

                            break;
                        case PCI_CARDBUS_TYPE: // PCI-Cardbus bridge
                            bType = PCI_CARDBUS_TYPE;
                            bIntPinNum = pciConfig.u.type2.InterruptPin;
                            bIntLine = pciConfig.u.type2.InterruptLine;

                    }

                    if(bIntPinNum) 
                    {
                        // If device has list in ASL, use its PIRQ to assing it PIRQ,
                        // if device is not find in table, we should to assign PIRQ
                        // by follow PCI to PCI Bridge specification rule.
#ifdef ENABLE_NM10
                        if(!GetDevicePIRQ((BYTE)bBusNumber, (BYTE)dwDev, (BYTE)bIntPinNum, pPirqLinkTable))
                        {    //PCI bridge doesn't generate interrupt to OS, the child PCI device can be found in the AML. 
                             break;
                        }
#else
                        if(bBusNumber == 0) 
                        {
                            if(!GetDevicePIRQ((BYTE)bBusNumber, (BYTE)dwDev, (BYTE)bIntPinNum, pPirqLinkTable))
                            {
                                break;
                            }
                        }

                        else         
                            AssingDevicePirq(bBusNumber, dwDev, dwFunc, bType, bIntPinNum, pPirqLinkTable);
#endif                        
                        switch(bIntPinNum)
                        {
                            case 1:
                                pRoute[dwCurIndex].INTA_LinkValue = pPirqLinkTable->DevAssignedLinkValue;
                                pRoute[dwCurIndex].INTA_IrqBitMap = 0x5cf8;
                                break;

                            case 2:
                                pRoute[dwCurIndex].INTB_LinkValue = pPirqLinkTable->DevAssignedLinkValue;
                                pRoute[dwCurIndex].INTB_IrqBitMap = 0x5cf8;
                                break;

                            case 3: 
                                pRoute[dwCurIndex].INTC_LinkValue = pPirqLinkTable->DevAssignedLinkValue;
                                pRoute[dwCurIndex].INTC_IrqBitMap = 0x5cf8;
                                break;
                                
                            case 4:
                                pRoute[dwCurIndex].INTD_LinkValue = pPirqLinkTable->DevAssignedLinkValue;
                                pRoute[dwCurIndex].INTD_IrqBitMap = 0x5cf8;
                                break;                           
                        }
                        
                        // If the PIRQ is not 0, we should assign IRQ by ASL identified available IRQ number
                        if(pPirqLinkTable->DevAssignedLinkValue) 
                        {  
                            bIrqNum = FindIrqForPirqx(pPirqLinkTable->DevAssignedLinkValue);
                        }

                        if(bType == PCI_DEVICE_TYPE)  pciConfig.u.type0.InterruptLine = bIrqNum;
                        if(bType == PCI_BRIDGE_TYPE)  pciConfig.u.type1.InterruptLine = bIrqNum;
                        if(bType == PCI_CARDBUS_TYPE) pciConfig.u.type2.InterruptLine = bIrqNum;

                        // If this device is a PCI to PCI Bridge device, we will to record
                        // it information to determine PIRQ for device that under to this bridge 
                        // and has not reported in ASL.
                        if(pciConfig.SubClass == 0x04 && pciConfig.BaseClass == 0x06) 
                        {
                            g_BridgeRecordTable[g_dwRecordInx].PCIBusNumber = bBusNumber;
                            g_BridgeRecordTable[g_dwRecordInx].PCIDeviceNumber = dwDev;
                            g_BridgeRecordTable[g_dwRecordInx].BridgeDevINTA_StartLinkValue = pPirqLinkTable->BridgeDevINTA_StartLinkValue;
                            g_BridgeRecordTable[g_dwRecordInx].BridgeDevINTB_StartLinkValue = pPirqLinkTable->BridgeDevINTB_StartLinkValue;
                            g_BridgeRecordTable[g_dwRecordInx].BridgeDevINTB_StartLinkValue = pPirqLinkTable->BridgeDevINTC_StartLinkValue;
                            g_BridgeRecordTable[g_dwRecordInx].BridgeDevINTC_StartLinkValue = pPirqLinkTable->BridgeDevINTD_StartLinkValue;
                            if((pciConfig.HeaderType & ~PCI_MULTIFUNCTION) == PCI_BRIDGE_TYPE) 
                            {
                                g_BridgeRecordTable[g_dwRecordInx].PrimaryBusNumber = pciConfig.u.type1.PrimaryBusNumber;
                                g_BridgeRecordTable[g_dwRecordInx].SecondaryBusNumber = pciConfig.u.type1.SecondaryBusNumber;
                                g_BridgeRecordTable[g_dwRecordInx].SubordinateBusNumber = pciConfig.u.type1.SubordinateBusNumber;
                            }
                            else if((pciConfig.HeaderType & ~PCI_MULTIFUNCTION) == PCI_CARDBUS_TYPE) 
                            {
                                g_BridgeRecordTable[g_dwRecordInx].PrimaryBusNumber = pciConfig.u.type2.PrimaryBusNumber;
                                g_BridgeRecordTable[g_dwRecordInx].CardbusBusNumber = pciConfig.u.type2.CardbusBusNumber;
                                g_BridgeRecordTable[g_dwRecordInx].SubordinateBusNumber = pciConfig.u.type2.SubordinateBusNumber;
                            }
                            g_dwRecordInx++;
                        }
                    }

                    pciConfig.Command = 0x07;
                    if (bBusNumber==2 && dwFunc == 1 && dwDev == 0) 
                    {
                        pciConfig.Command = 0x06;
                    }
#ifdef ENABLE_NM10
                    //Check against Invalid IRQ to avoid over-write the legacy configuration
                    //This logic also help to prevent CPU reset when over-write the IRQ of the PCI root-point
                    if (INVALID_IRQ == bIntLine) 
#endif
                        PCIWriteBusData(bBusNumber, dwDev, dwFunc, &pciConfig, 0,
                            sizeof(pciConfig) - sizeof(pciConfig.DeviceSpecific));
					

                    if (!isMultiFunc)
                    {
#ifdef ENABLE_NM10
						continue;
#else
                        break;
#endif
                    }
                    // Some Device of subfunction may has different interrupt pin, so we need to
                    // enumerte all functions to configure this PCI Routing Option Table
                   
#ifdef ENABLE_NM10
					dwCurIndex++;
					dwRtnindex++;
				    bRC = TRUE;
#else
					continue;
#endif
                }
            }
        }
    }

#ifdef BSP_PCI_ROUTING_OPTION_TABLE_DEBUG
    if(dwRtnindex) {
        UINT uPresentIndex;
        OALMSG(1,(L"***********   Created PCI Routing Option Table   ***********\r\n"));
        for(uPresentIndex = 0; uPresentIndex < dwRtnindex; uPresentIndex++) 
        {
            OALMSG(1, (L"[%02d]: Bus=%02d  Dev=0x%02X  INT A[0x%02X  0x%04X] ",
                uPresentIndex, pRoute[uPresentIndex].PCIBusNumber, pRoute[uPresentIndex].PCIDeviceNumber >> 3,
                pRoute[uPresentIndex].INTA_LinkValue, pRoute[uPresentIndex].INTA_IrqBitMap));
            OALMSG(1, (L"INTB[0x%02X  0x%04X]  INTC[0x%02X  0x%04X]  ",
                pRoute[uPresentIndex].INTB_LinkValue, pRoute[uPresentIndex].INTB_IrqBitMap,
                pRoute[uPresentIndex].INTC_LinkValue, pRoute[uPresentIndex].INTC_IrqBitMap));
            OALMSG(1, (L"INTD[0x%02X  0x%04X]  Slot=0x%02X  Resr=0x%02X  \r\n",
                pRoute[uPresentIndex].INTD_LinkValue, pRoute[uPresentIndex].INTD_IrqBitMap,
                pRoute[uPresentIndex].SlotNumber, pRoute[uPresentIndex].Reserved));
        }
    }
#endif

    *pdwIndex = dwRtnindex;
    return bRC;
}


//------------------------------------------------------------------------------
//
//  Function:  AssignIrqForAllPirqX
//
//
//
static void AssignIrqForAllPirqX()
{
    int i, j, k, l, mem[15], last;
    BYTE bTmpIrq; 
    WORD wMin;
    BOOL bFoundAssigned, bAssignNew; 
    AML_PIRQ_IRQ_TBL next, aTmpTbl[8];
    
    OALMSG(OAL_INFO && OAL_UEFI_DBG, (L"+AssignIrqForAllPirqX()\r\n"));
    
    memcpy(aTmpTbl, g_PirqTbl, sizeof(aTmpTbl));
   
#ifdef BSP_UEFI_DEBUG
    OALMSG(OAL_UEFI_DBG, (L"***************************   BEFORE *************************************\r\n"));
    OALMSG(OAL_UEFI_DBG, (L"---------- aTmpTbl[  ]:  bPIRQX     bIrqNums     wNumsIrq ----------------\r\n"));
    for( i = 0; i < MAX_PIRQ_NUM ; i++ )
    {
        OALMSG(OAL_UEFI_DBG, (L"           aTmpTbl[%d]     %d          %d          0x%3X   \r\n", i, aTmpTbl[i].bPIRQX, aTmpTbl[i].bIrqNums, aTmpTbl[i].wNumsIrq));
    }
#endif
    // Relocate ASL table to change it IRQ numbers sequence is from min to max
    for( i = 1; i < MAX_PIRQ_NUM ; i++ )
    {
        memcpy(&next, &aTmpTbl[i], sizeof(AML_PIRQ_IRQ_TBL));
        for ( j = i - 1 ; j >= 0 && next.bIrqNums < aTmpTbl[j].bIrqNums ; j-- )
        {
            memcpy(&aTmpTbl[j+1], &aTmpTbl[j], sizeof(AML_PIRQ_IRQ_TBL));
        }    
        memcpy(&aTmpTbl[j+1], &next, sizeof(AML_PIRQ_IRQ_TBL));
        OALMSG(OAL_UEFI_DBG, (L"B:aTmpTbl[%d]    bPIRQX %d    bIrqNums %d     wNumsIrq0x%3X   \r\n", j+1, aTmpTbl[j+1].bPIRQX, aTmpTbl[j+1].bIrqNums, aTmpTbl[j+1].wNumsIrq));
    }
    
#ifdef BSP_UEFI_DEBUG  
    OALMSG(OAL_UEFI_DBG, (L"***************************   AFTER *************************************\r\n"));
    OALMSG(OAL_UEFI_DBG, (L"---------- aTmpTbl[  ]:  bPIRQX     bIrqNums     wNumsIrq ----------------\r\n"));
    for( i = 0; i < MAX_PIRQ_NUM ; i++ )
    { 
        OALMSG(OAL_UEFI_DBG, (L"           aTmpTbl[%d]     %d          %d          0x%3X   \r\n", i, aTmpTbl[i].bPIRQX, aTmpTbl[i].bIrqNums, aTmpTbl[i].wNumsIrq));
    }
#endif  

    // Start assign IRQ to a PIRQ
    for( i = 0; i < MAX_PIRQ_NUM ; i++ )
    {
        k = 0;
        bAssignNew = FALSE;
        memset(mem, 0, sizeof(mem));

        // In 8259, the max IRQ number is 15, and IRQ0 ~ 2 are resvered for system usage
        for(j = 15; j >= 3; j--)   
        {
            bFoundAssigned = FALSE;
            bAssignNew = FALSE;
            if(aTmpTbl[i].wNumsIrq & 0x01 << j)
            {
                OALMSG(OAL_UEFI_DBG, (L" Find irq %d is supported\r\n", j));
                // Normally, the IRQ 12 is for Legacy PS2, and IRQ 4 is for Legacy COM 1, so can't use it
                if(j == 12 || j == 4) 
                {
                    continue;
                }

                // Try to found if the IRQ has been assign or not
                // If it has not, assign this IRQ directly.
                // If it has been assigned, try next available IRQ
                for(k = 0; k <= (int)g_dwInfoInx; k++)
                {
                    if(g_IrqAssignedInfoTbl[k].bIRQX == j) 
                    {
                        OALMSG(OAL_UEFI_DBG, (L"Find irq %d had assigned in g_IrqAssignedInfoTbl[%d].bIRQ%d\r\n", 
                            j, k, g_IrqAssignedInfoTbl[k].bIRQX));
                        bFoundAssigned = TRUE;
                        break;
                    }
                }
                
                // If it has not been assigned, assign this IRQ for PIRQX directly.
                if(!bFoundAssigned)
                {       
                    OALMSG(OAL_UEFI_DBG, (L" not find in g_IrqAssignedInfoTbl, so assign irq %d for PIRQ%d\r\n", 
                        j, aTmpTbl[i].bPIRQX));
                    // To Store PIRQX and it assigned IRQ, minus 1 is because array start from 0,
                    // but PIRQx start from 1. 
                    g_Irq2PirqTbl[aTmpTbl[i].bPIRQX-1].bIRQX = (BYTE) j;
                    g_Irq2PirqTbl[aTmpTbl[i].bPIRQX-1].bPIRQX = aTmpTbl[i].bPIRQX;
                        
                    // To store an IRQ has assigned and count it
                    g_IrqAssignedInfoTbl[g_dwInfoInx].bIRQX = (BYTE) j;
                    g_IrqAssignedInfoTbl[g_dwInfoInx].wIrqAssignCunt++;
                    g_dwInfoInx++;
                    OALMSG(OAL_UEFI_DBG, 
                        (L"A: Add into g_IrqAssignedInfoTbl, the g_dwInfoInxo is %d, wIrqAssignCunt is %d\r\n", 
                        g_dwInfoInx, g_IrqAssignedInfoTbl[g_dwInfoInx].wIrqAssignCunt)); 
                    bAssignNew = TRUE;
                    break;
                }
                else
                {
                    // If this IRQ has been assigned by other, count to remain has a IRQ supported
                    // but not available to assign now.
                    OALMSG(OAL_UEFI_DBG, (L" find in g_IrqAssignedInfoTbl, so not assign irq %d for PIRQ%d now\r\n", j, aTmpTbl[i].bPIRQX));
                    mem[k] = j;
                    k++;
                }
            }
        }
        if(!bAssignNew)
        {
            if(k > 0)
            {
                bTmpIrq = 0;
                // Into here, there are several IRQs has been assigned, 
                // we need to try all record IRQ and find one IRQ is using less.
                OALMSG(OAL_UEFI_DBG, (L" Try find which IRQ is using less\r\n"));
                wMin = 0xFFFF;  //initial it to max IRQ number
                last = 0;
                for(j = 0; j < k; j++)
                {
                    for(l = 0; l <= (int)g_dwInfoInx; l++)
                    {
                        if(mem[j] == g_IrqAssignedInfoTbl[l].bIRQX)
                        {
                            if(wMin > g_IrqAssignedInfoTbl[l].wIrqAssignCunt)
                            {
                                bTmpIrq = g_IrqAssignedInfoTbl[l].bIRQX;
                                wMin = g_IrqAssignedInfoTbl[l].wIrqAssignCunt;
                                last = l;
                                OALMSG(OAL_UEFI_DBG, (L"B: Found now less IRQ %d in g_IrqAssignedInfoTbl[%d], wIrqAssignCunt is %d\r\n",
                                bTmpIrq, l, g_IrqAssignedInfoTbl[l].wIrqAssignCunt));
                                break;
                            }
                        }
                    }
                }
                OALMSG(OAL_UEFI_DBG, (L" Found IRQ %d is using less for PIRQ%d \r\n", bTmpIrq, aTmpTbl[i].bPIRQX));
                g_Irq2PirqTbl[aTmpTbl[i].bPIRQX-1].bIRQX = bTmpIrq;
                g_Irq2PirqTbl[aTmpTbl[i].bPIRQX-1].bPIRQX = aTmpTbl[i].bPIRQX;
                g_IrqAssignedInfoTbl[last].wIrqAssignCunt++;
                OALMSG(OAL_UEFI_DBG, (L"B: Add into g_IrqAssignedInfoTbl, the g_dwInfoInxo is %d, wIrqAssignCunt is %d\r\n", 
                g_dwInfoInx, g_IrqAssignedInfoTbl[last].wIrqAssignCunt));
            }
        }
    }  
#ifdef BSP_UEFI_DEBUG   
    OALMSG(OAL_UEFI_DBG, (L"***************************  ASSIGNED *************************************\r\n"));
    OALMSG(OAL_UEFI_DBG, (L"----------   g_Irq2PirqTbl[  ]:  bPIRQX     bIRQX     -----------------\r\n"));
    for( i = 0; i < MAX_PIRQ_NUM ; i++ )
    { 
        OALMSG(OAL_UEFI_DBG, (L"             g_Irq2PirqTbl[%d]     %d       %d   \r\n", 
            i,  g_Irq2PirqTbl[i].bPIRQX,  g_Irq2PirqTbl[i].bIRQX));
    }
    
    OALMSG(OAL_UEFI_DBG, (L"*************************  ASSIGNED  INFO ***********************************\r\n"));
    OALMSG(OAL_UEFI_DBG, (L"---------- g_IrqAssignedInfoTbl[ ]:    bIRQX     wIrqAssignCunt     -----------------\r\n"));
    for( i = 0; i < (int)g_dwInfoInx ; i++ )
    { 
        OALMSG(OAL_UEFI_DBG, (L"           g_IrqAssignedInfoTbl[%d]     %2d           %2d   \r\n", 
            i,  g_IrqAssignedInfoTbl[i].bIRQX,  g_IrqAssignedInfoTbl[i].wIrqAssignCunt));
    }
#endif                      
                    
}   


//------------------------------------------------------------------------------
//
//  Function:  GetRoutingOption
//
//
//
static BOOL GetACPIRoutingOption(
                             __inout IRQRountingOptionsBuffer * pBuffer
                             )
{
    BYTE i;
    BYTE bBusNumber;
    DWORD dwIndex=0;
    DWORD dwRtnIndex = 0;
    DWORD dwCurPos=0;
    RoutingOptionTable* curTable;
    
    for(i = 0; i < MAX_PIRQ_NUM; i++)
    {
        g_Irq2PirqTbl[i].bPIRQX = 0;
        g_Irq2PirqTbl[i].bIRQX = 0xff;  //because IRQ has 0
    }
    
    memset(g_IrqAssignedInfoTbl, 0, sizeof(g_IrqAssignedInfoTbl));
    
    // Assign available IRQ to each PIRQX, the available IRQ information is come
    // from ASL parser
    AssignIrqForAllPirqX();
    
    // Get Max bus number for CreatePciRoutingOptionTable() to scan used bus
    bBusNumber = GetPciConfiguredBusNumber();
    
    memset(g_RoutingOptionTable, 0x00, sizeof(g_RoutingOptionTable));
    
    //Enumerate device and add into local routing table for IRQ configuration
    if ( (!CreatePciRoutingOptionTable(bBusNumber, g_RoutingOptionTable, &dwRtnIndex)) && 
         dwRtnIndex == 0)
    {
        OALMSG(1,(L"GetACPIRoutingOption return FAIL \r\n"));
        return FALSE;
    }
    
    OALMSG(OAL_INFO && OAL_PCI,(L"GetACPIRoutingOption(): Found ASL configured PCI Routing table.\r\n"));
    curTable = g_RoutingOptionTable;
    pBuffer->BufferSize=0;
    pBuffer->pDataBuffer = (PBYTE)(pBuffer->routingOptionTable);
    
    // We only need copy enumerated device to configure IRQ
    while ( (dwCurPos + sizeof(RoutingOptionTable) <= (sizeof(RoutingOptionTable) * dwRtnIndex)) && 
            dwIndex < MAX_DEVICE )
    {
        memcpy(pBuffer->routingOptionTable+dwIndex, curTable,sizeof(RoutingOptionTable));
        dwIndex++;
        curTable++;
        dwCurPos +=sizeof(RoutingOptionTable);
        pBuffer->BufferSize +=sizeof(RoutingOptionTable);
    }
    
    ScanConfiguredIrq (pBuffer);
    OALMSG(OAL_PCI,(L"GetACPIRoutingOption return SUCCESS \r\n"));
        return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  GetUEFIPciRoutingIrqTable
//
//
//
static void GetUEFIPciRoutingIrqTable()
{
    if (GetACPIRoutingOption(&irqRoutingOptionBuffer))
    {
        // We can find the IRQ routing information in ACPI table.
        return;
    }

    OALMSG(OAL_ERROR,(L"GetUEFIPciRoutingIrqTable: Failed\r\n"));
}

//------------------------------------------------------------------------------
//
//  Function:  OALIntrRequestIrqs
//
//
//
BOOL OALIntrRequestIrqs(
                        PDEVICE_LOCATION pDevLoc,
                        __inout UINT32 *pCount,
                        __out UINT32 *pIrq
                        )
{
    int Bus;
    int Device;
    int Func;

    // Figure out the Link number in routing table
    const RoutingOptionTable * pRoute;

    // This shouldn't happen
    if ( (pCount==NULL) || (pDevLoc==NULL) || (*pCount < 1)) 
    {
        return FALSE;
    }

    Bus = (pDevLoc->LogicalLoc >> 16) & 0xFF;
    Device = (pDevLoc->LogicalLoc >> 8) & 0xFF;
    Func = pDevLoc->LogicalLoc & 0xFF;

    // Figure out the Link number in routing table
    pRoute =(const RoutingOptionTable * )irqRoutingOptionBuffer.pDataBuffer;

    OALMSG(OAL_PCI,(L"OALIntrRequestIrqs:(Bus=%d,Device=%d,Pin=%d)\r\n",Bus,Device,pDevLoc->Pin));

    if (pRoute)
    {
        // serch table.
        DWORD dwCurPos=0;
        while (dwCurPos + sizeof(RoutingOptionTable)<=irqRoutingOptionBuffer.BufferSize)
        {
            OALMSG(OAL_PCI,(L"OALIntrRequestIrqs: for Bus=%d ,Device=%d SlotNumber=%d\r\n",
                pRoute->PCIBusNumber,(pRoute->PCIDeviceNumber)>>3,pRoute->SlotNumber));
            OALMSG(OAL_PCI,(L"     INTA_LinkValue=%x,INTA_IrqBitMap=%x\r\n",pRoute->INTA_LinkValue,pRoute->INTA_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTB_LinkValue=%x,INTB_IrqBitMap=%x\r\n",pRoute->INTB_LinkValue,pRoute->INTB_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTC_LinkValue=%x,INTC_IrqBitMap=%x\r\n",pRoute->INTC_LinkValue,pRoute->INTC_IrqBitMap));
            OALMSG(OAL_PCI,(L"     INTD_LinkValue=%x,INTC_IrqBitMap=%x\r\n",pRoute->INTD_LinkValue,pRoute->INTD_IrqBitMap));
            if (pRoute->PCIBusNumber== Bus && ((pRoute->PCIDeviceNumber)>>3)==Device)
            {
                // found
                BYTE bLinkNumber=0;
                WORD wIntPossibleBit=0;
                switch (pDevLoc->Pin)
                {
                case 1:
                    bLinkNumber=pRoute->INTA_LinkValue;
                    wIntPossibleBit=pRoute->INTA_IrqBitMap;
                    break;
                case 2:
                    bLinkNumber=pRoute->INTB_LinkValue;
                    wIntPossibleBit=pRoute->INTB_IrqBitMap;
                    break;
                case 3:
                    bLinkNumber=pRoute->INTC_LinkValue;
                    wIntPossibleBit=pRoute->INTC_IrqBitMap;
                    break;
                case 4:
                    bLinkNumber=pRoute->INTD_LinkValue;
                    wIntPossibleBit=pRoute->INTD_IrqBitMap;
                    break;
                }
                if (bLinkNumber!=0)
                {
                    DWORD dwIndex;
                    BYTE  bIrq=(BYTE)-1;

                    for (dwIndex = 0; dwIndex < NUM_IRQS; dwIndex++)
                    {
                        // Traverse the list for each irq and search for a matching
                        // bus, device, and linkNumber.
                        // If we find one this IRQ has already been mapped
                        pIrqLink currentIrqToLink = irqToLinkValue[dwIndex];
                        while (currentIrqToLink)
                        {
                            OALMSG(OAL_PCI,(L"Traverse IrqLinks - IRQ %d maps to link %x bus %d device %d func %d\r\n", dwIndex,
                                           currentIrqToLink->linkValue, currentIrqToLink->bus, currentIrqToLink->device, currentIrqToLink->func));
                            if(currentIrqToLink->linkValue == bLinkNumber &&
                               currentIrqToLink->bus == pRoute->PCIBusNumber &&
                               currentIrqToLink->device == ((pRoute->PCIDeviceNumber)>>3) &&
                               currentIrqToLink->func == Func)
                            {
                                if (pIrq)
                                {
                                    *pIrq=dwIndex;
                                }
                                if (pCount) 
                                {
                                    *pCount = 1;
                                }
                                OALMSG(OAL_PCI,
                                    (L"-OALIntrRequestIrqs: Found full IRQ match returning existing IRQ=%d\r\n",
                                    dwIndex));
                                return TRUE;
                            }
                            currentIrqToLink = currentIrqToLink->pNext;
                        }
                    }
                    for (dwIndex = 0; dwIndex < NUM_IRQS; dwIndex++)
                    {
                        // Traverse the list for each irq and search for a matching
                        // linkNumber only.  This is our second-best metric for finding a match.
                        pIrqLink currentIrqToLink = irqToLinkValue[dwIndex];
                        while (currentIrqToLink)
                        {
                            OALMSG(OAL_PCI,(L"Traverse IrqLinks - IRQ %d maps to link %x bus %d device %d\r\n", 
                                dwIndex, currentIrqToLink->linkValue, currentIrqToLink->bus, currentIrqToLink->device));
                            if(currentIrqToLink->linkValue == bLinkNumber)
                            {
                                if (pIrq) 
                                {
                                    *pIrq=dwIndex;
                                }
                                if (pCount) 
                                {
                                    *pCount = 1;
                                }
                                OALMSG(OAL_PCI,
                                    (L"-OALIntrRequestIrqs: Found linkNumber IRQ match returning existing IRQ=%d\r\n",
                                    dwIndex));
                                return TRUE;
                            }
                            currentIrqToLink = currentIrqToLink->pNext;
                        }
                    }
                }
                break;// False

            }
            dwCurPos +=sizeof(RoutingOptionTable);
            pRoute ++;
        }
    }
    return FALSE;
}

BOOL PCIInitConfigMechanism (UCHAR ucConfigMechanism);

//------------------------------------------------------------------------------
//
//  Function:  PCIInitBusInfo
//
//
//
void PCIInitBusInfo()
{
    if (PCIInitConfigMechanism (g_pX86Info->ucPCIConfigType & 0x03))
    {
        GetUEFIPciRoutingIrqTable ();
    }
    else
    {
        OALMSG(OAL_LOG_INFO, (TEXT("No PCI bus\r\n")));
    }
}

BOOL
PCIConfigureIrq (
    IN ULONG BusNumber,
    IN ULONG DeviceNumber,
    IN ULONG FunctionNumber,
    IN ULONG PinNumber,
    IN ULONG IrqToUse,
    IN BOOL  ForceIfMultipleDevs
    )
{
    OALMSG(OAL_PCI,(L"+PCIConfigureIrq - For UEFI build, we don't allow reconfig PCI IRQ at run-time \r\n"));

    return FALSE;
}

