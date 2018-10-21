//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2009 Intel Corporation All Rights Reserved. 
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
//



/*Module Name:  

GPIO.h

Abstract:  

Holds definitions private to the implementation of the machine independent
driver interface.

Notes: 


--*/

#ifndef __GPIO_H__
#define __GPIO_H__

#include <windows.h>
#include <nkintr.h>

#include <notify.h>
#include <memory.h>
#include <nkintr.h>

#include <pegdser.h>
#include <devload.h>
#include <ddkreg.h>
#include <ceddk.h>
#include <cebuscfg.h>
#include <linklist.h>
#include "GPIOpublic.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG


DBGPARAM dpCurSettings = {
    TEXT("GPIO"), {
        TEXT("Errors"),TEXT("Warnings"),TEXT("Init"),TEXT("Deinit"),
            TEXT("IOCtl"),TEXT("Open"),TEXT("Close"),TEXT("Thread"),
            TEXT("Function"),TEXT("Info"),TEXT("Undefined"),TEXT("Undefined"),
            TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
            0x8000
};

/* Debug Zones */
#define    ZONE_ERROR           (DEBUGZONE(0))
#define    ZONE_WARN            (DEBUGZONE(1))
#define    ZONE_INIT            (DEBUGZONE(2))
#define    ZONE_DEINIT          (DEBUGZONE(3))
#define    ZONE_IOCTL           (DEBUGZONE(4))
#define    ZONE_INFO            (DEBUGZONE(9))

#endif

#pragma once


//////////////////////////////////////////////////////////////////////////
//4 Cores Base Address
// SWcore register base address - Controller 0
#define GPIO_SWCORE_BASE_ADDR            (0xFED80000)
// Ncore register base address  - Controller 1
#define GPIO_NCORE_BASE_ADDR             (0xFED88000)
// Ecore register base address  - Controller 2
#define GPIO_ECORE_BASE_ADDR             (0xFED90000)
// SEcore register base address - Controller 3
#define GPIO_SECORE_BASE_ADDR            (0xFED98000)

//////////////////////////////////////////////////////////////////////////
//BIT function
#define BIT(n)                          (1UL << (n))
// Total south west core gpio pins
#define GPIO_TOTAL_PINS_SWCORE_MMIO     (56)
// Total ncore gpios pin in vlv2
#define GPIO_TOTAL_PINS_NCORE_MMIO      (59)
// Total ecore gpios pin in vlv2
#define GPIO_TOTAL_PINS_ECORE_MMIO      (24)
// Total south east core gpios pin 
#define GPIO_TOTAL_PINS_SECORE_MMIO     (55)
// Total gpios pin in vlv2
#define GPIO_TOTAL_PINS_MMIO            (GPIO_TOTAL_PINS_SWCORE_MMIO+GPIO_TOTAL_PINS_NCORE_MMIO+GPIO_TOTAL_PINS_SECORE_MMIO+GPIO_TOTAL_PINS_ECORE_MMIO)
// Pins per bank
#define GPIO_PINS_PER_BANK_MMIO         (32)
// Trigger status bits per bank
#define GPIO_TS_PER_BANK_MMIO           (32)
#define GPIO_TOTAL_BANKS_MMIO           ((GPIO_TOTAL_PINS_MMIO+GPIO_PINS_PER_BANK_MMIO-1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_SWCORE_MMIO    ((GPIO_TOTAL_PINS_SWCORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_SECORE_MMIO    ((GPIO_TOTAL_PINS_SECORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_NCORE_MMIO     ((GPIO_TOTAL_PINS_NCORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_ECORE_MMIO     ((GPIO_TOTAL_PINS_ECORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)

//the largest offset of every cores
//16 is reservation for dummy data
#define GPIO_LARGEST_SWCORE             (0x5C38 + 16)
#define GPIO_LARGEST_SECORE             (0x5850 + 16)
#define GPIO_LARGEST_NCORE              (0x5460 + 16)
#define GPIO_LARGEST_ECORE              (0x4858 + 16)

//////////////////////////////////////////////////////////////////////////
// Bit 0 (GPIO RX State) of CFG0 (MMIO)
#define GPIO_RX_STATE                  BIT(0)
// Bit 1 (GPIO TX State) of CFG0 (MMIO)
#define GPIO_TX_STATE                  BIT(1)
// Bit 15 (GPIO Enabled) of CFG0 (MMIO)
#define GPIO_ENABLE                       BIT(15)
//Bit 16 (PAD Mode) of CFG) of CFG0 (MMIO)
#define GPIO_PM_MASK                 (0xf << 16)
//Bit 10-8 (GPIO Configuration) of CFG0 (MMIO)
#define GPIO_CFG_MASK                 (BIT(8) | BIT(9) | BIT(10))
//Bit 8, 9 and 10 (GPIO RX/TX Enable Configuration)
#define GPIO_TX_EN                    (BIT(8))
#define GPIO_RX_EN                    (BIT(9))
//Direction mask copy the method of Baytrail instead of using function is CV
//Rx = input ; TX = output
#define DIRECTION_MASK                (GPIO_RX_EN | GPIO_TX_EN)

//////////////////////////////////////////////////////////////////////////
// Enumeration for GPIO communities
// Please refer to VLV2_GPIO_HAS
typedef enum
{
    GPIOInvalid,
    // South EAST community
    GPIO_SECORE,
    // North community
    GPIO_NCORE,
    // South West community
    GPIO_SWCORE,
	//East community
	GPIO_ECORE
} GPIO_CTRL;

#pragma pack(1)
// Structure for MMIO registers
// (please refer to c-psec)
typedef struct
{
    // Set TPE and TNE
    ULONG   ulPadConf0;
    // Set stress
    ULONG   ulPadConf1;
    // Set input output
  //  ULONG   ulPadValue;
  //  ULONG   ulReserved[1];
} GPIO_CORE_BANK_REGISTERS_MMIO, *PGPIO_CORE_BANK_REGISTERS_MMIO;
#pragma pack()
// Declare a structure which describes the GPIO bank.
typedef struct
{
    PGPIO_CORE_BANK_REGISTERS_MMIO   Registers[GPIO_PINS_PER_BANK_MMIO];          //pointer to hardware registers

} GPIO_CORE_BANK_MMIO, *PGPIO_CORE_BANK_MMIO;

//braswell south west , have to set te gpio pins as well
//56 PINS
static ULONG SWCORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_SWCORE_MMIO] =
{
	0x4400,		//FST_SPI_D2/GPIO_SW_0
	0x4408,		//FST_SPI_D0/GPIO_SW_1
	0x4410,		//FST_SPI_CLK/GPIO_SW_2
	0x4418,		//FST_SPI_D3/GPIO_SW_3
	0x4420,		//FST_SPI_CS1_B/GPIO_SW_4
	0x4428,		//FST_SPI_D1/GPIO_SW_5
	0x4430,		//FST_SPI_CS0_B/GPIO_SW_6
	0x4438,		//FST_SPI_CS2_B/GPIO_SW_7
	0x4800,		// UART1_RTS_B/GPIO_SW_8
    0x4808,		// UART1_RXD/GPIO_SW_9
	0x4810,		// UART2_RXD/GPIO_SW_10
    0x4818,		// UART1_CTS_B/GPIO_SW_11
	0x4820,		//UART2_RTS_B/GPIOC_76/GPIO_SW_12
	0x4828,		//UART1_TXD/GPIO_SW_13
	0x4830,		//UART2_TXD/GPIO_SW_14
	0x4838,		//UART2_CTS_B/GPIOC_77/GPIO_SW_15
	0x4C00,		//MF_HDA_CLK/GPIO_SW_16
	0x4C08,		//MF_HDA_RSTB/GPIO_SW_17
	0x4C10,		//MF_HDA_SDI0/GPIO_SW_18
	0x4C18,		//MF_HDA_SDO/GPIO_SW_19
	0x4C20,		//MF_HDA_DOCKRSTB/GPIO_SW_20
	0x4C28,		//MF_HDA_SYNC/GPIO_SW_21
	0x4C30,		//MF_HDA_SDI1/GPIO_SW_22
	0x4C38,		//MF_HDA_DOCKENB/GPIO_SW_23
	0x5000,		//I2C5_SDA/GPIOC_88/GPIO_SW_24
	0x5008,		//I2C4_SDA/GPIOC_86/GPIO_SW_25
	0x5010,		//I2C6_SDA/GPIOC_90/NMI_B/GPIO_SW_26
	0x5018,		//I2C5_SCL/GPIOC_89/GPIO_SW_27
	0x5020,		//I2C_NFC_SDA/GPIOC_92/GPIO_SW_28
	0x5028,		//I2C4_SCL/GPIOC_87/GPIO_SW_29
	0x5030,		//I2C6_SCL/GPIOC_91/SDMMC3_WP/GPIO_SW_30
	0x5038,		//I2C_NFC_SCL/GPIOC_93/GPIO_SW_31
	0x5400,		//I2C1_SDA/GPIOC_80/GPIO_SW_32
	0x5408,		//I2C0_SDA/GPIOC_78/GPIO_SW_33
	0x5410,		//I2C2_SDA/GPIOC_82/GPIO_SW_34
	0x5418,		//I2C1_SCL/GPIOC_81/CCUPLLLOCKEN/GPIO_SW_35
	0x5420,		//I2C3_SDA/GPIOC_84/GPIO_SW_36
	0x5428,		//I2C0_SCL/GPIOC_79/GPIO_SW_37
	0x5430,		//I2C2_SCL/GPIOC_83/GPIO_SW_38
	0x5438,		//I2C3_SCL/GPIOC_85k/GPIO_SW_39
	0x5800,		//SATA_GP0/GPIOC_0/GPIO_SW_40
	0x5808,		//SATA_GP1/GPIOC_1/SATA_DEVSLP0/GPIO_SW_41
	0x5810,		//SATA_LEDN/GPIOC_2/GPIO_SW_42
	0x5818,		//SATA_GP2/GPIO_SW_43
	0x5820,		//MF_SMB_ALERTB/GPIO_SW_44
	0x5828,		//SATA_GP3/GPIO_SW_45
	0x5830,		//MF_SMB_CLK/GPIO_SW_46
	0x5838,		//MF_SMB_DATA/GPIO_SW_47
	0x5C00,		//PCIE_CLKREQ0B/GPIOC_3/GPIO_SW_48
	0x5C08,		//PCIE_CLKREQ1B/GPIOC_4/GPIO_SW_49
	0x5C10,		//GP_SSP_2_CLK/GPIOC_62/SATA_DEVSLP1/GPIO_SW_50
	0x5C18,		//PCIE_CLKREQ2B/GPIOC_5/GPIO_SW_51
	0x5C20,		//GP_SSP_2_RXD/GPIOC_64/GPIO_SW_52
	0x5C28,		//PCIE_CLKREQ3B/GPIOC_6/GPIO_SW_53
	0x5C30,		//GP_SSP_2_FS/GPIOC_63/MHSI_CAWAKE/GPIO_SW_54
	0x5C38,		//GP_SSP_2_TXD/GPIOC_65/GPIO_SW_55
};

