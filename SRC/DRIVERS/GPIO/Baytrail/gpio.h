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


// Score register base address
#define GPIO_SCORE_BASE_ADDR          (0xFED0C000)
// Ncore register base address
#define GPIO_NCORE_BASE_ADDR          (0xFED0D000)
// SUS register base address
#define GPIO_SUS_BASE_ADDR            (0xFED0E000)


// SCore Virtual GPIO pin number start position
#define GPIO_SCORE_VIRTUAL_PINS_START (102)
// Total score gpios pin in vlv2(102 normal pins plus 21 virtual pins)
#define GPIO_TOTAL_PINS_SCORE_MMIO    (102+21)
// Total ncore gpios pin in vlv2
#define GPIO_TOTAL_PINS_NCORE_MMIO    (27)
// Total sus gpios pin in vlv2
#define GPIO_TOTAL_PINS_SUS_MMIO      (44)
// Total gpios pin in vlv2
#define GPIO_TOTAL_PINS_MMIO          (GPIO_TOTAL_PINS_SCORE_MMIO+GPIO_TOTAL_PINS_NCORE_MMIO+GPIO_TOTAL_PINS_SUS_MMIO)
// Pins per bank
#define GPIO_PINS_PER_BANK_MMIO       (32)
// Trigger status bits per bank
#define GPIO_TS_PER_BANK_MMIO         (32)
#define GPIO_TOTAL_BANKS_MMIO         ((GPIO_TOTAL_PINS_MMIO+GPIO_PINS_PER_BANK_MMIO-1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_SCORE_MMIO   ((GPIO_TOTAL_PINS_SCORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_NCORE_MMIO   ((GPIO_TOTAL_PINS_NCORE_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)
#define GPIO_TOTAL_BANKS_SUS_MMIO     ((GPIO_TOTAL_PINS_SUS_MMIO + GPIO_PINS_PER_BANK_MMIO -1)/GPIO_PINS_PER_BANK_MMIO)



// Bit 27 (direct_irq_en) of PCONF0 (MMIO)
#define DIRECT_IRQ_MASK               (1L<<27)
// Bit 26 (GD_TNE) of PCONF0 (MMIO)
#define TNE_BIT_MASK                  (1L<<26)
// Bit 25 (GD_TPE) of PCONF0 (MMIO)
#define TPE_BIT_MASK                  (1L<<25)
// Bit 24 (GD_level) of PCONF0 (MMIO)
#define GD_LVL_BIT_MASK               (1L<<24)
// Bit 2-0 (Func_Pin_Mux ) of PCONF0 (MMIO)
#define VV_PIN_MUX_MASK                0x07
// Bit 0 (Pad Val) of PAD_VAL (output must be enabled to write value)
#define LVL_BIT_MASK                  (1L<<0)
// Bit 2 (Iinenb) of PAD_VAL (input enable-active low)
#define INPUT_BIT_MASK                (1L<<2)
// Bit 1 (Ioutenb) of PAD_VAL (output enable-active low)
#define OUTPUT_BIT_MASK               (1L<<1)
#define DIRECTION_MASK                (INPUT_BIT_MASK | OUTPUT_BIT_MASK)
#define TS_STATUS_OFFSET              (0x800)
#define GPIO_TS_BASE_ADDR             (0x800)

