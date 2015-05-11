//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2013 Intel Corporation All Rights Reserved. 
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

//----------------------------------------------------------------------------------
// File: hal.h - This contains the constants and macros used for accessing the
//               XHCI Host Controller registers.
//----------------------------------------------------------------------------------

#ifndef HAL_H
#define HAL_H

#define MAX_SLOTS_NUMBER            (256)

// 
// Offsets of the Host Controller capability registers
// 
// Offset of the CAPLENGTH register
#define USB_XHCD_CAPLENGTH      (0x00)
// Offset of the HCIVERSION register
#define USB_XHCD_HCIVERSION     (0x02)
// Offset of the HCSPARAMS1 register
#define USB_XHCD_HCSPARAMS      (0x04)
// Offset of the HCSPARAMS2 register
#define USB_XHCD_HCSPARAMS2     (0x08)
// Offset of the HCSPARAMS3 register
#define USB_XHCD_HCSPARAMS3     (0x0C)
// Offset of the HCCPARAMS register
#define USB_XHCD_HCCPARAMS      (0x10)
// Offset of the HCSP_PORTROUTE register
#define USB_XHCD_DBOFF          (0x14)
// Offset of the HCSP_PORTROUTE register
#define USB_XHCD_RTSOFF         (0x18)

// 
// Offsets of the Host Controller operational registers
// 
// Offset of the USBCMD register
#define USB_XHCD_USBCMD         (0x00)
// Offset of the USBSTS register
#define USB_XHCD_USBSTS         (0x04)
#define USB_XHCD_PAGESIZE       (0x08)
#define USB_XHCD_DNCTLR         (0x14)
#define USB_XHCD_CRCR           (0x18)
#define USB_XHCD_DCBAAP         (0x30)
#define USB_XHCD_CONFIG         (0x38)
#define USB_XHCD_PORT_BASE      (0x400)
#define USB_XHCD_PORTPM_BASE    (0x404)

// 
// Offsets of the Host Controller runtime registers
// 
#define USB_XHCD_MFINDEX        (0x00)
#define USB_XHCD_INTER_SET      (0x20)
#define USB_XHCD_IMAN           (0x00)
#define USB_XHCD_IMOD           (0x04)
#define USB_XHCD_ERSTSZ         (0x08)
#define USB_XHCD_ERSTBA         (0x10)
#define USB_XHCD_ERDP           (0x18)

#define MAX_HALT  (16*125)
#define MAX_RESET (32*125)

#define HOST_CONTROLLER_EXTENDED_CAPABILITIES(p)   (((p)>>16)&0xffff)

#define HOST_CONTROLLER_LENGTH(p)   (((p)>>00)&0x00ff)
#define HOST_CONTROLLER_VERSION(p)  (((p) >> 16) & 0xffff)

#define EXTENDED_CAPABILITIES_ID(p)     (((p)>>0)&0xff)
#define EXTENDED_CAPABILITIES_NEXT(p)   (((p)>>8)&0xff)

#define EXTENDED_CAPABILITIES_PROTOCOL  (2)

#define COMMAND_EIE        (1 << 2)

#define INTERRUPT_REQUESTS  ((1 << 2) | (1 << 3) | (1 << 10))

#define STATUS_CNR  (1 << 11)

#define MAX_SLOTS(p)             (((p) >> 0) & 0xff)
#define MAX_PORTS(p)             (((p) >> 24) & 0x7f)
#define MAX_SCRATCHPAD_NUMBER(p) (((p) >> 27) & 0x1f)
#define SLOT_MASK                0xff

#define HOST_CONTROLLER_64_CONTEXT(p)   ((p) &(1 << 2))

#define DB_OFF_MASK (~0x3)
#define RTS_OFF_MASK    (~0x1f)

#define PORT_REGISTER_NUMBER   (4)
#define MAX_PORT_NUMBER (32)

#define COMMAND_RUN                 (1 << 0)
#define COMMAND_RESET               (1 << 1)
#define COMMAND_RING_RESERVED_BITS  (0x3f)

#define STATUS_HALT        (1 << 0)
#define STATUS_FATAL       (1 << 2)
#define STATUS_INTERRUPT   (1 << 3)

#define STATUS_CONNECT    (1 << 0)
#define STATUS_PE         (1 << 1)
#define STATUS_OC         (1 << 3)
#define STATUS_RESET      (1 << 4)

#define STATE_OFFSET    (5)
#define STATE_MASK      (0xf << 5)
#define STATE_POWER     (1 << 9)
#define DEVICE_U0       (0x0 << 5)
#define DEVICE_U3       (0x3 << 5)
#define DEVICE_RXDETECT (0x5 << 5)
#define DEVICE_POLLING  (0x7 << 5)
#define DEVICE_RESUME   (0xf << 5)

