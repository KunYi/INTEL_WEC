#ifndef _REGCOMMONDEF_
#define _REGCOMMONDEF_



/* define a enum to contain offset and mask for all fields in register */
#define REGISTER_ADD_REG_ENUM(_RegPrefix,_RegName,_RegEndBit,_RegStartBit)\
    _RegPrefix##_##_RegName##_OFFSET = _RegStartBit,\
    _RegPrefix##_##_RegName##_MASK =  (1UL<<(_RegEndBit - _RegStartBit)) | ((1UL<<(_RegEndBit - _RegStartBit)) -1 ),

#define DEF_REG_ENUM_TYPE(_RegName)\
    typedef enum _##_RegName##_REG_ENUM{\
    REGISTER_ALL_ITEMS\
}_RegName##_REG_ENUM;
#define DEC_REG_ENUM_TYPE(_RegName) _RegName##_REG_ENUM

/* define a struct to present all fields in register */
#define REGISTER_ADD_REG_FIELD(_RegPrefix,_RegName,_RegEndBit,_RegStartBit)\
    UINT32 _RegPrefix##_##_RegName;

#define DEF_REG_FIELD_TYPE(_RegName)\
    typedef struct _##_RegName##_REG_FIELD{\
    REGISTER_ALL_ITEMS\
}_RegName##_REG_FIELD;
#define DEC_REG_FIELD_TYPE(_RegName) _RegName##_REG_FIELD

/* define a function to format a register through struct */
#define REGISTER_FORMAT_REG_BY_STRUCT(_RegPrefix,_RegName,_RegEndBit,_RegStartBit)\
     reg |= ((UINT64)(pRegField->_RegPrefix##_##_RegName & _RegPrefix##_##_RegName##_MASK)) << _RegPrefix##_##_RegName##_OFFSET;

#define DEF_FORMAT_REG_BY_STRUCT_FUNC(_RegName)\
    static __forceinline UINT64 _RegName##FormatRegByStruct(const DEC_REG_FIELD_TYPE(_RegName) * pRegField)\
{\
    UINT64 reg = 0;\
    REGISTER_ALL_ITEMS\
    return reg;\
}

#define FORMAT_REG_BY_STRUCT(_RegName,_pRegField)\
    _RegName##FormatRegByStruct(_pRegField)

/* define a function to format a struct through a register*/
#define REGISTER_FORMAT_STRUCT_BY_REG(_RegPrefix,_RegName,_RegEndBit,_RegStartBit)\
    pRegField->_RegPrefix##_##_RegName = (reg >> _RegPrefix##_##_RegName##_OFFSET) & _RegPrefix##_##_RegName##_MASK;

#define DEF_FORMAT_STRUCT_BY_REG_FUNC(_RegName)\
    static __forceinline void _RegName##FormatStructByReg(UINT64 reg, DEC_REG_FIELD_TYPE(_RegName) * pRegField)\
{\
    REGISTER_ALL_ITEMS\
}

#define FORMAT_STRUCT_BY_REG(_RegName,_RegVal,_pRegField)\
    _RegName##FormatStructByReg(_RegVal,_pRegField)

#endif