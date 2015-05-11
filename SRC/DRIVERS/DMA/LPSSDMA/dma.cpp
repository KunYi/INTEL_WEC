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


//Module Name: DMA.C

/*
*    **********************************************************************
*    Intel's DMA Driver
*    **********************************************************************
*/
#include <windows.h>
#include <nkintr.h>
#include <Winbase.h>
#include <devload.h>    /* Registry API's */
#include <pm.h>


#include "dma.h"



//protect critical section

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
        DEBUGMSG (TRUE,(TEXT("LPSS DMA DLL_PROCESS_ATTACH - Process attached. \r\n")));
        DisableThreadLibraryCalls((HMODULE)hModule);
        break;

    case DLL_PROCESS_DETACH:
        DEBUGMSG (TRUE,(TEXT("LPSS DMA DLL_PROCESS_DETACH - Process dettached. \r\n")));
        break;
    default:
        break;
    }

    return TRUE;
}

BOOL InitChannelContext(PDEVICE_CONTEXT pDevice, DWORD channel)
{
    PDMA_CHANNEL_CONTEXT pChannelContext;
    pChannelContext = &pDevice->ChannelContext[channel];
    memset(pChannelContext,0,sizeof(*pChannelContext));

    ClearInterruptOnChannel(pDevice->DMAC_BASE,channel);
    EnableInterruptOnChannel(pDevice->DMAC_BASE,channel);
    //DisableChannel(pDevice->DMAC_BASE,channel);

    pChannelContext->pDeviceContext = pDevice;
    pChannelContext->ChannelEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
    pChannelContext->ChannelThread = CreateThread(
        NULL,
        0,
        ChannelThreadFunc,
        pChannelContext,
        CREATE_SUSPENDED,
        NULL);

    //CeSetThreadPriority( pChannelContext->ChannelThread , 250);

    pChannelContext->ChannelState = DMA_TRANSACTION_NOREQUEST;
    pChannelContext->Channel = channel;

    pChannelContext->pChannelLock = new(CLockObject);

    if ( pChannelContext->ChannelEvent == NULL
        || pChannelContext->ChannelThread == NULL)
    {
        DEBUGMSG (TRUE,(TEXT("LPSS DMA: Failed to Create Channel Thread or Event\n")));
        goto ErrExit;
    }
    if ( ((DWORD)-1) == ResumeThread(pChannelContext->ChannelThread))
    {
        pChannelContext->ChannelThread = NULL;
        DEBUGMSG (TRUE,(TEXT("LPSS DMA: Failed to Resume Channel Thread\n")));
        goto ErrExit;
    }
    return TRUE;
ErrExit:
    if (pChannelContext->ChannelEvent != NULL)
    {
        CloseHandle(pChannelContext->ChannelEvent);
        pChannelContext->ChannelEvent = NULL;
    }

    if (pChannelContext->ChannelThread != NULL)
    {
        CloseHandle(pChannelContext->ChannelThread);
        pChannelContext->ChannelThread = NULL;
    }

    delete (pChannelContext->pChannelLock);
     pChannelContext->pChannelLock = NULL;

    return FALSE;
}
/*************************************************************************

@func  HANDLE | DMA_Init | Initialize DMA device.
@parm  PCTSTR  | Identifier | Device identifier. The device loader
passes in the registry key that contains information
about the active device.

@remark This routine is called at device load time in order to perform 
any initialization. Typically the init routine does as little as 
possible,Create a memory space to store the device information.

@Return Value    Returns a pointer which is passed into the DMA_Open and 
DMA_Deinit entry points as a device handle.
*************************************************************************/

