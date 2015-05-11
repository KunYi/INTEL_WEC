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


//Module Name: I2C.C

/*
*	**********************************************************************
*	Intel's I2C Driver
*	**********************************************************************
*/

#include <windows.h>
#include <nkintr.h>
#include <Winbase.h>
#include <devload.h>    /* Registry API's */
#include <pm.h>
#include "I2Cpublic.h"
#include "i2c.h"

/*************************************************************************
 @func  BOOL | DllEntry | Process attach/detach api.
 
 @desc The return is a BOOL, representing success (TRUE) or 
        failure (FALSE).The result would generally comprise of success code or 
		failure code. The failure code will indicate reason for failure.
 *************************************************************************/
BOOL DllEntry(HANDLE  hDllHandle, DWORD   dwReason, LPVOID  lpreserved)
{   
    switch(dwReason) {
        case DLL_PROCESS_ATTACH:
            DEBUGMSG (ZONE_INFO,(TEXT("I2C DLL_PROCESS_ATTACH - Process attached. \r\n")));
            break;
        case DLL_PROCESS_DETACH:
            DEBUGMSG (ZONE_INFO,(TEXT("I2C DLL_PROCESS_DETACH - Process dettached. \r\n")));
            break;
        default:
            break;
    }/* end switch */
    
    return TRUE;
}

/*************************************************************************

 @func  HANDLE | I2C_Init | Initialize I2C device.
 @parm  PCTSTR  | Identifier | Device identifier. The device loader
                    passes in the registry key that contains information
                    about the active device.

 @remark This routine is called at device load time in order to perform 
         any initialization. Typically the init routine does as little as 
         possible,Create a memory space to store the device information.

 @Return Value	Returns a pointer which is passed into the I2C_Open and 
         I2C_Deinit entry points as a device handle.
 *************************************************************************/

