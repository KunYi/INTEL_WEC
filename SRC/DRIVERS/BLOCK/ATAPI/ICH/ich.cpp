//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2011 Intel Corporation All Rights Reserved. 
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
//
#include <atamain.h>
#include <atapipci.h>
#include <diskmain.h>
#include <ceddk.h>
#include <pm.h>
#include <ich.h>
#include "debug.h"

EXTERN_C DWORD
PCIConfig_ReadDword(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C WORD
PCIConfig_ReadWord(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C BYTE
PCIConfig_ReadByte(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset
    );

EXTERN_C void
PCIConfig_Write(
    ULONG BusNumber,
    ULONG Device,
    ULONG Function,
    ULONG Offset,
    ULONG Value,
    ULONG Size = sizeof(DWORD)
    );

// global this pointer
CIch *pICH;

EXTERN_C
CDisk *
CreateIch(HKEY hDevKey)
{
    DEBUGMSG(ZONE_INIT,(TEXT("CreateIch (built %s %s)\r\n"),TEXT(__DATE__),TEXT(__TIME__) ));
    return new CIch(hDevKey);
}

BOOL CIch::Init(HKEY hActiveKey)
{
    BOOL bRet;
    
    DEBUGMSG(ZONE_INIT,(TEXT("CIch::Init\r\n")));
    // Our derived class's SetBestTransferMode function needs to be call via a this pointer because our Init calls
    //  CPCIDisk::Init who in turn changes scope to CDisk::Init who then calls SetBestTransferMode and the only 
    //  way to get back to our SetBestTransferMode is via the pICH this pointer.  Our other derived functions are fine.
    m_dwDeviceFlags |= DFLAGS_DEVICE_ICH;
    pICH = this;
    bRet = CPCIDisk::Init(hActiveKey);
    if (bRet) {
        if ( m_fUseLBA48 && !(m_Id.GeneralConfiguration == 0x848A) && (m_Id.CommandSetFeatureEnabled2 & (1<<10)) )
        {    // flag 48-bit LBA support
            m_ICHFlags |= ICH_LBA48_SUPPORT;
        }
    }
    return bRet;
}

BOOL CIch::SendIO48Command(DWORD dwStartSector, DWORD dwNumberOfSectors, BYTE bCmd)
{
    
    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:SendIO48Command - Sector %d SectorsLeft %x Command %x\r\n"), dwStartSector,dwNumberOfSectors,bCmd));

    if(ZONE_CELOG) CeLogData(TRUE, CELID_ATAPI_IOCOMMAND, &bCmd, sizeof(bCmd), 0, CELZONE_ALWAYSON, 0, FALSE);

    // Prerequisite - DRDY set to 1
    SelectDevice(); 
    if (WaitOnBusy(FALSE)) 
    {
        DEBUGMSG(ZONE_ERROR, 
            (TEXT("CIch:SendIO48Command - HI1: Check_Status after device select failed (DevId %x) CMD(%02X) status(%02X), error(%02X), reason(%02X) \r\n"),
            m_dwDeviceId, bCmd, GetAltStatus(), GetError(), GetReason()));
        return (FALSE);
    }
    //WaitForDisc(WAIT_TYPE_READY, 1000);

    // write the 16bit sector count to sector count register, "most recently written" and "previous content"
    WriteSectorCount((BYTE)(dwNumberOfSectors>>8));    // sector count 15:8
    WriteSectorCount((BYTE)dwNumberOfSectors);         // sector count 7:0

    // write the 48bit LBA to the sector #, cyl low, cyl high, "most recently written" and "previous content"
    WriteLBAHigh(0); //(BYTE)(dwStartSector >> 40));
    WriteLBAMid(0); //(BYTE)(dwStartSector >> 32));
    WriteLBALow((BYTE)(dwStartSector >> 24));
    WriteLBAHigh((BYTE)(dwStartSector >> 16));
    WriteLBAMid((BYTE)(dwStartSector >> 8));
    WriteLBALow((BYTE)dwStartSector);

    // write LBA + DEV
    //WriteDriveHeadReg((BYTE)(ATA_HEAD_LBA_MODE | ((m_dwDevice == 0 ) ? ATA_HEAD_DRIVE_1 : ATA_HEAD_DRIVE_2)));
    SelectDeviceLBA();
                            
    WriteCommand(bCmd);
    return (TRUE);
}   

/*------------------------------------------------------------------------------------------*/

DWORD CIch::ReadWriteDiskDMA(PIOREQ pIOReq, BOOL fRead)
{
    DWORD dwError = ERROR_SUCCESS;
    PSG_REQ pSgReq = (PSG_REQ)pIOReq->pInBuf;
    DWORD dwSectorsToTransfer;
    SG_BUF CurBuffer[MAX_SG_BUF];
    BYTE bCmd;

    // 48-bit LBA?  if not, let the stock ReadWriteDiskDMA handle it...
    if ((m_ICHFlags & ICH_LBA48_SUPPORT) == 0)
        return CDisk::ReadWriteDiskDMA(pIOReq,fRead);

    DEBUGMSG( ZONE_IOCTL, (TEXT("ATAPI:CIch::ReadWriteDiskDMA48 Entered\r\n")));

    if ((pSgReq == NULL) ||
       (pIOReq->dwInBufSize < sizeof(SG_REQ))) {
        return ERROR_INVALID_PARAMETER;
    }   
    if ((pSgReq->sr_num_sec == 0) || 
       (pSgReq->sr_num_sg == 0)) {
        return  ERROR_INVALID_PARAMETER;
    }       

    if ((pSgReq->sr_start + pSgReq->sr_num_sec) > m_DiskInfo.di_total_sectors) {
        return ERROR_SECTOR_NOT_FOUND;
    }

    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:CIch::ReadWriteDiskDMA48 StartSector=%ld NumSectors=%ld NumSg=%ld\r\n"), pSgReq->sr_start, pSgReq->sr_num_sec, pSgReq->sr_num_sg));

    GetBaseStatus(); // Clear Interrupt if it is already set 

    DWORD dwStartBufferNum = 0, dwEndBufferNum = 0, dwEndBufferOffset = 0;
    DWORD dwNumSectors = pSgReq->sr_num_sec;
    DWORD dwStartSector = pSgReq->sr_start;

    // Process the SG buffers in blocks of MAX_SECT_PER_COMMAND.  Each DMA request will have a new SG_BUF array 
    // which will be a subset of the original request, and may start/stop in the middle of the original buffer.
    while (dwNumSectors) {

        dwSectorsToTransfer = (dwNumSectors > MAX_SECT_PER_COMMAND) ? MAX_SECT_PER_COMMAND : dwNumSectors;
    
        DWORD dwBufferLeft = dwSectorsToTransfer * BYTES_PER_SECTOR;
        DWORD dwNumSg = 0;

        while (dwBufferLeft) {
            DWORD dwCurBufferLen = pSgReq->sr_sglist[dwEndBufferNum].sb_len - dwEndBufferOffset;

            if (dwBufferLeft < dwCurBufferLen) {
                // The buffer left for this block is less than the current SG buffer length
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_buf = pSgReq->sr_sglist[dwEndBufferNum].sb_buf + dwEndBufferOffset;
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_len = dwBufferLeft;
                dwEndBufferOffset += dwBufferLeft;
                dwBufferLeft = 0;
            } else {
                // The buffer left for this block is greater than or equal to the current SG buffer length.  Move on to the next SG buffer.
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_buf = pSgReq->sr_sglist[dwEndBufferNum].sb_buf + dwEndBufferOffset;
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_len = dwCurBufferLen;
                dwEndBufferOffset = 0;
                dwEndBufferNum++;
                dwBufferLeft -= dwCurBufferLen;
            }    
            dwNumSg++;
        }
      
        bCmd = fRead ? ATA_CMD_READ_DMA_EXT : ATA_CMD_WRITE_DMA_EXT;

        WaitForInterrupt(0);    // Clear Interrupt 
        
        if (!SetupDMA(CurBuffer, dwNumSg, fRead)) {
            dwError = fRead ? ERROR_READ_FAULT : ERROR_WRITE_FAULT;
            DEBUGMSG(ZONE_ERROR, 
                (TEXT("ATAPI:CIch::ReadWriteDMA48 - SetupDMA failed (Cmd 0x%X) (DevId %x) status(%02X), error(%02X), reason(%02X) \r\n"),
                bCmd, m_dwDeviceId, GetAltStatus(), GetError(), GetReason()));
            goto ExitFailure;
        }    
        
        // set the 48-bit LBA address
        if (!SendIO48Command(dwStartSector, dwSectorsToTransfer, bCmd)) {
            dwError = fRead ? ERROR_READ_FAULT : ERROR_WRITE_FAULT;
            DEBUGMSG(ZONE_ERROR, 
                (TEXT("ATAPI:CIch::ReadWriteDMA48 - SendIO48Command failed (Cmd 0x%X) (DevId %x) status(%02X), error(%02X), reason(%02X) \r\n"),
                bCmd, m_dwDeviceId, GetAltStatus(), GetError(), GetReason()));
            AbortDMA();
            goto ExitFailure;
        }

        if (BeginDMA(fRead)) {
            if (m_fInterruptSupported) {
                if (!WaitForInterrupt(m_dwDiskIoTimeOut) || (CheckIntrState() == ATA_INTR_ERROR)) {  
                    DEBUGMSG(ZONE_ERROR, 
                        (TEXT("ATAPI:CIch::ReadWriteDMA48 - WaitforInterrupt failed (Cmd 0x%X) (DevId %x) status(%02X), error(%02X), reason(%02X) BMStatus(%02X) \r\n"),
                        bCmd, m_dwDeviceId, GetAltStatus(), GetError(), GetReason(), ReadBMStatus()));
                    dwError = ERROR_READ_FAULT;
                    AbortDMA();
                    goto ExitFailure;
                }
            } 
      
            if (EndDMA()) {
                WaitOnBusy(FALSE);
                CompleteDMA( CurBuffer, pSgReq->sr_num_sg, fRead);
            }    
        }   

        dwStartSector += dwSectorsToTransfer;
        dwStartBufferNum = dwEndBufferNum;
        dwNumSectors -= dwSectorsToTransfer;        
    }    
    
ExitFailure:    
    if (ERROR_SUCCESS != dwError) {
        ResetController(FALSE);
        if (m_fInterruptSupported) {
            WriteAltDriveController(ATA_CTRL_ENABLE_INTR);
        }
    }    
     pSgReq->sr_status = dwError;
    return dwError;
}

BYTE CIch::FetchBestTransferMode(bool fPIOOnly) 
{
    BOOL f40PinCable;

    // initialize transfer mode to PIO default mode, IORDY disabled
    m_bBestMode = 0x01;

    // determine whether IORDY is supported
    if (m_Id.Capabilities & IDENTIFY_CAPABILITIES_IOREADY_SUPPORTED) 
    {
        DEBUGMSG(ZONE_INIT, (_T("    IORDY supported\r\n")));
        // mark IORDY enabled in transfer mode
        m_bBestMode = 0x00;
    }

    // TranslationFieldsValid <=> Word 53
    if (m_Id.TranslationFieldsValid & 0x0002) 	//Check to see if fields reported in words (70:64) are valid
    {
        if (m_Id.MinimumPIOxferTimeWOFlow <= 0x0258) // cycle time = 600
        {
            m_bBestMode = ATA_PIO_FCT_MODE | 0;
        }
        if (m_Id.MinimumPIOxferTimeWOFlow <= 0x017f) // cycle time = 383
        {
            m_bBestMode = ATA_PIO_FCT_MODE | 1;
        } 
        if (m_Id.MinimumPIOxferTimeWOFlow <= 0x00f0) // cycle time = 240
        {
            m_bBestMode = ATA_PIO_FCT_MODE | 2;
        }

        // Word 64; determine "best" supported PIO mode
        if (m_Id.AdvancedPIOxferreserved & 0x01) // cycle time = 180
        {
            m_bBestMode = ATA_PIO_FCT_MODE | 3;
        }
        if (m_Id.AdvancedPIOxferreserved & 0x02) // cycle time = 120
        {
            m_bBestMode = ATA_PIO_FCT_MODE | 4;
        }
    }

    if (!fPIOOnly)
    {
        DEBUGMSG(ZONE_INIT,(TEXT("CIch::FetchBestTransferMode - CF card support value:GeneralConfiguration=%x,CommandSetSupported2=%x\r\n"),m_Id.GeneralConfiguration,m_Id.CommandSetSupported2)); 
#ifdef MWDMA 
        // MultiDMAModesSupported <=> Word 63, low-byte
        // determine "best" supported Multiword DMA mode
        if( m_Id.GeneralConfiguration != 0x848A && m_Id.GeneralConfiguration!=0x044A ){
            if (m_Id.MultiDmaModesSupported & 0x04) {
                m_bBestMode = ATA_DMA_MULTI_WORD_MODE | 0x2;
            }
            else if (m_Id.MultiDmaModesSupported & 0x02) {
                m_bBestMode = ATA_DMA_MULTI_WORD_MODE | 0x1;
            }
            else if (m_Id.MultiDmaModesSupported & 0x01) {
                m_bBestMode = ATA_DMA_MULTI_WORD_MODE | 0x0;
            }
        }
#endif 

        // TranslationFieldsValid <=> Word 53; UltraDMASupport <=> Word 88, low-byte
        // dump supported Ultra DMA modes
        if (m_Id.TranslationFieldsValid & 0x0004) {    //Check to see if fields reported in words 88 are valid

            if (m_Id.UltraDMASupport & 0x20) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x5;
            }
            else if (m_Id.UltraDMASupport & 0x10) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x4;
            }
            else if (m_Id.UltraDMASupport & 0x08) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x3;
            }
            else if (m_Id.UltraDMASupport & 0x04) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x2;
            }
            else if (m_Id.UltraDMASupport & 0x02) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x1;
            }
            else if (m_Id.UltraDMASupport & 0x01) {
                m_bBestMode = ATA_DMA_ULTRA_MODE | 0x0;
            }

            if (((m_bBestMode & ATA_DMA_ULTRA_MODE) == ATA_DMA_ULTRA_MODE)) 
            { 
                // only get Ultra DMA modes > 2 if you have an 80-pin cable so check the Identify Data word 93 bit 13
            //   if (m_pPort->m_pController->m_pIdeReg->dwPinCableDetection == 0x01)
            //   {
            //       f40PinCable = (m_Id.HardwareResetResult & (1<<13)) ? FALSE : TRUE;
            //   }
            //   else
            //   {
                DWORD Bus = m_pPort->m_pController->m_dwi.dwBusNumber;
                DWORD Device = m_pPort->m_pController->m_ppi.dwDeviceNumber;
                DWORD Function = m_pPort->m_pController->m_ppi.dwFunctionNumber;

                    ULONG val = PCIConfig_ReadDword(Bus, Device, Function, 0x54);

                    if (m_dwPort == FALSE) // Primary
                    {
                        if ((m_dwDeviceId & 1) == FALSE) // Master
                        {
                            f40PinCable = (val & 0x00000010) ? FALSE : TRUE;
                        }
                        else // Slave
                        {
                            f40PinCable = (val & 0x00000020) ? FALSE : TRUE;
                        }
                    }
                    else // Secondary
                    {
                        if ((m_dwDeviceId & 1) == FALSE) // Master
                        {
                            f40PinCable = (val & 0x00000040) ? FALSE : TRUE;
                        }
                        else // Slave
                        {
                            f40PinCable = (val & 0x00000080) ? FALSE : TRUE;
                        }
                    }
                }