DWORD DMA_Init( LPCTSTR pContext, LPCVOID lpvBusContext )
{

    DMA_DEVICE_CTXT *pDevice = NULL;

    HKEY hKey = NULL;
    HANDLE hBusAccess,hIsrHandler;
    PHYSICAL_ADDRESS phyAddr;

    DDKWINDOWINFO dwi;
    DDKPCIINFO pcii;
    DDKISRINFO isri;

    DWORD dwStatus= 0;
    DWORD dwLength = 0;
    DWORD dwBusNumber = 0;
    DWORD dwInterfaceType = 0;
    DWORD MMIOSpace= 0;
    LPVOID dwPhysAddr;
    DWORD   channel;

    // Create device structure and allocating memory 
    pDevice = (DMA_DEVICE_CTXT*)LocalAlloc(LPTR, sizeof(DMA_DEVICE_CTXT));

    if(pDevice == NULL)
    {
        DEBUGMSG(TRUE,(TEXT("DMA:DMA_INIT - Failed allocate DMA controller structure.\r\n")));
        goto ErrExit;
    }

    pDevice->pDeviceLock = new(CLockObject);

    pDevice->bIsTerminated = FALSE;

    // Open the Active key for the driver.
    hKey = OpenDeviceKey(pContext);
    DEBUGMSG(TRUE,(TEXT("DMA: DMA_Init() Active Registry Key= %s \n"),pContext));

    if(!hKey) 
    {
        DEBUGMSG(TRUE,(TEXT("DMA - Failed to open device key path.\r\n")));
		goto ErrExit;
    }

    pDevice->hDeviceKey = hKey;
    // read window information
    dwi.cbSize = sizeof(dwi);
    dwStatus = DDKReg_GetWindowInfo(hKey,&dwi);
    if(dwStatus != ERROR_SUCCESS) 
    {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: DDKReg_GetWindowInfo() failed %d\r\n"), dwStatus));
        goto ErrExit;
    }

    dwBusNumber = dwi.dwBusNumber;

    dwInterfaceType = dwi.dwInterfaceType;


    // memory space
    if(dwi.dwNumMemWindows  != 0)
    {
        phyAddr.LowPart = dwi.memWindows[0].dwBase;
        DEBUGMSG(ZONE_ERROR, (TEXT("DMA::Physical Base Address: %0x\r\n"),phyAddr.LowPart));
        dwLength = dwi.memWindows[0].dwLen;
        MMIOSpace = (DWORD)FALSE;

    }

    DEBUGMSG(TRUE, (TEXT("DMA:dwLength = 0x%x\r\n"),dwLength));

    // creates a handle that can be used for accessing a bus
    hBusAccess = CreateBusAccessHandle(pContext);
    if (!hBusAccess)
    {
        goto ErrExit;
    }
    pDevice->hBusAccess = hBusAccess;
    // read PCI id
    pcii.cbSize = sizeof(pcii);
    dwStatus = DDKReg_GetPciInfo(hKey, &pcii);
    if(dwStatus != ERROR_SUCCESS) 
    {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: DDKReg_GetPciInfo() failed %d\r\n"), dwStatus));
        goto ErrExit;
    }

    pDevice->dwDeviceId         = pcii.idVals[PCIID_DEVICEID];
    pDevice->dwISTLoopTrack    = TRUE;

    // read ISR information
    isri.cbSize = sizeof(isri);
    dwStatus = DDKReg_GetIsrInfo(hKey, &isri);
    if(dwStatus != ERROR_SUCCESS) 
    {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: DDKReg_GetIsrInfo() failed %d\r\n"), dwStatus));
        goto ErrExit;    
    }


    // sanity check return values
    if(isri.dwSysintr == SYSINTR_NOP) 
    {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: no sysintr specified in registry\r\n")));
        goto ErrExit;
    }
    if(isri.szIsrDll[0] != 0) 
    {
        if(isri.szIsrHandler[0] == 0 || isri.dwIrq == IRQ_UNSPECIFIED) 
        {
            DEBUGMSG(ZONE_ERROR, (_T("DMA: invalid installable ISR information in registry\r\n")));
            DMA_Deinit((DWORD)pDevice);
            return FALSE;
        }
    }

    pDevice->dwSysIntr = isri.dwSysintr;
    pDevice->dwIrq = isri.dwIrq;

    DEBUGMSG(ZONE_ERROR, (_T("DMA:IRQ assigned: %d\r\n"),isri.dwIrq));


    // Install ISR handler if there is one
    if(isri.szIsrDll[0] != 0) 
    {
        // Install ISR handler
        hIsrHandler = LoadIntChainHandler(isri.szIsrDll, isri.szIsrHandler, (BYTE) isri.dwIrq);
        pDevice->hIsrHandler = hIsrHandler;
        if(hIsrHandler == NULL) 
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("DMA: Couldn't install ISR handler\r\n")));
            goto ErrExit;
        } 
        else {
            // translate the target bus address to a virtual address
            if(!BusTransBusAddrToStatic(hBusAccess, (INTERFACE_TYPE)dwInterfaceType, dwBusNumber,
                phyAddr, dwLength, &MMIOSpace, &dwPhysAddr)) 
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("DMA: Failed BusTransBusAddrToStatic\r\n")));
                goto ErrExit;
            }

            // Get the register base for the controller.
            if(dwPhysAddr)
            {
                pDevice->pMemBaseAddr = (PUCHAR)dwPhysAddr;
                pDevice->DMAC_BASE =pDevice->pMemBaseAddr;
                pDevice->dwMemLen = dwLength;
            }

            DEBUGMSG(TRUE, (TEXT("DMA: Installed ISR handler, Dll = '%S', Handler = '%S',Device Id = 0x%x, PhysAddr = 0x%x, Sysintr = 0x%x\r\n"),
                isri.szIsrDll, isri.szIsrHandler,pDevice->dwDeviceId, phyAddr,pDevice->dwSysIntr));

            // Set up ISR handler
            pDevice->IsrInfo.DMAC_BASE = pDevice->DMAC_BASE;
            pDevice->IsrInfo.SysIntr = pDevice->dwSysIntr;
            if(!KernelLibIoControl(hIsrHandler, IOCTL_LPSSDMAISR_INFO, &pDevice->IsrInfo, sizeof(pDevice->IsrInfo), NULL, 0, NULL)) 
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("DMA: KernelLibIoControl call failed.\r\n")));
                goto ErrExit;
            }

        }
    }

    /* create event for handling hardware interrupts */
    pDevice->hInterruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(pDevice->hInterruptEvent == NULL) {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: DMA_Init: Failed create interrupt "\
            L"event\r\n")));
        goto ErrExit;        
    }
	
	pDevice->InterruptThreadStarted = FALSE;
    pDevice->hInterruptThread = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE)DMA_IST,pDevice,CREATE_SUSPENDED, NULL);
    if(pDevice->hInterruptThread == NULL) {
        DEBUGMSG(ZONE_ERROR,(_T("ERROR: DMA_Init - Failed to create interrupt "\
            L"thread\r\n")));
        goto ErrExit;       
    }
    //CeSetThreadPriority(pDevice->hInterruptThread, 200);

    /* initialize interrupt sysintr to event mapping  */
    if(!InterruptInitialize(pDevice->dwSysIntr,pDevice->hInterruptEvent, NULL, 0)) {
        DEBUGMSG(ZONE_ERROR, (_T("DMA: DMA_Init - Failed to initialize "\
            L"interrupt\r\n")));
        goto ErrExit;
    }


    for (channel = 0; channel < DMAH_NUM_CHANNELS; ++channel)
    {
        if (FALSE == InitChannelContext(pDevice,channel))
        {
            goto ErrExit;
        }
    }

    DisableDMAController(pDevice->DMAC_BASE);
    EnableDMAController(pDevice->DMAC_BASE);

    //Get the Thread start
    ResumeThread(pDevice->hInterruptThread);
	pDevice->InterruptThreadStarted = TRUE;
	
    return (DWORD)pDevice;
ErrExit:
    if (pDevice)
    {
        DMA_Deinit((DWORD)pDevice);
    }
    return FALSE;
} 





/*************************************************************************
@func  HANDLE | DMA_IST | Open the I2C device

@Description: This routine must be called when there hardware 
interrupt signal and it will service the request. Return complete to ISR

@rdesc This routine returns a HANDLE representing the device.
*************************************************************************/
DWORD DMA_IST(PDEVICE_CONTEXT pDevice)
{
    BOOL bRet = TRUE;
    DWORD bResult = FALSE;
    ULONG   StatusTfr = 0;
    DWORD   channel = 0;

    // loop here waiting for an Interrupt to occur
    while(pDevice->dwISTLoopTrack)
    {
        bResult = WaitForSingleObject(pDevice->hInterruptEvent,INFINITE);

        if(!pDevice->dwISTLoopTrack)
        {
            break;
        }

        StatusTfr = pDevice->IsrInfo.InterruptStatusTfr;
        pDevice->IsrInfo.InterruptStatusTfr = 0;

        for (channel = 0; channel < DMAH_NUM_CHANNELS; channel++)
        {
            PDMA_TRANS_CONTEXT pTransConext;
            PDMA_CHANNEL_CONTEXT pChannelContext;
            pChannelContext = &pDevice->ChannelContext[channel];
            pChannelContext->pChannelLock->Lock();

            
            if (pDevice->IsrInfo.InterruptStatusTfrPerChannel[channel] > 0)
            {
                pDevice->IsrInfo.InterruptStatusTfrPerChannel[channel] = 0;

                pTransConext = &pChannelContext->TransContext;

                if (pChannelContext->ChannelState == DMA_TRANSACTION_STOP)
                {
                    /* The channel is already stopped because it is canceled*/
                    pChannelContext->pChannelLock->Unlock();
                    continue;
                }

                switch(pTransConext->TransactionType)
                {
                case TRANSACTION_TYPE_SINGLE_RX:
                case TRANSACTION_TYPE_SINGLE_TX:
                    {
                        pTransConext->TotalTransferredLength += pTransConext->LastTransferredLength;
                        pChannelContext->ChannelState = DMA_TRANSACTION_STOP;
                        pDevice->ChannelContext[pChannelContext->Channel].ChannelNotifyCount++;
                        SetEvent(pDevice->ChannelContext[channel].ChannelEvent);
                        break;
                    }
                default:
                    ASSERT(0);
                }
            }

            pChannelContext->pChannelLock->Unlock();
        } /* for channel*/

        InterruptDone(pDevice->dwSysIntr);

    }//end dwISTLoopTrack

    return (DWORD)bRet;

}
/*************************************************************************
@func BOOL    | IsChannelDisabled    | Check whether channel disabled
@parm PUCHAR  | DMAC_BASE            | DMA controller base address
@parm ULONG   | channel            | DMA Channel
@rdesc Returns TRUE (disabled), FALSE (enabled)
*************************************************************************/
BOOL IsChannelDisabled(PUCHAR DMAC_BASE,ULONG channel)
{
    UINT64 reg;
    reg = READ_DMAC_REGISTER(DMAC_BASE,OFFSET_CHENREG_REGISTER);
    return !((reg>>channel) & 0x1);
}
BOOL PollDMACChannelRegisterBit(PUCHAR DMAC_BASE,ULONG Channel,ULONG RegOffset,ULONG BitOffset,ULONG PollValue,ULONG PollTimeInMicroSec)
{
    ULONG offset;
    ASSERT(Channel<DMAH_NUM_CHANNELS);
    ASSERT(RegOffset<STRIDE_CHANNEL);
    offset = Channel*STRIDE_CHANNEL + RegOffset;

    return PollDMACRegisterBit(DMAC_BASE,offset,BitOffset,PollValue,PollTimeInMicroSec);
}