// Enumeration for GPIO communities
// Please refer to VLV2_GPIO_HAS
typedef enum
{
    GPIOInvalid,
    // South community
    GPIO_SCORE,
    // North community
    GPIO_NCORE,
    // SUS community
    GPIO_SUS
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
    ULONG   ulPadValue;
    ULONG   ulReserved[1];
} GPIO_CORE_BANK_REGISTERS_MMIO, *PGPIO_CORE_BANK_REGISTERS_MMIO;
#pragma pack()
// Declare a structure which describes the GPIO bank.
typedef struct
{
    PGPIO_CORE_BANK_REGISTERS_MMIO   Registers[GPIO_PINS_PER_BANK_MMIO];          //pointer to hardware registers

} GPIO_CORE_BANK_MMIO, *PGPIO_CORE_BANK_MMIO;
/*
  The first 102 pins are normal pins
  The last 21 pins are virutal gpio pins
*/
static ULONG SCORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_SCORE_MMIO] =
{
    // GPIOSCore start (please refer VLV2 gpscore.doc)
    0x550,    // SATA_GP0/GPIOC_0
    0x590,    // SATA_GP1/GPIOC_1/SATA_DEVSLP0
    0x5D0,    // SATA_LEDN/GPIOC_2
    0x600,    // PCIE_CLKREQ0B/GPIOC_3
    0x630,    // PCIE_CLKREQ1B/GPIOC_4
    0x660,    // PCIE_CLKREQ2B/GPIOC_5
    0x620,    // PCIE_CLKREQ3B/GPIOC_6
    0x650,    // PCIE_CLKREQ4B/GPIOC_7/SDMMC3WP
    0x220,    // HDA_RSTB/GPIOC_8/GP_SSP_0_I2S_CLK
    0x250,    // HDA_SYNC/GPIOC_9/GP_SSP_0_I2S_FS
    0x240,    // HDA_CLK/GPIOC_10/GP_SSP_0_I2S_TXD
    0x260,    // HDA_SDO/GPIOC_11/GP_SSP_0_I2S_RXD
    0x270,    // HDA_SDI0/GPIOC_12/GP_SSP_1_I2S_CLK
    0x230,    // HDA_SDI1/GPIOC_13/GP_SSP_1_I2S_FS
    0x280,    // HDA_DOCKRSTB/GPIOC_14/GP_SSP_1_I2S_TXD
    0x540,    // HDA_DOCKENB/GPIOC_15/GP_SSP_1_I2S_RXD
    0x3E0,    // SDMMC1_CLK/GPIOC_16
    0x3D0,    // SDMMC1_D0/GPIOC_17
    0x400,    // SDMMC1_D1/GPIOC_18
    0x3B0,    // SDMMC1_D2/GPIOC_19
    0x360,    // SDMMC1_D3_CD_B/GPIOC_20
    0x380,    // MMC1_D4_SD_WE/GPIOC_21
    0x3C0,    // MMC1_D5/GPIOC_22
    0x370,    // MMC1_D6/GPIOC_23
    0x3F0,    // MMC1_D7/GPIOC_24
    0x390,    // SDMMC1_CMD/GPIOC_25
    0x330,    // MMC1_RESET_B/GPIOC_26/DEVSLP0
    0x320,    // SDMMC2_CLK/GPIOC_27
    0x350,    // SDMMC2_D0/GPIOC_28
    0x2F0,    // SDMMC2_D1/GPIOC_29
    0x340,    // SDMMC2_D2/GPIOC_30
    0x310,    // SDMMC2_D3_CD_B/GPIOC_31
    0x300,    // SDMMC2_CMD/GPIOC_32
    0x2B0,    // SDMMC3_CLK/GPIOC_33
    0x2E0,    // SDMMC3_D0/GPIOC_34
    0x290,    // SDMMC3_D1/GPIOC_35
    0x2D0,    // SDMMC3_D2/GPIOC_36
    0x2A0,    // SDMMC3_D3/GPIOC_37
    0x3A0,    // SDMMC3_CD_B/GPIOC_38
    0x2C0,    // SDMMC3_CMD/GPIOC_39
    0x5F0,    // SDMMC3_1P8_EN/GPIOC_40
    0x690,    // SDMMC3_PWR_EN_B/GPIOC_41
    0x460,    // LPC_AD0/GPIOC_42
    0x440,    // LPC_AD1/GPIOC_43
    0x430,    // LPC_AD2/GPIOC_44
    0x420,    // LPC_AD3/GPIOC_45
    0x450,    // LPC_FRAMEB/GPIOC_46
    0x470,    // LPC_CLKOUT0/GPIOC_47
    0x410,    // LPC_CLKOUT1/GPIOC_48
    0x480,    // LPC_CLKRUNB/GPIOC_49
    0x560,    // ILB_SERIRQ/GPIOC_50
    0x5A0,    // SMB_DATA/GPIOC_51
    0x580,    // SMB_CLK/GPIOC_52
    0x5C0,    // SMB_ALERTB/GPIOC_53
    0x670,    // SPKR/GPIOC_54/MHSI_CAWAKE
    0x4D0,    // MHSI_ACDATA/GPIOC_55
    0x4F0,    // MHSI_ACFLAG/GPIOC_56
    0x530,    // MHSI_ACREADY/GPIOC_57/UART3_TXD
    0x4E0,    // MHSI_ACWAKE/GPIOC_58
    0x510,    // MHSI_CADATA/GPIOC_59
    0x500,    // MHSI_CAFLAG/GPIOC_60
    0x520,    // MHSI_CAREADY/GPIOC_61/UART3_RXD
    0x0D0,    // GP_SSP_2_CLK/GPIOC_62/SATA_DEVSLP1
    0x0C0,    // GP_SSP_2_FS/GPIOC_63/MHSI_CAWAKE
    0x0F0,    // GP_SSP_2_RXD/GPIOC_64
    0x0E0,    // GP_SSP_2_TXD/GPIOC_65
    0x110,    // SPI1_CS0_B/GPIOC_66
    0x120,    // SPI1_MISO/GPIOC_67
    0x130,    // SPI1_MOSI/GPIOC_68
    0x100,    // SPI1_CLK/GPIOC_69
    0x020,    // UART1_RXD/GPIOC_70/MHSI_CAREADY
    0x010,    // UART1_TXD/GPIOC_71/MHSI_ACREADY
    0x000,    // UART1_RTS_B/GPIOC_72
    0x040,    // UART1_CTS_B/GPIOC_73
    0x060,    // UART2_RXD/GPIOC_74
    0x070,    // UART2_TXD/GPIOC_75
    0x090,    // UART2_RTS_B/GPIOC_76
    0x080,    // UART2_CTS_B/GPIOC_77
    0x210,    // I2C0_SDA/GPIOC_78
    0x200,    // I2C0_SCL/GPIOC_79
    0x1F0,    // I2C1_SDA/GPIOC_80
    0x1E0,    // I2C1_SCL/GPIOC_81/CCUPLLLOCKEN
    0x1D0,    // I2C2_SDA/GPIOC_82
    0x1B0,    // I2C2_SCL/GPIOC_83
    0x190,    // I2C3_SDA/GPIOC_84
    0x1C0,    // I2C3_SCL/GPIOC_85
    0x1A0,    // I2C4_SDA/GPIOC_86
    0x170,    // I2C4_SCL/GPIOC_87
    0x150,    // I2C5_SDA/GPIOC_88
    0x140,    // I2C5_SCL/GPIOC_89
    0x180,    // I2C6_SDA/GPIOC_90/NMI_B
    0x160,    // I2C6_SCL/GPIOC_91/SDMMC3_WP
    0x050,    // I2C_NFC_SDA/GPIOC_92
    0x030,    // I2C_NFC_SCL/GPIOC_93
    0x0A0,    // PWM0/GPIOC_94
    0x0B0,    // PWM1/GPIOC_95
    0x6A0,    // PLT_CLK0/GPIOC_96
    0x570,    // PLT_CLK1/GPIOC_97
    0x5B0,    // PLT_CLK2/GPIOC_98
    0x680,    // PLT_CLK3/GPIOC_99
    0x610,    // PLT_CLK4/GPIOC_100
    0x640,    // PLT_CLK5/GPIOC_101
    0x6B0,    // VGPIO0
    0x6C0,    // VGPIO1
    0x6D0,    // VGPIO2
    0x6E0,    // VGPIO3
    0x6F0,    // VGPIO4
    0x700,    // VGPIO5
    0x710,    // VGPIO6
    0x720,    // VGPIO7
    0x730,    // VGPIO8
    0x740,    // VGPIO9
    0x750,    // VGPIO10
    0x760,    // VGPIO11
    0x770,    // VGPIO12
    0x780,    // VGPIO13
    0x790,    // VGPIO14
    0x7A0,    // VGPIO15
    0x7B0,    // VGPIO16
    0x7C0,    // VGPIO17
    0x7D0,    // VGPIO18
    0x7E0,    // VGPIO19
    0x7F0,    // VGPIO20
};

