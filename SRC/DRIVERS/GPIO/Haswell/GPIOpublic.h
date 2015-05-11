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


#ifndef GPIOPUBLIC_H
#define GPIOPUBLIC_H

// Windows Header Files:
#include <windows.h>
#include <winioctl.h>
// C RunTime Header Files
#include <stdlib.h>


//******************************************************************************
// IOCTL code definition
//******************************************************************************

//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
#define IOCTL_GPIO_READ \
    CTL_CODE( FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_GPIO_WRITE \
    CTL_CODE( FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED , FILE_ANY_ACCESS  )

#define IOCTL_GPIO_DIRECTION \
    CTL_CODE( FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_GPIO_MUX \
    CTL_CODE( FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_GPIO_QUERY \
    CTL_CODE( FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS  )



typedef enum
{
    CONNECT_MODE_INVALID = 0,
    CONNECT_MODE_INPUT,
    CONNECT_MODE_OUTPUT,
    CONNECT_MODE_MAXIMUM  = CONNECT_MODE_OUTPUT
} GPIO_CONNECT_IO_PINS_MODE;

typedef struct
{
    ULONG pin;
     union
    {
        ULONG data;
        GPIO_CONNECT_IO_PINS_MODE ConnectMode;
    } u;

} GPIO_PIN_PARAMETERS, *PGPIO_PIN_PARAMETERS;


#endif /* GPIOPUBLIC_H */