/*************************************************************************
@func BOOL    | PollDMACRegisterBit    | Setting Poll time in DMA controller

@rdesc Returns TRUE on success, FALSE on failure 
*************************************************************************/
BOOL PollDMACRegisterBit(PUCHAR DMAC_BASE,ULONG RegOffset,ULONG BitOffset,ULONG PollValue,ULONG PollTimeInMicroSec)
{
    UINT64 reg;
    ULONG interval = 10;
    ULONG PollTime = max(1,PollTimeInMicroSec/interval);
    while(PollTime > 0)
    {
        reg = READ_DMAC_REGISTER(DMAC_BASE,RegOffset);
        reg = 0x1 & (reg >> BitOffset);
        if (reg == PollValue)
        {
            return TRUE;
        }
        Sleep(1);
        PollTime--;
    }

    return FALSE;
}

/*Disable Channel anyway*/
VOID DisableChannel(PUCHAR DMAC_BASE,ULONG channel)
{
    BOOL   Rtn;
    UINT64 reg = 0;
    //3 Disalbe channel in ChEnReg.CH_EN
    {
        DEC_REG_FIELD_TYPE(CHENREG) regCHENREG = {0};
        regCHENREG.CHENREG_CH_EN = CHENREG_CH_EN_ChannelDisable << channel;
        regCHENREG.CHENREG_CH_EN_WE = 1 << channel;
        reg = FORMAT_REG_BY_STRUCT(CHENREG,&regCHENREG);
        WRITE_DMAC_REGISTER(DMAC_BASE,OFFSET_CHENREG_REGISTER,reg);
    }

    //Poll ChEnReg.CH_EN that channel is diabled
    {
        Rtn = PollDMACRegisterBit(
            DMAC_BASE,
            OFFSET_CHENREG_REGISTER,
            channel,
            CHENREG_CH_EN_ChannelDisable,
            DEFAULT_POLL_REG_TIME_IN_MS);
    }
    ASSERT(Rtn);
}

BOOL DisableChannelBeforeCompleted(PUCHAR DMAC_BASE,ULONG channel)
{
    /* Refer to 7.7Disabling a Channel Prior to Transfer Completion in DW_ahb_dmac*/
    UINT64 reg = 0;
    BOOL status;
    BOOL Rtn;
    status = TRUE;
    //1. Set CFGx.CH_SUSP to halt transfers from the source
    {
        DEC_REG_FIELD_TYPE(CFGX) regCFGX = {0};
        reg = READ_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_CFGX_REGISTER);
        FORMAT_STRUCT_BY_REG(CFGX,reg,&regCFGX);

        regCFGX.CFGX_CH_SUSP = CFGX_CH_SUSP_Suspended;

        reg = FORMAT_REG_BY_STRUCT(CFGX,&regCFGX);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_CFGX_REGISTER,reg);
    }

    //2. Poll CFGx.FIFO_EMPTY
    {
        Rtn =  PollDMACChannelRegisterBit(
            DMAC_BASE,
            channel,
            OFFSET_CFGX_REGISTER,
            CFGX_FIFO_EMPTY_OFFSET,
            CFGX_FIFO_EMPTY_Empty,
            DEFAULT_POLL_REG_TIME_IN_MS);
    }
    status &= Rtn;
     
    //3 Disalbe channel in ChEnReg.CH_EN
    {
        DEC_REG_FIELD_TYPE(CHENREG) regCHENREG = {0};
        regCHENREG.CHENREG_CH_EN = CHENREG_CH_EN_ChannelDisable << channel;
        regCHENREG.CHENREG_CH_EN_WE = 1 << channel;
        reg = FORMAT_REG_BY_STRUCT(CHENREG,&regCHENREG);
        WRITE_DMAC_REGISTER(DMAC_BASE,OFFSET_CHENREG_REGISTER,reg);
    }

    // 4. Poll ChEnReg.CH_EN that channel is diabled
    {
        Rtn = PollDMACRegisterBit(
            DMAC_BASE,
            OFFSET_CHENREG_REGISTER,
            channel,
            CHENREG_CH_EN_ChannelDisable,
            DEFAULT_POLL_REG_TIME_IN_MS);
    }    
    status &= Rtn;
    return status;
}
/*************************************************************************
@func  HANDLE | ClearStatusInterrupt | Clear status interrupt

@Description: This routine clear the DMA controller status interrupt for all channel.

*************************************************************************/
void ClearStatusInterrupt(PUCHAR DMAC_BASE, ULONG Offset, ULONG InterruptStatus)
{
    int channel;
    for (channel = 0; channel < DMAH_NUM_CHANNELS; channel++)
    {
        if (InterruptStatus & (INT_STATUS << channel))
        {
            ULONG mask= INT_CLEAR;
            mask = mask << channel;
            WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+Offset), mask);
        }
    }
}


/*************************************************************************
@func  HANDLE | EnableDMAController | Enable DMA controller

@Description: This routine enable DMA controller

*************************************************************************/
void EnableDMAController(PUCHAR DMAC_BASE)
{
    UINT64 reg;
    DEC_REG_FIELD_TYPE(DMACFGREG) regDMACFGREG = {0};
    regDMACFGREG.DMACFGREG_DMA_EN = DMACFGREG_DMA_EN_Enabled;
    reg = FORMAT_REG_BY_STRUCT(DMACFGREG,&regDMACFGREG);
    WRITE_DMAC_REGISTER(DMAC_BASE,OFFSET_DMACFG_REGISTER,reg);
}

/*************************************************************************
@func  HANDLE | DisableDMAController | Disable DMA controller

@Description: This routine disable DMA controller

*************************************************************************/
void DisableDMAController(PUCHAR DMAC_BASE)
{
    UINT64 reg;
    DEC_REG_FIELD_TYPE(DMACFGREG) regDMACFGREG = {0};
    regDMACFGREG.DMACFGREG_DMA_EN = DMACFGREG_DMA_EN_Disabled;
    reg = FORMAT_REG_BY_STRUCT(DMACFGREG,&regDMACFGREG);
    WRITE_DMAC_REGISTER(DMAC_BASE,OFFSET_DMACFG_REGISTER,reg);

    PollDMACRegisterBit(DMAC_BASE,
        OFFSET_DMACFG_REGISTER,
        DMACFGREG_DMA_EN_OFFSET,
        DMACFGREG_DMA_EN_Disabled,
        DEFAULT_POLL_REG_TIME_IN_MS);

}