static ULONG NCORE_Pin2Offsetmapping[GPIO_TOTAL_PINS_NCORE_MMIO] =
{
    //GPIONCore start (please refer VLV2 gpncore.doc)
    0x130,  // HV_DDI0_HPD/GPIONC_0/PLLLOCKEN     
    0x120,  // HV_DDI0_DDC_SDA/GPIONC_1
    0x110,  // HV_DDI0_DDC_SCL/GPIONC_2
    0x140,  // PANEL0_VDDEN/GPIONC_3
    0x150,  // PANEL0_BKLTEN/GPIONC_4
    0x160,  // PANEL0_BKLTCTL/GPIONC_5   
    0x180,  // HV_DDI1HPD/GPIONC_6/OSCCLKCTRL
    0x190,  // HV_DDI1_DDC_SDA/GPIONC_7
    0x170,  // HV_DDI1_DDC_SCL/GPIONC_8
    0x100,  // PANEL1_VDDEN/GPIONC_9/LYA/LDO_ANALOG_OBS
    0x0E0,  // PANEL1_BKLTEN/GPIONC_10/LYA_B/ IDFT_VREF
    0x0F0,  // PANEL1_BKLTCTL/GPIONC_11/LDO_DIG_MON
    0x0C0,  // GP_INTD_DSI_TE1/GPIONC_12
    0x1A0,  // HV_DDI2_DDC_SDA/GPIONC_13
    0x1B0,  // HV_DDI2_DDC_SCL/GPIONC_14
    0x010,  // GP_CAMERASB00/GPIONC_15/valid
    0x040,  // GP_CAMERASB01/GPIONC_16/OBS_0
    0x080,  // GP_CAMERASB02/GPIONC_17/OBS_1
    0x0B0,  // GP_CAMERASB03/GPIONC_18/OBS_2
    0x000,  // GP_CAMERASB04/GPIONC_19/OBS_3
    0x030,  // GP_CAMERASB05/GPIONC_20/OBS_4
    0x060,  // GP_CAMERASB06/GPIONC_21/OBS_5
    0x0A0,  // GP_CAMERASB07/GPIONC_22/OBS_6
    0x0D0,  // GP_CAMERASB08/GPIONC_23/OBS_7
    0x020,  // GP_CAMERASB09/GPIONC_24
    0x050,  // GP_CAMERASB10/GPIONC_25
    0x090,  // GP_CAMERASB11/GPIONC_26
  
   
};

