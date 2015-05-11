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

i2c.h

Abstract:  

Holds definitions private to the implementation of the machine independent
driver interface.

Notes: 


--*/

#ifndef __I2C_H__
#define __I2C_H__

#include <windows.h>
#include <nkintr.h>
#include "../INC/types.h"
#include <notify.h>
#include <memory.h>
#include <nkintr.h>
#include <pegdser.h>
#include <devload.h>
#include <ddkreg.h>
#include <ceddk.h>
#include <cebuscfg.h>
#include <linklist.h>
#include <Giisr.h>
#include <Kfuncs.h>
#include "I2Cpublic.h"


#ifdef __cplusplus
extern "C" {
#endif
#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("I2C"), {
        TEXT("Errors"),TEXT("Warnings"),TEXT("Init"),TEXT("Deinit"),
            TEXT("IOCtl"),TEXT("IST"),TEXT("Open"),TEXT("Close"),
            TEXT("Info"),TEXT("Function"), TEXT("Thread"),TEXT("Undefined"),
            TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
            0x8000
};
/* Debug Zones */
#define    ZONE_ERROR           (DEBUGZONE(0))
#define    ZONE_WARN            (DEBUGZONE(1))
#define    ZONE_INIT            (DEBUGZONE(2))
#define    ZONE_DEINIT          (DEBUGZONE(3))
#define    ZONE_IOCTL           (DEBUGZONE(4))
#define    ZONE_IST		        (DEBUGZONE(5))
#define    ZONE_OPEN            (DEBUGZONE(6))
#define    ZONE_CLOSE           (DEBUGZONE(7))
#define    ZONE_INFO           (DEBUGZONE(8))
#define    ZONE_FUNC           (DEBUGZONE(9))
#endif //#ifdef DEBUG

#pragma once
//******************************************************************************
// I2C BUS SPEIFIC DEFINES
//******************************************************************************
typedef enum 
{
    I2C_BUS_SCL_HCNT = 0,
    I2C_BUS_SCL_LCNT,
    I2C_BUS_SDA_HOLD,
    I2C_BUS_SDA_SETUP,
    I2C_BUS_ITEMS
}I2C_BUS_SETTING;

typedef struct _I2C_BUS_SETTINGS
{
    ULONG   SS[I2C_BUS_ITEMS];
    ULONG   FS[I2C_BUS_ITEMS];

} I2C_BUS_SETTINGS, *PI2C_BUS_SETTINGS;


#define RX_FILL_VALUE           0
#define TX_FILL_VALUE           0

//******************************************************************************
// INTEL I2C SPEIFIC DEFINES
//******************************************************************************

//
// I2C bus descriptor
//
#define I2C_DMA_MAX_SEQ_OBJECTS_SUPPORTED   5


