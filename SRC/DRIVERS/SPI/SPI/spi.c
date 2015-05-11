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


//Module Name: SPI.C

/*
*	**********************************************************************
*	Intel's SPI Driver
*	**********************************************************************
*/

#include <windows.h>
#include <nkintr.h>
#include <Winbase.h>
#include <devload.h>    /* Registry API's */
#include <pm.h>
#include "spi.h"
#include <giisr.h>
#include <memory.h>


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
            DEBUGMSG (TRUE,(TEXT("SPI DLL_PROCESS_ATTACH - Process attached. \r\n")));
            break;
        case DLL_PROCESS_DETACH:
            DEBUGMSG (TRUE,(TEXT("SPI DLL_PROCESS_DETACH - Process dettached. \r\n")));
            break;
        default:
            break;
    }/* end switch */
    
    return TRUE;
}

/*************************************************************************

 @func  HANDLE | SPI_Init | Initialize SPI device.
 @parm  PCTSTR  | Identifier | Device identifier. The device loader
                    passes in the registry key that contains information
                    about the active device.

 @remark This routine is called at device load time in order to perform 
         any initialization. Typically the init routine does as little as 
         possible,Create a memory space to store the device information.

 @Return Value	Returns a pointer which is passed into the SPI_Open and 
         SPI_Deinit entry points as a device handle.
 *************************************************************************/
DWORD SPI_Init( LPCTSTR pContext, LPCVOID lpvBusContext )
{
	SPI_DEVICE_CTXT *pDevice = NULL;

	DWORD bRet = (DWORD)NULL;

	DDKWINDOWINFO dwi;
	DDKPCIINFO pcii;
	DDKISRINFO isri;
	GIISR_INFO Info;

	DWORD dwStatus = 0;
	DWORD MMIOSpace = 0;
	LPVOID dwPhysAddr;


	// Create device structure and allocating memory 
	pDevice = (SPI_DEVICE_CTXT*)LocalAlloc(LPTR, sizeof(SPI_DEVICE_CTXT));


	if(!pDevice)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("SPI:SPI_INIT - Failed allocate SPI controller structure.\r\n")));
		goto exit;
	}

	// Open the Active key for the driver.
    pDevice->hKey = OpenDeviceKey(pContext);
    if(pDevice->hKey == INVALID_HANDLE_VALUE) 
	{
        DEBUGMSG(ZONE_ERROR,(TEXT("SPI - Failed to open device key path.\r\n"),GetLastError()));
		pDevice->hKey = NULL;
		goto exit;
    }
	

	// read window information
	dwi.cbSize = sizeof(dwi);
	dwStatus = DDKReg_GetWindowInfo(pDevice->hKey,&dwi);
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("SPI: DDKReg_GetWindowInfo() failed %d\r\n"), dwStatus));
		goto exit;
	}

	pDevice->dwBusNumber = dwi.dwBusNumber;
	pDevice->dwInterfaceType = dwi.dwInterfaceType;

	
	// memory space
	if(dwi.dwNumMemWindows  != 0)
	{
		 pDevice->phyAddress.LowPart = dwi.memWindows[0].dwBase;
		 pDevice->dwMemLen = dwi.memWindows[0].dwLen;
		 MMIOSpace = (DWORD)FALSE;

	}
	else
	{
	   DEBUGMSG(ZONE_ERROR, (TEXT("SPI: Failed to get physical address of the device.\r\n")));
	   goto exit;	
	}
	
	 // creates a handle that can be used for accessing a bus
	pDevice->hSpiHandler = CreateBusAccessHandle(pContext);


	// read PCI id
	pcii.cbSize = sizeof(pcii);
	dwStatus = DDKReg_GetPciInfo(pDevice->hKey, &pcii);
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("SPI: DDKReg_GetPciInfo() failed %d\r\n"), dwStatus));
		goto exit;
	}

	pDevice->dwDeviceId = pcii.idVals[PCIID_DEVICEID];


	// read ISR information
	isri.cbSize = sizeof(isri);
	dwStatus = DDKReg_GetIsrInfo(pDevice->hKey, &isri);
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("SPI: DDKReg_GetIsrInfo() failed %d\r\n"), dwStatus));
		goto exit;
	}
	

	// sanity check return values
	if(isri.dwSysintr == SYSINTR_NOP) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("SPI: no sysintr specified in registry\r\n")));
		goto exit;
	}
	if(isri.szIsrDll[0] != 0) 
	{
		if(isri.szIsrHandler[0] == 0 || isri.dwIrq == IRQ_UNSPECIFIED) 
		{
			DEBUGMSG(ZONE_ERROR, (_T("SPI: invalid installable ISR information in registry\r\n")));
			goto exit;
		}
	}

	pDevice->dwSysIntr = isri.dwSysintr;
	pDevice->dwIrq = isri.dwIrq;


	// Install ISR handler if there is one
    if(isri.szIsrDll[0] != 0) 
    {
        // Install ISR handler
        pDevice->hIsrHandler = LoadIntChainHandler(isri.szIsrDll, isri.szIsrHandler, (BYTE) isri.dwIrq);
        if(pDevice->hIsrHandler == NULL) 
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("SPI: Couldn't install ISR handler\r\n")));
			goto exit;
        } 
		else {
			// translate the target bus address to a virtual address
           	if(!BusTransBusAddrToStatic(pDevice->hSpiHandler,(INTERFACE_TYPE)pDevice->dwInterfaceType,pDevice->dwBusNumber,
									     pDevice->phyAddress, pDevice->dwMemLen, &MMIOSpace, &dwPhysAddr)) 
			{
                DEBUGMSG(ZONE_ERROR, (TEXT("SPI: Failed BusTransBusAddrToStatic\r\n")));
				goto exit;
            }

			   // Get the register base for the controller.
				pDevice->pRegisters = (PSPI_REGISTERS)dwPhysAddr;
				pDevice->dwMemBaseAddr = (DWORD)pDevice->pRegisters;

		     DEBUGMSG(ZONE_INIT, (TEXT("SPI: Installed ISR handler, Dll = '%S', Handler = '%S',Device Id = 0x%x, PhysAddr = 0x%x, Sysintr = 0x%x\r\n"),
				 isri.szIsrDll, isri.szIsrHandler,pDevice->dwDeviceId, pDevice->phyAddress,pDevice->dwSysIntr));
            
		   // Set up ISR handler
		    Info.SysIntr    = pDevice->dwSysIntr;
			Info.CheckPort  = TRUE;
			Info.PortIsIO   = FALSE;
			Info.UseMaskReg = FALSE;
			Info.PortAddr   = (DWORD)dwPhysAddr;
			Info.PortSize   = sizeof(DWORD);
			Info.Mask  = SPI_INTERRUPT_STATUS_MASK;

	            
			if(!KernelLibIoControl(pDevice->hIsrHandler, IOCTL_GIISR_INFO, &Info, sizeof(Info), NULL, 0, NULL)) 
			{
				  DEBUGMSG(ZONE_ERROR, (TEXT("SPI: KernelLibIoControl call failed.\r\n")));
				  goto exit;
			}

		}
	}

	 /* create event for handling hardware interrupts */
    pDevice->hInterruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(pDevice->hInterruptEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("SPI: SPI_Init: Failed create interrupt "\
                                                      L"event\r\n")));
        goto exit;
    }

	 /* create event for completion read/write operation */
    pDevice->hWriteReadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(pDevice->hWriteReadEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("SPI: SPI_Init: Failed create interrupt "\
                                                      L"event\r\n")));
        goto exit;
    }

	pDevice->hInterruptThread = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE)SPI_IST, (LPVOID)pDevice, CREATE_SUSPENDED, NULL);
    if(pDevice->hInterruptThread == NULL) {
        DEBUGMSG(ZONE_ERROR,(_T("ERROR: SPI_Init - Failed to create interrupt "\
                                                     L"thread\r\n")));
        goto exit;
    }

    /* initialize interrupt sysintr to event mapping  */
   if(!InterruptInitialize(pDevice->dwSysIntr,pDevice->hInterruptEvent, NULL, 0)) {
        DEBUGMSG(ZONE_ERROR, (_T("SPI: SPI_Init - Failed to initialize "\
                                                       L"interrupt\r\n")));
        goto exit;
	}

    // Initialize controller specific settings.
	pDevice->SrcClockFreq = DEFAULT_SRC_CLK_FREQ;

	//Initialize the looping in IST
	pDevice->ISTLoopTrack = TRUE;
	
	//driver default status 
	pDevice->Status = TRANSFER_UNSUCCESSFUL;  

	InitSpiController(pDevice);

    //Initialize DMA specific settings.
    DmaInitSettings(pDevice);

    ResumeThread(pDevice->hInterruptThread);

	/* Return non-null value */
    return (DWORD)pDevice;

