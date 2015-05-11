#ifndef _CTLX_H_
#define _CTLX_H_

#include "RegCommonDef.h"

#define REGISTER_ALL_ITEMS\
/* 0 INT_EN */              REGISTER_ADD_ITEM(CTLX,INT_EN, 0, 0)\
/* 3:1 DST_TR_WIDTH */      REGISTER_ADD_ITEM(CTLX,DST_TR_WIDTH, 3, 1)\
/* 6:4 SRC_TR_WIDTH */      REGISTER_ADD_ITEM(CTLX,SRC_TR_WIDTH, 6, 4)\
/* 8:7 DINC */              REGISTER_ADD_ITEM(CTLX,DINC, 8, 7)\
/* 10:9 SINC */             REGISTER_ADD_ITEM(CTLX,SINC, 10, 9)\
/* 13:11 DEST_MSIZE */      REGISTER_ADD_ITEM(CTLX,DEST_MSIZE, 13, 11)\
/* 16:14 SRC_MSIZE */       REGISTER_ADD_ITEM(CTLX,SRC_MSIZE, 16, 14)\
/* 17 SRC_GATHER_EN */      REGISTER_ADD_ITEM(CTLX,SRC_GATHER_EN, 17, 17)\
/* 18 DST_SCATTER_EN */     REGISTER_ADD_ITEM(CTLX,DST_SCATTER_EN, 18, 18)\
/* 19 Undefined */                                          \
/* 22:20 TT_FC */           REGISTER_ADD_ITEM(CTLX,TT_FC, 22, 20)\
/* 24:23 DMS */             REGISTER_ADD_ITEM(CTLX,DMS, 24, 23)\
/* 26:25 SMS */             REGISTER_ADD_ITEM(CTLX,SMS, 26, 25)\
/* 27 LLP_DST_EN */         REGISTER_ADD_ITEM(CTLX,LLP_DST_EN, 27, 27)\
/* 28 LLP_SRC_EN */         REGISTER_ADD_ITEM(CTLX,LLP_SRC_EN, 28, 28)\
/* 31:29 Undefined */                                             \
/* 43:32 BLOCK_TS */        REGISTER_ADD_ITEM(CTLX,BLOCK_TS, 43, 32)\
/* 44 DONE */               REGISTER_ADD_ITEM(CTLX,DONE, 44, 44)\
/* 63:45 Undefined */


#define REGISTER_ADD_ITEM REGISTER_ADD_REG_ENUM
DEF_REG_ENUM_TYPE(CTLX)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_ADD_REG_FIELD
DEF_REG_FIELD_TYPE(CTLX)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_FORMAT_REG_BY_STRUCT
DEF_FORMAT_REG_BY_STRUCT_FUNC(CTLX)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_FORMAT_STRUCT_BY_REG
DEF_FORMAT_STRUCT_BY_REG_FUNC(CTLX)
#undef REGISTER_ADD_ITEM

#undef REGISTER_ALL_ITEMS


//refer to CLTx.SINC and CLTx.DINC
typedef enum _CLT_SINC_DINC_ENUM
{
    CLT_SINC_DINC_Increment = 0,
    CLT_SINC_DINC_Decrement = 1,
    CLT_SINC_DINC_NoChange = 2,
}CLT_SINC_DINC_ENUM;

//refer to Table 6-4 CTLx.TT_FC Field Decoding
typedef enum _CLT_TT_FC_ENUM
{
    CLT_TT_FC_Mem2Mem_dmac = 0, //Memory to Memory DW_ahb_dmac
    CLT_TT_FC_Mem2Per_dmac = 1, //Memory to Peripheral DW_ahb_dmac
    CLT_TT_FC_Per2Mem_dmac = 2, //Peripheral to Memory DW_ahb_dmac
    CLT_TT_FC_Per2Per_dmac = 3, //Peripheral to Peripheral DW_ahb_dmac

    CLT_TT_FC_Per2Mem_Per = 4, //Peripheral to Memory Peripheral
    CLT_TT_FC_Per2Per_SPer = 5, //Peripheral to Peripheral Source Peripheral
    CLT_TT_FC_Mem2Per_Per = 6, //Memory to Peripheral Peripheral
    CLT_TT_FC_Per2Per_DPer = 7, //Peripheral to Peripheral Destination Peripheral

}CLT_TT_FC_ENUM;

//Table 6-2 CTLx.SRC_MSIZE and DEST_MSIZE Decoding
static const ULONG CLTX_MSIZE_MAP[] = {1,4,8,16,32,64,128,256};
//Table 6-3 CTLx.SRC_TR_WIDTH and CTLx.DST_TR_WIDTH Decoding
static const ULONG CLTX_TR_WIDTH_MAP[] = {8,16,32,64,128,256,256,256};


#endif