/*------------------------------------------------------------------------------
// Intel I2C controller registers.
------------------------------------------------------------------------------*/
typedef struct _I2C_REGISTERS
{
    // Register structure to match the
    // mapping of controller hardware.

    __declspec(align(4))  UINT32  IC_CON;                  // 0x00 Control, Default Value: 0067h
    __declspec(align(4))  UINT32  IC_TAR;                  // 0x04 Target Address
    __declspec(align(4))  UINT32  IC_SAR;                  // 0x08 Not used, Slave  Address
    __declspec(align(4))  UINT32  IC_HS_MADDR;             // 0x0C Not used, High Speed Master Mode Code Address
    __declspec(align(4))  UINT32  IC_DATA_CMD;             // 0x10
    __declspec(align(4))  UINT32  IC_SS_SCL_HCNT;          // 0x14 Standard Speed I2C Clock SCL High Count
    __declspec(align(4))  UINT32  IC_SS_SCL_LCNT;          // 0x18 Standard Speed I2C Clock SCL Low Count
    __declspec(align(4))  UINT32  IC_FS_SCL_HCNT;          // 0x1C Fast Speed I2C Clock SCL High Count
    __declspec(align(4))  UINT32  IC_FS_SCL_LCNT;          // 0x20 Fast Speed I2C Clock SCL Low Count
    __declspec(align(4))  UINT32  IC_HS_SCL_HCNT;          // 0x24 High Speed I2C Clock SCL High Count
    __declspec(align(4))  UINT32  IC_HS_SCL_LCNT;          // 0x28 High Speed I2C Clock SCL Low Count
    __declspec(align(4))  UINT32  IC_INTR_STAT;            // 0x2C Interrupt Status
    __declspec(align(4))  UINT32  IC_INTR_MASK;            // 0x30 Interrupt Mask
    __declspec(align(4))  UINT32  IC_RAW_INTR_STAT;        // 0x34 Raw Interrupt Status
    __declspec(align(4))  UINT32  IC_RX_TL;                // 0x38 Receive FIFO Threshold
    __declspec(align(4))  UINT32  IC_TX_TL;                // 0x3C Transmit FIFO Threshold
    __declspec(align(4))  UINT32  IC_CLR_INTR;             // 0x40 Clear Combined and Individual Interrupt
    __declspec(align(4))  UINT32  IC_CLR_RX_UNDER;         // 0x44 Clear RX_UNDER Interrupt
    __declspec(align(4))  UINT32  IC_CLR_RX_OVER;          // 0x48 Clear RX_OVER Interrupt
    __declspec(align(4))  UINT32  IC_CLR_TX_OVER;          // 0x4C TX_OVER Interrupt
    __declspec(align(4))  UINT32  IC_CLR_RD_REQ;           // 0x50 Clear RD_REQ Interrupt
    __declspec(align(4))  UINT32  IC_CLR_TX_ABRT;          // 0x54 Clear TX_ABRT Interrupt
    __declspec(align(4))  UINT32  IC_CLR_RX_DONE;          // 0x58 Clear RX_DONE Interrupt
    __declspec(align(4))  UINT32  IC_CLR_ACTIVITY;         // 0x5C Clear ACTIVITY Interrupt
    __declspec(align(4))  UINT32  IC_CLR_STOP_DET;         // 0x60 Clear STOP_DET Interrupt
    __declspec(align(4))  UINT32  IC_CLR_START_DET;        // 0x64 Clear START_DET Interrupt
    __declspec(align(4))  UINT32  IC_CLR_GEN_CALL;         // 0x68 Clear GEN_CALL Interrupt
    __declspec(align(4))  UINT32  IC_ENABLE;               // 0x6C 
    __declspec(align(4))  UINT32  IC_STATUS;               // 0x70 
    __declspec(align(4))  UINT32  IC_TXFLR;                // 0x74 Not used, Transmit FIFO Level
    __declspec(align(4))  UINT32  IC_RXFLR;                // 0x78 Not used in CLT, Receive FIFO Level
    __declspec(align(4))  UINT32  IC_SDA_HOLD;             // 0x7C SDA Hold Time - it is IC_RESERVED for CLV
    __declspec(align(4))  UINT32  IC_TX_ABRT_SOURCE;       // 0x80 Transmit Abort Source
    __declspec(align(4))  UINT32  IC_SLV_DATA_NACK_ONLY;   // 0x84 
    __declspec(align(4))  UINT32  IC_DMA_CR;               // 0x88 
    __declspec(align(4))  UINT32  IC_DMA_TDLR;             // 0x8C 
    __declspec(align(4))  UINT32  IC_DMA_RDLR;             // 0x90 
    __declspec(align(4))  UINT32  IC_SDA_SETUP;            // 0x94 
    __declspec(align(4))  UINT32  IC_ACK_GENERAL_CALL;     // 0x98 
    __declspec(align(4))  UINT32  IC_ENABLE_STATUS;        // 0x9C 
                                                                // EO IP v1.15 registers
    __declspec(align(4))  UINT32  IC_RESERVED3[21];        // 0xA0-0xF0
    __declspec(align(4))  UINT32  IC_COMP_PARAM1;          // 0xF4
    __declspec(align(4))  UINT32  IC_COMP_VERSION;         // 0xF8
    __declspec(align(4))  UINT32  IC_COMP_TYPE;            // 0xFC
    __declspec(align(4))  UINT32  IC_RESERVED4[448];       // 0x100-0x7FC

    __declspec(align(4))  UINT32  PRV_CLOCKS;              // 0x800
    __declspec(align(4))  UINT32  PRV_RESETS;              // 0x804
    __declspec(align(4))  UINT32  PRV_GENERAL;             // 0x808
    __declspec(align(4))  UINT32  PRV_RESERVED;            // 0x80C
    __declspec(align(4))  UINT32  PRV_SW_LTR;              // 0x810
    __declspec(align(4))  UINT32  PRV_HW_LTR;              // 0x814
    __declspec(align(4))  UINT32  PRV_TX_ACK_COUNT;        // 0x818
    __declspec(align(4))  UINT32  PRV_RESERVED6;           // 0x81C
    __declspec(align(4))  UINT32  PRV_TX_COMPL_INTR_STAT;  // 0x820
    __declspec(align(4))  UINT32  PRV_TX_COMPL_INTR_CLR;   // 0x824

} I2C_REGISTERS, *PI2C_REGISTERS;