#define DEVICE_SPEED(p)         (((p) & (0xf << 10)) >> 10)
#define DEVICE_SUPERSPEED(p)    (((p) & (0xf << 10)) == (0x4 << 10))

#define PORT_CONNECTION_STATUS  (1 << 0)

#define LINK_STATE_STROBE           (1 << 16)
#define CONNECTION_STATUS_CHANGE    (1 << 17)
#define PORT_ENABLE_CHANGE          (1 << 18)
#define WARM_RESET_CHANGE           (1 << 19)
#define OVER_CURRENT_CHANGE         (1 << 20)
#define RESET_CHANGE                (1 << 21)
#define RESET_CLEAR                 (1 << 22)

#define EXTENDED_PORT_MAJOR(x)   (((x) >> 24) & 0xff)
#define EXTENDED_PORT_OFF(x)     ((x) & 0xff)
#define EXTENDED_PORT_COUNT(x)   (((x) >> 8) & 0xff)

#define INTERRUPT_PENDING(p)    ((p) & 0x1)
#define INTERRUPT_ENABLE(p)     (((p) & 0xfffffffe) | 0x2)
#define INTERRUPT_MASK          (0xffff)

#define MASK_SIZE       (0xffff << 16)
#define MASK_POINTER    (0xf)

#define HIGH_BUSY_BYTE   (1 << 3)

#define DB_TARGET_MASK       (0xFFFFFF00)
#define DB_STREAM_ID_MASK    (0x0000FFFF)
#define DB_TARGET_HOST       (0x0)
#define DB_STREAM_ID_HOST    (0x0)
#define DB_MASK              (0xff << 8)

#define EPI_TO_DB(p)    (((p) + 1) & 0xff)

#define TRANSFER_BLOCK_TO_ID(p) (((p) >> 16) & 0x1f)

#define COMPLETION_CODE_OFFSET  (24)
#define COMPLETION_STATUS(p)    (((p) & ((UINT)0xff << 24)) >> 24)

#define STATUS_DB_SUCCESS           (1)
#define STATUS_DB_ERROR             (2)
#define STATUS_BABBLE               (3)
#define STATUS_TRANSFER_ERROR       (4)
#define STATUS_TRANSFER_BLOCK_ERROR (5)
#define STATUS_STALL                (6)
#define STATUS_NERES                (7)
#define STATUS_BANDWITH_ERROR       (8)
#define STATUS_BAD_SPLIT            (11)
#define STATUS_BAD_ENDPOINT         (12)
#define STATUS_SHORT_TRANSFER       (13)
#define STATUS_UNDERRUN             (14)
#define STATUS_INVALID              (17)
#define STATUS_BANDWITH_OVERRUN     (18)
#define STATUS_CONTEXT_STATE        (19)
#define STATUS_STOP                 (26)
#define STATUS_STOP_INVALID         (27)
#define STATUS_SPLIT_ERROR          (36)

#define TRANSFER_BLOCK_TO_SLOT(p)       (((p) &((UINT)0xff<<24)) >> 24)
#define SLOT_TO_TRANSFER_BLOCK(p)       (((p) &(UINT)0xff) << 24)
#define TRANSFER_BLOCK_TO_ENDPOINT(p)   ((((p) &(0x1f << 16)) >> 16) - 1)
#define ENDPOINT_TO_TRANSFER_BLOCK(p)   ((((p) + 1) & 0x1f) << 16)

#define ENDPOINT_ID_TO_INDEX(p)   ((p) - 1)

#define TRANSFER_BLOCK_LENGTH(p)    ((p) & 0x1ffff)
#define TRANSFER_BLOCK_TARGET(p)    (((p) & 0x3ff) << 22)
#define TRANSFER_BLOCK_CYCLE        (1<<0)
#define TRANSFER_BLOCK_ISP          (1<<2)
#define TRANSFER_BLOCK_CHAIN        (1<<4)
#define TRANSFER_BLOCK_IOC          (1<<5)
#define TRANSFER_BLOCK_IDT          (1<<6)
#define TRANSFER_BLOCK_DIRECTION_IN (1<<16)

#define ENDPOINT_STATE_MASK       (0xf)
#define ENDPOINT_STATE_DISABLED   (0)
#define ENDPOINT_STATE_RUNNING    (1)
#define ENDPOINT_STATE_HALTED     (2)
#define ENDPOINT_STATE_STOPPED    (3)
#define ENDPOINT_STATE_ERROR      (4)

#define ENDPOINT_INTERVAL_TO_FRAMES(p)  (1 <<(p))

#define ENDPOINT_ISOC_OUT       (1)
#define ENDPOINT_BULK_OUT       (2)
#define ENDPOINT_INT_OUT        (3)
#define ENDPOINT_CTRL           (4)
#define ENDPOINT_ISOC_IN        (5)
#define ENDPOINT_BULK_IN        (6)
#define ENDPOINT_INT_IN         (7)