/*************************************************************************
@func  HANDLE | ClearInterruptOnChannel | Disable Interrupt of DMA Channel 

@Description: This routine disable the interrupt of specific DMA channel.

*************************************************************************/
void ClearInterruptOnChannel(PUCHAR DMAC_BASE,ULONG channel)
{
    //0 = no effect
    //1 = clear interrupt
    ULONG mask= INT_CLEAR;
    mask = mask << channel;
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_CLEAR_TFR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_CLEAR_BLK_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_CLEAR_STR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_CLEAR_DTR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_CLEAR_ERR_REGISTER), mask);
}

/*************************************************************************
@func  HANDLE | EnableInterruptOnChannel | Enable Interrupt of DMA Channel 

@Description: This routine enable the interrupt of specific DMA channel.

*************************************************************************/
void EnableInterruptOnChannel(PUCHAR DMAC_BASE,ULONG channel)
{
    //0 = no effect
    //1 = clear interrupt
    ULONG mask = INT_MASK_WE | INT_UNMASK;
    mask = mask<<channel;
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_MASK_TFR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_MASK_BLK_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_MASK_STR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_MASK_DTR_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+OFFSET_MASK_ERR_REGISTER), mask);

}

/*************************************************************************
@func  HANDLE | MaskInterruptRegistersOnChannel | Clear Interrupt of DMA Channel 

@Description: This routine clear the interrupt of specific DMA channel.

*************************************************************************/
void MaskInterruptRegistersOnChannel(PUCHAR DMAC_BASE,ULONG Offset,ULONG channel)
{
    ULONG mask = INT_MASK_WE | INT_MASK;
    mask = mask << channel;
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+Offset), mask);
}

/*************************************************************************
@func  HANDLE | UnMaskInterruptRegistersOnChannel | Set Interrupt of DMA Channel 

@Description: This routine set the interrupt of specific DMA channel.

*************************************************************************/
void UnMaskInterruptRegistersOnChannel(PUCHAR DMAC_BASE,ULONG Offset,ULONG channel)
{
    ULONG mask = INT_MASK_WE | INT_UNMASK;
    mask = mask << channel;
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+Offset), mask);
}

/*************************************************************************
@func ULONG   | GetChannelTransferSizeInByte    | Get the size of the data 
@parm PUCHAR  | DMAC_BASE                        | DMA controller base address
@parm ULONG   | channel                        | DMA Channel

@Description: Calculate the total number of data items already read from the source peripheral 
@rdesc Returns size of the channel 
*************************************************************************/
ULONG GetChannelTransferSizeInByte(PUCHAR DMAC_BASE, ULONG channel)
{
    ULONG Size;
    //BLOCK_TS indicates the total number of single transactions to perform for every block transfer
    //The width of the single transaction is determined by CTLx_SRC_TR_WIDTH
    //the read-back value is the total number of data items already read from the source peripheral

    UINT64 reg;
    DEC_REG_FIELD_TYPE(CTLX) regCTLx = {0};
    reg = READ_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_CTLX_REGISTER);
    FORMAT_STRUCT_BY_REG(CTLX,reg,&regCTLx);

    Size = regCTLx.CTLX_BLOCK_TS* (CLTX_TR_WIDTH_MAP[regCTLx.CTLX_SRC_TR_WIDTH] / 8);

    return Size;
}


/*************************************************************************
@func ULONG   | ProgramDmaForSingleBlockTX    | Set the configuration for TX block

@Description: Set the configuration for each channel of the TX block 
@rdesc Returns TRUE on success, FALSE on failure 
*************************************************************************/
BOOL ProgramDmaForSingleBlockTX(PDEVICE_CONTEXT pDevice,PDMA_TRANS_CONTEXT pTransContext)
{
    DMA_SBLOCK_CONTEXT SingleBlockCtxt;
    size_t offset = 0;
    DWORD  CurrentTransferredLength = 0;
    offset = pTransContext->TotalTransferredLength;

    CurrentTransferredLength = min(pTransContext->TransferSizeInByte - offset,DMAH_CHX_MAX_BLK_SIZE);
    pTransContext->LastTransferredLength = CurrentTransferredLength;

    SingleBlockCtxt.DMAC_BASE = pDevice->DMAC_BASE;
    SingleBlockCtxt.TxRx = DMAC_Peripheral_TX;
    SingleBlockCtxt.RequestLine = pTransContext->RequestLineTX;
    SingleBlockCtxt.SourceAddress = pTransContext->SysMemPhysicalAddress + offset;
    SingleBlockCtxt.DestinationAddress = pTransContext->PeripheralFIFOPhysicalAddress;
    SingleBlockCtxt.TransferWidth = 0;
    SingleBlockCtxt.TransferSizeInByte = CurrentTransferredLength;

    SingleBlockTransfer(&SingleBlockCtxt,pTransContext->ChannelTX);

    return TRUE;
}

/*************************************************************************
@func ULONG   | ProgramDmaForSingleBlockRX    | Set the configuration for RX block

@Description: Set the configuration for each channel of the RX block 
@rdesc Returns TRUE on success, FALSE on failure 
*************************************************************************/
BOOL ProgramDmaForSingleBlockRX(PDEVICE_CONTEXT pDevice,PDMA_TRANS_CONTEXT pTransContext)
{

    ULONG TransferSize = 0;
    DMA_SBLOCK_CONTEXT SingleBlockCtxt;
    size_t offset = 0;
    DWORD  CurrentTransferredLength = 0;

    offset = pTransContext->TotalTransferredLength;
    CurrentTransferredLength = min(pTransContext->TransferSizeInByte - offset,DMAH_CHX_MAX_BLK_SIZE);
    pTransContext->LastTransferredLength = CurrentTransferredLength;


    SingleBlockCtxt.DMAC_BASE = pDevice->DMAC_BASE;
    SingleBlockCtxt.TxRx = DMAC_Peripheral_RX;
    SingleBlockCtxt.RequestLine = pTransContext->RequestLineRX;

    SingleBlockCtxt.SourceAddress = pTransContext->PeripheralFIFOPhysicalAddress;
    SingleBlockCtxt.DestinationAddress = pTransContext->SysMemPhysicalAddress + offset;

    SingleBlockCtxt.TransferWidth = 0;
    SingleBlockCtxt.TransferSizeInByte = CurrentTransferredLength;

    SingleBlockTransfer(&SingleBlockCtxt,pTransContext->ChannelRX);

    
    return TRUE;
}
/*************************************************************************
@func  HANDLE | DMAC_EnableSourceRequestRegister | Enable source transaction request

@Description: Set Write enable bit for DMA channel. 

*************************************************************************/
void DMAC_EnableSourceRequestRegister(PUCHAR DMAC_BASE,ULONG channel)
{
    ULONG mask= REQ_REG_WRITE_MASK;
    mask = mask << channel;
    //you must write a 1 to the ReqSrcReg/ReqDstReg before writing a 1 to the SglReqSrcReg/SglReqDstReg register
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+REQ_SRC_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+SGLREQ_SRC_REGISTER), mask);
}

/*************************************************************************
@func  HANDLE | DMAC_EnableDestinationRequestRegister | Enable destination transaction request

@Description: Set Write enable bit for DMA channel. 

*************************************************************************/
void DMAC_EnableDestinationRequestRegister(PUCHAR DMAC_BASE,ULONG channel)
{
    ULONG mask= REQ_REG_WRITE_MASK;
    mask = mask << channel;
    //you must write a 1 to the ReqSrcReg/ReqDstReg before writing a 1 to the SglReqSrcReg/SglReqDstReg register
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+REQ_DST_REGISTER), mask);
    WRITE_REGISTER_ULONG((volatile ULONG *)(DMAC_BASE+SGLREQ_DST_REGISTER), mask);
}