// Defines to match the bit
// functionalities of each register.

/*------------------------------------------------------------------------------
// IC_CON register bits.
------------------------------------------------------------------------------*/
#define IC_CON_STANDARD_MODE            0x00000002
#define IC_CON_FAST_MODE                0x00000004
#define IC_CON_HIGH_SPEED_MODE          0x00000006
#define IC_CON_RESTART_EN               0x00000020
#define IC_CON_MASTER_MODE              0x00000041              // Bit 0 MASTER_MODE + Bit 6 IC_SLAVE_DISABLE

// SW define
//#define I2C_SPD_1000_KHZ                4                       //  Fast mode plus speed mode
//#define I2C_SPD_1700_KHZ                5            

/*------------------------------------------------------------------------------
// IC_TAR register bits.
------------------------------------------------------------------------------*/
#define IC_TAR_START_BYTE               0x00000C00
#define IC_TAR_10BITADDR_MASTER         0x00001000

/*------------------------------------------------------------------------------
// IC_DATA_CMD register bits.
------------------------------------------------------------------------------*/
#define IC_DATA_CMD_WRITE               0x00000000              // Bit 8 is zero
#define IC_DATA_CMD_READ                0x00000100
#define IC_DATA_CMD_DMA_READ            0x01
#define IC_DATA_CMD_STOP                0x00000200
#define IC_DATA_CMD_RESTART             0x00000400

/*------------------------------------------------------------------------------
// IC_STATUS register bits.
------------------------------------------------------------------------------*/
#define IC_STATUS_TFNF                  0x00000002
#define IC_STATUS_TFE                   0x00000004
#define IC_STATUS_RFNE                  0x00000008
#define IC_STATUS_RFF					0x00000010
#define IC_STATUS_MST_ACTIVITY          0x00000020
#define IC_STATUS_ACTIVITY				0x00000001

/*------------------------------------------------------------------------------
// IC_INTR_STAT & IC_INTR_MASK & IC_RAW_INTR_STAT register bits.
------------------------------------------------------------------------------*/
#define IC_INTR_GEN_CALL                0x00000800
#define IC_INTR_START_DET               0x00000400
#define IC_INTR_STOP_DET                0x00000200
#define IC_INTR_ACTIVITY                0x00000100
#define IC_INTR_RX_DONE                 0x00000080
#define IC_INTR_TX_ABRT                 0x00000040
#define IC_INTR_RD_REQ                  0x00000020
#define IC_INTR_TX_EMPTY                0x00000010
#define IC_INTR_TX_OVER                 0x00000008
#define IC_INTR_RX_FULL                 0x00000004
#define IC_INTR_RX_OVER                 0x00000002
#define IC_INTR_RX_UNDER                0x00000001

#define I2C_DISABLE_ALL_INTERRUPTS      0x000                   // Disable all interrupts
#define I2C_ENABLE_ALL_INTERRUPTS       0x2FF                   // Enable all interrupts

#define IC_INTR_LAST_BYTE               0x00100000
#define I2C_ALL_MASKABLE                0xFFF                   // All maskable interrupts bitmap
#define I2C_INTR_OFFSET                 0x2C 

// Emulated DMA Interrupt status
#define IC_INTR_DMA_TX                  0x00010000
#define IC_INTR_DMA_RX                  0x00020000
#define I2C_ALL_DMA_INTR                (IC_INTR_DMA_TX | IC_INTR_DMA_RX)

//
// I2C Controller Interrupt settings
//

// DMA bit fields
#define IC_DMA_CR_ENABLE_TX_DMA         0x2
#define IC_DMA_CR_ENABLE_RX_DMA         0x1
#define IC_DMA_CR_DISABLE_DMA           0x0

