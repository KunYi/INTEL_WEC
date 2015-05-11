
#ifndef _BYTALLREGISTERS_H_
#define _BYTALLREGISTERS_H_

#ifndef UINT64
#define UINT64 unsigned __int64
#endif 

/*include RegCommonDef.h before all Reg header file*/

#include "DmaCfgReg.h"
#include "CHENReg.h"
#include "SARx.h"
#include "DARx.h"
#include "LLPx.h"
#include "CTLx.h"
#include "CFGx.h"
#include "StatusInt.h"

#include "BYTRegistersOffset.h"


#define DMAH_CHX_MAX_BLK_SIZE       ((ULONG)(4095))

#define DMAH_NUM_CHANNELS           ((ULONG)(0x8))
#define STRIDE_CHANNEL              ((ULONG)(0x58))

typedef struct _DMAC_REGS_CONTEX{
    DEC_REG_FIELD_TYPE(DMACFGREG) DmaCfgReg;
    DEC_REG_FIELD_TYPE(CHENREG) CHENReg;
    DEC_REG_FIELD_TYPE(SARX) SARx;
    DEC_REG_FIELD_TYPE(DARX) DARx;
    DEC_REG_FIELD_TYPE(CFGX) CFGx;
    DEC_REG_FIELD_TYPE(CTLX) CTLx;
}DMAC_REGS_CONTEX;
#endif