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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

typedef enum {
    UEFI_ACPI_TABLE_APIC = 'CIPA',
    UEFI_ACPI_TABLE_FACS = 'FCAF',
    UEFI_ACPI_TABLE_RSDT = 'TDSR',
    UEFI_ACPI_TABLE_FACP = 'PCAF',
    UEFI_ACPI_TABLE_DSDT = 'TDSD',
    UEFI_ACPI_TABLE_NULL = 0
} UEFI_ACPI_TABLE;

// This struct is to store PCI Bridge device information, these information
// will be use to find PIRQ for a device not reported in ASL, the rule is to find
// it parent bus of PIRQ, and then follow PCI to PCI Bridge specification V1.2 as below
// to calculate mapped PIRQ:
//----------------------------
// DevNum    InitPin   PIRQx
// ---------------------------
//  4n        INTA     PIRQA
//  4n+1      INTA     PIRQB
//  4n+2      INTA     PIRQC
//  4n+3      INTA     PIRQD

typedef struct {
    BYTE  PCIBusNumber;
    DWORD PCIDeviceNumber;
    BYTE  PrimaryBusNumber;
    BYTE  SecondaryBusNumber;
    BYTE  CardbusBusNumber;
    BYTE  SubordinateBusNumber;
    BYTE  BridgeDevINTA_StartLinkValue;
    BYTE  BridgeDevINTB_StartLinkValue;
    BYTE  BridgeDevINTC_StartLinkValue;
    BYTE  BridgeDevINTD_StartLinkValue;
} BridgeRecordTable;
typedef struct {
    BYTE bPIRQX;
    BYTE bIRQX;
}IRQ_TO_PIRQ_TBL;

typedef struct {
    BYTE bIRQX;
    WORD wIrqAssignCunt;
}IRQ_INFO_TBL;

typedef struct acpi_parse_state
{
    UINT8                       *AmlStart;
    UINT8                       *AmlEnd;
    DWORD                       AmlLength;
    UINT8                       *RootPkgStart;
    UINT8                       *RootPkgEnd;
    UINT8                       *PkgStart;
    UINT8                       *PkgEnd;
    DWORD                       PkgLength;
    UINT8                       PkgADR;
    UINT8                       *Aml;      // Current aml byte code
} ACPI_PARSE_STATE;


typedef struct RoutingInfo 
{
    UINT8     DevNum;
    UINT8     IntPin;
    UINT8    IntLine;
    UINT8    Reserve;
    BOOL     fClaimed;
    struct RoutingInfo    *next;
    struct RoutingInfo  *child;
} ROUTING_INFO;


typedef struct {
    BYTE bPIRQX;
    BYTE bIrqNums;
    WORD wNumsIrq;
}AML_PIRQ_IRQ_TBL;


typedef struct _irq_descriptor
{
    UINT8    ResourceType;
    UINT8    IRQ_BitMask_0_7;
    UINT8    IRQ_BitMask_8_15;
    UINT8    EXT_INFO;
} IRQ_DESCRIPTOR;


typedef struct _prt_table
{
    UINT8    DevAssignedLinkValue;
    UINT8    BridgeDevINTA_StartLinkValue;
    UINT8    BridgeDevINTB_StartLinkValue;
    UINT8    BridgeDevINTC_StartLinkValue;
    UINT8    BridgeDevINTD_StartLinkValue;
} PRT_TABLE;


#define MAX_PIRQ_NUM    8
#define MAX_IRQ_NUM     11

AML_PIRQ_IRQ_TBL    g_PirqTbl[MAX_PIRQ_NUM];

BOOL    x86InitACPITables();
DWORD   AcpiPhysicalAddressToVirtual(DWORD, DWORD*);




//------------------------------------------------------------------------------
//
//  Function:  x86ParseDsdtTables
//
//  Based on some known restrictions, refer to design doc, parse AML code in DSDT 
//  table in order to retrieve the PCI routing information for each PCI bus. 
//  And also, PCI routing infomation wile save in the structure g_PRTInfo.
//
VOID    x86ParseDsdtTables();



//------------------------------------------------------------------------------
//
//  Function:  GetDevicePIRQ
//
//  Get the requested PCI device routing table by the specific Device number 
//  and the IntPin number from the g_PRTInfo. This is necessary as each [bus device function]
//  has a specific IntPin number.
BOOL    GetDevicePIRQ( BYTE bBusNum, BYTE bDevNum, BYTE bIntPinNum, PRT_TABLE *PRT);