#define DMA_STATUS_NONE                 0x0
#define DMA_STATUS_COMPLETE             0x1
#define DMA_STATUS_ABORTED              0x2
#define DMA_STATUS_ERROR                0x4
#define DMA_STATUS_CANCELLED            0x8

/*------------------------------------------------------------------------------
// IC_TX_ABRT_SOURCE register bits.
------------------------------------------------------------------------------*/
#define IC_TX_ABRT_7B_ADDR_NOACK        0x00000001
#define IC_TX_ABRT_7B_ADDR_NOACK        0x00000001
#define IC_TX_ABRT_10ADDR1_NOACK        0x00000002
#define IC_TX_ABRT_10ADDR2_NOACK        0x00000004
#define IC_TX_ABRT_TXDATA_NOACK         0x00000008
#define IC_TX_ABRT_ARB_LOST             0x00001000
#define IC_TX_ABRT_USER                 (1 << 16)

#define IC_TX_ABRT_ADDR_MASK            (IC_TX_ABRT_7B_ADDR_NOACK | \
                                         IC_TX_ABRT_10ADDR1_NOACK | \
                                         IC_TX_ABRT_10ADDR2_NOACK)
#define IC_TX_ABRT_ALL_MASK             (IC_TX_ABRT_ADDR_MASK | IC_TX_ABRT_TXDATA_NOACK)
#define IC_TX_ABRT_NO_ABRT              0x00000000

/*------------------------------------------------------------------------------
// IC_ENABLE register bits.
------------------------------------------------------------------------------*/
//
// I2C Controller enable & disable
// 
#define I2C_CONTROLLER_DISABLE          0x0                     // Disable I2C Controller
#define I2C_CONTROLLER_ENABLE           0x1                     // Enable I2C Controller
#define I2C_CONTROLLER_EN_STATUS        0x1                     // I2C Controller enabled status bit
#define I2C_CONTROLLER_USER_ABORT       0x2                     // User Abort request (LPT-LP B0+)

//
// I2C Controller enable & disable retry attempts
// 
#define I2C_MASTER_ENABLE_STALL         25                      // Stall time of 25us
#define I2C_MASTER_MAX_ATTEMPTS         10                      // Attempt the check 10 times

/*------------------------------------------------------------------------------
// I2C Slave addressing
------------------------------------------------------------------------------*/
#define I2C_MAXIMUM_7BIT_ADDRESS        0x7F                    // Maximum 7-bit slave address

/*------------------------------------------------------------------------------
// I2C Controller FIFO conditions
------------------------------------------------------------------------------*/
#define I2C_TX_FIFO_FULL                0x0                     // Tx FIFO is full
#define I2C_TX_FIFO_NOT_FULL            0x2                     // Tx FIFO is not full
#define I2C_RX_FIFO_NOT_EMPTY           0x8                     // Rx FIFO not empty
#define I2C_RX_FIFO_EMPTY               0x0                     // Rx FIFO is empty
#define I2C_RX_FIFO_NOT_FULL            0x0                     // Rx FIFO is not full
#define I2C_RX_FIFO_FULL                0x10                    // Rx FIFO is full

/*------------------------------------------------------------------------------
// I2C Controller Tx & Rx FIFO threshold limits
------------------------------------------------------------------------------*/
#define I2C_TX_THRESHOLD_ZERO           0x0                     // Transmit FIFO threshold limit is zero
#define I2C_TX_THRESHOLD_255            0xff                    // Transmit FIFO threshold limit is 255 (maximum)
#define I2C_RX_THRESHOLD_ZERO           0x0                     // Receive FIFO threshold limit is zero
#define I2C_RX_THRESHOLD_255            0xff                    // Receive FIFO threshold limit is 255 (maximum)

/*------------------------------------------------------------------------------
// VLV Private space bits
------------------------------------------------------------------------------*/
#define PRV_CLOCKS_CLK_EN               0x00000001              // RO: Indicate 0-100MHz (LPT-LP compatible) 1-133MHz (HS compatible)

#define PRV_RESETS_NOT_FUNC_RST         0x00000001              // RW: Reset the func clock domain
#define PRV_RESETS_NOT_APB_RST          0x00000002              // RW: Reset the apb domain

