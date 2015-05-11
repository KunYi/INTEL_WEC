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
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DEBUGMSG (ZONE_INFO,(TEXT("GPIO DLL_PROCESS_ATTACH - Process attached. \r\n")));
			DisableThreadLibraryCalls(hModule);
            break;

        case DLL_PROCESS_DETACH:
            DEBUGMSG (ZONE_INFO,(TEXT("GPIO DLL_PROCESS_DETACH - Process dettached. \r\n")));
            break;
		default:
			break;
    }

    return TRUE;
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
	ULONG * Pin2Offsetmapping = NULL;
	DWORD dwStatus;
	ULONG ulLen = sizeof(ULONG);
	PHYSICAL_ADDRESS phyAddr;
	ULONG ulCount;
	DWORD bRet = (DWORD)NULL;
	DWORD dwType = REG_DWORD;
	DWORD dwValue;
	DWORD dwBuffSize = sizeof(dwValue);

	//pContext is the drivers active registry key  
	/* Create device structure and allocating memory */
	pDevice = (GPIO_DEVICE_CTXT*)LocalAlloc(LPTR, sizeof(GPIO_DEVICE_CTXT));

	if(pDevice == NULL){
		DEBUGMSG(ZONE_ERROR,(TEXT("GPIO:GPI_INIT - Failed allocate GPIO controller structure.\r\n")));
		goto Err;
	}

	// Open the Active key for the driver.
    pDevice->hKey = OpenDeviceKey(pContext);
    if(pDevice->hKey==INVALID_HANDLE_VALUE) {
        DEBUGMSG(ZONE_ERROR,(TEXT("GPIO - Failed to open device key path.\r\n")));
		goto Err;
	}
    else
      {
          dwStatus = RegQueryValueEx( pDevice->hKey, TEXT("DeviceArrayIndex"), NULL, &dwType, (LPBYTE)&dwValue, &dwBuffSize);
		   if (dwStatus != ERROR_SUCCESS)
		   {
			    DEBUGMSG(ZONE_ERROR,(TEXT("GPIO - Error opening key - %0x\n"),dwStatus));
				RegCloseKey(pDevice->hKey);
				goto Err;
		    }
		   pDevice->dwDeviceIndex = dwValue;
       }

	 //configure SouthCore GPIO controller
	if(pDevice->dwDeviceIndex  == 0)
	{
		phyAddr.QuadPart = GPIO_SCORE_BASE_ADDR;
	    pDevice->dwMemBaseAddr = (DWORD)MmMapIoSpace(phyAddr, ulLen, FALSE);
		pDevice->ulMemLen = ulLen;
		
		if(pDevice->dwMemBaseAddr == 0)
		{
           DEBUGMSG(ZONE_ERROR,(TEXT("GPIO -Failed map SouthCore GPIO controller registers.\r\n")));
		   goto Err;
		}
		else
		{
			pDevice->ulControllerTotalPins = GPIO_TOTAL_PINS_SCORE_MMIO;
			pDevice->ulControllerPinsPerBank = GPIO_PINS_PER_BANK_MMIO;
			Pin2Offsetmapping = SCORE_Pin2Offsetmapping;
		}

	}
	//configure NorthCore GPIO controller
    else if(pDevice->dwDeviceIndex  == 1)
	{
		phyAddr.QuadPart = GPIO_NCORE_BASE_ADDR;
	    pDevice->dwMemBaseAddr = (DWORD)MmMapIoSpace(phyAddr, ulLen, FALSE);
		pDevice->ulMemLen = ulLen;
		if(pDevice->dwMemBaseAddr == 0)
		{
           DEBUGMSG(ZONE_ERROR,(TEXT("GPIO -Failed map NorthCore GPIO controller registers.\r\n")));
		   goto Err;
		}
		else
		{
			pDevice->ulControllerTotalPins = GPIO_TOTAL_PINS_NCORE_MMIO;
			pDevice->ulControllerPinsPerBank = GPIO_PINS_PER_BANK_MMIO;
			Pin2Offsetmapping = NCORE_Pin2Offsetmapping;
		}

	}
	//configure SUSCore GPIO controller
	else if(pDevice->dwDeviceIndex  == 2)
	{
		phyAddr.QuadPart = GPIO_SUS_BASE_ADDR;
	    pDevice->dwMemBaseAddr = (DWORD)MmMapIoSpace(phyAddr, ulLen, FALSE);
		pDevice->ulMemLen = ulLen;
		if(pDevice->dwMemBaseAddr == 0)
		{
           DEBUGMSG(ZONE_ERROR,(TEXT("GPIO -Failed map SUSCore GPIO controller registers\r\n")));
		   goto Err;
		}
		else
		{
			pDevice->ulControllerTotalPins = GPIO_TOTAL_PINS_SUS_MMIO;
			pDevice->ulControllerPinsPerBank = GPIO_PINS_PER_BANK_MMIO;
			Pin2Offsetmapping = SUS_Pin2Offsetmapping;
		}

	}

	if(Pin2Offsetmapping != NULL)
	{
		//Assigning register start add for each pins in each GPIO controller
		for( ulCount = 0; ulCount < pDevice->ulControllerTotalPins; ulCount++)
		{
			pDevice->MMIOBanks[ulCount / GPIO_PINS_PER_BANK_MMIO].Registers[ulCount % GPIO_PINS_PER_BANK_MMIO] = ((PGPIO_CORE_BANK_REGISTERS_MMIO)(pDevice->dwMemBaseAddr + Pin2Offsetmapping[ulCount]));			
		}
	}
	/*Return non-null value*/
    return (DWORD)pDevice;