DWORD I2C_Init( LPCTSTR pContext, LPCVOID lpvBusContext )
{
	I2C_DEVICE_CTXT *pDevice = NULL;

	PHYSICAL_ADDRESS phyAddr;

	DDKWINDOWINFO dwi;
	DDKISRINFO isri;
	DDKPCIINFO pcii;
	GIISR_INFO Info;

	DWORD dwStatus=0;
	DWORD dwType = REG_DWORD;
	DWORD dwLength =0;
	DWORD dwBusNumber =0;
	DWORD dwInterfaceType =0;
	DWORD MMIOSpace = 0;
	LPVOID dwPhysAddr;
	DWORD bRet = (DWORD)NULL;

	int i = 0;

	// Create device structure and allocating memory 
	pDevice = (I2C_DEVICE_CTXT*)LocalAlloc(LPTR, sizeof(I2C_DEVICE_CTXT));

	if(pDevice == NULL){
		DEBUGMSG(ZONE_ERROR,(TEXT("I2C:I2C_INIT - Failed allocate I2C controller structure.\r\n")));
		goto Err;
	}

	// Open the Active key for the driver.
    pDevice->hKey = OpenDeviceKey(pContext);

    if(pDevice->hKey==INVALID_HANDLE_VALUE) {
        DEBUGMSG(ZONE_ERROR,(TEXT("I2C - Failed to open device key path.\r\n")));
		goto Err;
    }
	
	// read window information
	dwi.cbSize = sizeof(dwi);
	dwStatus = DDKReg_GetWindowInfo(pDevice->hKey,&dwi);
	if(dwStatus != ERROR_SUCCESS) {
		DEBUGMSG(ZONE_ERROR, (_T("I2C: DDKReg_GetWindowInfo() failed %d\r\n"), dwStatus));
		goto Err;
	}

	dwBusNumber = dwi.dwBusNumber;
	dwInterfaceType = dwi.dwInterfaceType;

	
	// memory space
	if(dwi.dwNumMemWindows  != 0){
		 phyAddr.LowPart = dwi.memWindows[0].dwBase;
		 phyAddr.HighPart = 0;
		 dwLength = dwi.memWindows[0].dwLen;
		 MMIOSpace = (DWORD)FALSE;
	}
	else{
		DEBUGMSG(ZONE_ERROR, (TEXT("I2C: Failed to get physical address of the device.\r\n")));
		goto Err;
	}

    // creates a handle that can be used for accessing a bus
	pDevice->hBusAccess = CreateBusAccessHandle(pContext);
	if(pDevice->hBusAccess == NULL){
		DEBUGMSG(ZONE_ERROR,(TEXT("I2C:I2C_INIT - Failed to create handle for bus accessing.\r\n")));
		goto Err;
	}

	// read PCI id
	pcii.cbSize = sizeof(pcii);
	dwStatus = DDKReg_GetPciInfo(pDevice->hKey, &pcii);
	if(dwStatus != ERROR_SUCCESS) {
		DEBUGMSG(ZONE_ERROR, (_T("I2C: DDKReg_GetPciInfo() failed %d\r\n"), dwStatus));
		goto Err;
	}

	pDevice->dwDeviceId = pcii.idVals[PCIID_DEVICEID];
	

	// read ISR information
	isri.cbSize = sizeof(isri);
	dwStatus = DDKReg_GetIsrInfo(pDevice->hKey, &isri);
	if(dwStatus != ERROR_SUCCESS) {
		DEBUGMSG(ZONE_ERROR, (_T("I2C: DDKReg_GetIsrInfo() failed %d\r\n"), dwStatus));
		goto Err;
	}
	

	// sanity check return values
	if(isri.dwSysintr == SYSINTR_NOP) {
		DEBUGMSG(ZONE_ERROR, (_T("I2C: no sysintr specified in registry\r\n")));
		goto Err;
	}
	if(isri.szIsrDll[0] != 0) {
		if(isri.szIsrHandler[0] == 0 || isri.dwIrq == IRQ_UNSPECIFIED) {
			DEBUGMSG(ZONE_ERROR, (_T("I2C: invalid installable ISR information in registry\r\n")));
			goto Err;
		}
	}

	pDevice->dwSysIntr = isri.dwSysintr;
	pDevice->dwIrq = isri.dwIrq;


	// Install ISR handler if there is one
    if(isri.szIsrDll[0] != 0) 
    {
        // Install ISR handler
        pDevice->hIsrHandler = LoadIntChainHandler(isri.szIsrDll, isri.szIsrHandler, (BYTE) isri.dwIrq);
        if(pDevice->hIsrHandler == NULL) {
            DEBUGMSG(ZONE_ERROR, (TEXT("I2C: Couldn't install ISR handler\r\n")));
			goto Err;
        } 
		else 
		{
           	if(!BusTransBusAddrToStatic(pDevice->hBusAccess, (INTERFACE_TYPE)dwInterfaceType, dwBusNumber,
									     phyAddr, dwLength, &MMIOSpace, &dwPhysAddr)) 
			{
                DEBUGMSG(ZONE_ERROR, (TEXT("I2C: Failed BusTransBusAddrToStatic.\r\n")));
				goto Err;
            }

			// Get the register base for the controller.
			if(dwPhysAddr)
			{
				pDevice->pRegisters = (PI2C_REGISTERS)dwPhysAddr;
				pDevice->dwMemBaseAddr = (DWORD)pDevice->pRegisters;
				pDevice->dwMemLen = dwLength;
			}
           
		   // Set up ISR handler
		    Info.SysIntr    = pDevice->dwSysIntr;
			Info.CheckPort  = TRUE;
			Info.PortIsIO   = FALSE;
			Info.UseMaskReg = FALSE;
			Info.PortAddr   = (DWORD)dwPhysAddr + I2C_INTR_OFFSET;
			Info.PortSize   = sizeof(DWORD);
			Info.Mask		= I2C_ALL_MASKABLE;

	            
			if(!KernelLibIoControl(pDevice->hIsrHandler, IOCTL_GIISR_INFO, &Info, sizeof(Info), NULL, 0, NULL)) 
			{
				  DEBUGMSG(ZONE_ERROR, (TEXT("I2C: KernelLibIoControl call failed.\r\n")));
				  goto Err;
			}

		}
	}

	 /* create event for handling hardware interrupts */
    pDevice->hInterruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(pDevice->hInterruptEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("I2C: I2C_Init: Failed create interrupt event\r\n")));
		goto Err;
    }

	//Create event for handling read/write event
    pDevice->hReadWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(pDevice->hReadWriteEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("I2C: I2C_Init: Failed create Read/Write event\r\n")));
		goto Err;
    }
 
    pDevice->hInterruptThread = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE)I2C_IST, (LPVOID)pDevice, CREATE_SUSPENDED, NULL);
    if(pDevice->hInterruptThread == NULL) {
        DEBUGMSG(ZONE_ERROR,(_T("ERROR: I2C_Init - Failed to create interrupt thread\r\n")));
		goto Err;
    }
	
    /* initialize interrupt sysintr to event mapping  */
   if(!InterruptInitialize(pDevice->dwSysIntr,pDevice->hInterruptEvent, NULL, 0)) {
        DEBUGMSG(ZONE_ERROR, (_T("I2C: I2C_Init - Failed to initialize interrupt\r\n")));
		goto Err;

	}
	//disable the controller
	if(!i2cDisableController(pDevice)){
		DEBUGMSG(ZONE_ERROR, (_T("I2C: I2C_Init - Failed to disable I2C Controller\r\n")));
		goto Err;
	}


	 // Configure default HCNT/LCNT/SDA_HOLD values for SS, FS modes.

    pDevice->BusSetting.SS[I2C_BUS_SCL_HCNT]  = I2C_SS_SCL_HCNT_100MHZ;
    pDevice->BusSetting.SS[I2C_BUS_SCL_LCNT]  = I2C_SS_SCL_LCNT_100MHZ;
    pDevice->BusSetting.SS[I2C_BUS_SDA_HOLD]  = I2C_SS_SDA_HOLD_100MHZ;
    pDevice->BusSetting.SS[I2C_BUS_SDA_SETUP] = I2C_SS_SDA_SETUP_100MHZ;

    pDevice->BusSetting.FS[I2C_BUS_SCL_HCNT]  = I2C_FS_SCL_HCNT_100MHZ;
    pDevice->BusSetting.FS[I2C_BUS_SCL_LCNT]  = I2C_FS_SCL_LCNT_100MHZ;
    pDevice->BusSetting.FS[I2C_BUS_SDA_HOLD]  = I2C_FS_SDA_HOLD_100MHZ;
    pDevice->BusSetting.FS[I2C_BUS_SDA_SETUP] = I2C_FS_SDA_SETUP_100MHZ;
	
	//clear all interrupts during init i2c controller
	I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_INTR);

	// Disable all interrupts.
	i2cDisableInterrupts(pDevice);
	pDevice->InterruptMask = 0;

	//Initialize the looping in IST
	pDevice->ISTLoopTrack = TRUE;
	/*No transfer in the ini device. 
	Therefore status is set to default as below*/
	pDevice->status = TRANSFER_UNSUCCESSFUL;  
	
	if (ResumeThread(pDevice->hInterruptThread)== 0xFFFFFFFF){
		DEBUGMSG(ZONE_ERROR, (_T("I2C: I2C_Init - Failed to ResumeThread.\r\n")));
		goto Err;
	}

	return (DWORD)pDevice;

