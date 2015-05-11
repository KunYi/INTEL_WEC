
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
#define INTEL_LPC_DEVICEID_NM10   0x27bc

#define INTEL_LPC_DEVICEID_US15W  0x8119
#define INTEL_LPC_DEVICEID_DIAMONDVILLE  0x27B9
#define INTEL_LPC_DEVICEID_E600  0x8186
#define INTEL_LPC_DEVICEID_VLV2  0x0F1C

#define INTEL_LPC_DEVICEID_ICH9DH 0x2912
#define INTEL_LPC_DEVICEID_ICH9DO 0x2914
#define INTEL_LPC_DEVICEID_ICH9R  0x2916
#define INTEL_LPC_DEVICEID_ICH9ME 0x2917
#define INTEL_LPC_DEVICEID_ICH9   0x2918
#define INTEL_LPC_DEVICEID_ICH9M  0x2919

#define INTEL_LPC_DEVICEID_ICH10  0x3A10
#define INTEL_LPC_DEVICEID_ICH10D 0x3A14


// SMBus interface id
#define INTEL_SMBUS_DEVICEID_ICH8 0x283E

//LPC PCI device
#define INTEL_LPC_BUSNUM         0x0
#define INTEL_LPC_DEVICENUM      0x1f
#define INTEL_LPC_FUNCNUM        0x0
#define INTEL_LPC_OFFSET         0x02
#define INTEL_LPC_LENGTH         2

//ACPI Registers
#define ACPI_ICH8M_PMBASE_OFFSET   0x40
#define ACPI_ICH8M_BASE_MASK       0xFF80
#define ACPI_ICH8M_SMI_CTRL        0xFFFFFFEF
#define ACPI_ICH8M_SMI_CTRL_OFFSET 0x30
#define ACPI_E600_PMBASE_OFFSET    0x48
#define ACPI_E600_BASE_MASK        0xFFF0
#define ACPI_SLEEP_ENABLE          0x1c01
#define ACPI_PM1_CTRL_OFFSET       0x4
#define ACPI_SET_S5                0x3c01
#define ACPI_PMBASE_LENGTH         4