//------------------------------------------------------------------------------
//
//  Function:  AmlMemAlloc
//
//  This function is used for memory allocation in OEMInit(). It need to reserved
//  a pysical memory range 0x80220000 in config.bib
//
BYTE*   AmlMemAlloc( UINT32 memSize );



//------------------------------------------------------------------------------
//
//  Function:  AmlInitParser
//
//  Initialize the ACPI_PARSE_STATE structure and load the DSDT table.
//
//
void    AmlInitParser( ACPI_PARSE_STATE *Parser );



//------------------------------------------------------------------------------
//
//  Function:  AmlFindRootPCIBus
//
//  Search all AML to find out where the Root PCI bus is.
//
//
BOOL    AmlFindRootPCIBus( ACPI_PARSE_STATE *ParserState );



//------------------------------------------------------------------------------
//
//  Function:  AmlFindADeviceOp
//
//  Find a DEVICE() Op byte in the range of Root PCI bus, then decode its package
//    length and save this device info.
//  
BOOL    AmlFindADeviceOp( ACPI_PARSE_STATE *ParserState );



//------------------------------------------------------------------------------
//
//  Function:  AmlFindPRTMethod
//
//  Find a Method() Op with the _PRT object in the range of DEVICE(), then check
//    whether this DEVICE() is requred or not by comparing its device name.
//  
BOOL    AmlFindPRTMethod( ACPI_PARSE_STATE *ParserState );



//------------------------------------------------------------------------------
//
//  Function:  AmlGetPRTInfo
//
//  In _PRT Method, Search for the Return Op to figure out how many entry of 
//  rouing record need to get, then, get rouing entery one-by-one.
//
//    Mapping Fields of _PRT mapping packages :
//        
//    Field |        [ Address ] [ PCI pin number ] [ Source ] [ Source Index ]
//    Type  |          (DWORD)          (BYTE)        (NamePath)       (DWORD)
//
//
VOID    AmlGetPRTInfo( ACPI_PARSE_STATE *ParserState, ROUTING_INFO **RoutingTable );



//------------------------------------------------------------------------------
//
//  Function:  AmlDecodePkgLength
//
//  Decode the Package Length of a AML OP code, get how many bytes occupied for length and 
//  return the package length(in Byte) for this AML Op code.
//
DWORD   AmlDecodePkgLength( BYTE* aml ,BYTE *PkgByteCount );



//------------------------------------------------------------------------------
//
//  Function:  AmlIsRootPCIBus
//
//  Determined by checking " Name(_HID,EISAID ("PNP0A08"))" under this device OP.
//
//  "PNP0A08" is a index of Root PCI bridge.
//
BOOL    AmlIsRootPCIBus( BYTE *aml, DWORD PkgLength );



//------------------------------------------------------------------------------
//
//  Function:  AmlIs_PRTMethod
//
//  Determined by checking "_PRT" prefix under this Method OP.
//
//
BOOL    AmlIs_PRTMethod( BYTE *aml, DWORD PkgLength );



//------------------------------------------------------------------------------
//
//  Function:  AmlGetDeviceADR
//
//  Find a Name OP with _ADR object under this Device package in order to get
//    the address of this PCI device.
//  
//
BYTE    AmlGetDeviceADR( ACPI_PARSE_STATE *ParserState );



//------------------------------------------------------------------------------
//
//  Function:  AmlFindIntLinkDevice
//
//  Find Interrupt Link device in the range of Root PCI bus by searching a IntLinkID.
//  
//  IntLinkID is a AML unique stream bytes: "Name(_HID,EISAID("PNP0C0F"))"
//
//
BOOL    AmlFindIntLinkDevice( ACPI_PARSE_STATE *ParserState );



//------------------------------------------------------------------------------
//
//  Function:  AmlGetIRQDescriptor
//
//  Under _PRS object, parser the following BufferOp in order to get the PCI IRQ 
//  Descriptor, defined in ACPI spec., in which includes the available PIRQ number
//  for the PCI registers PIRQ[A-D] and PIRQ[E-H].
//
//
//  IntLinkID is a AML unique stream bytes: "Name(_HID,EISAID("PNP0C0F"))"
//
//
BOOL    AmlGetIRQDescriptor( ACPI_PARSE_STATE *ParserState );


//------------------------------------------------------------------------------
//
//  Function:  AmlSeekToPRS
//
//  Find a Name OP with _PRS object under this Device package.
//  
//
BOOL    AmlSeekToPRS( ACPI_PARSE_STATE *ParserState );


//=========================================================

#ifdef __cplusplus
}
#endif