//++++++++++++++ For cable detection :  use Word 93 of Identify Device data instead of the register 0x54 ++++++++++++++++
                    if ((m_Id.HardwareResetResult & (1<<13)) == 0)
                        f40PinCable = TRUE;
                    else 
                        f40PinCable = FALSE;
//-------------------------------------------------------------------------------------------            
                
                if ( (f40PinCable) && ((m_bBestMode & 0x7) > 2) )
                    { 
                        DEBUGMSG(ZONE_INIT,(TEXT("CIch::FetchBestTransferMode - no 80pin cable detected! reducing mode to 2.\r\n"))); 
                        m_bBestMode = (ATA_DMA_ULTRA_MODE | 0x2);
                    }
            // }
        }

        // make sure they haven't wildly set this from the registry setting but the drive won't support it!
        if (m_bBestMode <= (ATA_PIO_FCT_MODE | 4)) 
            m_fDMAActive = FALSE;
    }
    
    return m_bBestMode;
}

/*------------------------------------------------------------------------------------------*/

BOOL CIch::ResetController(BOOL bSoftReset)
{
    DWORD dwAttempts;
    BYTE   bStatus;
    

    DEBUGMSG(ZONE_INIT, (TEXT("ICH:ResetController entered\r\n")));


//    WriteAltDriveController( 0x8);
    // Read the status first
    bStatus = GetBaseStatus();
    DEBUGMSG(ZONE_INIT, (TEXT("ICH:ResetController - ATA_STATUS = %02X Error=%x!\r\n"), bStatus, GetError()));
    WaitForInterrupt(0);    // Clear Interrupt 

    //
    // We select the secondary device , that way we will get a response through the shadow
    // if only the master is connected.  Otherwise we will get a response from
    // the secondary device
    //
    
    WriteAltDriveController(ATA_CTRL_RESET | ATA_CTRL_DISABLE_INTR);

    WaitForDisc( WAIT_TYPE_BUSY, 500);
    
    WriteAltDriveController(ATA_CTRL_DISABLE_INTR);
    
    WaitForDisc( WAIT_TYPE_READY, 400);
    
    if (bSoftReset) {
        DEBUGMSG( ZONE_INIT, (TEXT("ICH:ResetController ...performing soft reset !\r\n")));
        AtapiSoftReset();
    } else {
        StallExecution(5000);
    }    

    for (dwAttempts = 0; dwAttempts < MAX_RESET_ATTEMPTS; dwAttempts++)
    {
        bStatus = GetBaseStatus();
        
        if ((bStatus != ATA_STATUS_IDLE) && (bStatus != 0x0))
        {
            DEBUGMSG( ZONE_INIT, (TEXT("ICH::ResetController: Current drive status = 0x%2.2X\r\n"), bStatus));
            
            if (bStatus == 0xFF)
            {
                SetDriveHead( ATA_HEAD_DRIVE_2);
                
                bStatus = GetBaseStatus();
                
                if ((bStatus != ATA_STATUS_IDLE) && (bStatus != 0x0))
                {
                    DEBUGMSG(ZONE_INIT, (TEXT("ICH::ResetController Drive 2 status = 0x%2.2X\r\n"), bStatus));
                    
                    if (bStatus == 0xFF)
                    {
                        DEBUGMSG(ZONE_INIT, (TEXT("ICH::ResetController no second device\r\n")));
                        return FALSE;
                    }
                }
                else
                {
                    break;
                }
            }
            StallExecution(5000);
        } else {
            break;
        }
    }
    if (dwAttempts == MAX_RESET_ATTEMPTS)
    {
        DEBUGMSG( ZONE_INIT, (TEXT("ICH:AtaResetController no response after %ld reset attempts\r\n"), MAX_RESET_ATTEMPTS));
        // soft reset device again.
              SelectDevice();
              AtapiSoftReset();
              bStatus = GetBaseStatus();
              if ((bStatus != ATA_STATUS_IDLE) && (bStatus != 0x0) && bStatus != 0xFF)
                     return FALSE;
              else
                     return TRUE;
    }
    DEBUGMSG( ZONE_INIT, (TEXT("ICH:ResetController: Controller reset done\r\n")));
    return TRUE;
} 