/*************************************************************************
@func  HANDLE | StartSingleBlockTransfer | Transfer single block data

@Description: This routines transfer the block data to FIFO.  

*************************************************************************/
void SingleBlockTransfer(DMA_SBLOCK_CONTEXT* pRegsContext, int channel)
{
    PUCHAR DMAC_BASE = pRegsContext->DMAC_BASE;
    UINT64 reg = 0;

    ClearInterruptOnChannel(DMAC_BASE,channel);

    UnMaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_TFR_REGISTER,channel);
    UnMaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_BLK_REGISTER,channel);
    UnMaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_ERR_REGISTER,channel);
    switch (pRegsContext->TxRx)
    {
    case DMAC_Peripheral_TX:
    case DMAC_Peripheral_RX_DUMMYWRITE:
        //DMA dst is FIFO
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_STR_REGISTER,channel);
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_DTR_REGISTER,channel);
        break;
    case DMAC_Peripheral_RX:
        //DMA src is FIFO
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_DTR_REGISTER,channel);
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_STR_REGISTER,channel);
        break;
    case DMAC_M2M:
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_STR_REGISTER,channel);
        MaskInterruptRegistersOnChannel(DMAC_BASE,OFFSET_MASK_DTR_REGISTER,channel);
        break;
    default:
        ASSERT(0);
    }



    //Program Source Address
    {
        DEC_REG_FIELD_TYPE(SARX) regSARx = {0};
        regSARx.SARX_SAR = pRegsContext->SourceAddress;
        reg = FORMAT_REG_BY_STRUCT(SARX,&regSARx);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_SARX_REGISTER,reg);
    }

    //Program Destination Address
    {
        DEC_REG_FIELD_TYPE(DARX) regDARx = {0};
        regDARx.DARX_DAR = pRegsContext->DestinationAddress;
        reg = FORMAT_REG_BY_STRUCT(DARX,&regDARx);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_DARX_REGISTER,reg);
    }

    //Program LLPx
    {
        DEC_REG_FIELD_TYPE(LLPX) regLLPx = {0};
        reg = FORMAT_REG_BY_STRUCT(LLPX,&regLLPx);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_LLPX_REGISTER,reg);
    }

    //Program CTLx
    {
        DEC_REG_FIELD_TYPE(CTLX) regCTLx = {0};
        regCTLx.CTLX_INT_EN = 1;
        regCTLx.CTLX_SRC_TR_WIDTH = pRegsContext->TransferWidth;
        regCTLx.CTLX_DST_TR_WIDTH = pRegsContext->TransferWidth;
        regCTLx.CTLX_SRC_MSIZE = 0;
        regCTLx.CTLX_DEST_MSIZE = 0;
        regCTLx.CTLX_LLP_DST_EN = 0;
        regCTLx.CTLX_LLP_SRC_EN = 0;

        regCTLx.CTLX_BLOCK_TS = pRegsContext->TransferSizeInByte / (CLTX_TR_WIDTH_MAP[regCTLx.CTLX_SRC_TR_WIDTH] / 8);
        regCTLx.CTLX_DONE = 0;


        switch (pRegsContext->TxRx)
        {
        case DMAC_Peripheral_TX:
            {
                //Write to Peripheral FIFO
                regCTLx.CTLX_TT_FC = CLT_TT_FC_Mem2Per_dmac;
                regCTLx.CTLX_SINC = CLT_SINC_DINC_Increment;
                regCTLx.CTLX_DINC = CLT_SINC_DINC_NoChange;
                break;
            }
        case DMAC_Peripheral_RX:
            {
                //Read from Peripheral FIFO
                regCTLx.CTLX_TT_FC = CLT_TT_FC_Per2Mem_dmac;
                regCTLx.CTLX_SINC = CLT_SINC_DINC_NoChange;
                regCTLx.CTLX_DINC = CLT_SINC_DINC_Increment;
                break;
            }
        case DMAC_Peripheral_RX_DUMMYWRITE:
            {
                //Write dummy data to Peripheral FIFO
                regCTLx.CTLX_TT_FC = CLT_TT_FC_Mem2Per_dmac;
                regCTLx.CTLX_SINC = CLT_SINC_DINC_NoChange;
                regCTLx.CTLX_DINC = CLT_SINC_DINC_NoChange;
                break;
            }
        case DMAC_M2M:
            {
                regCTLx.CTLX_TT_FC = CLT_TT_FC_Mem2Mem_dmac;
                regCTLx.CTLX_SINC = CLT_SINC_DINC_Increment;
                regCTLx.CTLX_DINC = CLT_SINC_DINC_Increment;
                break;
            }
        default:
            //not tested other Transfer Type yet
            ASSERT(0);
            break;
        } // switch (pRegsContext->TxRx)

        reg = FORMAT_REG_BY_STRUCT(CTLX,&regCTLx);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_CTLX_REGISTER,reg);
    }

    //Program CFG
    {
        DEC_REG_FIELD_TYPE(CFGX) regCFGx = {0};
        regCFGx.CFGX_FIFO_MODE = 0;
        regCFGx.CFGX_PROTCTL = 0;
        switch (pRegsContext->TxRx)
        {
        case DMAC_Peripheral_TX:
        case DMAC_Peripheral_RX_DUMMYWRITE:
            {
                //FIFO is DST
                regCFGx.CFGX_HS_SEL_DST = 0;
                regCFGx.CFGX_DEST_PER = pRegsContext->RequestLine;
                break;
            }
        case DMAC_Peripheral_RX:
            {
                //FIFO is SRC
                regCFGx.CFGX_HS_SEL_SRC = 0;
                regCFGx.CFGX_SRC_PER = pRegsContext->RequestLine;
                break;
            }
        case DMAC_M2M:
            {
                break;
            }
        default:
            //not tested other Transfer Type yet
            ASSERT(0);
            break;
        } // switch (pRegsContext->TxRx)

        reg = FORMAT_REG_BY_STRUCT(CFGX,&regCFGx);
        WRITE_CHANNEL_REGISTER(DMAC_BASE,channel,OFFSET_CFGX_REGISTER,reg);

    }

    //Enable Channel
    {
        DEC_REG_FIELD_TYPE(CHENREG) regCHENREG = {0};
        regCHENREG.CHENREG_CH_EN = 1 << channel;
        regCHENREG.CHENREG_CH_EN_WE = 1 << channel;
        reg = FORMAT_REG_BY_STRUCT(CHENREG,&regCHENREG);
        WRITE_DMAC_REGISTER(DMAC_BASE,OFFSET_CHENREG_REGISTER,reg);
    }

}

