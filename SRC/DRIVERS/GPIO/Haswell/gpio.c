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


//Module Name: GPIO.C

/*
*	**********************************************************************
*	Intel's GPIO Driver
*	**********************************************************************
*/

#include "gpio.h"




static HANDLE ghFile = INVALID_HANDLE_VALUE;


/*************************************************************************
 @func  BOOL | DllEntry | Process attach/detach api.
 
 @desc The return is a BOOL, representing success (TRUE) or 
        failure (FALSE).The result would generally comprise of success code or 
		failure code. The failure code will indicate reason for failure.
 *************************************************************************/
BOOL  APIENTRY DllMain(HANDLE hModule, DWORD dwReason, 
                LPVOID lpReserved)
{
    switch (dwReason){
	case DLL_PROCESS_ATTACH:
#ifdef DEBUG
		DEBUGREGISTER(hModule);
#endif //DEBUG
		DEBUGMSG (ZONE_DLLMAIN,(TEXT("GPIO DLL_PROCESS_ATTACH - Process attached. \r\n")));
		DisableThreadLibraryCalls(hModule);
		break;
    case DLL_PROCESS_DETACH:
		DEBUGMSG (ZONE_DLLMAIN,(TEXT("GPIO DLL_PROCESS_DETACH - Process dettached. \r\n")));
		break;
	default:
		break;
	}

    return TRUE;
}

/*************************************************************************
 @func  ULONG | GPIOGetBaseAddr | Get GPIO Base Address

 @Description: This function is an internal function, it will be called when initializing 

 @rdesc This routine returns a ULONG representing the GPIO base address.
 *************************************************************************/
ULONG GPIOGetBaseAddr()
{
	ULONG gpioBase = 0;

	PCIReadBusData(GPIO_BASE_ADDR_BUS_NUM, 
		GPIO_BASE_ADDR_DEVICE_NUM, 
		GPIO_BASE_ADDR_FUNC_NUM, 
		&gpioBase, 
		GPIO_BASE_ADDR_OFFSET, 
		GPIO_BASE_ADDR_LENGTH);
	DEBUGMSG(ZONE_INIT,(TEXT("GPI0:Initializing GPIO driver - GPI_INIT() gpioBase = 0x%8X.\r\n"), gpioBase));

	gpioBase = gpioBase & GPIO_BASE_ADDR_MASK;
	DEBUGMSG(ZONE_INIT,(TEXT("GPI0:Initializing GPIO driver - GPI_INIT() after gpioBase = 0x%8X.\r\n"), gpioBase));

	return gpioBase;
}

/*************************************************************************

 @func  HANDLE | GPI_Init | Initialize GPIO device.
 @parm  PCTSTR  | Identifier | Device identifier. The device loader
                    passes in the registry key that contains information
                    about the active device.

 @remark This routine is called at device load time in order to perform 
         any initialization. Typically the init routine does as little as 
         possible,Create a memory space to store the device information.

 @Return Value	Returns a pointer which is passed into the GPI_Open and 
         GPI_Deinit entry points as a device handle.
 *************************************************************************/

DWORD GPI_Init( LPCTSTR pContext, LPCVOID lpvBusContext )
{
	GPIO_DEVICE_CTXT *pDevice = NULL;
	HKEY hKey = NULL;
	ULONG gpioBase = 0;
	int i = 0;

	/* pContext is the drivers active registry key */
    DEBUGMSG(ZONE_INIT,(TEXT("GPI0:Initializing GPIO driver - GPI_INIT().\r\n")));
  
	/* Create device structure and allocating memory */
	pDevice = (GPIO_DEVICE_CTXT*)LocalAlloc(LPTR, sizeof(GPIO_DEVICE_CTXT));

	if(pDevice == NULL) {
		DEBUGMSG(ZONE_ERROR,(TEXT("GPIO:GPI_INIT - Failed allocate GPIO controller structure.\r\n")));
		GPI_Deinit((DWORD)pDevice);
		return (DWORD)NULL;
	}

	/* Open the Active key for the driver */
    hKey = OpenDeviceKey(pContext);
	DEBUGMSG(ZONE_INIT,(TEXT("GPIO: GPI_Init() Active Registry Key= %s \n"),pContext));

    if(!hKey) {
        DEBUGMSG(ZONE_ERROR,(TEXT("GPIO - Failed to open device key path.\r\n")));
    }

	pDevice->hKey = hKey;
	gpioBase = GPIOGetBaseAddr();
	pDevice->dwMemBaseAddr = gpioBase;
	pDevice->ulMemLen = GPIO_MEM_LEN;
	pDevice->ulControllerTotalPins = GPIO_TOTAL_PINS_IO;
	pDevice->ulControllerPinsPerBank = GPIO_PINS_PER_BANK_IO;

	/* Assigning register start add for each pins in each GPIO controller */
	for (i = 0; i < MAX_BANK_NUM; ++i) {
		pDevice->coreRegisterIO.ulAddrGpioUseSel[i] = (pDevice->dwMemBaseAddr + GPIO_USE_SEL_Pin2Offsetmapping[i]);
		pDevice->coreRegisterIO.ulAddrGpIoSel[i] = (pDevice->dwMemBaseAddr + GP_IO_SEL_Pin2Offsetmapping[i]);
		pDevice->coreRegisterIO.ulAddrGpLvl[i] = (pDevice->dwMemBaseAddr + GP_LVL_Pin2Offsetmapping[i]);
	}

    DEBUGMSG(ZONE_INIT,(TEXT("GPIO: GPI_Init() END\n")));

    return (DWORD)pDevice;
} 