//59 PINS , This is SUS for baytrail board!
static ULONG NCORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_NCORE_MMIO] =
{
    0x4400,		// GPIO_DFX0/GPIOS_22/SUS_VALID/GPIO_N_0
	0x4408, 	// GPIO_DFX3/GPIOS_23/SUS_OBS_0/GPIO_N_1
    0x4410, 	// GPIO_DFX7/GPIOS_24/SUS_OBS_1/GPIO_N_2
    0x4418, 	// GPIO_DFX1/GPIOS_25/SUS_OBS_2/GPIO_N_3
    0x4420, 	// GPIO_DFX5/GPIOS_26/SUS_OBS_3/GPIO_N_4
    0x4428,		// GPIO_DFX4/GPIOS_27/SUS_OBS_4/GPIO_N_5
	0x4430,		// GPIO_DFX8/GPIOS_28/SUS_OBS_5/GPIO_N_6
	0x4438,		// GPIO_DFX2/GPIOS_29/SUS_OBS_6/GPIO_N_7
	0x4440,		// GPIO_DFX6/GPIOS_30/SUS_OBS_7/GPIO_N_8
	0x4800,		//GPIO_SUS/GPIO_N_9
	0x4808,		//SEC_GPIO_SUS10/GPIO_N_10
	0x4810,		//GPIO_SUS3/SEC_TDI/PCI_WAKE3_B/DFX_VISA_OBS1/GPIO_N_11
	0x4818,		//GPIO_SUS7/PMU_SUSCLK3/DFX_VISA_OBS5/GPIO_N_12
	0x4820,		//GPIO_SUS1/SEC_TCK/PCI_WAKE1_B/DFX_VISA_VALID/GPIO_N_13
	0x4828,		//GPIO_SUS5/PMU_SUSCLK1/DFX_VISA_OBS3/GPIO_N_14
	0x4830,		//SEC_GPIO_SUS11/GPIO_N_15
	0x4838,		//GPIO_SUS4/SEC_TDO/DFX_VISA_OBS2/GPIO_N_16
	0x4840,		//SEC_GPIO_SUS8/DFX_VISA_OBS6/GPIO_N_17
	0x4848,		//GPIO_SUS2/SEC_TMS/PCI_WAKE2_B/DFX_VISA_OBS0/GPIO_N_18
	0x4850,		//GPIO_SUS6/PMU_SUSCLK2/DFX_VISA_OBS4/GPIO_N_19
	0x4858,		//CX_PREQ_B/GPIO_N_20
	0x4860,		//SEC_GPIO_SUS9/DFX_VISA_OBS7/GPIO_N_21
	0x4C00,		//TRST_B/GPIO_N_22
	0x4C08,		//TCK/GPIO_N_23
	0x4C10,		//PROCHOT_B/GPIO_N_24
	0x4C18,		//SVID0_DATA/GPIO_N_25
	0x4C20,		//TMS/GPIO_N_26
	0x4C28,		//CX_PRDY_B/GPIO_N_27
	0x4C40,		//SVID0_ALERT_B/GPIO_N_28
	0x4C48,		//TDO/GPIO_N_29
	0x4C50,		//SVID0_CLK/GPIO_N_30
	0x4C58,		//TDI/GPIO_N_31
	0x5000,		//GP_CAMERASB05/GPIONC_20/OBS_4/GPIO_N_32
	0x5008,		//GP_CAMERASB02/GPIO_N_33
	0x5010,		//GP_CAMERASB08/GPIONC_23/OBS_7/GPIO_N_34
	0x5018,		//GP_CAMERASB00/GPIONC_15/valid/GPIO_N_35
	0x5020,		//GP_CAMERASB06/GPIONC_21/OBS_5/GPIO_N_36
	0x5028,		//GP_CAMERASB10/GPIONC_25/GPIO_N_37
	0x5030,		//GP_CAMERASB03/GPIONC_18/OBS_2/GPIO_N_38
	0x5038,		//GP_CAMERASB09/GPIONC_24/GPIO_N_39
	0x5040,		//GP_CAMERASB01/GPIONC_16/OBS_0/GPIO_N_40
	0x5048,		//GP_CAMERASB07/GPIONC_22/OBS_6/GPIO_N_41
	0x5050,		//GP_CAMERASB11/GPIONC_26/GPIO_N_42
	0x5058,		//GP_CAMERASB04/GPIONC_19/OBS_3/GPIO_N_43
	0x5400,		//PANEL0_BKLTEN/GPIONC_4/GPIO_N_44
	0x5408,		//HV_DDI0_HPD/GPIONC_0/PLLLOCKEN/GPIO_N_45
	0x5410,		//HV_DDI2_DDC_SCL/GPIONC_14/GPIO_N_46
	0x5418,		//PANEL1_BKLTCTL/GPIONC_11/LDO_DIG_MON/GPIO_N_47
	0x5420,		//HV_DDI1HPD/GPIONC_6/OSCCLKCTRL/GPIO_N_48
	0x5428,		//PANEL0_BKLTCTL/GPIONC_5/GPIO_N_49
	0x5430,		//HV_DDI0_DDC_SDA/GPIONC_1/GPIO_N_50
	0x5438,		//HV_DDI2_DDC_SCL/GPIONC_14/GPIO_N_51
	0x5440,		//HV_DDI2_HPD/GPIO_N_52
	0x5448,		//PANEL1_VDDEN/GPIONC_9/LYA/LDO_ANALOG_OBS/GPIO_N_53
	0x5450,		//PANEL1_BKLTEN/GPIONC_10/LYA_B/GPIO_N_54
	0x5460,		//PANEL0_VDDEN/GPIONC_3/GPIO_N_55
};	

