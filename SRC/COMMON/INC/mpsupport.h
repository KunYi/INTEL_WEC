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
// Define the strucutres/functions used for MP support

#pragma once
#ifndef _MPSUPPORT_H_
#define _MPSUPPORT_H_

#pragma pack(push, 1)           // We want this structure packed exactly as declared!

typedef struct {
    USHORT Size;
    PVOID Base;
} FWORDPtr;

#pragma pack(pop)

//
// parameter passed to MP startup code, running in real-mode
//
typedef struct {
    DWORD       JumpInstr;
    DWORD       PageDirPhysAddr;
    ULONGLONG   tmpGDT[3];
    DWORD       Cr4Value;
    FWORDPtr    tmpGDTBase;
} MPStartParam, *PMPStartParam;


//
// x86 spec - BSP starts other CPUs by sending a "Startup IPI (SIPI)" to other cpus. All
//            CPUs will start execution in real mode, with cs:ip == [SIPI_VECOTR<<8]:0 
//            (The flat physical address of the SIPI startup address is SIPI_VECTOR << 12).
//            The flat startup address of SIPI must be < 1M (0x100000).
//            i.e. SIPI_VECTOR <= 0xff. 
//
//            For CEPC, bootloader and imager are setup to use address >= 1M, so the memory
//            are "free" to use, except for the memory used by bios. 
//            
//            For other x86 devices, make sure you update SIPI_VECTOR accordingly if the
//            memory below 1M will be used by image/bootloader (describe in config.bib for 
//            image, and boot.bib for bootloader).
//
#define SIPI_VECTOR             0x99                // use vector 0x99 for SIPI
#define SIPI_PAGE_PHYS_ADDR     (SIPI_VECTOR << 12) // SIPI page physical address

//
// IPI (Inter-Processor-Interrupt) Vector is used to handle inter-processor synchronization. 
// It must not conflict with any IRQ used by hardware interrupt. 
//
#define IPI_VECTOR              0xff                // use vector 0xff for IPI

//
// APIC registers
//
#define APIC_REGS_PHYS_BASE     0xFEE00000
typedef struct _APIC_REGS {
    DWORD reserved000[8];       // FEE00000-FEE0001F: reserved
    
    DWORD ApicId;               // FEE00020: APIC ID register
    DWORD reserved020[3];

    DWORD ApicVersion;          // FEE00030: APIC Version register
    DWORD reserved030[3];

    DWORD reserved040[16];      // FEE00040-FEE0007F: reserved

    DWORD TaskPrioReg;          // FEE00080: Task priority register
    DWORD reserved080[3];

    DWORD ArbPrioReg;           // FEE00090: Abritration priority register
    DWORD reserved090[3];
    
    DWORD ProcPrioReg;          // FEE000A0: Processor priority register
    DWORD reserved0a0[3];
    
    DWORD EOIReg;               // FEE000B0: EOI register
    DWORD reserved0b0[3];

    DWORD reserved0c0[4];       // FEE000C0: reserved
    
    DWORD LocDestReg;           // FEE000D0: Local Destination register
    DWORD reserved0d0[3];
    
    DWORD DestFmtReg;           // FEE000E0: destination format register
    DWORD reserved0e0[3];
    
    DWORD SpurIntVecReg;        // FEE000F0: Spurious Interrupt Vector register
    DWORD reserved0f0[3];

    BYTE  ISR[0x80];            // FEE00100-FEE0017F: In-Service Register
    BYTE  TMR[0x80];            // FEE00180-FEE001FF: Trigger Mode Register
    BYTE  IRR[0x80];            // FEE00200-FEE0027F: Interrupt Request Register
    
    DWORD ErrStatReg;           // FEE00280: Error Status register
    DWORD reserved280[3];
    
    DWORD reserved290[0x1C];    // FEE00290-FEE002FF: reserved

    DWORD IcrLow;               // FEE00300: Interrupt Command Register (0-31)
    DWORD reserved300[3];

    DWORD IcrHigh;              // FEE00310: Interrupt Command Register (32-64)
    DWORD reserved310[3];

    DWORD LvtTimer;             // FEE00320: LVT Timer Register
    DWORD reserved320[3];

    DWORD LvtThermal;           // FEE00330: LVT Thermal Sensor Register
    DWORD reserved330[3];

    DWORD LvtPerfCounter;       // FEE00340: LVT Perf Monitor Counter Register
    DWORD reserved340[3];

    DWORD LvtLint0;             // FEE00350: LVT LINT0 Register
    DWORD reserved350[3];

    DWORD LvtLint1;             // FEE00360: LVT LINT1 Register
    DWORD reserved360[3];

    DWORD LvtError;             // FEE00370: LVT Error Register
    DWORD reserved370[3];

    DWORD TimerInitCount;       // FEE00380: Initial Counter Register (For Timer)
    DWORD reserved380[3];

    DWORD TimerCurrCount;       // FEE00390: Current Count Register (For Timer)
    DWORD reserved390[3];

    DWORD reserved3a0[0x10];    // FEE003A0-FEE003DF: reserved

    DWORD DivConfig;            // FEE003E0: Divide Configuration Register (For Timer)
    DWORD reserved3e0[3];

    DWORD reserved3f0[4];       // FEE003F0-FEE002FF: reserved
}APIC_REGS;