exit:
	SPI_Deinit((DWORD)pDevice);
	return bRet;

}

/*************************************************************************
 @func  HANDLE | DmaInitSettings | Set variable for DMA use

 @Description: This routine set the default length where DMA will be in used
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/

VOID DmaInitSettings(PDEVICE_CONTEXT pDevice)
{
	//set default length to 16
	pDevice->ForceDma = DEFAULT_DMA_LENGTH;
	pDevice->UseDma = FALSE;


}

/*************************************************************************
 @func  HANDLE | DmaPreExecute | Disable DMA handshake and set threshold

 @Description: This routine disable DMA handshake and set SPI threshold 
 before DMA operation take place

 @rdesc This routine returns status representing the device.
 *************************************************************************/
VOID DmaPreExecute(PDEVICE_CONTEXT pDevice)
{

    // Disable DMA handshake
    SPI_TXRX_DMA_DISABLE(pDevice);

    // Set transmit FIFO watermarks
    SET_TRANSMIT_THRESHOLD(pDevice, 1, 200);
    SET_RECEIVE_THRESHOLD(pDevice, 0);

} 

/*************************************************************************
 @func  HANDLE | InitDMA | Initialize DMA for transaction

 @Description: This routine allocate common buffer and DMA notification event

 @rdesc This routine returns TRUE if successfully allocated common buffer 
 and event.Return FALSE if unsuccessful to allocate buffer and event.
 *************************************************************************/

BOOL InitDMA(PDEVICE_CONTEXT pDevice)
{
	BOOL bRet= TRUE;
	/*By Default set to zero to indicate amount of multi transfer needed*/
	pDevice->TotalDMATransRequested = 0;

	pDevice->DMABytesReturned = (DWORD)-1;
	//init dma - init common buffer
	pDevice->AdapterObject.ObjectSize = sizeof(pDevice->AdapterObject);
	pDevice->AdapterObject.InterfaceType = pDevice->dwInterfaceType;
	pDevice->AdapterObject.BusNumber = pDevice->dwBusNumber;


	//Create Common buffer for DMA transfer
	pDevice->VirtualAddress  = HalAllocateCommonBuffer(&(pDevice->AdapterObject),DEFAULT_DMA_BUFFER_SIZE ,(PPHYSICAL_ADDRESS)&pDevice->MemPhysicalAddress, FALSE);
	if(!pDevice->VirtualAddress)
	{
		DEBUGMSG(ZONE_ERROR, (_T("SPI: SPI_Init: Failed create Common "\
                                                      L"buffer for DMA.\r\n")));
	   pDevice->Status	= TRANSFER_UNSUCCESSFUL;
	   bRet = FALSE;	   
	}

	/*Create Event Handling for DMA completion notification*/
	pDevice->hDmaCompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(pDevice->hDmaCompletionEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("SPI: SPI_Init: Failed create DMA "\
                                                      L"event\r\n")));
		pDevice->Status	= TRANSFER_UNSUCCESSFUL;
        bRet = FALSE;
    }
	
	/*Count number of DMA transfer needed base on buffer length*/
	/*pDevice->TotalDMATransRequested will be zero if less to DEFAULT_DMA_BUFFER_SIZE */
	if(pDevice->BufLength >= DEFAULT_DMA_BUFFER_SIZE)
	{
		pDevice->TotalDMATransRequested = pDevice->BufLength/DEFAULT_DMA_BUFFER_SIZE;

	}

	return bRet;	
}
/*************************************************************************
 @func  HANDLE | DeinitDMA | De initialize DMA transaction

 @Description: This routine deallocate coomon buffer and close all handle 
 required for DMA handling.

 @rdesc This routine has no return value.
 *************************************************************************/