/*************************************************************************
 @func  HANDLE | GPI_Open | Open the GPIO device

 @Description: This routine must be called by the user to open the
 GPIO device. The HANDLE returned must be used by the application in
 all subsequent calls to the GPIO driver. This routine initialize 
 the device's data
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/

DWORD GPI_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode)
{
    DWORD dwRet =(DWORD)NULL;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext; 
	if(NULL == pDevice) {
        DEBUGMSG (ZONE_ERROR,(TEXT("GPIO: GPI_Open() - Failed to open device %d\r\n"),GetLastError()));
        return dwRet;
    }

 
    DEBUGMSG(ZONE_OPEN,(TEXT("GPIO: GPI_Open() END\n")));

	dwRet = (DWORD)pDevice;

    return dwRet;
} 

/*************************************************************************
 @func  DWORD | GPIOConnectIoPins | Set Io Direction

 @Description: This routine is called to set io direction of gpio pin 
 *************************************************************************/

DWORD GPIOConnectIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
	DWORD dwBankId = 0;
	DWORD dwPinNumber = 0;
	ULONG ulRegData = 0;

	ULONG ulUseSel = 0;
	ULONG ulMask = 0;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOConnectIoPins BEGIN\n")));

	if(NULL == pDevice) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOConnectIoPins - Failed to open device %d\r\n"),GetLastError()));
		return ERROR_INTEL_INVALID_HANDLE;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOConnectIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		return ERROR_INTEL_INVALID_PIN_NUMBER;
	}

	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;


	ulUseSel = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId]);
	ulMask = (1 << dwPinNumber);
	if (!(ulUseSel & ulMask)) {
		/* Not selected, GpioUseSel is 0, means native function */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOConnectIoPins - Not selected pin: %d\r\n"),Parameters->pin));
		return ERROR_INTEL_GPIO_NOT_SELECTED;
	}

	ulRegData = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpIoSel[dwBankId]);
	if (Parameters->u.ConnectMode == CONNECT_MODE_INPUT) {
		ulRegData |= (1 << dwPinNumber);
	}
	else if (Parameters->u.ConnectMode == CONNECT_MODE_OUTPUT) {
		ulRegData &= ~(1 << dwPinNumber);
	}
	else {
		/* invalide connectMode */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOConnectIoPins - Wrong Io direction of pin: %d\r\n"),Parameters->pin));
		return ERROR_INTEL_GPIO_WRONG_IO_DIRECTION;
	}
	WRITE_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpIoSel[dwBankId], ulRegData);

	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOConnectIoPins END\n")));
	
	return INTEL_SUCCESS;
} 
/*************************************************************************
 @func  BOOL | GPIOReadIoPins | Read an input pin

 @Description: This routine is called to read an input pin 
 *************************************************************************/