Err:
	GPI_Deinit((DWORD)pDevice);
    return bRet;
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
	if(NULL == pDevice){
        DEBUGMSG (ZONE_ERROR,(TEXT("GPIO: GPI_Open() - Failed to open device %d\r\n"),GetLastError()));
        return dwRet;
    }

	dwRet = (DWORD)pDevice;

    return dwRet;
} 

/*************************************************************************
 @func  HANDLE | GPI_Open | Open the GPIO device

 @Description: This routine must be called by the user to open the
 GPIO device. The HANDLE returned must be used by the application in
 all subsequent calls to the GPIO driver. This routine initialize 
 the device's data
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/

BOOL GPIOConnectIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
   BOOL dwRet = TRUE;
   DWORD dwBankId, dwPinNumber;
   ULONG ulRegData;

   GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOConnectIoPins - Failed to open device %d\r\n"),GetLastError()));
		dwRet = FALSE;
        return dwRet;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOConnectIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		dwRet = FALSE;
        return dwRet;
    }

	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;

    
	ulRegData = READ_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadValue);
	ulRegData |= DIRECTION_MASK;
	if (Parameters->u.ConnectMode == CONNECT_MODE_INPUT)
			ulRegData &= ~INPUT_BIT_MASK;			
	else if (Parameters->u.ConnectMode == CONNECT_MODE_OUTPUT)
			ulRegData &= ~OUTPUT_BIT_MASK;	

	WRITE_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadValue, ulRegData);  
	
	return dwRet;
} 
/*************************************************************************
 @func  BOOL | GPIOReadIoPins | Read an input pin

 @Description: This routine is called to read an input pin 
 *************************************************************************/
DWORD GPIOReadIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
   BOOL dwRet = TRUE;
   DWORD dwBankId, dwPinNumber;
   ULONG ulRegData;

   GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOReadIoPins - Failed to open device %d\r\n"),GetLastError()));
		dwRet = FALSE;
        return dwRet;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOReadIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		dwRet = FALSE;
        return dwRet;
    }

	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;	

	ulRegData = READ_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadValue);	

	
	Parameters->u.data = ulRegData & LVL_BIT_MASK;
	
	return dwRet;
} 
/*************************************************************************
@func  BOOL | GPIOWriteIoPins | Write an output pin

 @Description: This routine is called write high or low an output selected pin 
 *************************************************************************/
BOOL GPIOWriteIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
   DWORD dwRet = TRUE;
   DWORD dwBankId, dwPinNumber;
   ULONG ulRegData;
   ULONG ulPinValue;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOWriteIoPins - Failed to open device %d\r\n"),GetLastError()));
		dwRet = FALSE;
        return dwRet;
	}
	 if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOWriteIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		dwRet = FALSE;
        return dwRet;
    }

	ulPinValue = Parameters->u.data;	
	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
	dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;

	ulRegData     = READ_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadValue);
	  /*to support read after write pad value, input must be enabled for non-virtual pins*/
    ulRegData &= ~OUTPUT_BIT_MASK;
    ulRegData &= ~INPUT_BIT_MASK;

    if(ulPinValue)
        ulRegData |= LVL_BIT_MASK;
    else
        ulRegData &= ~LVL_BIT_MASK;
       
	WRITE_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadValue, ulRegData);	    
 
	return dwRet;
} 
/*************************************************************************
@func  BOOL | GPIOQueryIoPins | Query Pin's setting

 @Description: This routine is called to query current pin's setting
 *************************************************************************/