BOOL ResetChannel(PDEVICE_CONTEXT pDevice,DWORD channel)
{
    DWORD mask = INT_STATUS;
    mask = mask << channel;

    pDevice->ChannelContext[channel].pChannelLock->Lock();  
    pDevice->pDeviceLock->Lock();
    InterruptMask(pDevice->dwSysIntr,TRUE);  
    
    DisableChannel(pDevice->DMAC_BASE,channel);
    ClearInterruptOnChannel(pDevice->DMAC_BASE,channel);

    pDevice->IsrInfo.InterruptStatusTfrPerChannel[channel] = 0;
    
    InterruptMask(pDevice->dwSysIntr,FALSE);
    pDevice->pDeviceLock->Unlock();
    pDevice->ChannelContext[channel].pChannelLock->Unlock();
    
    
    return TRUE;

}
//Create a DMA Transaction
/*************************************************************************
@func BOOL    | CreateDMATransaction    | Prepare transaction context for 
each dma request
@Description: This routines create Transaction context Object from request 
context 
@rdesc Returns TRUE on success, FALSE on failure 
*************************************************************************/
BOOL CreateDMATransaction(PDEVICE_CONTEXT pDevice,PDMA_REQUEST_CTXT pDMARequest, DMA_TRANSACTION_TYPE TransType, DWORD channel,HANDLE hDmaCompleteEvent)
{
    BOOL status = TRUE;
    DMA_TRANSACTION_CONTEXT* pTransContext = NULL;

    pTransContext = &pDevice->ChannelContext[channel].TransContext;

    memset(pTransContext,0,sizeof(*pTransContext));

    pTransContext->TransactionType = TransType;

    pTransContext->TotalTransferredLength = 0;
    pTransContext->LastTransferredLength = 0;

    //assigning channel from request context
    pTransContext->ChannelTX = pDMARequest->ChannelTx;
    pTransContext->ChannelRX = pDMARequest->ChannelRx;


    //need to get max transfer length and fragment it here
    pTransContext->RequestLineTX = pDMARequest->RequestLineTx;
    pTransContext->RequestLineRX = pDMARequest->RequestLineRx;


    pTransContext->PeripheralFIFOPhysicalAddress = pDMARequest->PeripheralFIFOPhysicalAddress;
    pTransContext->SysMemPhysicalAddress =(ULONG)(pDMARequest->SysMemPhysAddress.LowPart);


    pTransContext->DummyDataWidthInByte = pDMARequest->DummyDataWidthInByte;
    pTransContext->DummyDataForI2CSPI = pDMARequest->DummyDataForI2CSPI;
    pTransContext->TransferSizeInByte = pDMARequest->TransactionSizeInByte;
    pTransContext->hDmaCompleteEvent = hDmaCompleteEvent;

    pTransContext->pBytesReturned = pDMARequest->pBytesReturned;

    if (pDMARequest->TotalTimeOutInMs == 0)
    {
        ASSERT(0);
    }

    switch (pDMARequest->IntervalTimeOutInMs)
    {
    case 0:
        pTransContext->IntervalTimeOutInMS = INFINITE;
        pTransContext->TotalTimeOutInMS = pDMARequest->TotalTimeOutInMs;

        if (pTransContext->TotalTimeOutInMS == 0
            || pTransContext->TotalTimeOutInMS == INFINITE)
        {
            pTransContext->TotalTimeOutInMS = DEFAULT_TOTALTIMEOUT_IN_MS;
        }

        break;
    case MAXDWORD:
        pTransContext->IntervalTimeOutInMS = INFINITE;

        pTransContext->TotalTimeOutInMS = pDMARequest->TotalTimeOutInMs;

        if (pTransContext->TotalTimeOutInMS == 0)
        {
            pTransContext->TotalTimeOutInMS = DEFAULT_TOTALTIMEOUT_IN_MS;
        }
        ASSERT(pDMARequest->TransactionSizeInByte == 1);
        break;
    default:
        pTransContext->IntervalTimeOutInMS = pDMARequest->IntervalTimeOutInMs;
        pTransContext->TotalTimeOutInMS = pDMARequest->TotalTimeOutInMs;
        if (pTransContext->TotalTimeOutInMS == 0)
        {
            pTransContext->TotalTimeOutInMS = DEFAULT_TOTALTIMEOUT_IN_MS;
        }
        break;
    }

    pTransContext->StartTickCount = GetTickCount();

    /*disable channel, set channel status, etc*/
    BOOL NeedResetChannel;
    pDevice->ChannelContext[channel].pChannelLock->Lock();
    NeedResetChannel = (pDevice->ChannelContext[channel].ChannelState != DMA_TRANSACTION_NOREQUEST);
    pDevice->ChannelContext[channel].pChannelLock->Unlock();

    if (NeedResetChannel)
    {
        ASSERT(0);
        ResetChannel(pDevice,channel);
    }

    ASSERT(IsChannelDisabled(pDevice->DMAC_BASE,channel));
    pDevice->ChannelContext[channel].ChannelState = DMA_TRANSACTION_STOP;
    pDevice->ChannelContext[channel].ChannelNotifyCount = 0x100;
    status = SetEvent(pDevice->ChannelContext[channel].ChannelEvent);

    return status;

}

/*************************************************************************
@func  HANDLE | DMA_Open | Open the GPIO device

@Description: This routine must be called by the user to open the
GPIO device. The HANDLE returned must be used by the application in
all subsequent calls to the GPIO driver. This routine initialize 
the device's data

@rdesc This routine returns a HANDLE representing the device.
*************************************************************************/

DWORD DMA_Open(DWORD hOpenContext, DWORD AccessCode, DWORD ShareMode)
{
    DWORD dwRet =(DWORD)NULL;

    DMA_DEVICE_CTXT *pDevice = (DMA_DEVICE_CTXT*)hOpenContext;


    if(NULL == pDevice)
    {
        DEBUGMSG (TRUE,(TEXT("DMA: DMA_Open() - Failed to open device %d\r\n"),GetLastError()));
        return dwRet;
    }


    DEBUGMSG(TRUE,(TEXT("DMA: DMA_Open() END\n")));


    dwRet = (DWORD)pDevice;

    return dwRet;
} 


BOOL ProgramDmaForSingleBlockGeneral(PDEVICE_CONTEXT pDevice,PDMA_TRANS_CONTEXT pTransContext)
{
    ASSERT(pDevice);
    ASSERT(pTransContext);

    switch(pTransContext->TransactionType)
    {
       case TRANSACTION_TYPE_SINGLE_RX:
           ProgramDmaForSingleBlockRX(pDevice,pTransContext);
           break;
       case TRANSACTION_TYPE_SINGLE_TX:
           ProgramDmaForSingleBlockTX(pDevice,pTransContext);
           break;
       default:
           ASSERT(0);
    }

    return FALSE;
}

#define FINISHED_NORMAL 0x1
#define TFINISHED_TIMEOUTTOTAL 0x2
#define TFINISHED_TIMEOUINTERVAL 0x4


