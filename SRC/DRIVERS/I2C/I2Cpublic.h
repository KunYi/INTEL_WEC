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

#ifndef __I2CPUBLIC_H__
#define __I2CPUBLIC_H__

// Windows Header Files:
#include <windows.h>
#include <winioctl.h>
// C RunTime Header Files
#include <stdlib.h>


//******************************************************************************
// IOCTL code definition
//******************************************************************************
#define FILE_DEVICE_I2C_CONTROLLER FILE_DEVICE_CONTROLLER

#define IOCTL_I2C_EXECUTE_WRITE     \
        CTL_CODE(FILE_DEVICE_I2C_CONTROLLER, 0x703, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_I2C_EXECUTE_READ      \
        CTL_CODE(FILE_DEVICE_I2C_CONTROLLER, 0x704, METHOD_BUFFERED, FILE_ANY_ACCESS)

//status navigation
#define TRANSFER_UNSUCCESSFUL			0x0
#define TRANSFER_SUCCESS 				0x1
#define TRANSFER_ABORTED				0x2

/*
* Transfer direction read/write
*/
typedef enum I2C_TRANSFER_DIRECTION
{
    TransferDirectionRead = 1,
    TransferDirectionWrite = 2

}I2C_TRANSFER_DIRECTION;

/*
* I2C transfer speed setting
*/
typedef enum _I2C_BUS_SPEED
{
    I2C_BUS_SPEED_100KHZ    = 1,
    I2C_BUS_SPEED_400KHZ    = 2
}I2C_BUS_SPEED;

/*
* I2C address mode setting
*/
typedef enum _I2C_ADDRESS_MODE
{
    AddressMode7Bit     = 1,
    AddressMode10Bit    = 2
} I2C_ADDRESS_MODE;


/*I2C Buffer Structure*/
typedef struct _I2C_TRANS_BUFFER
{
    DWORD               AddressMode;
    DWORD               Address;
    DWORD               BusSpeed;
    DWORD               Direction;
    DWORD               DataLength;
	DWORD               RemainingItem;
	PUCHAR              Data;
    
}I2C_TRANS_BUFFER, *PI2C_TRANS_BUFFER;


#endif