VOID DeinitDMA(PDEVICE_CONTEXT pDevice)
{
	// Release DMA Resources
	if(pDevice->VirtualAddress)
	{
		//Deinit common buffer allocation
		HalFreeCommonBuffer(&pDevice->AdapterObject,DEFAULT_DMA_BUFFER_SIZE,pDevice->MemPhysicalAddress,(PVOID)pDevice->VirtualAddress, FALSE);
		pDevice->VirtualAddress = NULL;
	}

	//Close DMA Completion Event Handling
	CloseHandle(pDevice->hDmaCompletionEvent);
	pDevice->hDmaCompletionEvent = NULL;
	CloseHandle(pDevice->hDmaHandler);
	pDevice->hDmaHandler = NULL;
	

}
/*************************************************************************
 @func  HANDLE | ProgramExecuteDmaTransaction | Setup DMA Request context

 @Description: This routine set up DMA request context.
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL ProgramExecuteDmaTransaction(PDEVICE_CONTEXT pDevice)  
{
    BOOL status  = FALSE;
	DWORD offset = 0;
	DWORD TotalTransferLength = 0;

	//DMA request struct
	DMA_REQUEST_CONTEXT DmaRequest = {0}; 	

	//Fix DMA request Line for SPI here
    DmaRequest.RequestLineTx = 0;
    DmaRequest.RequestLineRx= 1;

	//Fix the channel 0000 - SPI Tx, 0001 - SPI Rx
	DmaRequest.ChannelTx = LPSSDMA_CHANNEL0;
    DmaRequest.ChannelRx = LPSSDMA_CHANNEL1; 

	//Tx Source System Memory Address
	DmaRequest.SysMemPhysAddress = pDevice->MemPhysicalAddress;
	DmaRequest.VirtualAdd = pDevice->VirtualAddress;

	//Tx Destination Address
	DmaRequest.PeripheralFIFOPhysicalAddress = (ULONG)(pDevice->phyAddress.LowPart + offsetof(SPI_REGISTERS,SSDR));

    DmaRequest.DummyDataWidthInByte = 1;
    DmaRequest.DummyDataForI2CSPI = 0xFF;
   
	/* Event for completion DMA operation */
    DmaRequest.hDmaCompleteEvent = pDevice->hDmaCompletionEvent;

	DmaRequest.TransactionType = TRANSACTION_TYPE_SINGLE_TXRX;


	DmaRequest.pBytesReturned = &pDevice->DMABytesReturned;

    DmaRequest.IntervalTimeOutInMs = 0;
	DmaRequest.TotalTimeOutInMs = DEFAULT_TOTAL_TIMEOUT_IN_MS;

	/*If transfer length more than 4092, multi transfer is being done here */
	while(pDevice->TotalDMATransRequested && offset < pDevice->TotalDMATransRequested)
	{
		DmaRequest.TransactionSizeInByte = DEFAULT_DMA_BUFFER_SIZE;
		TotalTransferLength += DmaRequest.TransactionSizeInByte;
		status = StartExecuteDmaTransaction(pDevice,DmaRequest,offset); 
		offset++;

	}
	//Last DMA transaction or transfer less or equal 4092
	if((pDevice->BufLength - TotalTransferLength < DEFAULT_DMA_BUFFER_SIZE) && (pDevice->BufLength - TotalTransferLength != 0))
	{
		DmaRequest.TransactionSizeInByte = pDevice->BufLength - TotalTransferLength;
		status = StartExecuteDmaTransaction(pDevice,DmaRequest,offset);
		

	}
		
    return status;
}

/*************************************************************************
 @func  HANDLE | StartExecuteDmaTransaction | Start DMA execution
 set threshold

 @Description: This routine call IOControl to initate DMA operation. 

 @rdesc This routine returns TRANSFER_SUCCESS status if successfully finish 
 DMA  transaction.
 *************************************************************************/
BOOL StartExecuteDmaTransaction(PDEVICE_CONTEXT pDevice,DMA_REQUEST_CONTEXT DmaRequest,DWORD offset)
{	
	ULONG IoctlCode = 0;
	BOOL  result  = FALSE;
	ULONG BytesHandled;
	DWORD index = 0;
	
	DWORD TransferLength = DmaRequest.TransactionSizeInByte;

	//Copy data to Virtual memory for DMA Transaction
	if(pDevice->Direction == TransferDirectionWrite){
		for(index = 0; index < TransferLength; index++)
		{
			*((pDevice->VirtualAddress) + index) = (UCHAR) *((pDevice->pBuffer)+ index +(offset*DEFAULT_DMA_BUFFER_SIZE));
		}
	}
	if(pDevice->Direction == TransferDirectionRead){

		for(index = 0; index < TransferLength; index++)
		{
				*((pDevice->VirtualAddress) + index) = 0xFF;
		}
	}

    DmaPreExecute(pDevice);

	//set IOCTL base of read/write operation
	
    if (pDevice->Direction == TransferDirectionWrite)
    {
        SPI_TXRX_DMA_ENABLE(pDevice);
        IoctlCode = IOCTL_LPSSDMA_REQUEST_DMA_SPI_TX;
	}
    else if (pDevice->Direction == TransferDirectionRead)
    {           
        SPI_TXRX_DMA_ENABLE(pDevice);
        IoctlCode = IOCTL_LPSSDMA_REQUEST_DMA_SPI_RX;
    }
	
	if(pDevice->hDmaHandler != NULL)
	{
		result = (BOOL)DeviceIoControl(pDevice->hDmaHandler,
					 (DWORD)IoctlCode,
					 (LPVOID)&DmaRequest,
					 sizeof(DmaRequest),
					 NULL,
					 0,
					 &BytesHandled,
					 NULL);
	}
	
	if(WaitForSingleObject(DmaRequest.hDmaCompleteEvent,INFINITE) == WAIT_OBJECT_0)
	{
		memcpy_s(pDevice->pBuffer + (offset*DEFAULT_DMA_BUFFER_SIZE),TransferLength,pDevice->VirtualAddress,TransferLength);
		pDevice->Status	= TRANSFER_SUCCESS;	
	}	
	else
		pDevice->Status	= TRANSFER_UNSUCCESSFUL;	

	return pDevice->Status;
}
/*************************************************************************
 @func  HANDLE | GetDmaAdapterHandle | Get DMA handler

 @Description: This routine calls CreateFile to get DMA handle

 @rdesc This routine return handle if successful else return NULL.
 *************************************************************************/