DWORD GPIOReadIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
	DWORD dwBankId = 0;
	DWORD dwPinNumber = 0;
	ULONG ulRegData = 0;

	ULONG ulUseSel = 0;
	ULONG ulIoSel = 0;
	ULONG ulMask = 0;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOReadIoPins BEGIN\n")));

	if(NULL == pDevice) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOReadIoPins - Failed to open device %d\r\n"),GetLastError()));
        return ERROR_INTEL_INVALID_HANDLE;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOReadIoPins - Error pin number: %ud >= total pin: %ud\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
        return ERROR_INTEL_INVALID_PIN_NUMBER;
    }

	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;	


	/* 1 To promise the pin has been selected */
	ulUseSel = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId]);
	ulMask = (1 << dwPinNumber);
	if (!(ulUseSel & ulMask)) {
		/* Not selected, GpioUseSel is 0, means native function */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOReadIoPins - Not selected pin: %d\r\n"),Parameters->pin));
		return ERROR_INTEL_GPIO_NOT_SELECTED;
	}

	/* 2 To promise the pin Io direction is correct */
	ulIoSel = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpIoSel[dwBankId]);
	if (!(ulIoSel & ulMask)) {
		/* Not Read, that pin value is 0 */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOReadIoPins - IO Direction is not read, it is %d\r\n"), Parameters->u.ConnectMode));
		return ERROR_INTEL_GPIO_WRONG_IO_DIRECTION;				
	}

	/* 3 Finally read the pin value */
	ulRegData = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpLvl[dwBankId]);
	Parameters->u.data = (ulRegData >> dwPinNumber) & LOW_BIT_MASK;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOReadIoPins END\n")));
	
	return INTEL_SUCCESS;
} 
/*************************************************************************
@func  BOOL | GPIOWriteIoPins | Write an output pin

@Description: This routine is called write high or low an output selected pin 
 *************************************************************************/
DWORD GPIOWriteIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
	DWORD dwBankId = 0;
	DWORD dwPinNumber = 0;
	ULONG ulRegData = 0;
	ULONG ulPinValue = 0;

	ULONG ulUseSel = 0;
	ULONG ulIoSel = 0;
	ULONG ulMask = 0;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOWriteIoPins BEGIN\n")));

	if(NULL == pDevice) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOWriteIoPins - Failed to open device %d\r\n"),GetLastError()));
        return ERROR_INTEL_INVALID_HANDLE;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOWriteIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
        return ERROR_INTEL_INVALID_PIN_NUMBER;
    }

	ulPinValue = Parameters->u.data;	
	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;

	/* 1 To promise the pin has been selected */
	ulUseSel = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId]);
	ulMask = (1 << dwPinNumber);
	if (!(ulUseSel & ulMask)) {
		/* Not selected, GpioUseSel is 0, means native function */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOWriteIoPins - Not selected pin: %d\r\n"),Parameters->pin));
		return ERROR_INTEL_GPIO_NOT_SELECTED;
	}

	/* 2 To promise the pin Io direction is correct */
	ulIoSel = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpIoSel[dwBankId]);
	if (ulIoSel & ulMask) {
		/* Not write, that pin value is 1 */
		DEBUGMSG (TRUE,(TEXT("GPIO:GPIOWriteIoPins - IO Direction is not write, it is %d\r\n"), Parameters->u.ConnectMode));
		return ERROR_INTEL_GPIO_WRONG_IO_DIRECTION;
	}

	/* 3 Finally write the pin value */
	ulRegData = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpLvl[dwBankId]);
	if (ulPinValue){
		ulRegData |= (1 << dwPinNumber);
	}
	else{
		ulRegData &= ~(1 << dwPinNumber);
	}
	WRITE_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpLvl[dwBankId], ulRegData);


	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOWriteIoPins END\n")));
	
	return INTEL_SUCCESS;
} 
/*************************************************************************
@func  BOOL | GPIOQueryIoPins | Query Pin's setting

@Description: This routine is called to query current pin's setting
 *************************************************************************/
DWORD GPIOQueryIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
	DWORD dwBankId = 0;
	DWORD dwPinNumber = 0;
	ULONG ulRegData = 0;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOQueryIoPins BEGIN\n")));

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOQueryIoPins - Failed to open device %d\r\n"),GetLastError()));
        return ERROR_INTEL_INVALID_HANDLE;
	}
    if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOQueryIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
        return ERROR_INTEL_INVALID_PIN_NUMBER;
    }


	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
    dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;


	ulRegData = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId]);
	Parameters->u.data = (ulRegData >> dwPinNumber) & LOW_BIT_MASK;  

	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOQueryIoPins END\n")));
	
	return INTEL_SUCCESS;
} 
/*************************************************************************
 @func  BOOL | GPIOMuxIoPins | Mux setting

 @Description: This routine is called to configure multiplexing for 
			   selected GPIO pins.
 
 *************************************************************************/
