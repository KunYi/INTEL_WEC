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

#ifndef __SPIPUBLIC_H__
#define __SPIPUBLIC_H__

// Windows Header Files:
#include <windows.h>
#include <winioctl.h>
// C RunTime Header Files
#include <stdlib.h>


//******************************************************************************
// IOCTL code definition
//******************************************************************************
#define FILE_DEVICE_SPI_CONTROLLER FILE_DEVICE_CONTROLLER

#define IOCTL_SPI_EXECUTE_WRITE     \
        CTL_CODE(FILE_DEVICE_SPI_CONTROLLER, 0x703, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPI_EXECUTE_READ      \
        CTL_CODE(FILE_DEVICE_SPI_CONTROLLER, 0x704, METHOD_BUFFERED, FILE_ANY_ACCESS)


//status navigation
#define TRANSFER_UNSUCCESSFUL			0x0
#define TRANSFER_SUCCESS 				0x1

/*
* Transfer direction read/write
*/
typedef enum SPI_TRANSFER_DIRECTION
{
    TransferDirectionRead = 1,
    TransferDirectionWrite = 2

}SPI_TRANSFER_DIRECTION;

/*
*  SPI Mode Definitions
*  SPI_MODE | Clock Polarity | Clock Phase
*  0            0              0
*  1            0              1  
*  2            1              0
*  3            1              1
*/

typedef enum _SPI_BUS_MODE
{
    IO_SPI_MODE_0 = 0, 
    IO_SPI_MODE_1 = 1,
    IO_SPI_MODE_2 = 2,
    IO_SPI_MODE_3 = 3,
    IO_SPI_MODE_MAX
}SPI_BUS_MODE;

/*
* SPI transfer speed setting
*/
typedef enum _SPI_BUS_SPEED
{
    SPI_BUS_SPEED_125KHZ    = 1,
    SPI_BUS_SPEED_10MHZ    = 2
}SPI_BUS_SPEED;


/*SPI Buffer Structure*/
typedef struct _SPI_TRANS_BUFFER
{
    DWORD               BusSpeed;
	DWORD				Mode;
    DWORD               Direction;
    DWORD               Length;
	UCHAR               BitsPerEntry;
	DWORD               RemainingItem;
	PUCHAR              pBuffer;
    
}SPI_TRANS_BUFFER, *PSPI_TRANS_BUFFER;




#endif
