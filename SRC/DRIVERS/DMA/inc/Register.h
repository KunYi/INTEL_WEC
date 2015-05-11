
#pragma once

#include "BYTAllRegisters.h"

//dmac has no special requirement, but set it to 32bit for effective memory addess.
#define LPSS_DMAC_ALIGNMENT_32BIT FILE_LONG_ALIGNMENT

//now we use single block transfer, so the DMAH_CHx_MAX_BLK_SIZE of h/w determine the max length in single dma transfer
#define LPSS_DMAC_MAX_BLOCK_TS_DMA  DMAH_CHX_MAX_BLK_SIZE
//#define LPSS_DMAC_MAX_BLOCK_TS_DMA  999
#pragma message("LPSS_DMAC_DMA_MAX_FRAGMENTS set to 1 temporarily")
#define LPSS_DMAC_DMA_MAX_FRAGMENTS           10

//for many register, like Source Software Transaction Request Register(1ReqSrcReg)
//use a write enable bit to avoid the read-modified write operation
#define COMMON_WRITE_ENABLE_BIT         ((ULONG)(0x100))
#define COMMON_WRITE_BIT                ((ULONG)(0x1))
#define COMMON_WRITE_ENABLE_MASK        (COMMON_WRITE_ENABLE_BIT|COMMON_WRITE_BIT)
//
// Synopsys DMA Register Set
// DesignWare AHB DMAC Handbook
//

#define SAR_LO_REGISTER      0x00
#define SAR_HI_REGISTER      0x04
#define DAR_LO_REGISTER      0x08
#define DAR_HI_REGISTER      0x0C
#define LLP_LO_REGISTER      0x10
#define LLP_HI_REGISTER      0x14
#define CTL_LO_REGISTER      0x18
#define CTL_HI_REGISTER      0x1C
#define SSTAT_LO_REGISTER    0x20
#define SSTAT_HI_REGISTER    0x24
#define DSTAT_LO_REGISTER    0x28
#define DSTAT_HI_REGISTER    0x2C
#define SSTATAR_LO_REGISTER  0x30
#define SSTATAR_HI_REGISTER  0x34
#define DSTATAR_LO_REGISTER  0x38
#define DSTATAR_HI_REGISTER  0x3C
#define CFG_LO_REGISTER      0x40
#define CFG_HI_REGISTER      0x44
#define SGR_LO_REGISTER      0x48
#define SGR_HI_REGISTER      0x4C
#define DSR_LO_REGISTER      0x50
#define DSR_HI_REGISTER      0x54
//#define STRIDE_CHANNEL       0x58

//dma controller register

#define REQ_SRC_REGISTER        (0x368)
#define REQ_DST_REGISTER        (0x370)
#define SGLREQ_SRC_REGISTER     (0x378)
#define SGLREQ_DST_REGISTER     (0x380)
#define LST_SRC_REGISTER        (0x388)
#define LST_DST_REGISTER        (0x390)


#define DMACFG_REGISTER (0x398)
#define CH_EN_REGISTER  (0x3A0)

//Bit for Channel Enable Register
#define CHENREG_CHEN        ((ULONG)(0x1))
#define CHENREG_CHENWE      ((ULONG)(0x100))

/*
//Bit for Combined Interrupt Status Register
#define STATUSINT_TFR       ((ULONG)(0x1<<0))
#define STATUSINT_BLOCK     ((ULONG)(0x1<<1))
#define STATUSINT_SRCT      ((ULONG)(0x1<<2))
#define STATUSINT_DSTT      ((ULONG)(0x1<<3))
#define STATUSINT_ERR       ((ULONG)(0x1<<4))
#define STATUSINT_MASK      ((ULONG)((0x1<<5)-1))
*/
//Bit for Interrupt Status Registers
#define INT_STATUS          ((ULONG)(0x1))