// Init sequence values.
#define CONTROLLER_INIT_SEQ_RESETS              0x00000003
#define CONTROLLER_INIT_SEQ_PRV_CLOCK_PARAMS    0x80020003

#define PRV_GENERAL_LTR_EN              (1 << 1)
#define PRV_GENERAL_LTR_MODE_SW         (1 << 2)
#define PRV_GENERAL_IO_VOLD_1V8         (1 << 3)

// LPT-LP B0 fixes
#define PRV_GENERAL_LAST_FLAG           (1 << 4)
#define PRV_GENERAL_FIX_DIS_ACK_COUNT   (1 << 5)
#define PRV_GENERAL_FIX_DIS_LAST_BYTE   (1 << 6)
#define PRV_GENERAL_FIX_DIS_DMA_HANDS   (1 << 8)
#define PRV_GENERAL_FIX_DIS_STOP        (1 << 9)

// SW line controll
#define PRV_GENERAL_SCL_POST            (1 << 24)
#define PRV_GENERAL_SCL_PRE             (1 << 25)
#define PRV_GENERAL_SDA_POST            (1 << 26)
#define PRV_GENERAL_SDA_PRE             (1 << 27)
#define PRV_GENERAL_SCL_DRV_SHIFT        28
#define PRV_GENERAL_SCL_DRV             (1 << PRV_GENERAL_SCL_DRV_SHIFT)
#define PRV_GENERAL_SCL_SWEN            (1 << 29)
#define PRV_GENERAL_SDA_DRV_SHIFT        30
#define PRV_GENERAL_SDA_DRV             (1 << PRV_GENERAL_SDA_DRV_SHIFT)
#define PRV_GENERAL_SDA_SWEN            (1 << 31)

#define PRV_LTR_SNOOP_VAL_MASK          0x3FF
#define PRV_LTR_SNOOP_VAL_SHIFT         0
#define PRV_LTR_SNOOP_SCALE_1US         (2 << 10)
#define PRV_LTR_SNOOP_SCALE_32US        (3 << 10)
#define PRV_LTR_SNOOP_REQUIREMENT       (1 << 15)
#define PRV_LTR_NON_SNOOP_VAL_MASK      0x3FF
#define PRV_LTR_NON_SNOOP_VAL_SHIFT     16
#define PRV_LTR_NON_SNOOP_SCALE_1US     (2 << 26)
#define PRV_LTR_NON_SNOOP_SCALE_32US    (3 << 26)
#define PRV_LTR_NON_SNOOP_REQUIREMENT   (1 << 31)

#define PRV_LTR_NON_SNOOP_1US(value)    (PRV_LTR_NON_SNOOP_REQUIREMENT | \
                                         PRV_LTR_NON_SNOOP_SCALE_1US   | \
                                         (((value) & PRV_LTR_NON_SNOOP_VAL_MASK) << PRV_LTR_NON_SNOOP_VAL_SHIFT))

#define PRV_LTR_SNOOP_1US(value)        (PRV_LTR_SNOOP_REQUIREMENT | \
                                         PRV_LTR_SNOOP_SCALE_1US   | \
                                         (((value) & PRV_LTR_SNOOP_VAL_MASK) << PRV_LTR_SNOOP_VAL_SHIFT))

#define PRV_LTR_NON_SNOOP_32US(value)   (PRV_LTR_NON_SNOOP_REQUIREMENT | \
                                         PRV_LTR_NON_SNOOP_SCALE_32US  | \
                                         (((value) & PRV_LTR_NON_SNOOP_VAL_MASK) << PRV_LTR_NON_SNOOP_VAL_SHIFT))

#define PRV_LTR_SNOOP_32US(value)       (PRV_LTR_SNOOP_REQUIREMENT | \
                                         PRV_LTR_SNOOP_SCALE_32US  | \
                                         (((value) & PRV_LTR_SNOOP_VAL_MASK) << PRV_LTR_SNOOP_VAL_SHIFT))

#define PRV_LTR_NON_SNOOP(value)        (value > PRV_LTR_NON_SNOOP_VAL_MASK ? \
                                        PRV_LTR_NON_SNOOP_1US(value) :        \
                                        PRV_LTR_NON_SNOOP_32US(value/32))

