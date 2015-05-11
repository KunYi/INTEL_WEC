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



/*Module Name:  

GPIO.h

Abstract:  

Holds definitions private to the implementation of the machine independent
driver interface.

Notes: 


--*/

#ifndef __GPIO_H__
#define __GPIO_H__

#include <windows.h>
#include <nkintr.h>

#include <notify.h>
#include <memory.h>
#include <nkintr.h>

#include <pegdser.h>
#include <devload.h>
#include <ddkreg.h>
#include <ceddk.h>
#include <cebuscfg.h>
#include <linklist.h>


#include "GPIOpublic.h"

#include "gpioBaseAddr.h"



#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG


DBGPARAM dpCurSettings = {
    TEXT("GPIO"), {
        TEXT("Errors"),TEXT("Warnings"),TEXT("DllMain"),
			TEXT("Init"),TEXT("Deinit"),TEXT("IOCtl"),
			TEXT("Open"),TEXT("Close"),TEXT("Thread"),
            TEXT("Function"),TEXT("Undefined"),TEXT("Undefined"),
            TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
            0x001F
};

/* Debug Zones */
#define		ZONE_ERROR				(DEBUGZONE(0))
#define		ZONE_WARN				(DEBUGZONE(1))
#define		ZONE_DLLMAIN			(DEBUGZONE(2))
#define		ZONE_INIT				(DEBUGZONE(3))
#define		ZONE_DEINIT				(DEBUGZONE(4))
#define		ZONE_IOCTL				(DEBUGZONE(5))
#define		ZONE_OPEN				(DEBUGZONE(6))
#define		ZONE_CLOSE				(DEBUGZONE(7))
#define		ZONE_THREAD				(DEBUGZONE(8))
#define		ZONE_FUNCTION			(DEBUGZONE(9))
#endif

//
// Return Value
//
#define INTEL_SUCCESS						0
#define ERROR_INTEL_INVALID_HANDLE			1
#define ERROR_INTEL_INVALID_PIN_NUMBER		2
#define ERROR_INTEL_INVALID_IOCTL_NUMBER	3
#define ERROR_INTEL_GPIO_NOT_SELECTED		4
#define ERROR_INTEL_GPIO_WRONG_IO_DIRECTION	5
//To be continue...

//////////////////////////////////////////////////////////////////////////
//GPIO Base Address parameters, Please refer spec
#define GPIO_BASE_ADDR_BUS_NUM			(0x0)
#define GPIO_BASE_ADDR_DEVICE_NUM		(0x1F)
#define GPIO_BASE_ADDR_FUNC_NUM			(0x0)
#define GPIO_BASE_ADDR_OFFSET			(0x48)
#define GPIO_BASE_ADDR_LENGTH			(4)
#define GPIO_BASE_ADDR_MASK				(0x0000FFFE)
#define GPIO_MEM_LEN					(0x00000080)
//Total gpio pins
#define GPIO_TOTAL_PINS_IO				(76)//32 + 32 + 12 = 76
//Pins per bank
#define GPIO_PINS_PER_BANK_IO			(32)

//To get only the bit0
#define LOW_BIT_MASK					(0x00000001)



//////////////////////////////////////////////////////////////////////////
#define MAX_BANK_NUM 3
#define MAX_OTHER_NUM 7
static ULONG GPIO_USE_SEL_Pin2Offsetmapping[MAX_BANK_NUM] = 
{
	0x00,	// GPIO Use Select
	0x30,	// GPIO Use Select 2
	0x40	// GPIO Use Select 3
};
static ULONG GP_IO_SEL_Pin2Offsetmapping[MAX_BANK_NUM] = 
{
	0x04,	// GPIO Input/Output Select
	0x34,	// GPIO Input/Output Select 2
	0x44	// GPIO Input/Output Select 3
};
static ULONG GP_LVL_Pin2Offsetmapping[MAX_BANK_NUM] = 
{
	0x0C,	// GPIO Level for Input or Output
	0x38,	// GPIO Level for Input or Output 2
	0x48	// GPIO Level for Input or Output 3
};
static ULONG GP_OTHERS_Pin2Offsetmapping[MAX_OTHER_NUM] =
{
	0x18,    // 
	0x1C,    // 
	0x20,    // 
	0x24,
	0x28,
	0x2A,
	0x2C
};
static ULONG GP_RST_SEL_Pin2Offsetmapping[MAX_OTHER_NUM] = 
{
	0x60,	//
	0x64,	//
	0x68	//
};
typedef struct tagGPIO_CORE_IO
{
	ULONG ulAddrGpioUseSel[MAX_BANK_NUM];
	ULONG ulAddrGpIoSel[MAX_BANK_NUM];
	ULONG ulAddrGpLvl[MAX_BANK_NUM];
	ULONG ulAddrGpoBlink;
	ULONG ulAddrGpSerBlink;
	ULONG ulAddrGpSbCmdsts;
	ULONG ulAddrGpSbData;
	ULONG ulAddrNmiEn;
	ULONG ulAddrNmiSts;
	ULONG ulAddrInv;
	ULONG ulAddrGpRstSel[MAX_BANK_NUM];
}GPIO_CORE_IO, *PGPIO_CORE_IO;

typedef struct GPIO_DEVICE_CONTEXT
{
	DWORD dwMemBaseAddr;                     //Memory base address
	ULONG ulMemLen;                          //Size of memory base address
	DWORD ulControllerTotalPins;             //Total GPIO pins for a controller
	DWORD ulControllerPinsPerBank;			 //Pins per bank
	GPIO_CORE_IO coreRegisterIO;  //optimise the array data size
	//CRITICAL_SECTION m_csHardwareLock;       //Critical section
	HKEY	hKey;								//GPIO Registery
}GPIO_DEVICE_CTXT, *PDEVICE_CONTEXT;

/*GPIO driver to get base address*/
ULONG GPIOGetBaseAddr();


/*GPIO driver init function*/
DWORD GPI_Init( LPCTSTR pContext, LPCVOID lpvBusContext);

/*GPIO driver open*/
DWORD GPI_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode);

/*GPIO driver direction setting function*/
DWORD GPIOConnectIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to read as input function*/
DWORD GPIOReadIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to write as output function*/
DWORD GPIOWriteIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver query pin's setting function*/
DWORD GPIOQueryIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver to configure multiplexing for selected GPIO pins function*/
DWORD GPIOMuxIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters);

/*GPIO driver IOControl function*/
BOOL GPI_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut, 
                   DWORD dwLenOut,PDWORD pdwActualOut);

/*GPIO close function*/
BOOL GPI_Close(DWORD hOpenContext);

/*GPIO driver deinit function*/
BOOL GPI_Deinit(DWORD hDeviceContext);

#ifdef __cplusplus
}
#endif 

#endif // __GPIO_H__