//Bit for Control Register for Channel
//0 INT_EN
#define CTL_LO_INT_EN_OFFSET                ((ULONG)(0))
#define CTL_LO_INT_EN_BITS                  ((ULONG)(1))
#define CTL_LO_INT_EN_MASK                  ((ULONG)((0x1<<CTL_LO_INT_EN_BITS)-1))
//3:1 DST_TR_WIDTH
#define CTL_LO_DST_TR_WIDTH_OFFSET          ((ULONG)(1))
#define CTL_LO_DST_TR_WIDTH_BITS            ((ULONG)(3))
#define CTL_LO_DST_TR_WIDTH_MASK            ((ULONG)((0x1<<CTL_LO_DST_TR_WIDTH_BITS)-1)))
//6:4 SRC_TR_WIDTH
#define CTL_LO_SRC_TR_WIDTH_OFFSET          ((ULONG)(4))
#define CTL_LO_SRC_TR_WIDTH_BITS            ((ULONG)(3))
#define CTL_LO_SRC_TR_WIDTH_MASK            ((ULONG)((0x1<<CTL_LO_SRC_TR_WIDTH_BITS)-1))
//8:7 DINC
#define CTL_LO_DINC_OFFSET                  ((ULONG)(7))
#define CTL_LO_DINC_BITS                    ((ULONG)(2))
#define CTL_LO_DINC_MASK                    ((ULONG)((0x1<<CTL_LO_DINC_BITS)-1))
//10:9 SINC
#define CTL_LO_SINC_OFFSET                  ((ULONG)(9))
#define CTL_LO_SINC_BITS                    ((ULONG)(2))
#define CTL_LO_SINC_MASK                    ((ULONG)((0x1<<CTL_LO_SINC_BITS)-1))
//13:11 DEST_MSIZE
#define CTL_LO_DEST_MSIZE_OFFSET            ((ULONG)(11))
#define CTL_LO_DEST_MSIZE_BITS              ((ULONG)(3))
#define CTL_LO_DEST_MSIZE_MASK              ((ULONG)((0x1<<CTL_LO_DEST_MSIZE_BITS)-1))
//16:14 SRC_MSIZE
#define CTL_LO_SRC_MSIZE_OFFSET             ((ULONG)(14))
#define CTL_LO_SRC_MSIZE_BITS               ((ULONG)(3))
#define CTL_LO_SRC_MSIZE_MASK               ((ULONG)((0x1<<CTL_LO_SRC_MSIZE_BITS)-1))
//17 SRC_GATHER_EN
#define CTL_LO_SRC_GATHER_EN_OFFSET         ((ULONG)(17))
#define CTL_LO_SRC_GATHER_EN_BITS           ((ULONG)(1))
#define CTL_LO_SRC_GATHER_EN_MASK           ((ULONG)((0x1<<CTL_LO_SRC_GATHER_EN_BITS)-1))
//18 DST_SCATTER_EN
#define CTL_LO_DST_SCATTER_EN_OFFSET        ((ULONG)(18))
#define CTL_LO_DST_SCATTER_EN_BITS          ((ULONG)(1))
#define CTL_LO_DST_SCATTER_EN_MASK          ((ULONG)((0x1<<CTL_LO_DST_SCATTER_EN_BITS)-1))
//19 Undefined
//22:20 TT_FC
#define CTL_LO_TT_FC_OFFSET                 ((ULONG)(20))
#define CTL_LO_TT_FC_BITS                   ((ULONG)(3))
#define CTL_LO_TT_FC_MASK                   ((ULONG)((0x1<<CTL_LO_TT_FC_BITS)-1))
//24:23 DMS
#define CTL_LO_DMS_OFFSET                   ((ULONG)(23))
#define CTL_LO_DMS_BITS                     ((ULONG)(2))
#define CTL_LO_DMS_MASK                     ((ULONG)((0x1<<CTL_LO_DMS_BITS)-1))
//26:25 SMS
#define CTL_LO_SMS_OFFSET                   ((ULONG)(25))
#define CTL_LO_SMS_BITS                     ((ULONG)(2))
#define CTL_LO_SMS_MASK                     ((ULONG)((0x1<<CTL_LO_SMS_BITS)-1))
//27 LLP_DST_EN
#define CTL_LO_LLP_DST_EN_OFFSET            ((ULONG)(27))
#define CTL_LO_LLP_DST_EN_BITS              ((ULONG)(1))
#define CTL_LO_LLP_DST_EN_MASK              ((ULONG)((0x1<<CTL_LO_LLP_DST_EN_BITS)-1))
//28 LLP_SRC_EN
#define CTL_LO_LLP_SRC_EN_OFFSET            ((ULONG)(28))
#define CTL_LO_LLP_SRC_EN_BITS              ((ULONG)(1))
#define CTL_LO_LLP_SRC_EN_MASK              ((ULONG)((0x1<<CTL_LO_LLP_SRC_EN_BITS)-1))
//31:29 Undefined

#define CTL_HI_BLOCK_TS_OFFSET
#define CTL_HI_BLOCK_TS_BITS
#define CTL_HI_BLOCK_TS_EN_MASK

#define CTL_HI_DONE_OFFSET
#define CTL_HI_DONE_BITS
#define CTL_HI_DONE_EN_MASK


//Bit for Configuration Register for Channel
//10 HS_SEL_DST
#define CFG_LO_HS_SEL_DST_OFFSET                ((ULONG)(10))
#define CFG_LO_HS_SEL_DST_BITS                  ((ULONG)(1))
#define CFG_LO_HS_SEL_DST_MASK                  ((ULONG)((0x1<<CFG_LO_HS_SEL_DST_BITS)-1))

//11 HS_SEL_SRC
#define CFG_LO_HS_SEL_SRC_OFFSET                ((ULONG)(11))
#define CFG_LO_HS_SEL_SRC_BITS                  ((ULONG)(1))
#define CFG_LO_HS_SEL_SRC_MASK                  ((ULONG)((0x1<<CFG_LO_HS_SEL_SRC_BITS)-1))


//Bit for Software Handshaking Registers
#define REQ_REG_WRITE_MASK                      COMMON_WRITE_ENABLE_MASK

//Registe helper Macro
#define WRITE_DMA_REGISTER(_PDEVICE_CONTEXT_, _OFFSET_, _VALUE_) \
    WRITE_REGISTER_ULONG((volatile ULONG*)((_PDEVICE_CONTEXT_->DMAC_BASE) + (_OFFSET_)), (_VALUE_))

#define READ_DMA_REGISTER(_PDEVICE_CONTEXT_, _OFFSET_) \
    READ_REGISTER_ULONG((volatile ULONG*)((_PDEVICE_CONTEXT_->DMAC_BASE) + (_OFFSET_)))



//Set bits in UINT32
#define SET_BITS_UINT32(_dVal_, _BITS_, _OFFSET_) (((UINT32)_dVal_) | ((UINT32)_BITS_) << _OFFSET_)
#define GET_BITS_UINT32