Err:
	I2C_Deinit((DWORD)pDevice);
    return bRet;

} 


/*************************************************************************
 @func  HANDLE | i2cRxRemainingData | Count remaining Rx Request data 																		

 @Description: Count the exact data remaining for Rx Transfer

 @rdesc This routine returns the remaining data need to be transfered.
*************************************************************************/
DWORD i2cRxRemainingData(PDEVICE_CONTEXT pDevice)
{
	return (pDevice->RxRequestLength - pDevice->RxTransferredLength);	
}

/*************************************************************************
 @func  HANDLE | I2C_IST | Open the I2C device

 @Description: This routine must be called when there hardware 
 interrupt signal and it will service the request. Return complete to ISR
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/
DWORD I2C_IST(PDEVICE_CONTEXT pDevice)
{

	BOOL bRet = TRUE;
	DWORD stat = 0;
	DWORD enabled = 0;


   // loop here waiting for an Interrupt to occur
 while(pDevice->ISTLoopTrack)
	{
		 
		WaitForSingleObject(pDevice->hInterruptEvent,INFINITE);

		if (!pDevice->ISTLoopTrack){
			bRet= FALSE;
			break;
		}
		
		//check whether controller enabled & interrupt register set
		enabled = I2C_REGISTER_READ(pDevice->pRegisters->IC_ENABLE);
		stat  = I2C_REGISTER_READ(pDevice->pRegisters->IC_RAW_INTR_STAT);
						
		if(enabled || (stat & ~IC_INTR_ACTIVITY))
		{
			// Acknowledge requested interrupts.It also clear all the interrupt in the registers
			 stat = i2cAcknowledgeInterrupts(pDevice, pDevice->InterruptMask);
					
			if((stat & IC_INTR_TX_ABRT   ) != 0)
			{
				pDevice->InterruptMask &= 0;
				i2cDisableInterrupts(pDevice);
				pDevice->status = TRANSFER_ABORTED;
				SetEvent(pDevice->hReadWriteEvent);
				DEBUGMSG(ZONE_ERROR,(TEXT("I2C:I2C_IST - IC_INTR_TX_ABRT.\r\n")));
			}		 
			else if((stat  & IC_INTR_TX_EMPTY ) != 0)
			{	//call write operation	
				if(!pDevice->bIoComplete)									
					TransferDataWrite(pDevice);
				//validate before clearing
				if(pDevice->bIoComplete)
				{
					pDevice->status = TRANSFER_SUCCESS;  //write successful
					pDevice->InterruptMask &= 0;
					i2cDisableInterrupts(pDevice);
					SetEvent(pDevice->hReadWriteEvent);
				}	
			}			
			else if((stat & IC_INTR_RX_UNDER  ) != 0)
			{
				pDevice->InterruptMask &= 0;
				i2cDisableInterrupts(pDevice);
				pDevice->status = TRANSFER_ABORTED;
				SetEvent(pDevice->hReadWriteEvent);
				DEBUGMSG(ZONE_IST,(TEXT("I2C:I2C_IST - IC_INTR_RX_UNDER.\r\n")));
			}
			else if((stat & IC_INTR_STOP_DET  ) != 0)
			{
				if(pDevice->Direction == TransferDirectionRead && pDevice->bIoComplete)
				{	
					if (I2C_REGISTER_READ(pDevice->pRegisters->IC_STATUS)& (IC_STATUS_RFNE |IC_STATUS_RFF))
					{
						//final transfer
						//Stop Bit being set by master,last transaction in operation
						TransferDataRead(pDevice);					
						pDevice->status = TRANSFER_SUCCESS;
					}
					else
						pDevice->status = TRANSFER_UNSUCCESSFUL;		
				}
				//last transaction - clear all 
				i2cDisableInterrupts(pDevice);
				pDevice->InterruptMask &= 0;
				SetEvent(pDevice->hReadWriteEvent);
						
			}
			else if((stat & IC_INTR_RX_FULL ) != 0)
			{
				if(!pDevice->bIoComplete)
				{
					while(i2cRxRemainingData(pDevice) != 0)
					{
						if (I2C_REGISTER_READ(pDevice->pRegisters->IC_STATUS)& (IC_STATUS_RFNE |IC_STATUS_RFF))
						{
							TransferDataRead(pDevice);
							pDevice->status = TRANSFER_SUCCESS; 
						}
						else {
							pDevice->status = TRANSFER_UNSUCCESSFUL;
							break;
						}						
					}//end of while loop
				 PrepareForRead(pDevice);
				}
			}
		}			
		InterruptDone(pDevice->dwSysIntr);
		
	}//end bKeepLooping

	i2cDisableInterrupts(pDevice);
	InterruptDisable (pDevice->dwSysIntr);
	CloseHandle(pDevice->hInterruptEvent);

	return (DWORD)bRet;

}


/*************************************************************************
 @func  HANDLE | I2C_Open | Open the I2C device

 @Description: This routine must be called by the user to open the
 I2C device. The HANDLE returned must be used by the application in
 all subsequent calls to the I2C driver. This routine initialize 
 the device's data
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/

DWORD I2C_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode)
{
    DWORD dwRet =(DWORD)NULL;

	I2C_DEVICE_CTXT *pDevice = (I2C_DEVICE_CTXT*)hOpenContext; 
	  
	if(pDevice == NULL){
        DEBUGMSG (ZONE_ERROR,(TEXT("I2C: I2C_Open() - Failed to open device %d\r\n"),GetLastError()));
        return dwRet;
    }

	dwRet = (DWORD)pDevice;

    return dwRet;
}

/*************************************************************************
 @func  HANDLE | i2cDisableController | Disable I2C controller

 @Description: This routine must disable all I2C controller
 
 @rdesc Returns TRUE on success, FALSE on failure
 *************************************************************************/
