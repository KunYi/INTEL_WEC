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

#ifndef _ICH_H_
#define _ICH_H_
#include <pm.h>
#include <atapipcicd.h>

#define ATA_REG_LBA_LOW     3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HIGH    5
#define ATA_REG_DEV_CTL     6
#define 	DEV_CTL_HOB		8

class CIch : public CPCIDiskAndCD
//class CIch : public CPCIDisk
{

public:
	BYTE 					m_bBestMode;
	ULONG					m_ICHFlags;

    CIch(HKEY hKey) : CPCIDiskAndCD(hKey)   
    {
		m_LastDx = D0;
		m_NewDx = D0;
		m_ICHFlags = 0;
		m_bBestMode = 0;
   }

    virtual BOOL Init(HKEY hActiveKey);
	virtual BYTE FetchBestTransferMode(bool fPIOOnly);
	BOOL  SendIO48Command(DWORD dwStartSector, DWORD dwNumberOfSectors, BYTE bCmd);
	virtual DWORD ReadWriteDiskDMA(PIOREQ pIOReq, BOOL fRead);
	virtual BOOL ResetController(BOOL bSoftReset);

    inline void SelectDeviceLBA() 
    {
        ATA_WRITE_BYTE(m_pATAReg+ATA_REG_DRV_HEAD, (BYTE)(ATA_HEAD_LBA_MODE | ((m_dwDevice == 0 ) ? ATA_HEAD_DRIVE_1 : ATA_HEAD_DRIVE_2)));
    }
    inline void SelectHOB() 
    {
        ATA_WRITE_BYTE(m_pATAReg+ATA_REG_DEV_CTL, DEV_CTL_HOB);
    } 
    inline void ClearHOB() 
    {
        ATA_WRITE_BYTE(m_pATAReg+ATA_REG_DEV_CTL, 0);
    } 
    inline void WriteLBALow(BYTE bValue) 
    {
        ATA_WRITE_BYTE(m_pATAReg + ATA_REG_LBA_LOW, bValue);
    }    

    inline void WriteLBAMid(BYTE bValue) 
    {
        ATA_WRITE_BYTE(m_pATAReg + ATA_REG_LBA_MID, bValue);
    }    

    inline void WriteLBAHigh(BYTE bValue) 
    {
        ATA_WRITE_BYTE(m_pATAReg + ATA_REG_LBA_HIGH, bValue);
    }    
    
	CEDEVICE_POWER_STATE	m_LastDx;
	CEDEVICE_POWER_STATE	m_NewDx;
};    

// ICH defines (most additions to existing defines in DISKMAIN.H)

// add 80-pin cable detect
#define DFLAGS_DEVICE_80PINCABLEDETECT  (1 << 13)	   // 80-pin cable detect bit in HardwareResetResults

// add UltraDMA100 support
#define UDMA_MODE5_CYCLE_TIME       	20
#define UDMA_MODE5          			(1 << 16)
#define IDD_UDMA_MODE5_ACTIVE           (1 << 5)
#define ATA_DMA_ULTRA_MODE 				0x40    		// DMA Ultra Transfer Mode
#define DFLAGS_DEVICE_ICH    			(1 << 31)    	// If Intel ICH controller.

#define IDENTIFY_CAPABILITIES_STANDBYTIMER_SUPPORTED    (1 << 13)
#define ATA_SET_STANDBYTIMER			0xE2			// set feature command for standby timer
#ifndef IDE_COMMAND_SET_FEATURE							// defined in the module where it is used )-;
#define IDE_COMMAND_SET_FEATURE         0xEF
#endif
#define IDE_COMMAND_CHECK_POWER_MODE	0xE5