HANDLE GetDmaAdapterHandle(PDEVICE_CONTEXT pDevice)
{

	HANDLE hDma = NULL;
   
	// Get a handle of DMA driver. Which causes the driver's XXX_Open function to be called
   hDma = CreateFile(DMA_DEVICE_NAME, 
          GENERIC_READ|GENERIC_WRITE,       
          FILE_SHARE_READ|FILE_SHARE_WRITE,
          NULL,
          OPEN_EXISTING,
          0,
          NULL
          );

   if (hDma == INVALID_HANDLE_VALUE) 
   { 
		DEBUGMSG(ZONE_ERROR,(TEXT("SPI:Failed to get DMA handler. %d\r\n"),GetLastError()));
		hDma= NULL;

	}
       
	return hDma;
}


/*************************************************************************
 @func  HANDLE | InitSpiController | Initialize SPI Controller

 @Description: This routine enable SW CS mode, assert resets and enable clock 
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL InitSpiController(PDEVICE_CONTEXT pDevice)
{

	BOOL bRet = TRUE;
	
    // Start initializing controller hardware

    // Assert resets
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->PRV_RESETS, ~(ULONG)(PRV_RESETS_NOT_FUNC_RST | PRV_RESETS_NOT_APB_RST));

    // Release resets
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->PRV_RESETS, PRV_RESETS_NOT_FUNC_RST | PRV_RESETS_NOT_APB_RST);

    // Enable Clocks & Set Clocks ratio
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->PRV_CLOCKS, 0);

    
    WRITE_REGISTER_ULONG(&pDevice->pRegisters->PRV_CLOCKS, PRV_CLOCKS_CLK_EN | (PRV_CLOCKS_M_VAL_DEFAULT << PRV_CLOCKS_M_VAL_SHIFT) | (PRV_CLOCKS_N_VAL_DEFAULT << PRV_CLOCKS_N_VAL_SHIFT) | PRV_CLOCKS_UPDATE);
    CLR_REGISTER_BITS(pDevice->pRegisters->PRV_CLOCKS, PRV_CLOCKS_UPDATE);

    // Enable SW CS mode - don't touch CS state - we don't know what is required for device
    SET_REGISTER_BITS(pDevice->pRegisters->PRV_CS_CTRL, PRV_CS_CTRL_MODE_SW);

    // Disable Controller
    CLR_REGISTER_BITS(pDevice->pRegisters->SSCR0, SSCR0_SSE_EN);


	return bRet;

}

/*************************************************************************
 @func  HANDLE | DeinitSpiController | DeInitialize spi controller

 @Description: Clear all interrupt and disable controller 
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL DeinitSpiController(PDEVICE_CONTEXT pDevice)
{

	spiDisableInterrupts(pDevice);

	// Disable controller to reset FIFO
    CLR_REGISTER_BITS(pDevice->pRegisters->SSCR0, SSCR0_SSE_EN);

	return TRUE;

}
/*************************************************************************
 @func  HANDLE | CheckControllerError | Check spi controller error 

 @Description: Check the ROR status from SSSR.  
 
 @rdesc Returns TRUE on no error, FALSE on error. 
 *************************************************************************/
