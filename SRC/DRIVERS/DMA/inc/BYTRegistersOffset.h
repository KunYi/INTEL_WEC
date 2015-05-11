#ifndef _BYTREGISTERSOFFSET_H_
#define _BYTREGISTERSOFFSET_H_


#define OFFSET_SARX_REGISTER      0x00
#define OFFSET_DARX_REGISTER      0x08
#define OFFSET_LLPX_REGISTER      0x10
#define OFFSET_CTLX_REGISTER      0x18
#define OFFSET_SSTATX_REGISTER    0x20
#define OFFSET_DSTATX_REGISTER    0x28
#define OFFSET_SSTATARX_REGISTER  0x30
#define OFFSET_DSTATARX_REGISTER  0x38
#define OFFSET_CFGX_REGISTER      0x40
#define OFFSET_SGRX_REGISTER      0x48
#define OFFSET_DSRX_REGISTER      0x50

#define OFFSET_STATUS_TFR_REGISTER  (0x2E8)
#define OFFSET_STATUS_BLK_REGISTER  (0x2F0)
#define OFFSET_STATUS_STR_REGISTER  (0x2F8)
#define OFFSET_STATUS_DTR_REGISTER  (0x300)
#define OFFSET_STATUS_ERR_REGISTER  (0x308)

#define OFFSET_MASK_TFR_REGISTER    (0x310)
#define OFFSET_MASK_BLK_REGISTER    (0x318)
#define OFFSET_MASK_STR_REGISTER    (0x320)
#define OFFSET_MASK_DTR_REGISTER    (0x328)
#define OFFSET_MASK_ERR_REGISTER    (0x330)

#define OFFSET_CLEAR_TFR_REGISTER   (0x338)
#define OFFSET_CLEAR_BLK_REGISTER   (0x340)
#define OFFSET_CLEAR_STR_REGISTER   (0x348)
#define OFFSET_CLEAR_DTR_REGISTER   (0x350)
#define OFFSET_CLEAR_ERR_REGISTER   (0x358)

#define OFFSET_STATUS_INT_REGISTER  (0x360)

#define OFFSET_DMACFG_REGISTER      (0x398)
#define OFFSET_CHENREG_REGISTER     (0x3A0)

#endif