#define MAX_BULK_ERRORS         (1)
#define MAX_ISOC_ERRORS         (3)

#define CTX_SIZE_64 64
#define CTX_SIZE_32 32

#define TRANSFER_BLOCK_TYPE_BITMASK     (0xfc00)
#define TRANSFER_BLOCK_TYPE(p)          ((p) << 10)
#define TRANSFER_BLOCK_NORMAL           (1)
#define TRANSFER_BLOCK_SETUP            (2)
#define TRANSFER_BLOCK_DATA             (3)
#define TRANSFER_BLOCK_STATUS           (4)
#define TRANSFER_BLOCK_LINK             (6)
#define TRANSFER_BLOCK_ENABLE_SLOT      (9)
#define TRANSFER_BLOCK_DISABLE_SLOT     (10)
#define TRANSFER_BLOCK_ADDRESS          (11)
#define TRANSFER_BLOCK_ENDPOINT_CFG     (12)
#define TRANSFER_BLOCK_RESET_ENDPOINT   (14)
#define TRANSFER_BLOCK_SET_DEQ          (16)
#define TRANSFER_BLOCK_RESET_DEVICE     (17)
#define TRANSFER_BLOCK_NOOP             (23)
#define TRANSFER_BLOCK_TRANSFER         (32)
#define TRANSFER_BLOCK_COMPLETE         (33)
#define TRANSFER_BLOCK_PORT_STATUS      (34)

#define BLOCKS_IN_SEGMENT    (256)
#define SEGMENT_SIZE        (BLOCKS_IN_SEGMENT * 16)
#define TRB_MAX_BUFF_SIZE   (1 << 16)

#define LAST_CONTEXT_MASK   ((UINT)0x1f << 27)
#define LAST_CONTEXT(p)     ((p) << 27)
#define SLOT                (1 << 0)
#define ENDPOINT_ZERO       (1 << 1)

#define ROOT_HUB_PORT(p)    (((p) & 0xff) << 16)

#define ERST_NUM_SEGS       (1)

#define DISABLED_SLOT       (0)

#define LEAST_32(n)((UINT)(n))
#define MOST_32(n)((UINT)(n >> 32))

#define USB_XHCD_MFINDEX_FRAME_INDEX_MASK 0x00001FFF

#define USB_CTRL_SET_TIMEOUT        (5000)

#define USBXHCI_TIMEOUT OS_CONVERT_MILLISECONDS_TO_WAIT_VALUE(3000)
#define USB_DIR_IN (0x80)

#define USB_ENDPOINT_NUMBER_MASK    (0x0f)
#define USB_ENDPOINT_DIR_MASK       (0x80)

#define PORT_READ_ONLY  ((1<<0) |(1<<3) |(0xf<<10) |(1<<30))
#define PORT_READ_WRITE ((0xf<<5) |(1<<9) |(0x3<<14) |(0x7<<25))

#define USB_XHCD_PORTPM_TEST_OFFSET     (28)
#define USB_XHCD_PORTPM_TEST_MASK       (0xF << USB_XHCD_PORTPM_TEST_OFFSET)

#define LINK_HEAD   (0)
#define LINK_TAIL   (1)

#define MAX_EP_NUMBER (31)
#define MAX_STR_SIZE (256)

#define HOST_CONTROLLER_64_CONTEXT_SIZE (2048)
#define HCC_32BYTE_CONTEXT_SIZE (1024)

#define XHCI_PAGE_SHIFT          12
#define MAX_PACKET_SS            512
#define MAX_PACKET_HS            64
#define MAX_PACKET_LS            8

#define REMAINDER_SHIFT          10
#define REMAINDER_BIT_START      17
#define REMAINDER_BIT_END        21
#define REMAINDER_MAX_SHIFT      ((1 << (REMAINDER_BIT_END - REMAINDER_BIT_START + 1)) - 1)

#define MAX_SS_INTERVAL          15

#define PORT_RESET_TIMEOUT       60
#define PORT_RESET_ATTEMPT       10

#define BYTES_IN_DWORD           4
#define BITS_IN_DWORD            32
#define BITS_IN_WORD             16
  
#define MAX_ABORTS_NUMBER        5

#define USB30_MAJOR_REVISION     0x03

#define XHCD_OK              (0)
#define XHCD_FAIL            (1)
#define XHCD_TIMEDOUT        (2)
#define XHCD_NOMEMORY        (3)
#define XHCD_INVALID         (4)
#define XHCD_EP_STATE_ERROR  (5)
#define XHCD_SLOT_DISABLED   (6)
#define XHCD_TRANSFER_ERROR  (7)
#define XHCD_NODEVICE        (8)
#define XHCD_EVENT_ERROR     (9)
#define XHCD_DROP_ZERO_ERROR (10)

#define BAD_DEVICE_STATUS  0xFFFFFFFF

#endif // HAL_H