//12 used PINS 12 invalid pins
static ULONG ECORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_ECORE_MMIO] =
{
	0x4400,		//PMU_SLP_S3_B/GPIO_E_0
	0x4408,		//PMU_BATLOW_B/GPIO_E_1
	0x4410,		//SUS_STAT_B/GPIOS_18/GPIO_E_2
	0x4418,		//PMU_SLP_S0IX_B/GPIOS_13/GPIO_E_3
	0x4420,		//PMU_AC_PRESENT/GPIO_E_4
	0x4428,		//PMU_PLTRST_B/GPIO_E_5
	0x4430,		//PMU_SUSCLK/GPIOS_12/GPIO_E_6
	0x4438,		//PMU_SLP_LAN_B/GPIOS_14/ULPI_RESET/GPIO_E_7
	0x4440,		//PMU_PWRBTN_B/GPIOS_16/GPIO_E_8
	0x4448,		//PMU_SLP_S4_B/GPIO_E_9
	0x4450,		//PMU_WAKE_B/GPIOS_15/GPIO_E_10
	0x4458,		//PMU_WAKE_LAN_B/GPIOS_17/GPIO_E_11
    0x4800,		//MF_GPIO_3/GPIO_E_12
	0x4808, 	//MF_GPIO_7/GPIO_E_13
	0x4810, 	//MF_I2C1_SCL/GPIO_E_14
	0x4818, 	//MF_GPIO_1/GPIO_E_15
	0x4820, 	//MF_GPIO_5/GPIO_E_16
	0x4828, 	//MF_GPIO_9/GPIO_E_17
	0x4830, 	//MF_GPIO_0/GPIO_E_18
	0x4838, 	//MF_GPIO_4/GPIO_E_19
	0x4840, 	//MF_GPIO_8/GPIO_E_20
	0x4848, 	//MF_GPIO_2/GPIO_E_21
	0x4850, 	//MF_GPIO_6/GPIO_E_22
	0x4858, 	//MF_I2C1_SDA/GPIO_E_23
};