BOOL CheckControllerError(PDEVICE_CONTEXT pDevice)
{
	DWORD stat_SSSR = 0;
	
	stat_SSSR  = SPI_REGISTER_READ(pDevice->pRegisters->SSSR);
	
	if (stat_SSSR & SSSR_ROR)
		{
			DEBUGMSG(ZONE_ERROR,(TEXT("SPI:SPI_IST - Receive FIFO overun.\r\n")));
			return FALSE;
		}
		
  return TRUE;
			
}
/*************************************************************************
 @func  HANDLE | SPI_IST | Open the SPI device

 @Description: This routine must be called when there hardware 
 interrupt signal and it will service the request. Return complete to ISR
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/
DWORD SPI_IST(PDEVICE_CONTEXT pDevice)
{
	BOOL bRet = TRUE;
	DWORD stat = 0;
	DWORD EnabledIntr;


   // loop here waiting for an Interrupt to occur
   while(pDevice->ISTLoopTrack)
	{	
		WaitForSingleObject(pDevice->hInterruptEvent,INFINITE);
		if(!pDevice->ISTLoopTrack)
		{
			bRet= FALSE;
			break;
		
		}

		//check whether controller enabled & interrupt register set
		stat  = SPI_REGISTER_READ(pDevice->pRegisters->SSSR);
		EnabledIntr = SPI_REGISTER_READ(pDevice->pRegisters->SSCR1);
				
		if ((stat & SSSR_TFS)  && (EnabledIntr & SSCR1_TIE))
		{
			
			while(!pDevice->bIoComplete)  
			{
				TransferDataWrite(pDevice);	
				
				pDevice->Status = TRANSFER_SUCCESS;
				if((EnabledIntr & SSCR1_RIE) && (stat & SSSR_RNE))
				{
					while((pDevice->TxTransferredLength - pDevice->RxTransferredlength != 0))
					{
						//check ROR (Receive FIFO Overrun, SSSR bit 7)
						if (CheckControllerError(pDevice))
						{
							//call read operation
							TransferDataRead(pDevice);
						}
						else
						{
							pDevice->Status = TRANSFER_UNSUCCESSFUL;
							break;
						}
					}
				}
				if(pDevice->Status == TRANSFER_UNSUCCESSFUL)
					break;
			}//end of while loop
			

			pDevice->InterruptMask &= 0;
			spiDisableInterrupts(pDevice);
			ControllerCompleteTransfer(pDevice);
			//Tell the kernel that the interrupt has been serviced
			SetEvent(pDevice->hWriteReadEvent);
						
		}
		else if ((stat & SSSR_RFS) && (EnabledIntr & SSCR1_RIE))
		{
			while((pDevice->BufLength - pDevice->RxTransferredlength != 0))
			{
				//check ROR (Receive FIFO Overrun, SSSR bit 7)
				if (CheckControllerError(pDevice))
				{
					//call read operation
					TransferDataRead(pDevice);
					pDevice->Status =TRANSFER_SUCCESS;
				}
				else
				{
					pDevice->Status =TRANSFER_UNSUCCESSFUL;
					break;
				}

				if(pDevice->BufLength - pDevice->RxTransferredlength != 0)
				{
					PrepareForRead(pDevice);
				}

			}//end of while loop

			pDevice->InterruptMask &= 0;
			spiDisableInterrupts(pDevice);
			ControllerCompleteTransfer(pDevice);
			//Tell the kernel that the interrupt has been serviced
			SetEvent(pDevice->hWriteReadEvent);

		}

	  //final interrupt done
	  InterruptDone(pDevice->dwSysIntr);	


	}//end bKeepLooping

    //when and if the driver unloads and the thread exits release resources
    spiDisableInterrupts(pDevice);
	InterruptDisable (pDevice->dwSysIntr);
	CloseHandle(pDevice->hInterruptEvent);

	return (DWORD)bRet;

}

/*************************************************************************
 @func  HANDLE | PrepareForTransfer | Initialize and prepare for Transfer

 @Description: This routine set target ClockPolarity,ClockPhase,ClockRate,
 transmit and receive threshold and configure read/write operation
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL PrepareForTransfer(PDEVICE_CONTEXT pDevice,PSPI_TRANS_BUFFER pTransmissionBuf)
{

	BOOL bRet = TRUE;
	DWORD SCF = 0;
    DWORD CR  = 0;
    DWORD CD  = 0;


	//set up the buffer to hold bytes read/write
	pDevice->Direction = pTransmissionBuf->Direction;
	pDevice->BufLength = pTransmissionBuf->Length;
	
	// Create device structure and allocating memory 
	pDevice->pBuffer = (PUCHAR)LocalAlloc(LPTR,(pDevice->BufLength * sizeof(UCHAR)));
	if(pDevice->pBuffer == NULL)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("SPI:PrepareForTransfer - Failed allocate buffer.\r\n")));
		bRet = FALSE;
		goto exit;
	}
	
	if(pDevice->pBuffer)
	{
		memcpy_s(pDevice->pBuffer,pDevice->BufLength,pTransmissionBuf->pBuffer, pDevice->BufLength);
	}


    // Set target settings

    pDevice->ChipSelect           = 0;    // Fixed
    pDevice->DataFrameSize        = 8;    // Only support 8bits for alpha release
    pDevice->ConnectionSpeed      = pTransmissionBuf->BusSpeed;
    pDevice->Mode                 = pTransmissionBuf->Mode;

	// Target ClockPolarity 
    pDevice->SerialClockPolarity  = SPI_SPOLARITY(pDevice->Mode);

	// Target ClockPhase
    pDevice->SerialClockPhase     = SPI_SPHASE(pDevice->Mode);


	// The clock divider value provides for a SPI bus connection speed
    // equal to or less than the value specified in the ACPI DSDT table
    // DCM comment: floating point in kernel mode requires special attention
    //double SCF = (double)pDevice->RegistrySettings.SrcClkFreq;
    //double CR  = (double)lpssdescriptor->ConnectionSpeed;
    //
    //pSettings->ClockDivider = (ULONG)ceil(SCF/CR);


    SCF = pDevice->SrcClockFreq;
    CR  = pDevice->ConnectionSpeed;
    CD  = SCF / CR;


	 // Hardware restriction on VLV
    if((CD < MIN_SER) || (CD > MAX_SER))
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("SPI:Invalid Source Clock Divider %x, must be at least 4 and less than 4096\r\n"),CD));

        bRet = FALSE;
        goto exit;
    }

	 // Target ClockRate.
    pDevice->ClockDivider = CD - 1; // To compensate for the +1 in the HW bit rate calculation


    // DCM: these two setings are new with VLV
    // LoopbackMode
    // DCM: commenting out for now    pSettings->LoopbackMode = pDevice->RegistrySettings.LoopbackMode;
    pDevice->LoopbackMode = 0; // DCM: Need to update when registry gets fixed

    // Number of bytes per FIFO entry
    pDevice->BytesPerEntry = (pDevice->DataFrameSize + 7) / 8;

    if(pDevice->BytesPerEntry > 2)
    {
        pDevice->BytesPerEntry = 4;
    }

    // Configure hardware for transfer.
	SpiTransferPreSettings(pDevice);

	//select the chip for first time transfer
	AssertChipSelect(pDevice);

	//init variables
	pDevice->TxTransferredLength = 0;
	pDevice->RxTransferredlength = 0;
	pDevice->RequestTransferLength = 0;
	pDevice->Status = TRANSFER_UNSUCCESSFUL;
	

	// Set transmit threshold to 1 to generate interrupt when TXFIFO is empty
    SET_TRANSMIT_THRESHOLD(pDevice, 1, 1);
    SET_RECEIVE_THRESHOLD(pDevice, 1);


	//SetInterrupt Mask
	pDevice->InterruptMask = SSSR_TFS | SSSR_RFS;

 	//Set the direction read/write
	pDevice->Direction = pTransmissionBuf->Direction;
	
    // Determine if DMA is needed or required
	if(pDevice->ForceDma  >= DMA_MIN_LENGTH && pDevice->ForceDma <= pDevice->BufLength)
	{
		pDevice->hDmaHandler = GetDmaAdapterHandle(pDevice);
		if((pDevice->hDmaHandler != NULL) && InitDMA(pDevice))
		{
			pDevice->UseDma = TRUE;
			spiDisableInterrupts(pDevice);
		}
		//Reset back to avoid previous setting take place
		else
			pDevice->UseDma = FALSE;

	}
	else
	{
			pDevice->UseDma = FALSE;
	}
	
	//switch to PIO mode if DMA set not to be used.
	if(!pDevice->UseDma)
	{
		if(pTransmissionBuf->Direction == TransferDirectionRead)
		{
			//set the direction read/write
			pDevice->Direction = TransferDirectionRead;

			PrepareForRead(pDevice);

			pDevice->InterruptMask = SSSR_RFS;
			spiEnableInterrupts(pDevice);
		 
		}
		else if(pTransmissionBuf->Direction == TransferDirectionWrite)
		{
			//set direction read/write
			pDevice->Direction = TransferDirectionWrite;

			spiEnableInterrupts(pDevice);
		}
	}


exit:
	return bRet;


}

/*************************************************************************
 @func  HANDLE | SpiTransferPreSettings | Pre setting on registers

 @Description: This routine set Set clock divider, set EDDS and 
 DDS (registers are 0 based) 
 
 @rdesc This routine returns status representing the device.
 *************************************************************************/