// support 48bit LBA
#define  ATA_CMD_FLUSH_CACHE_EXT				0xEA
#define  ATA_CMD_READ_DMA_EXT					0x25
#define  ATA_CMD_READ_DMA_QUEUED_EXT			0x26
#define  ATA_CMD_READ_LOG_EXT					0x2F
#define  ATA_CMD_READ_MULTIPLE_EXT				0x29
#define  ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT	0x27
#define  ATA_CMD_READ_SECTOR_EXT				0x24
#define  ATA_CMD_READ_VERIFY_SECTOR				0x42
#define  ATA_CMD_SET_MAX_ADDRESS_EXT			0x37
#define  ATA_CMD_WRITE_DMA_EXT					0x35
#define  ATA_CMD_DMA_QUEUED_EXT					0x36
#define  ATA_CMD_WRITE_LOG_EXT					0x3F
#define  ATA_CMD_WRITE_MULTIPLE_EXT				0x39
#define  ATA_CMD_WRITE_SECTOR_EXT				0x34

//
#define	ICH_PM_SUPPORT				0x01
#define	ICH_APM_SUPPORT				0x02
#define	ICH_STANDBYTIMER_SUPPORT	0x04
#define	ICH_STANDBYTIMER_SET		0x08
#define ICH_LBA48_SUPPORT			0x10

// "this" pointer for init call back to CIch
#include "debug.h"		// for ZONE_INIT
#include "atapipci.h"	// for class CPCIDisk
#include "ich.h"		// for class CIch
extern CIch *pICH;		// this pointer required to get at our SetBestTransferMode from CDisk::Init

 