//55 PINS
static ULONG SECORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_SECORE_MMIO] =
{
	0x4400,		//MF_PLT_CLK0/GPIO_SE_0
	0x4408,		//PWM1/GPIOC_95/GPIO_SE_1
	0x4410,		//MF_PLT_CLK1/GPIO_SE_2
	0x4418,		//MF_PLT_CLK4/GPIO_SE_3
	0x4420,		//MF_PLT_CLK3/GPIO_SE_4
	0x4428,		//PWM0/GPIOC_94/GPIO_SE_5
	0x4430,		//MF_PLT_CLK5/GPIO_SE_6
	0x4438,		//MF_PLT_CLK2/GPIO_SE_7
	0x4800,		//SDMMC2_D3_CD_B/GPIOC_31/GPIO_SE_8
	0x4808,		//SDMMC1_CLK/GPIOC_16/GPIO_SE_9
	0x4810,		//SDMMC1_D0/GPIOC_17/GPIO_SE_10
	0x4818,		//SDMMC2_D1/GPIOC_29/GPIO_SE_11
	0x4820,		//SDMMC2_CLK/GPIOC_27/GPIO_SE_12
	0x4828,		//SDMMC1_D2/GPIOC_19/GPIO_SE_13
	0x4830,		//SDMMC2_D2/GPIOC_30/GPIO_SE_14
	0x4838,		//SDMMC2_CMD/GPIOC_32/GPIO_SE_15
	0x4840,		//SDMMC1_CMD/GPIOC_25/GPIO_SE_16
	0x4848,		//SDMMC1_D1/GPIOC_18/GPIO_SE_17
	0x4850,		//SDMMC2_D0/GPIOC_28/GPIO_SE_18
	0x4858,		//SDMMC1_D3_CD_B/GPIOC_20/GPIO_SE_19
	0x4C00,		//SDMMC3_D1/GPIOC_35/GPIO_SE_20
	0x4C08,		//SDMMC3_CLK/GPIOC_33/GPIO_SE_21
	0x4C10,		//SDMMC3_D3/GPIOC_37/GPIO_SE_22
	0x4C18,		//SDMMC3_D2/GPIOC_36/GPIO_SE_23
	0x4C20,		//SDMMC3_CMD/GPIOC_39/GPIO_SE_24
	0x4C28,		//SDMMC3_D0/GPIOC_34/GPIO_SE_25
	0x5000,		//MF_LPC_AD2/GPIO_SE_26
	0x5008,		//LPC_CLKRUNB/GPIOC_49/GPIO_SE_27
	0x5010,		//MF_LPC_AD0/GPIO_SE_28
	0x5018,		//LPC_FRAMEB/GPIOC_46/GPIO_SE_29
	0x5020,		//MF_LPC_CLKOUT1/GPIO_SE_30
	0x5028,		//MF_LPC_AD3/GPIO_SE_31
	0x5030,		//MF_LPC_CLKOUT0/GPIO_SE_32
	0x5038,		//MF_LPC_AD1/GPIO_SE_33
	0x5400,		//SPI1_MISO/GPIOC_67/GPIOSE_34
	0x5408,		//SPI1_CS0_B/GPIOC_66/GPIO_SE_35
	0x5420,		//SPI1_CLK/GPIOC_69/GPIO_SE_36
	0x5418,		//MMC1_D6/GPIOC_23/GPIO_SE_37
	0x5420,		//SPI1_MOSI/GPIOC_68/GPIO_SE_38
	0x5428,		//MMC1_D5/GPIOC_22/GPIO_SE_39
	0x5430,		//SPI1_CS1_B/GPIO_SE_40
	0x5438,		//MMC1_D4_SD_WE/GPIOC_21/GPIO_SE_41
	0x5440,		//MMC1_D7/GPIOC_24/GPIO_SE_42
	0x5448,		//MMC1_RCLK/GPIO_SE_43
	0x5800,		//USB_OC1_B/GPIOS_20/GPIO_SE_44
	0x5808,		//PMU_RESETBUTTON_B/GPIO_SE_45
	0x5810,		//GPIO_ALERT/GPIO_SE_46
	0x5818,		//SDMMC3_PWR_EN_B/GPIOC_41/GPIO_SE_47
	0x5820,		//ILB_SERIRQ/GPIOC_50/GPIO_SE_48
	0x5828,		//USB_OC0_B/GPIOS_19/GPIO_SE_49
	0x5830,		//SDMMC3_CD_B/GPIOC_38/GPIO_SE_50
	0x5838,		//SPKR/GPIOC_54/MHSI_CAWAKE/GPIO_SE_51
	0x5840,		//SUSPWRDNACK/GPIOS_11/GPIO_SE_52
	0x5848,		//SPARE_PIN/GPIO_SE_53
	0x5850,		//SDMMC3_1P8_EN/GPIOC_40/GPIO_SE_54
};	