static ULONG SUS_Pin2Offsetmapping[GPIO_TOTAL_PINS_SUS_MMIO] =
{
    // GPIOSUS start (please refer VLV2 gpssus.doc)
    0x1D0,  // GPIO_SUS0/RO_BYPASS
    0x210,  // GPIO_SUS1/SEC_TCK/PCI_WAKE1_B/DFX_VISA_VALID
    0x1E0,  // GPIO_SUS2/SEC_TMS/PCI_WAKE2_B/DFX_VISA_OBS0
    0x1F0,  // GPIO_SUS3/SEC_TDI/PCI_WAKE3_B/DFX_VISA_OBS1
    0x200,  // GPIO_SUS4/SEC_TDO/DFX_VISA_OBS2
    0x220,  // GPIO_SUS5/PMU_SUSCLK1/DFX_VISA_OBS3
    0x240,  // GPIO_SUS6/PMU_SUSCLK2/DFX_VISA_OBS4
    0x230,  // GPIO_SUS7/PMU_SUSCLK3/DFX_VISA_OBS5
    0x260,  // SEC_GPIO_SUS8/DFX_VISA_OBS6
    0x250,  // SEC_GPIO_SUS9/DFX_VISA_OBS7
    0x120,  // SEC_GPIO_SUS10
    0x070,  // SUSPWRDNACK/GPIOS_11 
    0x0B0,  // PMU_SUSCLK/GPIOS_12
    0x140,  // PMU_SLP_S0IX_B/GPIOS_13
    0x110,  // PMU_SLP_LAN_B/GPIOS_14/ULPI_RESET
    0x010,  // PMU_WAKE_B/GPIOS_15
    0x080,  // PMU_PWRBTN_B/GPIOS_16
    0x0A0,  // PMU_WAKE_LAN_B/GPIOS_17
    0x130,  // SUS_STAT_B/GPIOS_18
    0x0C0,  // USB_OC0_B/GPIOS_19
    0x000,  // USB_OC1_B/GPIOS_20
    0x020,  // SPI_CS1_B/GPIOS_21
    0x170,  // GPIO_DFX0/GPIOS_22/SUS_VALID
    0x270,  // GPIO_DFX1/GPIOS_23/SUS_OBS_0
    0x1C0,  // GPIO_DFX2/GPIOS_24/SUS_OBS_1
    0x1B0,  // GPIO_DFX3/GPIOS_25/SUS_OBS_2
    0x160,  // GPIO_DFX4/GPIOS_26/SUS_OBS_3
    0x150,  // GPIO_DFX5/GPIOS_27/SUS_OBS_4
    0x180,  // GPIO_DFX6/GPIOS_28/SUS_OBS_5
    0x190,  // GPIO_DFX7/GPIOS_29/SUS_OBS_6
    0x1A0,  // GPIO_DFX8/GPIOS_30/SUS_OBS_7
    0x330,  // USB_ULPI_0_CLK/GPIOS_31
    0x380,  // USB_ULPI_0_DATA0/GPIOS_32
    0x360,  // USB_ULPI_0_DATA1/GPIOS_33
    0x310,  // USB_ULPI_0_DATA2/GPIOS_34
    0x370,  // USB_ULPI_0_DATA3/GPIOS_35
    0x300,  // USB_ULPI_0_DATA4/GPIOS_36
    0x390,  // USB_ULPI_0_DATA5/GPIOS_37
    0x320,  // USB_ULPI_0_DATA6/GPIOS_38
    0x3A0,  // USB_ULPI_0_DATA7/GPIOS_39
    0x340,  // USB_ULPI_0_DIR/GPIOS_40
    0x350,  // USB_ULPI_0_NXT/GPIOS_41
    0x3B0,  // USB_ULPI_0_STP/GPIOS_42
    0x280,  // USB_ULPI_0_REFCLK/GPIOS_43
};



/*GPIO Device Driver's Context Structure*/
typedef struct GPIO_DEVICE_CONTEXT
{
	DWORD					dwDeviceIndex;                     //Id of GPIO controller
    DWORD					dwMemBaseAddr;                     //Memory base address
	ULONG					ulMemLen;                          //Size of memory base address
	DWORD					ulControllerTotalPins;             //Total GPIO pins for a controller
	DWORD					ulControllerPinsPerBank;			 //Pins per bank
	GPIO_CORE_BANK_MMIO		MMIOBanks[GPIO_TOTAL_BANKS_SCORE_MMIO];  //optimise the array data size
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