BOOL SpiTransferPreSettings(PDEVICE_CONTEXT pDevice)
{

	BOOL bRet = TRUE;
	DWORD minTranSize = 0;

	// set entry size (for n*32 set 32)
    minTranSize = pDevice->DataFrameSize;

    if(minTranSize > 32)
    {
        minTranSize = 32;
    }

	CLR_REGISTER_BITS(pDevice->pRegisters->SSCR0, (ULONG)SSCR0_SSE_EN);

    // Setup SPI controller properties
    // Set clock divider, set EDDS and DDS (registers are 0 based)


    WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSCR0, 
                                 ((pDevice->ClockDivider & SSCR0_SCR_MASK) << SSCR0_SCR_SHIFT) |
                                 (((minTranSize - 1) >> 4) << SSCR0_EDDS_SHIFT) |
                                 (((minTranSize - 1) & SSCR0_DSS_MASK) << SSCR0_DSS_SHIFT));

    /*
    pDevice->pRegisters->SSCR1 = SSCR1_TTE |
                                 (pTargetSettings->LoopbackMode        ? SSCR1_LBM : 0) |
                                 (pTargetSettings->SerialClockPhase    ? SSCR1_SPH : 0) |
                                 (pTargetSettings->SerialClockPolarity ? SSCR1_SPO : 0) |
                                 (pTargetSettings->ChipSelectPolarity  ? SSCR1_IFS : 0);
                                 */

    WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSCR1, 
                                 SSCR1_TTE |
                                 (pDevice->LoopbackMode        ? SSCR1_LBM : 0) |
                                 (pDevice->SerialClockPhase    ? SSCR1_SPH : 0) |
                                 (pDevice->SerialClockPolarity ? SSCR1_SPO : 0) |
                                 (pDevice->ChipSelectPolarity  ? SSCR1_IFS : 0));

    // Enable controller
    // pDevice->pRegisters->SSCR0 |= SSCR0_SSE_EN;
    SET_REGISTER_BITS(pDevice->pRegisters->SSCR0, SSCR0_SSE_EN);
 



	return bRet;


}

/*************************************************************************
 @func  HANDLE | spiDisableInterrupts | Disable interrupts

 @Description: This routine disable interrupt for spi controller
 
 *************************************************************************/
VOID spiDisableInterrupts(PDEVICE_CONTEXT pDevice)
{
     // SSCR0 has TIM & RIM interrupt disable bits
    SET_REGISTER_BITS(pDevice->pRegisters->SSCR0, (ULONG)SPI_INTERRUPT_SSCR0_MASK);

    // SSCR1 has interrupt enable bits
    CLR_REGISTER_BITS(pDevice->pRegisters->SSCR1, (ULONG)SPI_INTERRUPT_SSCR1_MASK);
 
}
/*************************************************************************
 @func  HANDLE | spiEnableInterrupts | Disable interrupts

 @Description: This routine disable interrupt for spi controller
 
 *************************************************************************/
VOID spiEnableInterrupts(PDEVICE_CONTEXT pDevice)
{

  DWORD   sscr0_old, sscr1_old;
  DWORD   sscr0_new, sscr1_new;

    sscr0_old = READ_REGISTER_ULONG(&pDevice->pRegisters->SSCR0);
    sscr1_old = READ_REGISTER_ULONG(&pDevice->pRegisters->SSCR1);

    sscr0_new = (sscr0_old & ~SPI_INTERRUPT_SSCR0_MASK)         |
                ((pDevice->InterruptMask & SSSR_TUR) ? 0 : SSCR0_TIM)    |
                ((pDevice->InterruptMask & SSSR_ROR) ? 0 : SSCR0_RIM);

    sscr1_new = (sscr1_old & ~SPI_INTERRUPT_SSCR1_MASK)         |
                ((pDevice->InterruptMask & SSSR_TFS)  ? SSCR1_TIE : 0)   |
                ((pDevice->InterruptMask & SSSR_RFS)  ? SSCR1_RIE : 0)   |
                ((pDevice->InterruptMask & SSSR_PINT) ? SSCR1_PINTE : 0) |
                ((pDevice->InterruptMask & SSSR_TINT) ? SSCR1_TINTE : 0) |
                ((pDevice->InterruptMask & SSSR_EOC)  ? 0 : 0)           |
                ((pDevice->InterruptMask & SSSR_BCE)  ? SSCR1_EBCEI : 0);


    WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSCR0, sscr0_new);

    WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSCR1, sscr1_new);
 
}
/*************************************************************************
 @func  HANDLE | AssertChipSelect | Select chip to active low

 @Description: This routine to set SS (slave select) to active low
 
 *************************************************************************/
VOID AssertChipSelect(PDEVICE_CONTEXT pDevice)
{

    if(pDevice->ChipSelectPolarity)
    {
        SET_REGISTER_BITS(pDevice->pRegisters->PRV_CS_CTRL, PRV_CS_CTRL_STATE_HI_BIT);
    }
    else
    {
        CLR_REGISTER_BITS(pDevice->pRegisters->PRV_CS_CTRL, PRV_CS_CTRL_STATE_HI_BIT);
    }

}
/*************************************************************************
 @func  HANDLE | DeassertChipSelect | Deassert Chip select

 @Description: This routine set SS to active high
 
 *************************************************************************/
VOID DeassertChipSelect(PDEVICE_CONTEXT pDevice)
{

    if(pDevice->ChipSelectPolarity)
    {
        CLR_REGISTER_BITS(pDevice->pRegisters->PRV_CS_CTRL, PRV_CS_CTRL_STATE_HI_BIT);
    }
    else
    {
        SET_REGISTER_BITS(pDevice->pRegisters->PRV_CS_CTRL, PRV_CS_CTRL_STATE_HI_BIT);
    }

}
/*************************************************************************
 @func  HANDLE | TransferDataWrite | Execute Write Operation

 @Description: This routine execute write operation from buffer to FIFO

 *************************************************************************/