/*GPIO Device Driver's Context Structure*/
typedef struct GPIO_DEVICE_CONTEXT
{
	DWORD					dwDeviceIndex;                     //Id of GPIO controller
    DWORD					dwMemBaseAddr;                     //Memory base address
	ULONG					ulMemLen;                          //Size of memory base address
	DWORD					ulControllerTotalPins;             //Total GPIO pins for a controller
	DWORD					ulControllerPinsPerBank;			 //Pins per bank
	GPIO_CORE_BANK_MMIO		MMIOBanks[GPIO_TOTAL_BANKS_SWCORE_MMIO];  //optimise the array data size
	HKEY					hKey;
}GPIO_DEVICE_CTXT, *PDEVICE_CONTEXT;



/*GPIO driver init function*/
DWORD GPI_Init( LPCTSTR pContext, LPCVOID lpvBusContext);

/*GPIO driver open*/
DWORD GPI_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode);

/*GPIO driver direction setting function*/
BOOL GPIOConnectIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to read as input function*/
DWORD GPIOReadIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to write as output function*/
BOOL GPIOWriteIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver query pin's setting function*/
BOOL GPIOQueryIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to configure multiplexing for selected GPIO pins function*/
BOOL GPIOMuxIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver IOControl function*/
BOOL GPI_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut, 
                   DWORD dwLenOut,PDWORD pdwActualOut);

/*GPIO close function*/
BOOL GPI_Close(DWORD hOpenContext);

/*GPIO driver deinit function*/
BOOL GPI_Deinit(DWORD hDeviceContext);

#ifdef __cplusplus
}
#endif 

#endif // __GPIO_H__