//Enum for Multi-Processor Floating Point Strcuture (MP FPS) Information
typedef enum {
    MP_FPS_VERSION = 1,            //To get the MP version
    MP_CONFIG_TABLE_PHY_ADDR = 2,  //To retrieve the MP Config Table Phy Addr
    MP_IMCRP =3                    //To check the IMCR presence bit
} MP_FPS_TYPE;

#define MAX_IOAPIC_ENTRY 	64	//max of I/O Apic entries
#define MAX_IA_ENTRY 		256 //max of I/O interrupt Assignment entries
#define MAX_BUS_ENTRY		256

#define MP_POLARITY_MASK     (3)         // 00-01: Polarity of APIC I/O input signals
#define MP_TRIGGER_MASK      (3<<2)      // 02-03: Trigger mode of APIC input signals

#define MP_PIN_MASK     (3)         // 00-01: Polarity of APIC I/O input signals
#define MP_DEVICE_MASK      (31<<2)     // 02-03: Trigger mode of APIC input signals

#define SHIFTLEFT(b) (1 << (b))

#define APIC_CHIP_SIZE           0x40
#define APIC_IND                 0x00
#define APIC_DAT                 0x10
#define APIC_EOIR                0x40
#define APIC_IND_ID              0x00
#define APIC_IND_VER             0x01
#define APIC_IND_REDIR_TBL_BASE  0x10


//data Structure for Multi-processor Floating Point Structure (MP FPS)
typedef struct {
	CHAR    	Signature[4];   // FPS Signature "_MP_"
	ULONG    PhysAddrPtr;   // Physical address of the MP configuration table.
	UCHAR    Length;        // The length of the floating pointer structure table
	UCHAR    Spec;          // The version number of the MP specification supported.
	UCHAR    CheckSum;      // Checksum of the complete pointer structure
	UCHAR    Feature1;      // Bits 0-7: MP System Configuration Type.
	UCHAR    Feature2;      // Bit 7: IMCRP (IMCR presence bit)
	UCHAR    Feature3;      // Reserved
	UCHAR    Feature4;      // Reserved
	UCHAR    Feature5;      // Reserved
}MP_FPS;


typedef struct  {
	UCHAR type;
	UCHAR apicid;		// Local APIC number
	UCHAR apicver;		// Its versions
	UCHAR cpuflag;
	ULONG cpufeature;
	ULONG  featureflag;	// CPUID feature value
	ULONG  reserved[2];
}MP_CPU;