BOOL TransferDataWrite(PDEVICE_CONTEXT pDevice)
{

	DWORD   index         = 0;
    DWORD   requestLength = 0;
    DWORD    bytesPerEntry = 0;
    DWORD    txFifoSpace   = 0;
    DWORD    fifoEntry = 0;
    DWORD    bpe = 0;
    UINT8    bufferData;


    index  = pDevice->TxTransferredLength;
    requestLength = pDevice->BufLength;
    bytesPerEntry = pDevice->BytesPerEntry;
    txFifoSpace   = SPI_TX_FIFO_SIZE - GET_TRANSMIT_FIFO_LEVEL(pDevice);


    // Perform write.
    while((index < requestLength) && (txFifoSpace > 0))
    {
        // prepare fifo entry
        fifoEntry = 0;

        for(bpe = 0; bpe < bytesPerEntry; bpe++)
        {

	    bufferData =(UCHAR) *((pDevice->pBuffer)+ index);

            fifoEntry |= (bufferData << (bpe * 8));
            index++;
        }

        //  Place data into the FIFO   
        WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSDR, fifoEntry);

        txFifoSpace--;
		pDevice->TxTransferredLength+= bytesPerEntry;
    }
		// Single or last item in sequence
        if (requestLength - index  == 0)
        {
			 pDevice->bIoComplete = TRUE;

		}

	return TRUE;

}
/*************************************************************************
 @func  HANDLE | PrepareForRead | Read data from Fifo

 @Description: This routine execute read operation from FIFO
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/
BOOL PrepareForRead(PDEVICE_CONTEXT pDevice)
{

    size_t   index         = 0;
    size_t   requestLength = 0;
    ULONG    bytesPerEntry = 0;
    ULONG    txFifoFillRq  = 0;                 // one byte could be still processed
    ULONG    rxFifoSpace   = 0;
    ULONG    txFifoSpace   = 0;


    index         = pDevice->RequestTransferLength;
	requestLength = pDevice->BufLength;
    bytesPerEntry = pDevice->BytesPerEntry;

    rxFifoSpace   = SPI_RX_FIFO_SIZE - GET_RECEIVE_FIFO_LEVEL(pDevice) - 1;


    txFifoSpace   = SPI_TX_FIFO_SIZE - GET_TRANSMIT_FIFO_LEVEL(pDevice);


    // Determine if another request will fit into the TX FIFO
    // and received data will fit into RX FIFO
    // Need to write 0xFF to generate clock for Read operation(Slave to write)

    while(index < requestLength && txFifoSpace && rxFifoSpace)
    {
        //
        //  Place the next read request into the FIFO
        //

        WRITE_REGISTER_ULONG(&pDevice->pRegisters->SSDR, 0xFFFFFFFF);

        txFifoFillRq++;
        index += bytesPerEntry;
        pDevice->RequestTransferLength++;
        txFifoSpace--;
        rxFifoSpace--;
    }


    SET_RECEIVE_THRESHOLD(pDevice, (txFifoFillRq > 1 ? txFifoFillRq - 1 : 0));

	
    	

    return TRUE;
}  
/*************************************************************************
 @func  HANDLE | TransferDataRead | Read operation 

 @Description: This routine read data from FIFO to buffer

 *************************************************************************/
BOOL TransferDataRead(PDEVICE_CONTEXT pDevice)
{
 
    DWORD   index         = 0;
    DWORD   requestLength = 0;
    DWORD   bytesPerEntry = 0;
    DWORD   rxFifoEntries = 0;
    DWORD   fifoEntry = 0;
    DWORD   bpe = 0;

    index         = pDevice->RxTransferredlength;
    requestLength = pDevice->BufLength;
    bytesPerEntry = pDevice->BytesPerEntry;

    rxFifoEntries = GET_RECEIVE_FIFO_LEVEL(pDevice);


    // Perform read.
    // Determine if another byte is available in the RX FIFO

    while(index < requestLength && rxFifoEntries)
    {
        //
        //  Retrieve the data from the receive buffer
        //

        fifoEntry = READ_REGISTER_ULONG(&pDevice->pRegisters->SSDR);

        for(bpe = 0; bpe < bytesPerEntry; bpe++)
        {

			//transfer from FIFO to buffer
			*((pDevice->pBuffer)+ index) = (UCHAR)(fifoEntry & 0xFF);
			
            index++;
            pDevice->RxTransferredlength++;
            fifoEntry >>= 8;
        }

        rxFifoEntries--;
		
    }

	pDevice->InterruptMask &= ~(ULONG)SSSR_RFS;


    return TRUE;
}
/*************************************************************************
 @func  HANDLE | ControllerCompleteTransfer | Complete Transfer operation 

 @Description: This routine reset device context after complete transfer

 *************************************************************************/
BOOL ControllerCompleteTransfer(PDEVICE_CONTEXT pDevice)
{

    // Update request context with information from this transfer
	if(pDevice->bIoComplete)
	{
		pDevice->RxTransferredlength = 0;
		pDevice->TxTransferredLength = 0;
	}
	
	// Last call for controller post transfer process before removal of request
    // I2C: Generate stop on the bus. This surpresses restart condition.
    // SPI: Deasserting the ChipSelect.
      // Add 100us delay for HW to handle the last data after TX FIFO level is set to zero.
    Sleep(1);

    DeassertChipSelect(pDevice);
	pDevice->bIoComplete = FALSE;

	return TRUE;
}
/*************************************************************************
 @func  HANDLE | SPI_Open | Open the SPI device

 @Description: This routine must be called by the user to open the
 GPIO device. The HANDLE returned must be used by the application in
 all subsequent calls to the SPI driver. This routine initialize 
 the device's data
 
 @rdesc This routine returns a HANDLE representing the device.
 *************************************************************************/
DWORD SPI_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode)
{
	DWORD dwRet =(DWORD)NULL;

	SPI_DEVICE_CTXT *pDevice = (SPI_DEVICE_CTXT*)hOpenContext; 
	  

	if(pDevice == NULL)
	{
        DEBUGMSG (ZONE_ERROR,(TEXT("SPI: SPI_Open() - Failed to open device %d\r\n"),GetLastError()));
        return dwRet;
    }


	dwRet = (DWORD)pDevice;

    return dwRet;
}