#define PRV_LTR_SNOOP(value)            (value > PRV_LTR_SNOOP_VAL_MASK ? \
                                        PRV_LTR_SNOOP_1US(value) :        \
                                        PRV_LTR_SNOOP_32US(value/32))

#define PRV_TX_ACK_CNT_VALUE_MASK       0xFFFF
#define PRV_TX_ACK_CNT_OVERFLOW         (1 << 16)
#define PRV_TX_ACK_CNT_CLEAR            (1 << 19)

#define PRV_TX_COMPL_INTR_STAT_INT      (1 << 0)
#define PRV_TX_COMPL_INTR_STAT_MASK     (1 << 1)
#define PRV_TX_COMPL_INTR_CLR_CLEAR     (1 << 0)
#define PRV_TX_COMPL_INTR_MASKED        (1 << 1)

/*------------------------------------------------------------------------------
// FIFO and Clock related register bits
------------------------------------------------------------------------------*/
#define I2C_RX_FIFO_SIZE                256
#define I2C_TX_FIFO_SIZE                256
#define LPSS_FIFO_SIZE                  0xFF

#define I2C_MAX_TRANSFER_LENGTH         (64 * 1024 - 1)         // max bytes per transfer

//
// I2C Controller byte transfer time
// 
#define I2C_BYTE_TIME_100KHz            (10 * 1000 / 100)       // byte time of 100us
#define I2C_BYTE_TIME_400KHz            (10 * 1000 / 400)       // byte time of 25us
#define I2C_BYTE_TIME_1000KHz           (10 * 1000 / 1000)      // byte time of 10us
#define I2C_BYTE_TIME_3400KHz           (10 * 1000 / 3400)      // byte time of 3us
#define I2C_BYTE_TIME_1700KHZ           (10 * 1000 / 1700)      // byte time of 6us

// LTR value calculation - worst scenario - 1 byte
#define LTR_VALUE_100KHz                (ULONG)(((I2C_TX_FIFO_SIZE/2) * 8 * 1000) / 100)        // 1280us
#define LTR_VALUE_400KHz                (ULONG)(((I2C_TX_FIFO_SIZE/2) * 8 * 1000) / 400)        // 320us
#define LTR_VALUE_1000KHz               (ULONG)(((I2C_TX_FIFO_SIZE/2) * 8 * 1000) / 1000)       // 128us
#define LTR_VALUE_3400KHz               (ULONG)(((I2C_TX_FIFO_SIZE/2) * 8 * 1000) / 3400)       // 37us

#define I2C_SS_SCL_HCNT_16MHZ           0x48                    // Standard speed HCNT Value
#define I2C_SS_SCL_LCNT_16MHZ           0x4F                    // Standard speed LCNT Value
#define I2C_FS_SCL_HCNT_16MHZ           0x08                    // Full speed HCNT Value
#define I2C_FS_SCL_LCNT_16MHZ           0x17                    // Full speed LCNT Value
#define I2C_HS_SCL_HCNT_16MHZ           0x03                    // High speed HCNT Value
#define I2C_HS_SCL_LCNT_16MHZ           0x06                    // High speed LCNT Value

#define I2C_SS_SCL_HCNT_20MHZ           0x5C                    // Standard speed HCNT Value
#define I2C_SS_SCL_LCNT_20MHZ           0x63                    // Standard speed LCNT Value
#define I2C_FS_SCL_HCNT_20MHZ           0x0B                    // Full speed HCNT Value
#define I2C_FS_SCL_LCNT_20MHZ           0x1E                    // Full speed LCNT Value
#define I2C_HS_SCL_HCNT_20MHZ           0x04                    // High speed HCNT Value
#define I2C_HS_SCL_LCNT_20MHZ           0x0C                    // High speed LCNT Value

#define I2C_SS_SCL_HCNT_50MHZ           0xDB                    // Standard speed HCNT Value
#define I2C_SS_SCL_LCNT_50MHZ           0xFE                    // Standard speed LCNT Value
#define I2C_FS_SCL_HCNT_50MHZ           0x20                    // Full speed HCNT Value
#define I2C_FS_SCL_LCNT_50MHZ           0x42                    // Full speed LCNT Value
#define I2C_FP_SCL_HCNT_50MHZ           0x0C                    // Full speed HCNT Value for FastMode+
#define I2C_FP_SCL_LCNT_50MHZ           0x16                    // Full speed LCNT Value for FastMode+
#define I2C_HS_SCL_HCNT_50MHZ           0x0C                    // High speed HCNT Value
#define I2C_HS_SCL_LCNT_50MHZ           0x16                    // High speed LCNT Value