BOOL i2cDisableController(PDEVICE_CONTEXT pDevice)
{

	BOOL bRet = FALSE;
	int i;

	//IC_ENABLE - write 0 to disable controller
	I2C_REGISTER_WRITE(pDevice->pRegisters->IC_ENABLE,I2C_CONTROLLER_DISABLE);
		
    //  Wait for the I2C controller to be disabled 
	//IC_ENABLE_STATUS - (0- inactive, 1- active)
	bRet = I2C_REGISTER_READ(pDevice->pRegisters->IC_ENABLE_STATUS) & I2C_CONTROLLER_EN_STATUS;

	for ( i = 0; bRet && (I2C_MASTER_MAX_ATTEMPTS > i); i++)
    {
		//refer to DW_apb_i2c.pdf version 1.15a, section 3.8.3: Disabling DW_apb_i2c
		//Stall time of 25us (miliseconds - 0.25)
		 Sleep(1);
         bRet =I2C_REGISTER_READ(pDevice->pRegisters->IC_ENABLE_STATUS) & I2C_CONTROLLER_EN_STATUS;
    }

    if (bRet){
		DEBUGMSG(ZONE_ERROR,(TEXT("I2C:Error - Timeout while trying to disable I2C controller\n")));
        // Disable all interrupts
        pDevice->InterruptMask = I2C_DISABLE_ALL_INTERRUPTS;
	}
    
	return (!bRet);

}

/*************************************************************************
 @func  HANDLE | i2cEnableController | Enable I2C controller

 @Description: This routine enable all I2C controller
 
 @rdesc Returns TRUE on success, FALSE on failure
 *************************************************************************/
BOOL i2cEnableController(PDEVICE_CONTEXT pDevice)
{

	BOOL bRet = FALSE;
	int i;

	I2C_REGISTER_WRITE(pDevice->pRegisters->IC_ENABLE,I2C_CONTROLLER_ENABLE);


    //  Wait for the I2C controller to be enabled
    bRet = I2C_REGISTER_READ(pDevice->pRegisters->IC_ENABLE_STATUS) & I2C_CONTROLLER_EN_STATUS;

    for (i = 0; (bRet == FALSE) && (I2C_MASTER_MAX_ATTEMPTS > i); i++)
    {
        bRet = I2C_REGISTER_READ(pDevice->pRegisters->IC_ENABLE_STATUS) & I2C_CONTROLLER_EN_STATUS;
    }

    if (bRet == FALSE)
    {
		DEBUGMSG(ZONE_ERROR,(TEXT("I2C:Error - Timeout while trying to enable I2C controller\n")));
        // Disable all interrupts
        pDevice->InterruptMask = I2C_DISABLE_ALL_INTERRUPTS;
    }

	return bRet;

}

/*************************************************************************
 @func  HANDLE | i2cEnableInterrupts |Enable interrupt for I2C controller

 @Description: This routine enable interrupt register with 
 Interrupt mask for I2C controller

 *************************************************************************/

VOID i2cEnableInterrupts(PDEVICE_CONTEXT pDevice,ULONG InterruptMask)
{

	pDevice->InterruptMask = InterruptMask;
	//Set Enabled Interrupt
	pDevice->InterruptEnabled = TRUE;

	//Set interrupt Mask register to trigger interrupt
	I2C_REGISTER_WRITE(	pDevice->pRegisters->IC_INTR_MASK , (pDevice->InterruptMask & ~(ULONG)IC_INTR_LAST_BYTE));

} 