BOOL GPIOQueryIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
   DWORD dwRet = TRUE;
   DWORD dwBankId, dwPinNumber;
   ULONG ulRegData;


	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOQueryIoPins - Failed to open device %d\r\n"),GetLastError()));
		dwRet = FALSE;
        return dwRet;
	}
    if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOQueryIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		dwRet = FALSE;
        return dwRet;
    }

	dwBankId = Parameters->pin / pDevice->ulControllerPinsPerBank;
    dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;
    
    ulRegData = READ_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadConf0);
    
    Parameters->u.data = ulRegData & VV_PIN_MUX_MASK;    
 	
	return dwRet;
} 
/*************************************************************************
 @func  BOOL | GPIOMuxIoPins | Mux setting

 @Description: This routine is called to configure multiplexing for 
			   selected GPIO pins.
 
 *************************************************************************/
BOOL GPIOMuxIoPins(PDEVICE_CONTEXT hOpenContext,PGPIO_PIN_PARAMETERS Parameters)
{
   BOOL dwRet = TRUE;
   DWORD dwBankId, dwPinNumber;
   ULONG ulRegData;
   ULONG ulPinValue;

    /* request structure pointing to hOpenContext handler */
	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOMuxIoPins - Failed to open device %d\r\n"),GetLastError()));
		dwRet = FALSE;
        return dwRet;
	}
	if(Parameters->pin >= pDevice->ulControllerTotalPins){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPIOMuxIoPins - Error pin number: %d > total pin: %d\r\n"),Parameters->pin,pDevice->ulControllerTotalPins));
		dwRet = FALSE;
        return dwRet;
    }

    ulPinValue = Parameters->u.data;
	dwBankId =  Parameters->pin / pDevice->ulControllerPinsPerBank;
    dwPinNumber = Parameters->pin % pDevice->ulControllerPinsPerBank;

    ulRegData  = READ_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadConf0);
  
    /*to support read after write pad value, input must be enabled for non-virtual pins*/
    ulRegData &= ~VV_PIN_MUX_MASK;
    ulRegData |= ulPinValue & VV_PIN_MUX_MASK;

    WRITE_REGISTER_ULONG(&pDevice->MMIOBanks[dwBankId].Registers[dwPinNumber]->ulPadConf0, ulRegData);
	
	return dwRet;
} 

/*************************************************************************
  @func BOOL        | GPI_IOControl | Device IO control routine
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
    BOOL bRet = FALSE;

	GPIO_DEVICE_CTXT *pDevice = (GPIO_DEVICE_CTXT*)hOpenContext;

	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPI_IOControl - Failed to open device %d\r\n"),GetLastError()));
        return bRet;
	}

	switch (dwCode) {
		case IOCTL_GPIO_MUX:
			bRet = GPIOMuxIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
			break;
		case IOCTL_GPIO_DIRECTION:
			bRet = GPIOConnectIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
			break;
		case IOCTL_GPIO_READ:	
			bRet = GPIOReadIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
			break;
		case IOCTL_GPIO_WRITE:
			bRet = GPIOWriteIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
			break;
		case IOCTL_GPIO_QUERY:
			bRet = GPIOQueryIoPins(pDevice,(PGPIO_PIN_PARAMETERS)pBufIn);
			break;
		default:
			DEBUGMSG(ZONE_ERROR,(TEXT("GPIO: GPI_IOControl()  Invalid index: %d\n"),dwCode));
			bRet= FALSE;
			break;
	}

    return bRet;
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

	if(pDevice == NULL){
		DEBUGMSG (ZONE_ERROR,(TEXT("GPIO:GPI_Close - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;
	}

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

     if(pDevice)
	 {
	   if(pDevice->dwMemBaseAddr)
		   MmUnmapIoSpace((VOID*)pDevice->dwMemBaseAddr,pDevice->ulMemLen);
     }

	 if(pDevice->hKey!= NULL) { RegCloseKey(pDevice->hKey); }
     /* Free device structure */
	 LocalFree(pDevice);       

	return bRet;
} 