#define I2C_SS_SDA_HOLD_50MHZ           0x9
#define I2C_FS_SDA_HOLD_50MHZ           0x9
#define I2C_FP_SDA_HOLD_50MHZ           0x5
#define I2C_HS_SDA_HOLD_50MHZ           0x1

#define I2C_SS_SCL_HCNT_100MHZ          0x200                   // Standard speed HCNT Value
#define I2C_SS_SCL_LCNT_100MHZ          0x200                   // Standard speed LCNT Value
#define I2C_FS_SCL_HCNT_100MHZ          0x55                    // Full speed HCNT Value
#define I2C_FS_SCL_LCNT_100MHZ          0x96                    // Full speed LCNT Value
#define I2C_FP_SCL_HCNT_100MHZ          0x18                    // Full speed HCNT Value for FastMode+
#define I2C_FP_SCL_LCNT_100MHZ          0x40                    // Full speed LCNT Value for FastMode+
#define I2C_HS_SCL_HCNT_100MHZ          0x06                    // High speed HCNT Value
#define I2C_HS_SCL_LCNT_100MHZ          0x20                    // High speed LCNT Value
#define I2C_FS_FOR_HS_SCL_HCNT_100MHZ   0x55                    // FS HCNT value for HS mode
#define I2C_FS_FOR_HS_SCL_LCNT_100MHZ   0x96                    // FS LCNT value for HS mode
#define I2C_HS_1700K_SCL_HCNT_100MHZ    0x0a                    //1.7M HCNT value for HS mode
#define I2C_HS_1700K_SCL_LCNT_100MHZ     0x25                   //1.7M LCNT value for HS mode

#define I2C_SS_SDA_HOLD_100MHZ          0x06
#define I2C_FS_SDA_HOLD_100MHZ          0x06
#define I2C_FP_SDA_HOLD_100MHZ          0x06
#define I2C_HS_SDA_HOLD_100MHZ          0x06

#define I2C_SS_SDA_SETUP_100MHZ         0x06
#define I2C_FS_SDA_SETUP_100MHZ         0x06
#define I2C_FP_SDA_SETUP_100MHZ         0x06
#define I2C_HS_SDA_SETUP_100MHZ         0x06

#define I2C_SS_SCL_HCNT_133MHZ          0x28F                   // Standard speed HCNT Value
#define I2C_SS_SCL_LCNT_133MHZ          0x2BA                   // Standard speed LCNT Value
#define I2C_FS_SCL_HCNT_133MHZ          0x71                    // Full speed HCNT Value
#define I2C_FS_SCL_LCNT_133MHZ          0xCE                    // Full speed LCNT Value
#define I2C_FP_SCL_HCNT_133MHZ          0x24                    // Full speed HCNT Value for FastMode+
#define I2C_FP_SCL_LCNT_133MHZ          0x53                    // Full speed LCNT Value for FastMode+
#define I2C_HS_SCL_HCNT_133MHZ          0x06                    // High speed HCNT Value
#define I2C_HS_SCL_LCNT_133MHZ          0x30                    // High speed LCNT Value
#define I2C_FS_FOR_HS_SCL_HCNT_133MHZ   0x71                    // FS HCNT value for HS mode
#define I2C_FS_FOR_HS_SCL_LCNT_133MHZ   0xCE                    // FS LCNT value for HS mode

#define I2C_SS_SDA_HOLD_133MHZ          0x06
#define I2C_FS_SDA_HOLD_133MHZ          0x06
#define I2C_FP_SDA_HOLD_133MHZ          0x06
#define I2C_HS_SDA_HOLD_133MHZ          0x06

#define I2C_SS_SDA_SETUP_133MHZ         0x06
#define I2C_FS_SDA_SETUP_133MHZ         0x06
#define I2C_FP_SDA_SETUP_133MHZ         0x06
#define I2C_HS_SDA_SETUP_133MHZ         0x06