/*************************************************************************
 @func  HANDLE | SetRuntimeClock | Set Clock for I2C bus

 @Description: This routine set speed clock for I2C bus for standard 
 and fast mode

 *************************************************************************/
VOID SetRuntimeClock(PDEVICE_CONTEXT hOpenContext)
{
	
	I2C_DEVICE_CTXT *pDevice = (I2C_DEVICE_CTXT*)hOpenContext;

    if (pDevice->ConnectionSpeed == I2C_BUS_SPEED_400KHZ)
    {
           I2C_REGISTER_WRITE(pDevice->pRegisters->IC_FS_SCL_HCNT,pDevice->BusSetting.FS[I2C_BUS_SCL_HCNT]);
           I2C_REGISTER_WRITE(pDevice->pRegisters->IC_FS_SCL_LCNT,pDevice->BusSetting.FS[I2C_BUS_SCL_LCNT]);
           I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SDA_HOLD,pDevice->BusSetting.FS[I2C_BUS_SDA_HOLD]);
           I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SDA_SETUP,pDevice->BusSetting.FS[I2C_BUS_SDA_SETUP]);       
    }
    else if (pDevice->ConnectionSpeed == I2C_BUS_SPEED_100KHZ)
    {	
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SS_SCL_HCNT,pDevice->BusSetting.SS[I2C_BUS_SCL_HCNT]);
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SS_SCL_LCNT, pDevice->BusSetting.SS[I2C_BUS_SCL_LCNT]);
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SDA_HOLD,pDevice->BusSetting.SS[I2C_BUS_SDA_HOLD]);
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_SDA_SETUP,pDevice->BusSetting.SS[I2C_BUS_SDA_SETUP]);
    }

}

/*************************************************************************
 @func  HANDLE | PrepareForTransfer | Initialize and prepare for Transfer

 @Description: This routine set target add, adress mode, Bus speed and 
determine read or write operation 
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL PrepareForTransfer(PDEVICE_CONTEXT pDevice,PI2C_TRANS_BUFFER pTransmissionBuf)
{

	BOOL bRet = TRUE;
	DWORD DeviceAddr = 0;

	ULONG    interruptMask = IC_INTR_STOP_DET |
                             IC_INTR_TX_ABRT  |
                             IC_INTR_RX_OVER  |
                             IC_INTR_TX_EMPTY |
                             IC_INTR_LAST_BYTE;

	 
	// Target address
    pDevice->Address = pTransmissionBuf->Address;
    // Set the Address mode
    pDevice->AddressMode = pTransmissionBuf->AddressMode;

	if (  pDevice->AddressMode == AddressMode7Bit )
    {
        if ( pDevice->AddressMode > 0x7F )
        {
			DEBUGMSG (ZONE_ERROR,(TEXT("I2C:ControllerConfigureSettings  - Invalid 7 bit slave address 0X%x\r\n"),pDevice->AddressMode));
            pDevice->AddressMode = 0;
            bRet = FALSE;
            goto exit;
        }
    }
    else
    {
        if (  pDevice->AddressMode > 0x3FF )
        {
			DEBUGMSG (ZONE_ERROR,(TEXT("I2C:ControllerConfigureSettings  - Invalid 10 bit slave address  0X%x\r\n"),pDevice->AddressMode));
            pDevice->AddressMode = 0;
            bRet = FALSE;
            goto exit;
        }
    }
	// Set Bus speed
	pDevice->ConnectionSpeed = pTransmissionBuf->BusSpeed;

	//set up the buffer to hold bytes read/write
	pDevice->BufLength = pTransmissionBuf->DataLength;
	pDevice->BufferData = (PUCHAR) LocalAlloc(LPTR, pDevice->BufLength * sizeof(UCHAR));
	if(pDevice->BufferData == NULL)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("I2C:PrepareForTransfer - Failed allocate buffer.\r\n")));
		bRet = FALSE;
		goto exit;
	}

	memcpy(pDevice->BufferData, pTransmissionBuf->Data, pDevice->BufLength);

	// Set the TX FIFO threshold to zero(empty) so that an 
    // interrupt is generated anytime we can transmit 
	I2C_REGISTER_WRITE ( pDevice->pRegisters->IC_TX_TL,I2C_TX_THRESHOLD_ZERO);

	// Disable I2C controller to change the mode, device ID and bus speed
    // Disabling of I2C Controller will flush the TX & RX FIFOs
	bRet = i2cDisableController(pDevice);
	if(!bRet)
	{
		DEBUGMSG (ZONE_ERROR,(TEXT("I2C:i2cDisableController  - Unable to disable the controller!!\r\n")));
		goto exit;
	}

    //  Determine the type of slave device address that is being used
    //  7 or 10-bit and set the address
    DeviceAddr = pDevice->Address;

    if (pDevice->AddressMode == AddressMode10Bit)
        DeviceAddr |= IC_TAR_10BITADDR_MASTER;

    I2C_REGISTER_WRITE( pDevice->pRegisters->IC_TAR,DeviceAddr);

    //  Select device speed and master mode
    I2C_REGISTER_WRITE(pDevice->pRegisters->IC_CON,( ( (pDevice->ConnectionSpeed & 3) << 1 ) |IC_CON_MASTER_MODE));
   
    SetRuntimeClock(pDevice);

	//finally enable the controller after all configuration settings
	bRet = i2cEnableController(pDevice);
	if(!bRet)
	{
		DEBUGMSG (ZONE_ERROR,(TEXT("I2C:i2cEnableController  - Unable to enable the controller!!\r\n")));
		goto exit;
	}

    //reset all the status count before read/write operation
	pDevice->RxRequestLength = 0;
	pDevice->bIoComplete = FALSE;
	pDevice->RxTransferredLength = 0;
	pDevice->TxTransferredLength = 0;
	pDevice->status = TRANSFER_UNSUCCESSFUL; 

	if(pTransmissionBuf->Direction == TransferDirectionRead)
	{
		//set the direction read/write
		pDevice->Direction = TransferDirectionRead;
		PrepareForRead(pDevice);
	}
	else if(pTransmissionBuf->Direction == TransferDirectionWrite)
	{
		//set direction read/write
		pDevice->Direction = TransferDirectionWrite;

		//set interrupt to trigger write operation
		i2cEnableInterrupts(pDevice,interruptMask);
	}
	
exit:
	return bRet;


}
/*************************************************************************
 @func  HANDLE | PrepareForRead | Read data from Fifo

 @Description: This routine execute read operation from FIFO
 
 *************************************************************************/