DWORD GPIOMuxIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
	DWORD dwBankId = 0;
	DWORD dwPinNumber = 0;
	ULONG ulRegData = 0;
	ULONG ulPinValue = 0;

    /* request structure pointing to hOpenContext handler */
	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO:GPIOMuxIoPins BEGIN\n")));

	if(NULL == pDevice) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOMuxIoPins - Failed to open device %d\r\n"),GetLastError()));
        return ERROR_INTEL_INVALID_HANDLE;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOMuxIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
        return ERROR_INTEL_INVALID_PIN_NUMBER;
    }

    ulPinValue = Parameters->u.data;
	dwBankId =  Parameters->pin / pDevice->ulControllerPinsPerBank;
    dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;

	ulRegData = READ_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId]);
	if (ulPinValue) {
		ulRegData |= (1 << dwPinNumber);
	}
	else {
		ulRegData &= ~(1 << dwPinNumber);
	}
	WRITE_PORT_ULONG((PULONG)pDevice->coreRegisterIO.ulAddrGpioUseSel[dwBankId], ulRegData);

	DEBUGMSG(ZONE_FUNCTION,(TEXT("GPIO: GPIOMuxIoPins() END\n")));
	
	return INTEL_SUCCESS;
} 

/*************************************************************************
 @func BOOL         | GPI_IOControl | Device IO control routine
 @parm pGPIOhandle  | hOpenContext  | GPIO handle
 @parm DWORD        | dwCode        | io control code to be performed
 @parm PBYTE        | pBufIn        | input data to the device
 @parm DWORD        | dwLenIn       | number of bytes being passed in
 @parm PBYTE        | pBufOut       | output data from the device
 @parm DWORD        | dwLenOut      | maximum number of bytes to receive 
                                      from device
 @parm PDWORD       | pdwActualOut  | actual number of bytes received from 
                                      device
 @rdesc Returns TRUE on success, FALSE on failure
 @remark  Routine exported by a device driver. 
 *************************************************************************/
BOOL GPI_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn, 
                   PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
	DWORD dwRet = INTEL_SUCCESS;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPI_IOControl - Failed to open device %d\r\n"),GetLastError()));
		return FALSE;
	}

	DEBUGMSG(ZONE_IOCTL,(TEXT("GPIO: GPI_IOControl() Index: %0x BEGIN\n"),dwCode));

	switch (dwCode) {
	case IOCTL_GPIO_MUX:
		dwRet = GPIOMuxIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
		break;
	case IOCTL_GPIO_DIRECTION:
		dwRet = GPIOConnectIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
		break;
	case IOCTL_GPIO_READ:
		dwRet = GPIOReadIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
		break;
	case IOCTL_GPIO_WRITE:
		dwRet = GPIOWriteIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
		break;
	case IOCTL_GPIO_QUERY:
		dwRet = GPIOQueryIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
		break;
	default:
		DEBUGMSG(ZONE_ERROR,(TEXT("GPIO: GPI_IOControl()  Invalid index: %d\n"),dwCode));
		dwRet = ERROR_INTEL_INVALID_IOCTL_NUMBER;
		break;
	}
	DEBUGMSG(ZONE_IOCTL,(TEXT("GPIO: GPI_IOControl() Index: %d END\n"),dwCode));

	if (dwRet == INTEL_SUCCESS) {
		return TRUE;
	}
	else {
		return FALSE;
	}
} 
/*************************************************************************
 @func BOOL     | GPI_Close | close the GPIO device.
 @parm GPIO_handle	| hOpenContext | Context pointer returned from GPI_Open
 @rdesc TRUE if success; FALSE if failure
 @remark This routine is called by the device manager to close the device.
 *************************************************************************/
BOOL GPI_Close(DWORD hOpenContext)
{
    BOOL bRet = TRUE;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(pDevice == NULL) {
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPI_Close - Incorrect device context. %d\r\n"), GetLastError()));
		return bRet = FALSE;
	}

    DEBUGMSG(ZONE_CLOSE,(TEXT("GPIO: GPI_Close() END\n")));

    return bRet;
} 
/*************************************************************************
 @func  BOOL | GPI_Deinit | De-initialize GPIO
 @parm 	GPIO_handle | hDeviceContext | Context pointer returned from GPI_Init
 @rdesc None.
 *************************************************************************/
BOOL GPI_Deinit(DWORD hDeviceContext)
{
    BOOL bRet = TRUE;

	/* Unallocate the memory*/
	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hDeviceContext;

	if(pDevice) {
		if (pDevice->hKey) {
			RegCloseKey(pDevice->hKey);
		}
		/* Free device structure */
		LocalFree(pDevice);  
	} 

	DEBUGMSG(ZONE_DEINIT,(TEXT("GPIO: GPI_Deinit() END\n")));
	return bRet;
} 