/*************************************************************************
  @func BOOL        | SPI_IOControl | Device IO control routine
 @parm pGPIOhandle  | hOpenContext  | SPI handle
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
BOOL SPI_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn, 
                   PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
	PPOWER_CAPABILITIES pPowerCaps = NULL;
	SPI_DEVICE_CTXT *pDevice = (SPI_DEVICE_CTXT*)hOpenContext;
	PSPI_TRANS_BUFFER spiTransfer = NULL;

	if(NULL == pDevice)
	{

		DEBUGMSG (ZONE_ERROR,(TEXT("SPI:SPI_IOControl - Failed to open device %d\r\n"),GetLastError()));
		return FALSE;
	}

	switch (dwCode) {
		case IOCTL_SPI_EXECUTE_WRITE:
			spiTransfer = (PSPI_TRANS_BUFFER)pBufIn;
			
			if(PrepareForTransfer(pDevice,spiTransfer))
			{
				//This will be skipped if it is in PIO mode
				if(pDevice->UseDma)
				{
					if (ProgramExecuteDmaTransaction(pDevice)){
						memcpy_s(pBufOut,pDevice->BufLength,pDevice->pBuffer,pDevice->BufLength);
					}
					//Uninitialize DMA Common buffer before exiting
					DeinitDMA(pDevice);
					
				}

				//PIO mode take place
				if(!pDevice->UseDma)
				{
					if(WaitForSingleObject(pDevice->hWriteReadEvent,INFINITE) == WAIT_OBJECT_0){
						if (pDevice->Status	== TRANSFER_SUCCESS){
							memcpy_s(pBufOut,pDevice->BufLength,pDevice->pBuffer,pDevice->BufLength);
						}						 	     
					}
				}//end of PIO mode	
			}
			else
			{
				pDevice->Status	= TRANSFER_UNSUCCESSFUL;
			}

			/* Free device structure */
			if(pDevice->pBuffer != NULL)
			{
				LocalFree(pDevice->pBuffer);
				pDevice->pBuffer = NULL;
			}

			break;
		case IOCTL_SPI_EXECUTE_READ:
			spiTransfer = (PSPI_TRANS_BUFFER)pBufIn;
			if(PrepareForTransfer(pDevice,spiTransfer))
			{
				//This will be skipped if it is in PIO mode
				if(pDevice->UseDma)
				{
					if (ProgramExecuteDmaTransaction(pDevice)){
						memcpy_s(pBufOut,pDevice->BufLength,pDevice->pBuffer,pDevice->BufLength);
					}
					//Uninitialize DMA Common buffer before exiting
					DeinitDMA(pDevice);
				}

				//PIO mode take place
				if(!pDevice->UseDma) 
				{
					if(WaitForSingleObject(pDevice->hWriteReadEvent,INFINITE) == WAIT_OBJECT_0){
						if (pDevice->Status	== TRANSFER_SUCCESS){
							memcpy_s(pBufOut,pDevice->BufLength,pDevice->pBuffer,pDevice->BufLength);
						}					
					}				      
				}
			}
			else
			{
				pDevice->Status	= TRANSFER_UNSUCCESSFUL;
			}
		   /* Free device structure */
			if(pDevice->pBuffer != NULL)
			{
				LocalFree(pDevice->pBuffer);
				pDevice->pBuffer = NULL;
			}

		   break;
		case IOCTL_POWER_CAPABILITIES:
			pPowerCaps = (PPOWER_CAPABILITIES)pBufOut;     
			memset(pPowerCaps, 0, sizeof(POWER_CAPABILITIES));

              // set full powered and non powered state
            pPowerCaps->DeviceDx = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
            pPowerCaps->WakeFromDx = DX_MASK(D0) | DX_MASK(D3);
			*pdwActualOut = sizeof(POWER_CAPABILITIES);
			pDevice->Status	= TRANSFER_SUCCESS;	
			break;
		default:
			DEBUGMSG(ZONE_ERROR,(TEXT("SPI: SPI_IOControl()  Invalid index: %d\n"),dwCode));
			break;
	}	
	return pDevice->Status;
} 

/*************************************************************************
 @func BOOL     | SPI_Close | close the SPI device.
 @parm SPI_handle	| hOpenContext | Context pointer returned from SPI_Open
 @rdesc TRUE if success; FALSE if failure
 @remark This routine is called by the device manager to close the device.
 *************************************************************************/
BOOL SPI_Close(DWORD hOpenContext)
{
    BOOL bRet = TRUE;

	SPI_DEVICE_CTXT *pDevice = (SPI_DEVICE_CTXT*)hOpenContext;

	//check for correct device context
	if(!pDevice)
	{
		DEBUGMSG (ZONE_ERROR,(TEXT("SPI:SPI_Close - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;

	}

    return bRet;
} 

/*************************************************************************
 @func  BOOL | SPI_Deinit | De-initialize SPI
 @parm 	SPI_handle | hDeviceContext | Context pointer returned from SPI_Init
 @rdesc None.
 *************************************************************************/
BOOL SPI_Deinit(DWORD hDeviceContext)
{
    BOOL bRet = TRUE;
	
	/* Unallocate the memory*/
    SPI_DEVICE_CTXT *pDevice = (SPI_DEVICE_CTXT*)hDeviceContext;

	//check for correct device context
	if(pDevice == NULL)
	{
		DEBUGMSG (ZONE_ERROR,(TEXT("SPI:SPI_Deinit - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;

	}

	//uninitialize the controller
	DeinitSpiController(pDevice);

	//uninitialize the IST loop
	pDevice->ISTLoopTrack = FALSE;
	 /* close interrupt thread */
    if(pDevice->hInterruptThread != NULL) {
        CloseHandle(pDevice->hInterruptThread);    /* close handle */
    }
    
    if(pDevice->hInterruptEvent != NULL) { CloseHandle(pDevice->hInterruptEvent); }

	if(pDevice->hIsrHandler != NULL){FreeIntChainHandler(pDevice->hIsrHandler);}
   

	 /* disable interrupt */
    if(pDevice->dwSysIntr != 0) {
        InterruptDisable(pDevice->dwSysIntr);
        KernelIoControl(IOCTL_HAL_RELEASE_SYSINTR, &pDevice->dwSysIntr, 
                                    sizeof(pDevice->dwSysIntr), NULL, 0, NULL);
    }


    /* close read/write op */
    if(pDevice->hWriteReadEvent != NULL) { CloseHandle(pDevice->hWriteReadEvent); }

	if(pDevice->hSpiHandler != NULL) { CloseBusAccessHandle(pDevice->hSpiHandler);}
	/* close registry key */
	if(pDevice->hKey!= NULL) { RegCloseKey(pDevice->hKey); }

		
     /* Free device structure */
	 LocalFree(pDevice); 

	return bRet;
} 