//data Structure for Multi-processor table configure (MP TABLE)
typedef struct {
	CHAR     Signature[4];   // mp table Signature "_PCMP_"
	USHORT   Length;        // table size
	UCHAR    Spec;
	UCHAR    CheckSum;
	UCHAR    Oem[8];
	UCHAR    Productid[12];
	DWORD    Oemptr;
	USHORT   Oemsize;
	USHORT   Oemcount;
	DWORD    lapic;
	DWORD    reserved;
}MP_TABLE;

typedef struct {
	UCHAR	type;
	UCHAR 	busid;
	UCHAR 	bustype[6];
}MP_BUS;

typedef struct  {
	UCHAR	type;
	UCHAR	apicid;
	UCHAR	apicver;
	UCHAR	flags;
	DWORD apicaddr;
}MP_IOAPIC;

typedef struct  {
	UCHAR	type;
	UCHAR	irqtype;
	USHORT	irqflag;
	UCHAR	srcbus;
	UCHAR 	srcbusirq;
	UCHAR 	dstapic;
	UCHAR 	dstirq;
}MP_INTSRC;

typedef struct  {
	UCHAR	type;
	UCHAR	irqtype;
	USHORT	irqflag;
	UCHAR	srcbusid;
	UCHAR	srcbusirq;
	UCHAR	destapic;
	UCHAR	destapiclint;
}MP_LINTSRC;

//------------------------------------------------------------------------------
//
//  Function:  
//  GetMpInfo
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
BOOL GetMPFpsInfo(MP_FPS_TYPE Type, DWORD *pdwData);
BOOL MpGlobalSystemInterruptInfo();
void MpPciIrqMapping();
void x86InitMPTable();
void x86UnInitMPTable();

// Struct of PCI header
typedef struct
{
    USHORT vendorID;
    USHORT deviceID;
    USHORT command;
    USHORT status;
    UCHAR  revisionID;
    UCHAR  progIf;
    UCHAR  subClass;
    UCHAR  baseClass;
    UCHAR  cacheLineSize;
    UCHAR  latencyTimer;
    UCHAR  headerType;
    UCHAR  bist;
    DWORD  bar[6];
    DWORD  cis;
    USHORT subVendorId;
    USHORT subId;
    DWORD  romBaseAddress;
    UCHAR  capabilities;
    UCHAR  reserved[7];
    UCHAR  interruptLine;
    UCHAR  interruptPin;
    UCHAR  minGnt;
    UCHAR  maxLat;
} PciConfig_Header;

// struct of PCI bridge header
typedef struct
{
    USHORT vendorID;
    USHORT deviceID;
    USHORT command;
    USHORT status;
    UCHAR  revisionID;
    UCHAR  progIf;
    UCHAR  subClass;
    UCHAR  baseClass;
    UCHAR  cacheLineSize;
    UCHAR  latencyTimer;
    UCHAR  headerType;
    UCHAR  bist;
    DWORD  bar[2];
    UCHAR  primaryBusNumber;
    UCHAR  secondaryBusNumber;
    UCHAR  subordinateBusNumber;
    UCHAR  secondaryLatencyTimer;
    UCHAR  ioBase;
    UCHAR  ioLimit;
    USHORT secondaryStatus;
    USHORT memoryBase;
    USHORT memoryLimit;
    USHORT preFetchMemoryBase;
    USHORT prefetchMemoryLimit;
    DWORD  preFetchBaseUpper;
    DWORD  prefetchLimitUpper;
    USHORT ioBaseUpper;
    USHORT ioLimitUpper;
    UCHAR  capabilities;
    UCHAR  reserved[3];
    DWORD  romBaseAddress;
    UCHAR  interruptLine;
    UCHAR  interruptPin;
    USHORT bridgeControl;
} PciConfig_BridgeHeader;

//struct of  LPC (Intel Low Pin Count chip) PCI header
typedef struct
{
    PciConfig_Header  header;

    UCHAR  deviceSpecific1[145];
	DWORD  gen_cntl;
    UCHAR  deviceSpecific2[28];
    DWORD  rcba;
} PciConfig_Lpc;

#endif