/*I2C driver Register function*/
#define I2C_REGISTER_WRITE(_REG_, _VALUE_)\
    WRITE_REGISTER_ULONG((volatile ULONG*)&(_REG_), (_VALUE_))

#define I2C_REGISTER_READ(_REG_) \
    READ_REGISTER_ULONG((volatile ULONG*)&(_REG_))

#define I2C_REGISTER_AND_SET_OP(_REG_, _VALUE_) \
    I2C_REGISTER_WRITE( (_REG_), (I2C_REGISTER_READ((_REG_)) & (_VALUE_)) )

#define I2C_REGISTER_OR_SET_OP(_REG_, _VALUE_) \
    I2C_REGISTER_WRITE( (_REG_), (I2C_REGISTER_READ((_REG_)) | (_VALUE_)) )




/*I2C Device Driver's Context Structure*/
typedef struct I2C_DEVICE_CONTEXT
{
	DWORD                   dwDeviceId;  
	DWORD                   dwMemBaseAddr;    //Memory base address
	DWORD                   dwMemLen;         //Size of memory base address
	DWORD                   dwIrq;       /* I2C irq */
    DWORD                   dwSysIntr;   /* system interrupt */
	ULONG					InterruptMask; /* Mask for Interrupt*/
	BOOL                    InterruptEnabled;
    HANDLE                  hInterruptEvent;  /* event mapped to system interrupt */
    HANDLE                  hInterruptThread;   /* system thread */
    HANDLE					hReadWriteEvent;  /*event mapped to read write operation*/
	HANDLE					hIsrHandler;
	HANDLE					hBusAccess;
	PI2C_REGISTERS          pRegisters;
	I2C_BUS_SETTINGS        BusSetting;
	PUCHAR					BufferData;
	DWORD                   BufLength;
	HKEY					hKey;

	// Bytes read/written in the current FIFO threshold.
	DWORD						 RxRequestLength;
	DWORD                        AddressMode;
	DWORD                        Address;
    DWORD                        ConnectionSpeed;
	DWORD                        Direction;
	DWORD						 ISTLoopTrack;			//Track the looping in SPI_IST
	BOOL                         bIoComplete;
	DWORD					     RxTransferredLength;	/* Actual amount of data that being read from slave device*/
	DWORD					     TxTransferredLength;	/* Actual amount of data that being transfer to slave device*/
	DWORD						 status; //status navi

} I2C_DEVICE_CTXT, *PDEVICE_CONTEXT;


/*I2C driver init function*/
DWORD I2C_Init( LPCTSTR pContext, LPCVOID lpvBusContext);

/*I2C driver open*/
DWORD I2C_Open(DWORD pDevice, DWORD AccessCode, DWORD ShareMode);

/*I2C driver open*/
static DWORD I2C_IST(PDEVICE_CONTEXT pDevice);

/*I2C driver IOControl function*/
BOOL I2C_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut, 
                   DWORD dwLenOut,PDWORD pdwActualOut);

/*I2C close function*/
BOOL I2C_Close(DWORD hOpenContext);

/*I2C driver deinit function*/
BOOL I2C_Deinit(DWORD hDeviceContext);

//******************************************************************************
// Function for Read/Write Operation
//******************************************************************************
BOOL PrepareForTransfer(PDEVICE_CONTEXT pDevice,PI2C_TRANS_BUFFER pTransmissionBuf);

VOID TransferDataWrite(PDEVICE_CONTEXT pDevice);

VOID PrepareForRead(PDEVICE_CONTEXT pDevice);

VOID TransferDataRead(PDEVICE_CONTEXT pDevice);

//******************************************************************************
// Function to enable and disable I2C controller
//******************************************************************************
BOOL i2cEnableController(PDEVICE_CONTEXT pDevice);

BOOL i2cDisableController(PDEVICE_CONTEXT pDevice);

//******************************************************************************
// Function related to interrupt
//******************************************************************************

ULONG i2cAcknowledgeInterrupts(PDEVICE_CONTEXT pDevice, ULONG InterruptMask);

VOID i2cEnableInterrupts(PDEVICE_CONTEXT pDevice,ULONG InterruptMask);

VOID i2cDisableInterrupts(PDEVICE_CONTEXT pDevice);



#ifdef __cplusplus
}
#endif 
#endif // __I2C_H__