VOID PrepareForRead(PDEVICE_CONTEXT pDevice)
{

	DWORD index         = pDevice->RxRequestLength;
    DWORD requestLength = pDevice->BufLength;
	DWORD StopInFifo	= 0;
	ULONG data32		= 0;

    ULONG    interruptMask = IC_INTR_STOP_DET |
                             IC_INTR_TX_ABRT  |
                             IC_INTR_RX_OVER  |
                             IC_INTR_RX_UNDER;
                             
    ULONG    txFifoFillRq  = 0;                 // one byte could be still processed
    ULONG    rxFifoSpace   = I2C_RX_FIFO_SIZE - I2C_REGISTER_READ(pDevice->pRegisters->IC_RXFLR) - 1;
    ULONG    txFifoSpace   = I2C_TX_FIFO_SIZE - I2C_REGISTER_READ(pDevice->pRegisters->IC_TXFLR);

	// Disable serial clock (this is WA for abort during fifo fill)   
    I2C_REGISTER_AND_SET_OP(pDevice->pRegisters->PRV_CLOCKS, ~(ULONG)PRV_CLOCKS_CLK_EN);        

    // indicate there was no stop put in fifo
    StopInFifo = 0;

    // Perform write.
    // Determine if another request will fit into the TX FIFO 
    // and received data will fit into RX FIFO
	while(index < requestLength && txFifoSpace && rxFifoSpace)
    {
        //  Place the next read request into the FIFO
		
        data32 = IC_DATA_CMD_READ;

        // This is a first byte of continued transfer in sequence
        if (index == 0)
            data32 |= IC_DATA_CMD_RESTART;

        // Single or last item in sequence need stop bit to be set
        if ((requestLength - index - 1 == 0))
        {
            data32 |= IC_DATA_CMD_STOP;
            StopInFifo = 1;
            interruptMask &= ~(IC_INTR_TX_EMPTY);

            // This is 1 byte request
            if (requestLength == 1)
                data32 |= IC_DATA_CMD_RESTART;

			pDevice->bIoComplete = TRUE;
        }

		//set bit 8 to zero for read operation
		I2C_REGISTER_WRITE( pDevice->pRegisters->IC_DATA_CMD,data32);

        txFifoFillRq++;
        index++;
		pDevice->RxRequestLength++;
        rxFifoSpace--;
        txFifoSpace--;
    }
    // Enable serial clock to start data reception
    I2C_REGISTER_OR_SET_OP(pDevice->pRegisters->PRV_CLOCKS, PRV_CLOCKS_CLK_EN);

    // If there was no stop set - we are waiting for rx full interrupt
    // else we are waiting for stop to collect rest of data

   if (StopInFifo == 0){
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_RX_TL ,(txFifoFillRq > 1 ? txFifoFillRq - 1 : 0));
        interruptMask |=IC_INTR_RX_FULL;
    }
 
     //set interrupt to trigger write operation
	i2cEnableInterrupts(pDevice,interruptMask);
} 

/*************************************************************************
 @func  HANDLE | TransferDataRead | Read operation 

 @Description: This routine read data from FIFO to buffer

 *************************************************************************/
VOID TransferDataRead(PDEVICE_CONTEXT pDevice)
{
	DWORD index = 0;
    DWORD requestLength = 0;
    ULONG rxFifoBytes = 0;

    // Perform read.
    // Determine if another byte is available in the RX FIFO
	index         = pDevice->RxTransferredLength;
	requestLength = pDevice->BufLength;
	rxFifoBytes   = I2C_REGISTER_READ(pDevice->pRegisters->IC_RXFLR);

	while(index < requestLength && rxFifoBytes)
	{
		//  Retrieve the data from the receive buffer
		UINT8 data = 0;

		data = (UCHAR)I2C_REGISTER_READ(pDevice->pRegisters->IC_DATA_CMD);

		//transfer from FIFO to buffer
		*((pDevice->BufferData)+ index) = data;
		pDevice->RxTransferredLength++;
		index++;
		rxFifoBytes--;
	}
}   

