#ifndef _DMACFGREG_H_
#define _DMACFGREG_H_
#include "RegCommonDef.h"

#define REGISTER_ALL_ITEMS \
/* 0 DMA_EN */          REGISTER_ADD_ITEM(DMACFGREG,DMA_EN, 0, 0)\
/* 63:1 Undefined */

#define REGISTER_ADD_ITEM REGISTER_ADD_REG_ENUM
DEF_REG_ENUM_TYPE(DMACFGREG)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_ADD_REG_FIELD
DEF_REG_FIELD_TYPE(DMACFGREG)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_FORMAT_REG_BY_STRUCT
DEF_FORMAT_REG_BY_STRUCT_FUNC(DMACFGREG)
#undef REGISTER_ADD_ITEM

#define REGISTER_ADD_ITEM REGISTER_FORMAT_STRUCT_BY_REG
DEF_FORMAT_STRUCT_BY_REG_FUNC(DMACFGREG)
#undef REGISTER_ADD_ITEM

#undef REGISTER_ALL_ITEMS

typedef enum _DMACFGREG_DMA_EN_ENUM{
    DMACFGREG_DMA_EN_Disabled = 0,
    DMACFGREG_DMA_EN_Enabled = 1,
}DMACFGREG_DMA_EN_ENUM;

#endif 