#pragma pack(1)
//
// Data returned by the ATAPI_CMD_IDENTIFY command (for ATA/ATAPI-6)
//
typedef struct _IDENTIFY_DATA6 {
    USHORT GeneralConfiguration;            // 00   Mandatory for ATAPI
    USHORT NumberOfCylinders;               // 01   Not used for ATAPI
    USHORT Reserved1;                       // 02   Not used for ATAPI
    USHORT NumberOfHeads;                   // 03   Not used for ATAPI
    USHORT UnformattedBytesPerTrack;        // 04   Not used for ATAPI
    USHORT UnformattedBytesPerSector;       // 05   Not used for ATAPI
    USHORT SectorsPerTrack;                 // 06   Not used for ATAPI
    USHORT VendorUnique1[3];                // 07-09    Not used for ATAPI
    USHORT SerialNumber[10];                // 10   Optional for ATAPI
    USHORT BufferType;                      // 20   Not used for ATAPI
    USHORT BufferSectorSize;                // 21   Not used for ATAPI
    USHORT NumberOfEccBytes;                // 22   Not used for ATAPI
    USHORT FirmwareRevision[4];             // 23   Mandatory for ATAPI
    USHORT ModelNumber[20];                 // 27   Mandatory for ATAPI
    UCHAR  MaximumBlockTransfer;            // 47 low byte     Not used for ATAPI
    UCHAR  VendorUnique2;                   // 47 high byte    Not used for ATAPI
    USHORT DoubleWordIo;                    // 48   Not used for ATAPI
    USHORT Capabilities;                    // 49   Mandatory for ATAPI
    USHORT Capabilities2;                   // 50 bit 0 = 1 to indicate a device specific Standby timer value minimum
    UCHAR  VendorUnique3;                   // 51 low byte      Mandatory for ATAPI
    UCHAR  PioCycleTimingMode;              // 51 high byte     Mandatory for ATAPI
    UCHAR  VendorUnique4;                   // 52 low byte      Mandatory for ATAPI
    UCHAR  DmaCycleTimingMode;              // 52 high byte     Mandatory for ATAPI
    USHORT TranslationFieldsValid;          // 53 (low bit)     Mandatory for ATAPI
    USHORT NumberOfCurrentCylinders;        // 54   Not used for ATAPI
    USHORT NumberOfCurrentHeads;            // 55   Not used for ATAPI
    USHORT CurrentSectorsPerTrack;          // 56   Not used for ATAPI
    ULONG  CurrentSectorCapacity;           // 57 & 58          Not used for ATAPI
    UCHAR  MultiSectorCount;                // 59 low           Not used for ATAPI
    UCHAR  MultiSectorSettingValid;         // 59 high (low bit)Not used for ATAPI
    ULONG  TotalUserAddressableSectors;     // 60 & 61          Not used for ATAPI
    UCHAR  SingleDmaModesSupported;         // 62 low byte      Mandatory for ATAPI
    UCHAR  SingleDmaTransferActive;         // 62 high byte     Mandatory for ATAPI
    UCHAR  MultiDmaModesSupported;          // 63 low byte      Mandatory for ATAPI
    UCHAR  MultiDmaTransferActive;          // 63 high byte     Mandatory for ATAPI
    UCHAR  AdvancedPIOxferreserved;         // 64 low byte      Mandatory for ATAPI
    UCHAR  AdvancedPIOxfer;                 // 64 high byte     Mandatory for ATAPI
    USHORT MinimumMultiwordDMATime;         // 65 Mandatory for ATAPI
    USHORT ManuRecomendedDMATime;           // 66 Mandatory for ATAPI
    USHORT MinimumPIOxferTimeWOFlow;        // 67 Mandatory for ATAPI
    USHORT MinimumPIOxferTimeIORDYFlow;     // 68 Mandatory for ATAPI
    USHORT ReservedADVPIOSupport[2];        // 69 Not used for ATAPI
    USHORT TypicalProcTimeForOverlay;       // 71 Optional for ATAPI
    USHORT TypicalRelTimeForOverlay;        // 72 Optional for ATAPI
    USHORT MajorRevisionNumber;             // 73 Optional for ATAPI
    USHORT MinorRevisionNumber;             // 74 Optional for ATAP  
    USHORT QueueDepth;                      // 75
    USHORT Reserved6[4];                    // 76-79
    USHORT MajorVersionNumber;              // 80
    USHORT MinorVersionNumber;              // 81
    USHORT CommandSetSupported1;            // 82
    USHORT CommandSetSupported2;            // 83
    USHORT CommandSetFeaturesSupported;     // 84
    USHORT CommandSetFeatureEnabled1;       // 85
    USHORT CommandSetFeatureEnabled2;       // 86
    USHORT CommandSetFeatureDefault ;       // 87
    UCHAR  UltraDMASupport;                 // 88 High
    UCHAR  UltraDMAActive;                  // 88 Low
    USHORT TimeRequiredForSecurityErase;    // 89 Time Required For Security Erase Unit Completion
    USHORT TimeReuiregForEnhancedSecurtity; // 90 Time Required For Enhanced Security Erase Unit Completion
    USHORT CurrentAdvancePowerMng;          // 91 CurrentAdvanced Power Managemnt Value
    USHORT MasterPasswordRevisionCode;      // 92 Master Password Revision Code
    USHORT HardwareResetResult;             // 93 Hardware Reset Result
    UCHAR  CurrentAcousticManagement;       // 94 Acoustic Management (low byte = current; high byte = vendor recommended)
	UCHAR  VendorAcousticManagement;       
	USHORT Reserved7a[99-95+1];				// 95-99
	union {									// 100-103 Maximum User LBA for 48-bit Address feature set
		ULONG  lMaxLBAAddress[2];
		DOUBLE dMaxLBAAddress;
	};
	USHORT Reserved7b[126-104+1];			// 104-126
    USHORT MediaStatusNotification:2;       // 127 Removable Media Status Notification Support
    USHORT SecurityStatus;                  // 128 Security Status
    USHORT Reserved8[31];                   // 129-159 Vendor Specific
    USHORT CFAPowerMode1;                   // 160
    USHORT Reserved9[94];                   // 161-254
    USHORT IntegrityWord;                   // 255 Checksum & Signature    
} IDENTIFY_DATA6, *PIDENTIFY_DATA6;

#pragma pack()
#endif //_ICH_H_