/*************************************************************************
 @func  HANDLE | i2cDisableInterrupts | Disable interrupts

 @Description: This routine disable interrupt for I2c controller
 
 *************************************************************************/
VOID i2cDisableInterrupts(PDEVICE_CONTEXT pDevice)
{
    // Disable all interrupts.
	I2C_REGISTER_WRITE(pDevice->pRegisters->IC_INTR_MASK,I2C_DISABLE_ALL_INTERRUPTS);

	pDevice->InterruptEnabled = FALSE;
 
}

/*************************************************************************
 @func  HANDLE | i2cAcknowledgeInterrupts | Clear all interrupt bits

 @Description: This routine responsible to clear all interrupt bit by
 reading those registers

 @rdesc Returns interrupt status
 *************************************************************************/
ULONG i2cAcknowledgeInterrupts(PDEVICE_CONTEXT pDevice, ULONG InterruptMask)
{
    ULONG   RawInterruptStatus;

    // Acknowledge requested interrupts.It also clear all those interrupts 
	RawInterruptStatus = I2C_REGISTER_READ(pDevice->pRegisters->IC_RAW_INTR_STAT) & InterruptMask;

    if (RawInterruptStatus & IC_INTR_RX_UNDER)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_RX_UNDER);       

    if (RawInterruptStatus & IC_INTR_RX_OVER)
        I2C_REGISTER_READ( pDevice->pRegisters->IC_CLR_RX_OVER);       

    if (RawInterruptStatus & IC_INTR_RD_REQ)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_RD_REQ);     

    if (RawInterruptStatus & IC_INTR_TX_ABRT)
		I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_TX_ABRT);		     

    if (RawInterruptStatus & IC_INTR_RX_DONE)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_RX_DONE);     

    if (RawInterruptStatus & IC_INTR_ACTIVITY)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_ACTIVITY);      

    if (RawInterruptStatus & IC_INTR_STOP_DET)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_STOP_DET);

    if (RawInterruptStatus & IC_INTR_START_DET)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_START_DET);

    if (RawInterruptStatus & IC_INTR_GEN_CALL)
        I2C_REGISTER_READ(pDevice->pRegisters->IC_CLR_GEN_CALL);

	return RawInterruptStatus;
}

/*************************************************************************
 @func  HANDLE | TransferDataWrite | Execute Write Operation

 @Description: This routine execute write operation from buffer to FIFO

 *************************************************************************/
VOID TransferDataWrite(PDEVICE_CONTEXT pDevice)
{
	DWORD   index		= pDevice->TxTransferredLength;
    ULONG   Length		= pDevice->BufLength;
    UCHAR	data		= 0;
    ULONG	data32		= 0;
	
    ULONG    txFifoSpace   = I2C_TX_FIFO_SIZE - I2C_REGISTER_READ(pDevice->pRegisters->IC_TXFLR);

    ULONG    interruptMask = IC_INTR_STOP_DET |
                             IC_INTR_TX_ABRT  |
                             IC_INTR_RX_OVER  |
                             IC_INTR_TX_EMPTY |
                             IC_INTR_LAST_BYTE;

	// Disable serial clock (this is WA for abort during fifo fill)   
    I2C_REGISTER_AND_SET_OP(pDevice->pRegisters->PRV_CLOCKS, ~(ULONG)PRV_CLOCKS_CLK_EN);     

    // Perform write.
    // Determine if another byte will fit into the transmit FIFO

   while(index < Length && txFifoSpace )
    {
        //Place the next transmit byte into the FIFO
        data   = 0;
        data32 = 0;


		data =(UCHAR) *((pDevice->BufferData)+ index);
        data32 = (ULONG)data;

        // This is a first byte of continued transfer in sequence
        if (index == 0)
            data32 |= IC_DATA_CMD_RESTART;			

        // Single or last item in sequence need stop bit to be set
        if (Length - index - 1  == 0)
        {
            data32 |= IC_DATA_CMD_STOP;

            // This is 1 byte request
            if (Length == 1)
                data32 |= IC_DATA_CMD_RESTART;

		   pDevice->bIoComplete = TRUE;

        }
				
        I2C_REGISTER_WRITE(pDevice->pRegisters->IC_DATA_CMD,data32);
		pDevice->TxTransferredLength++;
        index++;
        txFifoSpace--;

    }// end while loop

	// Enable serial clock to start data reception
    I2C_REGISTER_OR_SET_OP(pDevice->pRegisters->PRV_CLOCKS, PRV_CLOCKS_CLK_EN);
    
    i2cEnableInterrupts(pDevice,interruptMask);
    
}