DWORD ChannelThreadFunc(LPVOID pvarg)
{
    PDMA_CHANNEL_CONTEXT pChannelContext;
    PDEVICE_CONTEXT pDevice;
    PDMA_TRANS_CONTEXT pTransContext;
    DWORD  dwWaitTime;
    size_t  TransferWatchDog;

    pChannelContext = (PDMA_CHANNEL_CONTEXT)pvarg;
    if (pChannelContext == NULL ||
        pChannelContext->pDeviceContext == NULL)
    {
        ASSERT(0);
        return (DWORD)-1;
    }

    pDevice = pChannelContext->pDeviceContext;
    dwWaitTime = INFINITE;
    TransferWatchDog = 0;

    /* while loop to wait a singal*/
    while (!pDevice->bIsTerminated)
    {
        DWORD bResult;
        BOOL Rtn;
        BOOL  TransferFinished;
        DWORD TransferFinishedReason;
        BOOL SuccessDisabled;

        pTransContext = &pChannelContext->TransContext;
        SuccessDisabled = TRUE;
        TransferFinished = FALSE;
        TransferFinishedReason = 0;

        bResult = WaitForSingleObject(pChannelContext->ChannelEvent,dwWaitTime);

        /* Must Lock after ChannelEvent is signaled*/
        pChannelContext->pChannelLock->Lock(); 

#ifdef _DEBUG
        DWORD ChannelNotifyCount = pDevice->ChannelContext[pChannelContext->Channel].ChannelNotifyCount;
        DWORD tf = pDevice->IsrInfo.InterruptStatusTfrPerChannel[pChannelContext->Channel];
        DEBUGMSG (ZONEID_ERR,(TEXT("DMA: ChannelThreadFunc Channel %d WaitForSingleObject %d ChannelNotifyCount %d TFR %d\r\n"), pChannelContext->Channel,bResult,ChannelNotifyCount,tf));
#endif
        DMA_CHANNEL_STATUS ChannelState;
        ChannelState = pChannelContext->ChannelState;
        ASSERT(ChannelState != DMA_TRANSACTION_NOREQUEST);

        if ( bResult == WAIT_TIMEOUT && ChannelState == DMA_TRANSACTION_STOP)
        {
            /*WaitForSingleObject is time out but IST is already lock the ChannelLock
            and then singal the ChannelEvent.
            In this case,  the ChannelEvent's state remains signaled and we need reset event
            */
            ResetEvent(pChannelContext->ChannelEvent);
        }

        if (bResult == WAIT_OBJECT_0 && ChannelState != DMA_TRANSACTION_STOP)
        {
            /*Every time if ChannelEvent is signaled, the channel should be in DMA_TRANSACTION_STOP state*/
            DEBUGMSG (TRUE,(TEXT("DMA:Channel is in Wrong state DMA_TRANSACTION_IN_TRANSFER\r\n")));
            ASSERT(0);
        }

        DWORD CurrentTickCount = 0;
        CurrentTickCount = GetTickCount();
        if (CurrentTickCount - pTransContext->StartTickCount > pTransContext->TotalTimeOutInMS)
        {
            /* When total time out */
            if (ChannelState != DMA_TRANSACTION_STOP)
            {
                DWORD len;
                SuccessDisabled = DisableChannelBeforeCompleted(pDevice->DMAC_BASE,pChannelContext->Channel);
                len = GetChannelTransferSizeInByte(pDevice->DMAC_BASE,pChannelContext->Channel);
                pChannelContext->ChannelState = DMA_TRANSACTION_STOP;
                pTransContext->TotalTransferredLength += len;
            }
            TransferFinished = TRUE;
            TransferFinishedReason = TFINISHED_TIMEOUTTOTAL;
        }
        else
        {
            /* no total time out*/
            switch (ChannelState)
            {
            case DMA_TRANSACTION_STOP:
                {
                    TransferFinished = 
                        (pTransContext->TotalTransferredLength < pTransContext->TransferSizeInByte)?FALSE:TRUE;

                    if (!TransferFinished)
                    {
                        DWORD Tfr;

                        /* before start a tranfer, the tfr of channel in IST should be zero*/
                        Tfr = pDevice->IsrInfo.InterruptStatusTfrPerChannel[pChannelContext->Channel];
                        ASSERT(Tfr == 0);

                        TransferWatchDog = pTransContext->TotalTransferredLength;
                        Rtn = ProgramDmaForSingleBlockGeneral(pDevice,&pChannelContext->TransContext);
                        pChannelContext->ChannelState = DMA_TRANSACTION_IN_TRANSFER;

                    }else
                    {
                        TransferFinishedReason = FINISHED_NORMAL;
                    }
                    break;
                }
            case DMA_TRANSACTION_IN_TRANSFER:
                {
                    /*need sync to IST to  pTransContext->TotalTransferredLength */
                    DWORD len;
                    len = GetChannelTransferSizeInByte(pDevice->DMAC_BASE,pChannelContext->Channel);

                    if (pTransContext->TotalTransferredLength > TransferWatchDog)
                    {
                        /* some sync issue happened*/
                        ASSERT(0);
                    }

                    if (pTransContext->TotalTransferredLength + len > TransferWatchDog)
                    {
                        /*transfer is living, just wait for next*/
                        TransferFinished = FALSE;
                        TransferWatchDog = pTransContext->TotalTransferredLength + len;
                    }
                    else
                    {
                        /* no new data transfered , need cancel*/
                        SuccessDisabled = DisableChannelBeforeCompleted(pDevice->DMAC_BASE,pChannelContext->Channel);
                        len = GetChannelTransferSizeInByte(pDevice->DMAC_BASE,pChannelContext->Channel);
                        pChannelContext->ChannelState = DMA_TRANSACTION_STOP;
                        pTransContext->TotalTransferredLength += len;
                        TransferFinished = TRUE;
                        TransferFinishedReason = TFINISHED_TIMEOUINTERVAL;
                    }
                    break;    
                }

            default:
                ASSERT(0);
            }
        }

        /* Get next waiting time */
        DWORD PassedTime = GetTickCount() - pTransContext->StartTickCount;
        if (pChannelContext->TransContext.IntervalTimeOutInMS == INFINITE)
        {
            /* No Interval TimeOut, only wait TotalTimeout*/
            dwWaitTime = max(PassedTime, pTransContext->TotalTimeOutInMS) - PassedTime;
        }
        else
        {
            ASSERT(pChannelContext->TransContext.IntervalTimeOutInMS < pTransContext->TotalTimeOutInMS);
            dwWaitTime = max(PassedTime, pTransContext->TotalTimeOutInMS) - PassedTime;
            dwWaitTime = min(dwWaitTime,pChannelContext->TransContext.IntervalTimeOutInMS);
        }

        if (TransferFinished)
        {
            dwWaitTime = INFINITE;/*Wait next dma request*/
            ASSERT(pTransContext->pBytesReturned);
            if (pTransContext->pBytesReturned)
            {
                *(pTransContext->pBytesReturned) = pTransContext->TotalTransferredLength;
            }
            TransferWatchDog = 0;

            ASSERT(pChannelContext->ChannelState == DMA_TRANSACTION_STOP);
            pChannelContext->ChannelState = DMA_TRANSACTION_NOREQUEST;
            pTransContext->pBytesReturned = NULL;

            pDevice->ChannelContext[pChannelContext->Channel].ChannelNotifyCount=0;
        }

        if (TransferFinishedReason == TFINISHED_TIMEOUTTOTAL
            || TransferFinishedReason == TFINISHED_TIMEOUINTERVAL)
        {
            DWORD channel;
            channel = pChannelContext->Channel;
            /* we clean all interrupting in the cancel case*/

            if (!SuccessDisabled)
            {
                DEBUGMSG (TRUE,(TEXT("DMA:DisableChannelBeforeCompleted failed for channel %d\r\n"),channel));
            }

            pDevice->pDeviceLock->Lock();     
            InterruptMask(pDevice->dwSysIntr,TRUE);         /*sync to ISR*/
            
            ASSERT(IsChannelDisabled(pDevice->DMAC_BASE,channel));
            ClearInterruptOnChannel(pDevice->DMAC_BASE,channel);
            pDevice->IsrInfo.InterruptStatusTfrPerChannel[channel] = 0;

            InterruptMask(pDevice->dwSysIntr,FALSE);
            pDevice->pDeviceLock->Unlock();
        }

        pChannelContext->pChannelLock->Unlock();

        /* after all, we notify other driver the dma request is completed */
        /* if it is an TX RX combined request for SPI, we only notify once*/
        if (TransferFinished
            && pTransContext->hDmaCompleteEvent)
        {
            SetEvent(pTransContext->hDmaCompleteEvent);
        }

    }
    return 0;
}
/*************************************************************************
@func BOOL        | DMA_IOControl | Device IO control routine
@parm pGPIOhandle  | hOpenContext  | DMA handle
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
BOOL DMA_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn, 
                   PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
    BOOL status = TRUE;
    PPOWER_CAPABILITIES pPowerCaps = NULL;
    DMA_DEVICE_CTXT *pDevice = (DMA_DEVICE_CTXT*)hOpenContext;
    PDMA_REQUEST_CTXT pDMARequest = NULL;

    pDMARequest = (PDMA_REQUEST_CTXT)pBufIn;

    if(!pDevice)
    {
        DEBUGMSG (TRUE,(TEXT("DMA:DMA_IOControl - Failed to open device %d\r\n"),pDevice));
        return FALSE;
    }

    if (!pDMARequest)
    {
        DEBUGMSG (TRUE,(TEXT("DMA:DMA_IOControl - DMA Request is Empty\r\n")));
        return FALSE;
    }

    switch (dwCode) {
    case IOCTL_LPSSDMA_REQUEST_DMA_SPI_TX:
    case IOCTL_LPSSDMA_REQUEST_DMA_SPI_RX:
        ASSERT(pDMARequest->TransactionType == TRANSACTION_TYPE_SINGLE_TXRX);
        ASSERT(pDMARequest->IntervalTimeOutInMs == 0);

        CreateDMATransaction(pDevice,
            pDMARequest,
            TRANSACTION_TYPE_SINGLE_RX,
            pDMARequest->ChannelRx,
            pDMARequest->hDmaCompleteEvent);

        CreateDMATransaction(pDevice,
            pDMARequest,
            TRANSACTION_TYPE_SINGLE_TX,
            pDMARequest->ChannelTx,
            NULL);
        break;
    case IOCTL_LPSSDMA_REQUEST_DMA_UART_TX:
        ASSERT(pDMARequest->TransactionType == TRANSACTION_TYPE_SINGLE_TX);
        CreateDMATransaction(pDevice,
            pDMARequest,
            pDMARequest->TransactionType,
            pDMARequest->ChannelTx,
            pDMARequest->hDmaCompleteEvent);
        break;

    case IOCTL_LPSSDMA_REQUEST_DMA_UART_RX:
        ASSERT(pDMARequest->TransactionType == TRANSACTION_TYPE_SINGLE_RX);

        CreateDMATransaction(pDevice,
            pDMARequest,
            pDMARequest->TransactionType,
            pDMARequest->ChannelRx,
            pDMARequest->hDmaCompleteEvent);

        //*pdwActualOut = pDevice->dwTransferredLength;

        break;
    case IOCTL_POWER_CAPABILITIES:
        pPowerCaps = (PPOWER_CAPABILITIES)pBufOut;     
        memset(pPowerCaps, 0, sizeof(POWER_CAPABILITIES));

        // set full powered and non powered state
        pPowerCaps->DeviceDx = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
        pPowerCaps->WakeFromDx = DX_MASK(D0) | DX_MASK(D3);
        *pdwActualOut = sizeof(POWER_CAPABILITIES);
        status= TRUE;
        break;
    default:
        DEBUGMSG(1,(TEXT("DMA: DMA_IOControl()  Invalid index: %d\n"),dwCode));
        status= FALSE;
        break;
    }

    return status;
} 
/*************************************************************************
@func BOOL     | GPI_Close | close the GPIO device.
@parm GPIO_handle    | hOpenContext | Context pointer returned from GPI_Open
@rdesc TRUE if success; FALSE if failure
@remark This routine is called by the device manager to close the device.
*************************************************************************/
BOOL DMA_Close(DWORD hOpenContext)
{
    BOOL bRet = TRUE;

    DMA_DEVICE_CTXT *pDevice = (DMA_DEVICE_CTXT*)hOpenContext;

    //check for correct device context
    if(pDevice == NULL)
    {
        DEBUGMSG (TRUE,(TEXT("DMA:DMA_Close - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;

    }

    DEBUGMSG(1,(TEXT("DMA: DMA_Close() END\n")));
    return bRet;
} 
/*************************************************************************
@func  BOOL | GPI_Deinit | De-initialize GPIO
@parm     GPIO_handle | hDeviceContext | Context pointer returned from GPI_Init
@rdesc None.
*************************************************************************/
BOOL DMA_Deinit(DWORD hDeviceContext)
{
    BOOL bRet = TRUE;

    /* Unallocate the memory*/
    DMA_DEVICE_CTXT *pDevice = (DMA_DEVICE_CTXT*)hDeviceContext;

    //check for correct device context
    if(pDevice == NULL)
    {
        DEBUGMSG (TRUE,(TEXT("DMA:DMA_Deinit - Incorrect device context. %d\r\n"),GetLastError()));
        return bRet = FALSE;

    }

    //uninitialize the controller
    //DeinitSpiController(pDevice);

    /* close interrupt thread */
    if(pDevice->hInterruptThread ) 
    {
        if (pDevice->InterruptThreadStarted && pDevice->hInterruptEvent)
        {
            pDevice->dwISTLoopTrack = FALSE;
            SetEvent(pDevice->hInterruptEvent);
            WaitForSingleObject(pDevice->hInterruptThread, INFINITE);
        }
        CloseHandle(pDevice->hInterruptThread);    /* close handle */
		pDevice->hInterruptThread = NULL;
    }

 

    /* disable interrupt */
    if(pDevice->dwSysIntr != 0) {
        InterruptDisable(pDevice->dwSysIntr);
        KernelIoControl(IOCTL_HAL_RELEASE_SYSINTR, &pDevice->dwSysIntr, 
            sizeof(pDevice->dwSysIntr), NULL, 0, NULL);

        if (pDevice->hInterruptEvent)
        {
            CloseHandle(pDevice->hInterruptEvent); 
            pDevice->hInterruptEvent = NULL;
        }
    }

    if (pDevice->hDeviceKey)
    {
        RegCloseKey (pDevice->hDeviceKey);
        pDevice->hDeviceKey = NULL;
    }
    if (pDevice->hBusAccess)
    {
        CloseHandle(pDevice->hBusAccess);
        pDevice->hBusAccess = NULL;
    }
    if (pDevice->pDeviceLock)
    {
        delete pDevice->pDeviceLock;
    }

    if (pDevice->hIsrHandler)
    {
        FreeIntChainHandler(pDevice->hIsrHandler);
    }
    /* Free device structure */
    LocalFree(pDevice); 

    DEBUGMSG(1,(TEXT("DMA:DMA_Deinit() END\n")));

    return bRet;
} 