/*************************************************************************
  @func BOOL        | I2C_IOControl | Device IO control routine
 @parm pI2Chandle   | hOpenContext  | I2C handle
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
BOOL I2C_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn, 
                   PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
	
	PPOWER_CAPABILITIES pPowerCaps = NULL;
	I2C_DEVICE_CTXT *pDevice = (I2C_DEVICE_CTXT*)hOpenContext;
	PI2C_TRANS_BUFFER i2cTransfer = NULL;
	
	if(NULL == pDevice){
		DEBUGMSG (ZONE_ERROR,(TEXT("I2C:I2C_IOControl - Failed to open device %d\r\n"),GetLastError()));
        return FALSE;  
	}

	switch (dwCode) {
		case IOCTL_I2C_EXECUTE_WRITE:
			if (PrepareForTransfer(pDevice,(PI2C_TRANS_BUFFER)pBufIn))
					WaitForSingleObject(pDevice->hReadWriteEvent, INFINITE);
			else 
				pDevice->status =TRANSFER_UNSUCCESSFUL;

			if (pDevice->BufferData != NULL){
					  LocalFree(pDevice->BufferData);
					  pDevice->BufferData = NULL;
			}
			break;
		case IOCTL_I2C_EXECUTE_READ:
			i2cTransfer =(PI2C_TRANS_BUFFER)pBufIn;
			if (PrepareForTransfer(pDevice,i2cTransfer))
			{
				if(WaitForSingleObject(pDevice->hReadWriteEvent, INFINITE)==WAIT_OBJECT_0)
				{
					if(pDevice->status == TRANSFER_SUCCESS)
						memcpy(pBufOut, pDevice->BufferData, pDevice->BufLength);													
				}
			}						
			else 
				pDevice->status =TRANSFER_UNSUCCESSFUL;

			if (pDevice->BufferData != NULL){
				LocalFree(pDevice->BufferData);
				pDevice->BufferData = NULL;
			}
			break;
		case IOCTL_POWER_CAPABILITIES:
			pPowerCaps = (PPOWER_CAPABILITIES)pBufOut;     
			memset(pPowerCaps, 0, sizeof(POWER_CAPABILITIES));

              // set full powered and non powered state
            pPowerCaps->DeviceDx = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
            pPowerCaps->WakeFromDx = DX_MASK(D0) | DX_MASK(D3);
			*pdwActualOut = sizeof(POWER_CAPABILITIES);
			break;
		default:
			DEBUGMSG(ZONE_ERROR,(TEXT("I2C: I2C_IOControl()  Invalid index: %d\n"),dwCode));
			break;
	}

    return pDevice->status;
} 
/*************************************************************************
 @func BOOL         | I2C_Close    | close the I2C device.
 @parm I2C_handle	| hOpenContext | Context pointer returned from I2C_Open
 @rdesc TRUE if success; FALSE if failure
 @remark This routine is called by the device manager to close the device.
 *************************************************************************/
BOOL I2C_Close(DWORD hOpenContext)
{
    BOOL bRet = TRUE;

	I2C_DEVICE_CTXT *pDevice = (I2C_DEVICE_CTXT*)hOpenContext;

	//check for correct device context
	if(pDevice == NULL){
		DEBUGMSG (ZONE_ERROR,(TEXT("I2C:I2C_Close - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;
	}
    return bRet;
} 
/*************************************************************************
 @func  BOOL | I2C_Deinit | De-initialize I2C
 @parm 	I2C_handle | hDeviceContext | Context pointer returned from I2C_Init
 @rdesc None.
 *************************************************************************/
BOOL I2C_Deinit(DWORD hDeviceContext)
{
    BOOL bRet = TRUE;
	
	/* Unallocate the memory*/
    I2C_DEVICE_CTXT *pDevice = (I2C_DEVICE_CTXT*)hDeviceContext;

	//check for correct device context
	if(pDevice == NULL){
		DEBUGMSG (ZONE_ERROR,(TEXT("I2C:I2C_Deinit - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;
	}

	//uninitialize the IST loop
	pDevice->ISTLoopTrack = FALSE;
	 /* close interrupt thread */
    if(pDevice->hInterruptThread != NULL) {
        CloseHandle(pDevice->hInterruptThread);    /* close handle */
    }
	/* close interrupt handler */
    if(pDevice->hInterruptEvent != NULL) { CloseHandle(pDevice->hInterruptEvent); }
	if(pDevice->hIsrHandler != NULL){FreeIntChainHandler(pDevice->hIsrHandler);}
    


	 /* disable interrupt */
    if(pDevice->dwSysIntr != 0) {
        InterruptDisable(pDevice->dwSysIntr);
        KernelIoControl(IOCTL_HAL_RELEASE_SYSINTR, &pDevice->dwSysIntr, 
                                    sizeof(pDevice->dwSysIntr), NULL, 0, NULL);
    }

	if(pDevice->hReadWriteEvent != NULL) { CloseHandle(pDevice->hReadWriteEvent); }
	if(pDevice->hBusAccess != NULL) { CloseBusAccessHandle(pDevice->hBusAccess); }
			

	if(pDevice->hKey!= NULL) { RegCloseKey(pDevice->hKey); }

	
     /* Free device structure */
	LocalFree(pDevice); 
	 
	return bRet;
} 



