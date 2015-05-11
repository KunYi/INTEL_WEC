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

// /////////////////////////////////////////////////////////////////////////////
// Notes
//
// DMA: The ScsiPassThroughDMA handles possible exceptions internally and resets
//      everything automatically and so there is no need to put a try/except
//      around these calls.
//
//      ReadCdRomDMA doesn't do this (and it is overloaded by both the ALi and
//      Promise code anyways.  This is why there is a try/except around calls
//      to this function.
//

#include <atamain.h>
#include <atapipcicd.h>

// /////////////////////////////////////////////////////////////////////////////
// AtapiIoctl
//
// pIOReq->pBytesReturned is safe (supplied by DSK_IOControl).  Exceptions are
// caught in CDisk::PerformIoctl.  And, we can map pInBuf and pOutBuf in-place,
// since they are copies setup by DSK_IOControl.
//
DWORD CPCIDiskAndCD::AtapiIoctl( PIOREQ pIOReq )
{
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG(ZONE_IOCTL, (TEXT("ATAPI:PerformIoctl: %x DeviceId: %x \r\n"), pIOReq->dwCode, m_dwDeviceId));

    switch( pIOReq->dwCode )
    {
        case IOCTL_DISK_GET_MEDIA_CHANGE_COUNT:
        {
            DWORD dwMediaChangeCount = 0;

            if( pIOReq->dwOutBufSize < sizeof(m_dwMediaChangeCount) )
            {
                dwError = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            if( pIOReq->pOutBuf )
            {
                dwError = ERROR_INVALID_PARAMETER;
                break;
            }

            dwMediaChangeCount = (DWORD)InterlockedCompareExchange( (LONG*)&m_dwMediaChangeCount,
                                                                    (LONG)m_dwMediaChangeCount,
                                                                    (LONG)m_dwMediaChangeCount );

            if( !CeSafeCopyMemory( pIOReq->pOutBuf,
                                   &dwMediaChangeCount,
                                   sizeof(dwMediaChangeCount) ) )
            {
                dwError = ERROR_INVALID_PARAMETER;
            }
            break;
        }
        case IOCTL_SCSI_PASS_THROUGH:
        {
            dwError = ScsiPassThrough( pIOReq );
            break;
        }
        case IOCTL_SCSI_PASS_THROUGH_DIRECT:
        {
            dwError = ScsiPassThroughDirect( pIOReq );
            break;
        }
        case IOCTL_CDROM_READ_SG:
            PUCHAR  savedoldptrs[MAX_SG_BUF];   // This will hold a copy of the user mode pointers that get overwritten
                                                // ValidateSg
            SG_BUF  mappedbufs[MAX_SG_BUF];     // Temporary dummy array to convert SGX_BUF to SG_BUF in the call to UnmapSG

            // Sterilize buffer.  Note that it's correct to pass the address of
            // of m_pSterileCdRomReadRequest, as it may be reallocated.
            if (FALSE == SterilizeCdRomReadRequest(
                &m_pSterileCdRomReadRequest,
                &m_cbSterileCdRomReadRequest,
                (PCDROM_READ)pIOReq->pInBuf,
                pIOReq->dwInBufSize,
                ARG_O_PTR,
                savedoldptrs,
                _countof(savedoldptrs))) {
                dwError = ERROR_INVALID_PARAMETER;
                break;
            }
            // Replace buffer with sterile container.
            pIOReq->pInBuf = (LPBYTE)m_pSterileCdRomReadRequest;
            DEBUGCHK(pIOReq->pInBuf);
            if (FALSE == AtapiIsUnitReadyEx()) {
                dwError = ERROR_NOT_READY;
                break;
            }
            // Execute IOCTL.
            dwError = ReadCdRom((CDROM_READ*)pIOReq->pInBuf, pIOReq->pBytesReturned);

            // Cleanup pointers mapped in SterilizeCdRomReadRequest

            ASSERT(m_pSterileCdRomReadRequest->sgcount <= MAX_SG_BUF);
            for(DWORD i = 0; i < m_pSterileCdRomReadRequest->sgcount; i++) {
                mappedbufs[i].sb_buf = m_pSterileCdRomReadRequest->sglist[i].sb_buf;
                mappedbufs[i].sb_len = m_pSterileCdRomReadRequest->sglist[i].sb_len;
            }

            if (FAILED(UnmapSg(
                        mappedbufs,
                        savedoldptrs,
                        m_pSterileCdRomReadRequest->sgcount,
                        ARG_O_PTR)))

            {
                ASSERT(!"Cleanup call to CeCloseCallerBuffer failed unexpectedly");
                dwError = ERROR_GEN_FAILURE;
            }
            break;
        case IOCTL_CDROM_RAW_READ:
            {
                // Technically, this IOCTL isn't documented, but it's defined
                // in cdioctl.h.
                CDROM_READ     CdRomRead;
                PRAW_READ_INFO pRawReadInfo = (PRAW_READ_INFO)pIOReq->pInBuf;
                // Validate buffers.
                if (NULL == pIOReq->pInBuf) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                if (NULL == pIOReq->pOutBuf) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Validate size.
                if (sizeof(RAW_READ_INFO) > pIOReq->dwInBufSize) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Execute IOCTL.
                if (FALSE == AtapiIsUnitReadyEx()) {
                    dwError = ERROR_NOT_READY;
                    break;
                }
                CdRomRead.StartAddr.Mode = CDROM_ADDR_LBA;
                CdRomRead.bRawMode = TRUE;
                CdRomRead.sgcount = 1;
                CdRomRead.TrackMode = CDDA;
                CdRomRead.StartAddr.Address.lba = pRawReadInfo->DiskOffset.LowPart;
                CdRomRead.TransferLength = (DWORD)(pRawReadInfo->SectorCount & 0xffffffff);
                if (FAILED(CeOpenCallerBuffer(
                    (PVOID *)&CdRomRead.sglist[0].sb_buf,
                    pIOReq->pOutBuf,
                    pIOReq->dwOutBufSize,
                    ARG_O_PTR,
                    FALSE))) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                CdRomRead.sglist[0].sb_len = pIOReq->dwOutBufSize;
                dwError = ReadCdRom(&CdRomRead, pIOReq->pBytesReturned);
                if (dwError != ERROR_SUCCESS) break;
                if (FAILED(CeCloseCallerBuffer(
                    (PVOID)CdRomRead.sglist[0].sb_buf,
                    pIOReq->pOutBuf,
                    pIOReq->dwOutBufSize,
                    ARG_O_PTR))) {
                    dwError = ERROR_GEN_FAILURE;
                }
            }
            break;
        case IOCTL_CDROM_TEST_UNIT_READY:
            {
                CDROM_TESTUNITREADY CdRomTestUnitReady;
                LPBYTE              pUnsafe = NULL;
                DWORD               cbUnsafe = 0;
                // Validate buffer and swap unsafe buffer and safe copy.
                if (pIOReq->pInBuf) {
                    pUnsafe = pIOReq->pInBuf;
                    pIOReq->pInBuf = (LPBYTE)&CdRomTestUnitReady;
                    cbUnsafe = pIOReq->dwInBufSize;
                }
                else if(pIOReq->pOutBuf) {
                    pUnsafe = pIOReq->pOutBuf;
                    pIOReq->pOutBuf = (LPBYTE)&CdRomTestUnitReady;
                    cbUnsafe = pIOReq->dwOutBufSize;
                }
                else {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Validate size.
                if (sizeof(CDROM_TESTUNITREADY) != cbUnsafe) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Execute IOCTL.
                if (FALSE == AtapiIsUnitReady(pIOReq)) {
                    dwError = ERROR_NOT_READY;
                    break;
                }
                // Return safe copy to unsafe buffer.
                if (0 == CeSafeCopyMemory((LPVOID)pUnsafe, (LPVOID)&CdRomTestUnitReady, cbUnsafe)) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
            }
            break;
        case IOCTL_CDROM_DISC_INFO:
            {
                LPBYTE pUnsafe = NULL;
                DISC_INFO OutType = DI_NONE;

                // Validate buffer.
                if (NULL == pIOReq->pOutBuf) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }

                if( pIOReq->pInBuf )
                {
                    if( pIOReq->dwInBufSize != sizeof(DISC_INFO) )
                    {
                        dwError = ERROR_INVALID_PARAMETER;
                        break;
                    }

                    __try
                    {
                        OutType = (DISC_INFO)*pIOReq->pInBuf;
                    }
                    __except( EXCEPTION_EXECUTE_HANDLER )
                    {
                        dwError = ERROR_INVALID_PARAMETER;
                        break;
                    }

                    if( OutType > DI_SCSI_INFO )
                    {
                        dwError = ERROR_INVALID_PARAMETER;
                        break;
                    }
                }

                if( OutType == DI_SCSI_INFO )
                {
                    if( pIOReq->dwOutBufSize < sizeof(DISC_INFORMATION) )
                    {
                        dwError = ERROR_INVALID_PARAMETER;
                        break;
                    }
                }
                else
                {
                    if( pIOReq->dwOutBufSize < sizeof(CDROM_DISCINFO) )
                    {
                        dwError = ERROR_INVALID_PARAMETER;
                        break;
                    }

                    //
                    // This function never returned anything other than ERROR_SUCCESS.
                    //
                    dwError = ERROR_SUCCESS;
                    break;
                }

                // Swap unsafe buffer and safe copy.
                pUnsafe = pIOReq->pOutBuf;

                pIOReq->pOutBuf = new BYTE[pIOReq->dwOutBufSize];
                if( !pIOReq->pOutBuf )
                {
                    dwError = ERROR_NOT_ENOUGH_MEMORY;
                    break;
                }

                ZeroMemory( pIOReq->pOutBuf, pIOReq->dwOutBufSize );

                // Execute IOCTL.
                if (FALSE == AtapiIsUnitReadyEx()) {
                    dwError = ERROR_NOT_READY;
                    break;
                }

                dwError = AtapiGetDiscInfo(pIOReq);
                if (dwError != ERROR_SUCCESS) {
                    AtapiDumpSenseData();
                    delete [] pIOReq->pOutBuf;
                    break;
                }
                // Return safe copy to unsafe buffer.
                if (0 == CeSafeCopyMemory( (LPVOID)pUnsafe,
                                           (LPVOID)pIOReq->pOutBuf,
                                           pIOReq->dwOutBufSize) )
                {
                    delete [] pIOReq->pOutBuf;
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }

                delete [] pIOReq->pOutBuf;
            }
            break;
        case IOCTL_CDROM_EJECT_MEDIA:
            dwError = AtapiLoadMedia(TRUE);
            break;
        case IOCTL_CDROM_LOAD_MEDIA:
            dwError = AtapiLoadMedia(FALSE);
            break;
        case IOCTL_CDROM_ISSUE_INQUIRY:
            {
                INQUIRY_DATA InquiryData;
                // Validate buffer.
                if (NULL == pIOReq->pOutBuf) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Validate size.
                if (sizeof(INQUIRY_DATA) > pIOReq->dwOutBufSize) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Execute IOCTL.
                if (FALSE == AtapiIssueInquiry(&InquiryData)) {
                    dwError = ERROR_GEN_FAILURE;
                    break;
                }
                // Return safe copy to unsafe buffer.
                if (0 == CeSafeCopyMemory((LPVOID)pIOReq->pOutBuf, (LPVOID)&InquiryData, pIOReq->dwOutBufSize)) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                dwError = ERROR_SUCCESS;
            }
            break;
        case IOCTL_CDROM_READ_TOC:
            {
                CDROM_TOC CdRomToc;
                LPBYTE    pUnsafe = NULL;
                DWORD     cbUnsafe = 0;
                // Validate buffer and swap unsafe buffer and safe copy.
                if (NULL != pIOReq->pInBuf) {
                    pUnsafe = pIOReq->pInBuf;
                    pIOReq->pInBuf = (LPBYTE)&CdRomToc;
                    cbUnsafe = pIOReq->dwInBufSize;
                }
                else if (NULL != pIOReq->pOutBuf) {
                    pUnsafe = pIOReq->pOutBuf;
                    pIOReq->pOutBuf = (LPBYTE)&CdRomToc;
                    cbUnsafe = pIOReq->dwOutBufSize;
                }
                else {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Validate size.
                if (sizeof(CDROM_TOC) > cbUnsafe) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                // Execute IOCTL.
                if (!AtapiIsUnitReadyEx()) {
                    dwError = ERROR_NOT_READY;
                    break;
                }
                if (FALSE == AtapiGetToc(&CdRomToc)) {
                    dwError = ERROR_GEN_FAILURE;
                    break;
                }
                // Return safe copy to unsafe buffer.
                if (0 == CeSafeCopyMemory((LPVOID)pUnsafe, (LPVOID)&CdRomToc, cbUnsafe)) {
                    dwError = ERROR_INVALID_PARAMETER;
                    break;
                }
                dwError = ERROR_SUCCESS;
            }
            break;
        //
        // DVD
        //
        case IOCTL_DVD_START_SESSION:
        case IOCTL_DVD_READ_KEY:
            // DVDReadKey validates args.
            if (FALSE == AtapiIsUnitReadyEx()) {
                dwError = ERROR_NOT_READY;
                break;
            }
            dwError = DVDReadKey(pIOReq);
            break;
        case IOCTL_DVD_END_SESSION:
        case IOCTL_DVD_SEND_KEY:
            // DVDSendKey validates args.
            if (FALSE == AtapiIsUnitReadyEx()) {
                dwError = ERROR_NOT_READY;
                break;
            }
            dwError = DVDSendKey(pIOReq);
            break;
        case IOCTL_DVD_GET_REGION:
            // DVDGetRegion validates args.
            dwError = DVDGetRegion(pIOReq);
            break;
        case IOCTL_DVD_SET_REGION:
            // DVDSetRegion validates args.
            dwError = DVDSetRegion(pIOReq);
            break;
        //
        // CDDA
        //
        case IOCTL_CDROM_READ_Q_CHANNEL:
            // AtapiReadQChannel validates args.
            if (FALSE == AtapiIsUnitReadyEx()) {
                dwError = ERROR_NOT_READY;
                break;
            }
            dwError = AtapiReadQChannel(pIOReq);
            break;
        case IOCTL_CDROM_PLAY_AUDIO_MSF:
        case IOCTL_CDROM_SEEK_AUDIO_MSF:
        case IOCTL_CDROM_RESUME_AUDIO:
        case IOCTL_CDROM_STOP_AUDIO:
        case IOCTL_CDROM_PAUSE_AUDIO:
        case IOCTL_CDROM_SCAN_AUDIO:
            if (FALSE == AtapiIsUnitReadyEx()) {
                dwError = ERROR_NOT_READY;
                break;
            }
            // ControlAudio validates args.
            dwError = ControlAudio(pIOReq);
            break;
        default:
            dwError = ERROR_NOT_SUPPORTED;
            break;
    }
    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiSendCommand
//
BOOL CPCIDiskAndCD::AtapiSendCommand( PCDB pCdb, WORD wCount, BOOL fDMA )
{
    DEBUGMSG( ZONE_IO,
              (TEXT("ATAPI:AtapiSendCommand - %x\r\n"),
              pCdb->CDB6GENERIC.OperationCode) );

    GetBaseStatus();

    SelectDevice();

    if( WaitOnBusy( FALSE ) )
    {
        if( GetError() & ATA_STATUS_ERROR )
        {
            return FALSE;
        }
    }

    // DRQ should never be set when issuing a command.
    // This indicates that the device is waiting on data from us.
    // So what was the last command issued to the device that left it in this
    //    state?
    VERIFY( WaitForDisc( WAIT_TYPE_NOT_DRQ, 0, 100 ) );

    //
    // Since we don't support command queueing right now, this just sets the tag
    // to zero.
    //
    WriteSectorCount( 0 );

    //
    // The LBA low register is not used for this operation.
    //
    WriteSectorNumber( 0 );

    // Set the byte tranfer count
    if( wCount == 0)
    {
        WriteLowCount( 0xFE );
        WriteHighCount( 0xFF );
    }
    else
    {
        WriteLowCount( (BYTE)(0xff & wCount) );
        WriteHighCount( (BYTE)(0xff & (wCount >> 8)) );
    }


    //
    // Set PIO or DMA Mode as specified in bFlags. 0 = PIO, 1 = DMA
    //
    WriteFeature( fDMA ? 0x1 : 0x0 );

    WaitForDisc( WAIT_TYPE_NOT_BUSY, 20 );

    // Write ATAPI into  command register

    SelectDevice();

    WriteCommand( ATAPI_CMD_COMMAND );

    WaitForDisc( WAIT_TYPE_NOT_BUSY, 20 );

    //
    // Check how device is reporting CPU attention: DRQ or INTRQ?
    // INTRQ within 10 ms!!!
    //
    if( m_fInterruptSupported && IsDRQTypeIRQ() )
    {
        //
        // Note that this interrupt wait isn't actually found the in the host
        // protocol for the PACKET command, but there is an interrupt reason
        // indicating that we should send the command data and the desktop also
        // uses this interrupt.
        //
        if( !WaitForInterrupt( m_dwDiskIoTimeOut ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendCommand - Wait for ATA_INTR_CMD Interrupt (DevId %x) \r\n"), m_dwDeviceId));
            //ASSERT(FALSE);
            //return FALSE;
        }
        else
        {
            BYTE bReason = GetReason();
            if( bReason != ATA_IR_CoD )
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("ATAPI:AtapiSendCommand - Interrupt reason is not valid: %d\r\n"), bReason));
                ASSERT(FALSE);
                return FALSE;
            }
        }

        if( GetAltStatus() & ATA_STATUS_ERROR )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendCommand - Error after sending command: %d\r\n"), GetError()) );
            return FALSE;
        }
    }

    //
    // Device will assert DRQ  within (50us or 3ms) if no interrupt id used
    // Wait for not BSY and DRQ
    if( !WaitForDRQ() )
    {
        DEBUGMSG( ZONE_IO,
                  (TEXT("ATAPIPCI:AtapiSendCommand 1 - ATAWaitForDisc failed ")
                   TEXT("with: %x (DevId %x)\r\n"),
                  GetError(),
                  m_dwDeviceId) );
        return FALSE;

    }

    // Write the ATAPI Command Packet.
    WriteWordBuffer( (LPWORD)pCdb, GetPacketSize() / sizeof(WORD) );

    return TRUE;
}


#pragma warning(push)         // bStatus && bError are only used in DEBUG builds
#pragma warning( disable:4189 )
// /////////////////////////////////////////////////////////////////////////////
// AtapiReceiveData
//
BOOL CPCIDiskAndCD::AtapiReceiveData( PSGX_BUF pSgBuf,
                                      const DWORD dwSgCount,
                                      LPDWORD pdwBytesRead,
                                      BOOL fGetSense )
{
    BOOL fResult = TRUE;
    BYTE *pCurrentBuffer = NULL;
    DWORD dwSgLeft = dwSgCount;
    DWORD dwTransferCount = 0;
    DWORD dwReadCount = 0;
    DWORD dwThisCount = 0;
    DWORD dwLen = 0;
    PSGX_BUF pCurrentSegment = NULL;

    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:AtapiReceiveData - Entered SgCount=%ld.\r\n"), dwSgCount));

    //
    // This amounts to waiting for 50ms.
    //
    if( !WaitForDisc( WAIT_TYPE_READY, 5000, 100 ) )
    {
        DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiReceiveData - Device never set RDY bit after command.\r\n")) );
        //
        // The DRDY bit is a bit confusing.  From revision 6, 7.15.6.2:
        //   - When the bit is clear, the device should attempt to execute commands.
        //   - Should be set to one prior to command completion for PACKET devices,
        //     except for DEVICE RESET or EXECUTE DEVICE DIAGNOSTIC.
        //
        // However, section 8.23 describes the PACKET command and says that this
        // bit is not available until successful command completion, and then it
        // shall be set to 1.
        //

        //fResult = FALSE;
        //goto exit_atapireceivedata;
    }

    pCurrentSegment = pSgBuf;

    // Illegal arguments
    if( !pCurrentSegment && dwSgCount > 0 )
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("Atapi!CPCIDiskAndCD::AtapiReceiveData> pSgBuf null\r\n")));
        fResult = FALSE;
        goto exit_atapireceivedata;
    }

    // The TEST UNIT READY command processor will call this with a null scatter/gather
    // buffer and a null scatter/gather buffer count--which is valid.  We only
    // want to map the caller buffer if the buffer and count are non-null.

    if( pCurrentSegment && dwSgCount > 0 )
    {
        pCurrentBuffer = (LPBYTE)pCurrentSegment->sb_buf;
        dwLen = pCurrentSegment->sb_len;
    }

    m_wNextByte = 0xFFFF; // There is no byte left from the previous transaction.

    for(;;)
    {
        if( m_fInterruptSupported )
        {
            //
            //  Waiting for ATA_INTR_READ or ATA_INTR_WRITE  or ATA_INTR_READY
            //
            if( !WaitForInterrupt( 10000 ) )
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("ATAPI:AtapiReceiveData - Wait for ATA_INTR_READ failed (Timeout)\r\n")));
                fResult = FALSE;
                goto exit_atapireceivedata;
            }

            WORD wState = CheckIntrState();

            //
            // Return Error if not IO Interrupt
            //
            if( (wState == ATA_INTR_ERROR) || (GetAltStatus() & ATA_STATUS_ERROR) )
            {
                //
                // If the Status register has the error bit set, that means we
                // should have sense data and there's no use duplicating error
                // messages here.  Also, this could be as simple as testing unit ready
                // when the device isn't ready.
                //
                if( wState == ATA_INTR_ERROR )
                {
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::AtapiReceiveData - Wait for ATA_INTR_READ failed: Interrupt Reason: %d, AltStatus: %d  \r\n"), wState, GetAltStatus()));
                }

                fResult = FALSE;
                goto exit_atapireceivedata;
            }

            if( wState == ATA_INTR_READY )
            {
                if( (dwReadCount == 0) && (dwSgCount != 0) )
                {
                    BYTE bStatus = GetAltStatus();
                    BYTE bError = GetError();

                    DEBUGMSG( ZONE_CDROM, (TEXT("ATAPI:AtapiReceiveData - Exiting with Interrupt Ready and No Data Transferred - Err(%d), Sta(%d) (Device=%ld)\r\n"), bError, bStatus, m_dwDeviceId));
                    fResult = FALSE;
                    goto exit_atapireceivedata;
                }

                goto exit_atapireceivedata;
            }
        };

        //
        // Wait until the device is ready for data transfer.
        //
        if( !WaitForDRQ() )
        {
            DEBUGMSG( ZONE_CDROM, (TEXT("ATAPI:AtapiReceiveData Failed at WaitForDRQ Status=%02X Error=%02X Deivce=%ld\r\n"), GetAltStatus(), GetError(), m_dwDeviceId));
            fResult = FALSE;
            goto exit_atapireceivedata;
        }

        //
        //  Read Transfer Counter set by Device.
        //
        dwTransferCount = GetCount();

        DEBUGMSG(ZONE_IO, (TEXT(">>>Read Transfer Count : %x  SG=%x \r\n"),dwTransferCount,dwSgLeft));

        while( (dwSgLeft>0) && (dwTransferCount>0) )
        {
            dwThisCount = min(dwTransferCount, dwLen);

            if( pCurrentBuffer )
            {
                ReadBuffer( pCurrentBuffer, dwThisCount );
                dwTransferCount -= dwThisCount;
                dwReadCount += dwThisCount;
            }

            pCurrentBuffer += dwThisCount;
            dwLen -= dwThisCount;

            if( dwLen == 0 )
            {
                // Go to the next SG
                dwSgLeft--;
                pCurrentSegment++;

                if( dwSgLeft && pCurrentSegment )
                {
                    dwLen = pCurrentSegment->sb_len;
                    pCurrentBuffer = (LPBYTE)pCurrentSegment->sb_buf;
                }
            }

        } // End of while loop

        // Discard the rest of data if left.

        while( dwTransferCount > 0 )
        {
            ReadWord();
            dwTransferCount -= 2;
        }

        if( pdwBytesRead )
        {
            *pdwBytesRead = dwReadCount;
        }

        if( !dwSgLeft )
        {
            break;
        }
    }

exit_atapireceivedata:
    if( !fResult && fGetSense )
    {
        if( !AtapiGetSenseInfo( &m_SenseData, _T("AtapiReceiveData") ) )
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("ATAPI:AtapiReceiveData - failed: %x\r\n"), GetError()) );
        }
    }
    else if( fResult && fGetSense )
    {
        if( m_dwAtapiState & (ATAPI_STATE_FORMATTING |
                              ATAPI_STATE_PREPARING) )
        {
            if( m_dwAtapiState & ATAPI_STATE_FORMATTING )
            {
                DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:[AtapiReceiveData] Formatting complete\r\n")) );
                m_dwAtapiState = m_dwAtapiState & ~ATAPI_STATE_FORMATTING;
            }

            if( m_dwAtapiState & ATAPI_STATE_PREPARING )
            {
                DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Drive preperations complete\r\n")) );
                m_dwAtapiState = m_dwAtapiState & ~ATAPI_STATE_PREPARING;
            }
        }
    }

    return fResult;
}
#pragma warning(pop)

// /////////////////////////////////////////////////////////////////////////////
// AtapiSendData
//
BOOL CPCIDiskAndCD::AtapiSendData( PSGX_BUF pSgBuf,
                                   DWORD dwSgCount,
                                   LPDWORD pdwBytesWrite )
{
    BOOL fResult = FALSE;
    DWORD dwSgLeft = dwSgCount;
    DWORD dwTransferCount = 0;
    DWORD dwWriteCount = 0;
    DWORD dwThisCount = 0;
    BYTE *pCurrentBuffer = NULL;
    PSGX_BUF pCurrentSegment = NULL;

    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:AtapiSendData - Entered SgCount=%ld.\r\n"), dwSgCount));

    pCurrentSegment = pSgBuf;

    m_wNextByte = 0xFFFF; // There is no byte left from the previous transaction.

    for(;;)
    {
        if( m_fInterruptSupported )
        {
            //
            //  Waiting for ATA_INTR_READ or ATA_INTR_WRITE  or ATA_INTR_READY
            //
            if( !WaitForInterrupt( 10000 ) )
            {
                ASSERT( !"Do we need to increase the amount of time we wait here?" );
                DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendData - Wait for ATA_INTR_READ failed (DevId %x) \r\n"), m_dwDeviceId));
                goto exit_atapisenddata;
            }

            //
            // Return Error if not IO Interrupt
            //
            WORD wState = CheckIntrState();
            if( wState == ATA_INTR_ERROR )
            {
                DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendData - Wait for ATA_INTR_READ failed (DevId %x) \r\n"), m_dwDeviceId));
                goto exit_atapisenddata;

            }
            if( wState == ATA_INTR_READY )
            {
                if( dwSgLeft == 0 )
                {
                    fResult = TRUE;
                }
                else
                {
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendData - Exiting with Interrupt Ready signal Device=%ld\r\n"), m_dwDeviceId));
                }
                goto exit_atapisenddata;
            }

        };

        //
        // Wait until device is ready for  data transfer.
        //
        if( !WaitForDRQ() )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiSendData Failed at WaitForDRQ Status=%02X Error=%02X Deivce=%ld\r\n"), GetAltStatus(), GetError(), m_dwDeviceId));
            goto exit_atapisenddata;
        }

        //
        //  Read Transfer Counter set by Device.
        //
        dwTransferCount = GetCount();

        if( dwTransferCount && !dwSgLeft )
        {
            ASSERT( !"We are expected to transfer data and have nothing left to transfer!" );
            ResetController( TRUE );
            fResult = FALSE;
            goto exit_atapisenddata;
        }

        DEBUGMSG( ZONE_IO, (TEXT(">>>Read Transfer Count : %x  SG=%x \r\n"),dwTransferCount,dwSgLeft));

        while( (dwSgLeft>0) && (dwTransferCount>0) )
        {
            ASSERT(pCurrentSegment);
            dwThisCount = min(dwTransferCount, pCurrentSegment->sb_len);

            if( pCurrentSegment->sb_buf )
            {
                pCurrentBuffer = (LPBYTE)pCurrentSegment->sb_buf;
            }

            if( pCurrentBuffer )
            {
                WriteBuffer( pCurrentBuffer, dwThisCount );
                dwTransferCount -= dwThisCount;
                dwWriteCount += dwThisCount;
            }

            pCurrentSegment->sb_len -= dwThisCount;
            pCurrentSegment->sb_buf += dwThisCount;

            if( pCurrentSegment->sb_len == 0 )
            {
                // Go to the next SG
                dwSgLeft--;
                pCurrentSegment++;
            }

        } // End of while loop

        if( pdwBytesWrite )
        {
            *pdwBytesWrite = dwWriteCount;
        }

        //
        // If interrupts are supported, then we will keep going and expect to
        // get an interrupt stating that we are finished.
        //
        if( !m_fInterruptSupported && !dwSgLeft )
        {
            break;
        }
    }

    //
    // Give this up to 10 seconds... yeah, that's a lot.  But we'll query
    // every 1/2 second to see if things are ready to move along.
    // This shouldn't happen when we support interrupts.
    //
    if( !WaitForDisc(WAIT_TYPE_NOT_BUSY, 1000000, 50000) ||
        !WaitForDisc(WAIT_TYPE_NOT_DRQ, 1000000, 50000) )
    {
        ASSERT( !"After transferring all of our data, the command isn't finished." );
        fResult = FALSE;
        ResetController( TRUE );
    }

    fResult = TRUE;
exit_atapisenddata:
    if( !fResult )
    {
        if( !AtapiGetSenseInfo( &m_SenseData, _T("AtapiSendData") ) )
        {
            DEBUGMSG (ZONE_ERROR, (TEXT("ATAPI:AtapiSendData - failed: %x\r\n"), GetError()) );
        }
    }
    else
    {
        if( m_dwAtapiState & ATAPI_STATE_FORMATTING )
        {
            DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:[AtapiSendData] Formatting complete\r\n")) );
            m_dwAtapiState = m_dwAtapiState & ~ATAPI_STATE_FORMATTING;
        }
        if( m_dwAtapiState & ATAPI_STATE_PREPARING )
        {
            DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Drive preperations complete\r\n")) );
            m_dwAtapiState = m_dwAtapiState & ~ATAPI_STATE_PREPARING;
        }
    }

    return fResult;
}


// /////////////////////////////////////////////////////////////////////////////
// AtapiPIOTransfer
//
DWORD CPCIDiskAndCD::AtapiPIOTransfer( CDB* pCdb,
                                       BOOL fDataIn,
                                       PSGX_BUF pSgxBuf,
                                       DWORD BufCount,
                                       DWORD* pdwBytesReturned )
{
    DWORD dwError = ERROR_IO_DEVICE;

    //
    // Special case RequestSenseInfo so that we never miss a disc change
    // notification.
    //
    if( pCdb->CDB6GENERIC.OperationCode == SCSIOP_REQUEST_SENSE )
    {
        if( !fDataIn )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_atapipiotransfer;
        }

        if( AtapiGetSenseInfo( &m_SenseData, _T("AtapiPIOTransfer") ) )
        {
            DWORD dwCopied = 0;
            DWORD dwBytes = (DWORD)sizeof(m_SenseData);
            DWORD x = 0;

            while( dwBytes && x < BufCount )
            {
                DWORD dwToCopy = min( dwBytes, pSgxBuf[x].sb_len );

                if( !CeSafeCopyMemory( pSgxBuf[x].sb_buf,
                                       &((UCHAR*)&m_SenseData)[dwCopied],
                                       dwToCopy ) )
                {
                    dwError = ERROR_INVALID_PARAMETER;
                    goto exit_atapipiotransfer;
                }

                dwCopied += dwToCopy;
                dwBytes -= dwToCopy;
                x += 1;
            }

            *pdwBytesReturned = dwCopied;
            dwError = ERROR_SUCCESS;
        }

        goto exit_atapipiotransfer;
    }

    if( !AtapiSendCommand( pCdb, 0, FALSE ) )
    {
        goto exit_atapipiotransfer;
    }

    if (fDataIn)
    {
        if( !AtapiReceiveData( pSgxBuf,
                               BufCount,
                               pdwBytesReturned ) )
        {
            goto exit_atapipiotransfer;
        }
    }
    else
    {
        if( !AtapiSendData ( pSgxBuf,
                             BufCount,
                             pdwBytesReturned ) )
        {
            goto exit_atapipiotransfer;
        }
    }

    dwError = ERROR_SUCCESS;

exit_atapipiotransfer:
    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiIsUnitReady
//
BOOL CPCIDiskAndCD::AtapiIsUnitReady( PIOREQ pIOReq )
{
    CDB Cdb = {0};
    BOOL fRet = TRUE;
    DWORD dwRet = 0;

    if( !IsRemoveableDevice() )
    {
        goto exit_atapisunitready;
    }

    Cdb.CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( NULL, 0, &dwRet ) )
        {
            fRet = FALSE;
        }
    }
    else
    {
        fRet = FALSE;
    }

    if( pIOReq && pIOReq->pInBuf )
    {
        ((CDROM_TESTUNITREADY *)pIOReq->pInBuf)->bUnitReady = fRet;
    }

    if( pIOReq && pIOReq->pOutBuf )
    {
        ((CDROM_TESTUNITREADY *)pIOReq->pOutBuf)->bUnitReady = fRet;
    }

exit_atapisunitready:

    return fRet;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiIsUnitReadyEx
//
BOOL CPCIDiskAndCD::AtapiIsUnitReadyEx()
{
    DWORD dwCount;

    for( dwCount = 0; dwCount < 5; dwCount++ )
    {
        if( AtapiIsUnitReady() )
        {
            m_dwLastCheckTime = GetTickCount();
            break;
        }
        StallExecution( 100 );
    }

    if( dwCount == 5 )
    {
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiGetSenseInfo
//
BOOL CPCIDiskAndCD::AtapiGetSenseInfo( SENSE_DATA *pSenseData,
                                       const TCHAR* strFunctionName )
{
    CDB Cdb = {0};
    SGX_BUF SgBuf = {0};
    DWORD dwRet = 0;

    Cdb.CDB6GENERIC.OperationCode = SCSIOP_REQUEST_SENSE;
    Cdb.CDB6GENERIC.CommandUniqueBytes[2] = sizeof(SENSE_DATA);

    SgBuf.sb_len = sizeof(SENSE_DATA);
    SgBuf.sb_buf = (PBYTE) pSenseData;

    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( &SgBuf, 1, &dwRet, FALSE ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("AtapiReceiveData Failed!!!\r\n")));
            return FALSE;
        }
    }
    else
    {
         return FALSE;
    }

    ReportSenseResults( pSenseData, strFunctionName );

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// ReportSenseResults
//
#pragma warning(push)         // strFunctionName is only used in DEBUG builds
#pragma warning( disable:4100 )
void CPCIDiskAndCD::ReportSenseResults( SENSE_DATA*  //pSenseData
                                      , const TCHAR* strFunctionName
                                      )
{
    BOOL fPrintGenericMsg = TRUE;

    if( m_SenseData.SenseKey == SCSI_SENSE_NOT_READY )
    {
        if( m_SenseData.AdditionalSenseCode == SCSI_ADSENSE_NO_MEDIA_IN_DEVICE )
        {
            if( !(m_dwAtapiState & ATAPI_STATE_EMPTY) )
            {
                DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Media removed\r\n")) );
                m_dwAtapiState = m_dwAtapiState | ATAPI_STATE_EMPTY;
            }
            fPrintGenericMsg = FALSE;
        }
        else if( m_SenseData.AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY )
        {
            if( m_SenseData.AdditionalSenseCodeQualifier == 0x04 )
            {
                if( !(m_dwAtapiState & ATAPI_STATE_FORMATTING) )
                {
                    DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Media formatting\r\n")) );
                    m_dwAtapiState = m_dwAtapiState | ATAPI_STATE_FORMATTING;
                }
                fPrintGenericMsg = FALSE;
            }
            else if( m_SenseData.AdditionalSenseCodeQualifier == 0x01 )
            {
                if( !(m_dwAtapiState & ATAPI_STATE_PREPARING) )
                {
                    DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Preparing drive\r\n")) );
                    m_dwAtapiState = m_dwAtapiState | ATAPI_STATE_PREPARING;
                }
                fPrintGenericMsg = FALSE;
            }
        }
    }
    else if( m_SenseData.SenseKey == SCSI_SENSE_UNIT_ATTENTION )
    {
        if( m_SenseData.AdditionalSenseCode == SCSI_ADSENSE_MEDIUM_CHANGED )
        {
            if( m_SenseData.AdditionalSenseCodeQualifier == 0x00 )
            {
                DEBUGMSG(ZONE_CDROM, (TEXT("ATAPI:Media changed\r\n")) );
                m_dwAtapiState = m_dwAtapiState & ~ATAPI_STATE_EMPTY;
                InterlockedIncrement( (LONG*)&m_dwMediaChangeCount );
                fPrintGenericMsg = FALSE;
            }
        }
    }

    if( fPrintGenericMsg )
    {
        DEBUGMSG (ZONE_ERROR, (TEXT("ATAPI:%s SenseData: %x/%x/%x\r\n"), strFunctionName ? strFunctionName : _T(""),
                                                                         m_SenseData.SenseKey,
                                                                         m_SenseData.AdditionalSenseCode,
                                                                         m_SenseData.AdditionalSenseCodeQualifier) );
    }

}
#pragma warning(pop)

// /////////////////////////////////////////////////////////////////////////////
// AtapiIssueInquiry
//
BOOL CPCIDiskAndCD::AtapiIssueInquiry( INQUIRY_DATA *pInqData )
{
    CDB Cdb = {0};
    SGX_BUF SgBuf = {0};
    DWORD dwRet = 0;

    ASSERT( pInqData != NULL );

    Cdb.CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
    Cdb.CDB6INQUIRY3_23.AllocationLength[1] = sizeof(INQUIRY_DATA);

    SgBuf.sb_len = sizeof(INQUIRY_DATA);
    SgBuf.sb_buf = (PBYTE) pInqData;

    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( &SgBuf, 1, &dwRet ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("AtapiIssueInquiry Failed\r\n")));
            return FALSE;
        }
    }
    else
    {
       return FALSE;
    }

    return TRUE;
 }


// /////////////////////////////////////////////////////////////////////////////
// AtapiLoadMedia
//
DWORD CPCIDiskAndCD::AtapiLoadMedia( BOOL bEject )
{
    CDB Cdb = {0};
    DWORD dwRet = 0;
    DWORD dwError = ERROR_SUCCESS;
    SGX_BUF SgBuf = {0};

    DEBUGMSG(ZONE_IOCTL, (TEXT("ATAPI:ATAPILoadMedia - Entered. Load=%s\r\n"), bEject ? L"TRUE": L"FALSE"));

    Cdb.LOAD_UNLOAD.OperationCode = SCSIOP_LOAD_UNLOAD;
    Cdb.LOAD_UNLOAD.Start = bEject ? 0 : 1;
    Cdb.LOAD_UNLOAD.LoadEject = 1;

    SgBuf.sb_len = 0;
    SgBuf.sb_buf = NULL;

    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( &SgBuf, 0, &dwRet ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::LoadMedia failed on receive\r\n")));
            dwError = ERROR_READ_FAULT;
        }

        if( !bEject )
        {
            Sleep( 5000 );
            AtapiIsUnitReadyEx();
        }
    }
    else
    {
       return ERROR_GEN_FAILURE;
    }

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiReadQChannel
//
DWORD CPCIDiskAndCD::AtapiReadQChannel( PIOREQ pIOReq )
{
    CDB Cdb = {0};
    SGX_BUF SgBuf = {0};
    DWORD dwRet = 0;
    DWORD dwError = ERROR_SUCCESS;
    CDROM_SUB_Q_DATA_FORMAT* pcsqdf = (CDROM_SUB_Q_DATA_FORMAT *)pIOReq->pInBuf;
    SUB_Q_CHANNEL_DATA sqcd;
    DWORD dwDataSize = 0;

    if( !pcsqdf ||
        !pIOReq->pOutBuf ||
        (sizeof(CDROM_SUB_Q_DATA_FORMAT) != pIOReq->dwInBufSize) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_atapireadqchannel;
    }

    switch( pcsqdf->Format )
    {
    case IOCTL_CDROM_CURRENT_POSITION:
        dwDataSize = sizeof(SUB_Q_CURRENT_POSITION);
        break;

    case IOCTL_CDROM_MEDIA_CATALOG:
        dwDataSize = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
        break;

    case IOCTL_CDROM_TRACK_ISRC:
        dwDataSize = sizeof(SUB_Q_TRACK_ISRC);
        break;

    default:
        dwError = ERROR_BAD_FORMAT;
        goto exit_atapireadqchannel;
    }

    if( pIOReq->dwOutBufSize < dwDataSize )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_atapireadqchannel;
    }

    Cdb.SUBCHANNEL.OperationCode = SCSIOP_READ_SUB_CHANNEL;
    Cdb.SUBCHANNEL.SubQ = 1;
    ((BYTE*)&Cdb)[3] = (BYTE)(pcsqdf->Format);
    ((BYTE*)&Cdb)[8] = (BYTE)dwDataSize;

    SgBuf.sb_len = dwDataSize;
    SgBuf.sb_buf = (PBYTE)&sqcd;

    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( &SgBuf, 1, &dwRet ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::ReadQChannel failed on receive\r\n")));
            dwError = ERROR_READ_FAULT;
        }
        else
        {
            memcpy( pIOReq->pOutBuf, (LPBYTE)&sqcd, dwDataSize );
        }
    }
    else
    {
        DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::ReadQChannel failed on SendCommand\r\n")));
        dwError = ERROR_GEN_FAILURE;
    }

exit_atapireadqchannel:
    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiGetToc
//
BOOL CPCIDiskAndCD::AtapiGetToc( CDROM_TOC * pTOC )
{
    CDB Cdb = {0};
    SGX_BUF SgBuf = {0};
    DWORD dwRet = 0;
    BOOL fRet = TRUE;

    Cdb.READ_TOC.OperationCode = SCSIOP_READ_TOC;
    Cdb.READ_TOC.Msf = 1;
    Cdb.READ_TOC.AllocationLength[0] = (BYTE)((sizeof(CDROM_TOC) >> 8) & 0x0FF);
    Cdb.READ_TOC.AllocationLength[1] = (BYTE)(sizeof(CDROM_TOC) & 0x0FF);

    SgBuf.sb_len = sizeof(CDROM_TOC);
    SgBuf.sb_buf = (PBYTE) pTOC;

    if( AtapiSendCommand( &Cdb ) )
    {
        if( !AtapiReceiveData( &SgBuf, 1, &dwRet ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::GetToc failed on receive\r\n")));
            fRet = FALSE;
        }
        else
        {
            WORD wTOCDataLen = (MAKEWORD(pTOC->Length[1], pTOC->Length[0]) - 2);

            //
            //      Minus 4 for the the entire header
            //

            if( wTOCDataLen != (dwRet - 4) )
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("ATAPI:AtapiGetToc - Bad Length in TOC Header = %d, Expecting = %d\r\n"), wTOCDataLen, dwRet - 4));
                fRet = FALSE;
            }

            if( (wTOCDataLen % sizeof(TRACK_DATA)) != 0 )
            {
                DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:AtapiGetToc - Data length  = %d which is not a multiple of CDREADTOC_MSFINFO size\r\n"), wTOCDataLen));
                fRet = FALSE;
            }
        }
    }
    else
    {
        DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::GetToc failed on SendCommand\r\n")));
        fRet = FALSE;
    }

    return fRet;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiGetDiscInfo
//
DWORD CPCIDiskAndCD::AtapiGetDiscInfo( PIOREQ pIOReq )
{
    CDB Cdb = {0};
    DWORD dwError = ERROR_SUCCESS;
    SGX_BUF SgBuf;
    DWORD dwRet = 0;

    //
    // Clear any pending interrupts.
    //
    GetBaseStatus();

    SgBuf.sb_len = pIOReq->dwOutBufSize;
    SgBuf.sb_buf = (PBYTE)pIOReq->pOutBuf;

    Cdb.READ_DISC_INFORMATION.OperationCode = ATAPI_PACKET_CMD_READ_DISC_INFO;
    Cdb.READ_DISC_INFORMATION.AllocationLength[0] = (UCHAR)(SgBuf.sb_len >> 8);
    Cdb.READ_DISC_INFORMATION.AllocationLength[1] = (UCHAR)(SgBuf.sb_len & 0xff);

    if( !pIOReq->pOutBuf ||
        (pIOReq->dwOutBufSize< sizeof(DISC_INFORMATION)) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    __try
    {
        if( AtapiSendCommand( &Cdb ) )
        {
            if( !AtapiReceiveData( &SgBuf, 1, &dwRet ) )
            {
                DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI::GetDiscInfo failed on receive\r\n")));
                return ERROR_GEN_FAILURE;
            }
        }
        else
        {
            return ERROR_GEN_FAILURE;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        return ERROR_INVALID_PARAMETER;
    }

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// AtapiDumpSenseData
//
void CPCIDiskAndCD::AtapiDumpSenseData( SENSE_DATA* pSenseData )
{
    SENSE_DATA SenseData;

    if( pSenseData )
    {
        memcpy( &SenseData, pSenseData, sizeof(SENSE_DATA) );
    }
    else
    {
        if( !AtapiGetSenseInfo( &m_SenseData, _T("AtapiDumpSenseData") ) )
        {
            DEBUGMSG( ZONE_ERROR, (TEXT(" Unable to get CD Sense info\r\n")));
            return;
        }

        memcpy( &SenseData, &m_SenseData, sizeof(SENSE_DATA) );
    }

    DEBUGMSG( ZONE_CDROM, (TEXT("Sense Info\r\n")));
    DEBUGMSG( ZONE_CDROM, (TEXT("   Error Code = 0x%2.2X, Segment Number = %d, Sense Key = 0x%2.2X\r\n"), SenseData.ErrorCode, SenseData.SegmentNumber, SenseData.SenseKey));
    DEBUGMSG( ZONE_CDROM, (TEXT("   Information = 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\r\n"), SenseData.Information[0], SenseData.Information[1], SenseData.Information[2], SenseData.Information[3]));
    DEBUGMSG( ZONE_CDROM, (TEXT("   Additional Sense Length = %d\r\n"), SenseData.AdditionalSenseLength));
    DEBUGMSG( ZONE_CDROM, (TEXT("   Command Specific Information = 0x%2.2X 0x%2.2X 0x%2.2X 0x%2.2X\r\n"),SenseData.CommandSpecificInformation[0], SenseData.CommandSpecificInformation[1], SenseData.CommandSpecificInformation[2], SenseData.CommandSpecificInformation[3]));
    DEBUGMSG( ZONE_CDROM, (TEXT("   ASC = 0x%2.2X, ASCQ = 0x%2.2X, FRUC = 0x%2.2X\r\n"), SenseData.AdditionalSenseCode, SenseData.AdditionalSenseCodeQualifier, SenseData.FieldReplaceableUnitCode));
    DEBUGMSG( ZONE_CDROM, (TEXT("   Sense Key Specfic = 0x%2.2X 0x%2.2X 0x%2.2X\r\n"), SenseData.SenseKeySpecific[0], SenseData.SenseKeySpecific[1], SenseData.SenseKeySpecific[2]));
}

// /////////////////////////////////////////////////////////////////////////////
// ScsiPassThrough
//
// Both the input and the output parameters should be SCSI_PASS_THROUGH.
//
// For commands sending data to the device, the input parameter must have size
//     allocated for the data to send.
// For command retrieving data from the device, the output parameter must have
//     space allocated to receive the data.
// In either case, any sense data returned will be in the output parameter
//     if it is requested.
//
// The number of bytes returned will always be set in the output parameter along
//     with the dwBytesReturned parameter.
//
DWORD CPCIDiskAndCD::ScsiPassThrough( PIOREQ pIOReq )
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwBytesReturned = 0;

    PSENSE_DATA pSenseData = NULL;
    SCSI_PASS_THROUGH* pSPTIn = NULL;
    SCSI_PASS_THROUGH* pSPTOut = NULL;
    SCSI_PASS_THROUGH* pSPTBuffer = NULL;

    //
    // Validate the embedded user buffers.
    //
    if( NULL == pIOReq->pInBuf )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    if( pIOReq->dwInBufSize < sizeof(SCSI_PASS_THROUGH) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    if( pIOReq->dwOutBufSize < sizeof(SCSI_PASS_THROUGH) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    *pIOReq->pBytesReturned = 0;

    //
    // The pass through buffer could be changed at any time and we need
    // to make sure that we handle this situation gracefully.
    //
    if( FAILED(CeOpenCallerBuffer( (VOID**)&pSPTIn,
                                   pIOReq->pInBuf,
                                   pIOReq->dwInBufSize,
                                   ARG_O_PTR,
                                   ::GetDirectCallerProcessId() != ::GetCurrentProcessId() )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    //
    // Make sure that the offsets and sizes reported in the SCSI_PASS_THROUGH
    // structure make sense and can be trusted later.
    //
    if( !ValidatePassThrough( pSPTIn, pIOReq->dwInBufSize ) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    if( !pIOReq->pOutBuf ||
        FAILED(CeOpenCallerBuffer( (VOID**)&pSPTOut,
                                   pIOReq->pOutBuf,
                                   pIOReq->dwOutBufSize,
                                   ARG_IO_PTR,
                                   ::GetDirectCallerProcessId() != ::GetCurrentProcessId() )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    //
    // Make sure that the offsets and sizes reported in the SCSI_PASS_THROUGH
    // structure make sense and can be trusted later.
    //
    if( !ValidatePassThrough( pSPTOut, pIOReq->dwOutBufSize ) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthrough;
    }

    pSenseData = (PSENSE_DATA)((BYTE*)pSPTOut + pSPTOut->SenseInfoOffset);

    if( pSPTIn->DataIn )
    {
        pSPTBuffer = pSPTIn;
    }
    else
    {
        pSPTBuffer = pSPTOut;
    }

    if (ZONE_CELOG) CeLogData(TRUE, CELID_ATAPI_SPTCOMMAND, pSPTIn, sizeof(SCSI_PASS_THROUGH), 0, CELZONE_ALWAYSON, 0, FALSE);

    if( m_fDMAActive &&
        ( pSPTIn->Cdb[0] == SCSIOP_READ ||
          pSPTIn->Cdb[0] == SCSIOP_READ12 ||
          pSPTIn->Cdb[0] == SCSIOP_READ_CD ) )
    {
        dwError = ScsiPassThroughDMA( (CDB*)pSPTIn->Cdb,
                                      pIOReq->pOutBuf + pSPTOut->DataBufferOffset,
                                      pSPTOut->DataTransferLength,
                                      &dwBytesReturned );
    }
    else
    {

        //
        // We use pBuffer internally just to make it easy to acess the
        // SCSI_PASS_THROUGH structure as a byte array.
        //
        DWORD dwBufCount = 0;
        SGX_BUF SgxBuf = { 0 };

        if( pSPTBuffer->DataTransferLength )
        {
            SgxBuf.sb_buf = (BYTE*)pSPTBuffer + pSPTBuffer->DataBufferOffset;
            SgxBuf.sb_len = pSPTBuffer->DataTransferLength;
            dwBufCount = 1;
        }

        dwError = AtapiPIOTransfer( (PCDB)pSPTIn->Cdb,
                                    pSPTIn->DataIn != SCSI_IOCTL_DATA_OUT,
                                    &SgxBuf,
                                    dwBufCount,
                                    &dwBytesReturned );
    }

    pSPTOut->DataTransferLength = dwBytesReturned;
    if( pSPTIn->DataIn == SCSI_IOCTL_DATA_OUT )
    {
        //
        // We've set the DataTransferLength correctly.  I'm not sure what we
        // should actually return in the dwReturnedBytes parameter, but for now
        // I'm setting this to zero if we wrote the data to disk instead of
        // reading it in.
        //
        dwBytesReturned = 0;
    }

    if( pSPTOut->SenseInfoLength )
    {
        //
        // We need to set this to zero if there was no error and the user
        // requested sense information.
        //
        pSPTOut->SenseInfoLength = 0;

        if( dwError )
        {
            //
            // There was an error, so copy the sense data and set the size of
            // the data here that we are returning.
            //
            if( memcpy( pSenseData, &m_SenseData, sizeof(m_SenseData) ) )
            {
                pSPTOut->SenseInfoLength = sizeof(m_SenseData);
                dwBytesReturned += sizeof(m_SenseData);
            }
        }
    }

exit_scsipassthrough:
    if( pSPTIn &&
        FAILED(CeCloseCallerBuffer( pSPTIn,
                                    pIOReq->pInBuf,
                                    pIOReq->dwInBufSize,
                                    ARG_IO_PTR )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if( pSPTOut &&
        FAILED(CeCloseCallerBuffer( pSPTOut,
                                    pIOReq->pInBuf,
                                    pIOReq->dwInBufSize,
                                    ARG_IO_PTR )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if( dwError == ERROR_SUCCESS )
    {
        *pIOReq->pBytesReturned = dwBytesReturned;
    }

    if( ZONE_CELOG ) CeLogData( TRUE, CELID_ATAPI_SPTRESULT, &m_SenseData, sizeof(SENSE_DATA), 0, CELZONE_ALWAYSON, 0, FALSE );

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// ScsiPassThroughDirect
//
DWORD CPCIDiskAndCD::ScsiPassThroughDirect( PIOREQ pIOReq )
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwBytesReturned = 0;

    PSCSI_PASS_THROUGH_DIRECT pSPTIn = NULL;
    PSCSI_PASS_THROUGH_DIRECT pSPTOut = NULL;
    PSCSI_PASS_THROUGH_DIRECT pSPTBuffer = NULL;

    PSENSE_DATA pSenseData = NULL;
    SGX_BUF SgxBuf = { 0 };
    DWORD dwBufCount = 0;
    DWORD dwBytesToUnlock = 0;
    BOOL fDataIn = FALSE;

    BOOL fUserMode = ::GetDirectCallerProcessId() != ::GetCurrentProcessId();

    //
    // Validate the embedded user buffers.
    //
    if( NULL == pIOReq->pInBuf )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pIOReq->dwInBufSize < sizeof(SCSI_PASS_THROUGH_DIRECT) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pIOReq->dwOutBufSize < sizeof(SCSI_PASS_THROUGH_DIRECT) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    //
    // Make a local copy that we know won't be tampered with while we're
    // using it.
    //
    if( FAILED(CeOpenCallerBuffer( (VOID**)&pSPTIn,
                                   pIOReq->pInBuf,
                                   pIOReq->dwInBufSize,
                                   ARG_O_PTR,
                                   ::GetDirectCallerProcessId() != ::GetCurrentProcessId() )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pSPTIn->DataBuffer && !pSPTIn->DataTransferLength )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pSPTIn->DataTransferLength && !pSPTIn->DataBuffer )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    //
    // Make a local copy that we know won't be tampered with while we're
    // using it.
    //
    if( FAILED(CeOpenCallerBuffer( (VOID**)&pSPTOut,
                                   pIOReq->pOutBuf,
                                   pIOReq->dwOutBufSize,
                                   ARG_IO_PTR,
                                   ::GetDirectCallerProcessId() != ::GetCurrentProcessId() )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pSPTOut->DataBuffer && !pSPTOut->DataTransferLength )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pSPTOut->DataTransferLength && !pSPTOut->DataBuffer )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    //
    // Make sure that we can trust the sense information fields.
    //
    if( pSPTOut->SenseInfoLength )
    {
        if( pSPTOut->SenseInfoOffset < sizeof(SCSI_PASS_THROUGH_DIRECT) )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdirect;
        }

        if( pIOReq->dwOutBufSize < pSPTOut->SenseInfoOffset )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdirect;
        }

        if( pIOReq->dwOutBufSize - pSPTOut->SenseInfoOffset < pSPTOut->SenseInfoLength )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdirect;
        }

        if( pSPTOut->SenseInfoLength < sizeof(SENSE_DATA) )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdirect;
        }
    }

    pSenseData = (PSENSE_DATA)((BYTE*)pSPTOut + pSPTOut->SenseInfoOffset);

    fDataIn = pSPTIn->DataIn != SCSI_IOCTL_DATA_OUT;
    if( fDataIn )
    {
        pSPTBuffer = pSPTOut;
    }
    else
    {
        pSPTBuffer = pSPTIn;
    }

    //
    // Validate the buffer if it is coming from user mode.
    //
    if( pSPTBuffer->DataTransferLength &&
        fUserMode &&
        !::IsValidUsrPtr( pSPTBuffer->DataBuffer,
                          pSPTBuffer->DataTransferLength,
                          fDataIn ? TRUE : FALSE ) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }

    if( pSPTBuffer->DataTransferLength )
    {
        SgxBuf.sb_buf = (PUCHAR)pSPTBuffer->DataBuffer;
        SgxBuf.sb_len = pSPTBuffer->DataTransferLength;
        dwBufCount = 1;
    }

    if( pSPTBuffer->DataBuffer &&
        !LockPages( pSPTBuffer->DataBuffer,
                    pSPTBuffer->DataTransferLength,
                    NULL,
                    fDataIn ? LOCKFLAG_WRITE | LOCKFLAG_READ : LOCKFLAG_READ ) )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdirect;
    }
    dwBytesToUnlock = pSPTBuffer->DataTransferLength;

    if( m_fDMAActive &&
        ( pSPTIn->Cdb[0] == SCSIOP_READ ||
          pSPTIn->Cdb[0] == SCSIOP_READ12 ||
          pSPTIn->Cdb[0] == SCSIOP_READ_CD ||
          pSPTIn->Cdb[0] == SCSIOP_WRITE12 ) )
    {
        dwError = ScsiPassThroughDMA( (CDB*)pSPTIn->Cdb,
                                      (BYTE*)pSPTBuffer->DataBuffer,
                                      pSPTBuffer->DataTransferLength,
                                      &dwBytesReturned );
    }
    else
    {
        dwError = AtapiPIOTransfer( (CDB*)pSPTIn->Cdb,
                                    fDataIn,
                                    &SgxBuf,
                                    dwBufCount,
                                    &dwBytesReturned );
    }

    pSPTOut->DataTransferLength = dwBytesReturned;
    if( !fDataIn )
    {
        //
        // We've set the DataTransferLength correctly.  I'm not sure what we
        // should actually return in the dwReturnedBytes parameter, but for now
        // I'm setting this to zero if we wrote the data to disk instead of
        // reading it in.
        //
        dwBytesReturned = 0;
    }

    if( pSPTOut->SenseInfoLength )
    {
        //
        // We need to set this to zero if there was no error and the user
        // requested sense information.
        //
        pSPTOut->SenseInfoLength = 0;

        if( dwError )
        {
            //
            // There was an error, so copy the sense data and set the size of
            // the data here that we are returning.
            //
            if( memcpy( pSenseData, &m_SenseData, sizeof(SENSE_DATA) ) )
            {
                pSPTOut->SenseInfoLength = sizeof(m_SenseData);
                dwBytesReturned += sizeof(m_SenseData);
            }
        }
    }

exit_scsipassthroughdirect:
    //
    // Unlock the pages before we change the DataTransferLength.
    //
    if( dwBytesToUnlock )
    {
        //
        // If we went through DMA and the buffer was aligned, then the pages
        // will be unlocked already.
        //
        if( !UnlockPages( pSPTBuffer->DataBuffer, dwBytesToUnlock ) )
        {
            dwError = GetLastError();
            ASSERT(FALSE);
        }
    }

    if( pSPTIn &&
        FAILED(CeCloseCallerBuffer( pSPTIn,
                                    pIOReq->pInBuf,
                                    pIOReq->dwInBufSize,
                                    ARG_IO_PTR )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if( pSPTOut &&
        FAILED(CeCloseCallerBuffer( pSPTOut,
                                    pIOReq->pOutBuf,
                                    pIOReq->dwOutBufSize,
                                    ARG_IO_PTR )) )
    {
        dwError = ERROR_INVALID_PARAMETER;
    }

    if( dwError == ERROR_SUCCESS )
    {
        *pIOReq->pBytesReturned = dwBytesReturned;
    }

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// ScsiPassThroughDMA
//
// This function needs changed so that it can easily be used with any pass
// through command.  We want to limit the number of sectors that we transfer
// in any given DMA burst and so we're currently breaking up the CDB when
// necessary.  However, there should be another way to do this, so that we
// limit the amount of data that will be transfered in a burst instead of
// changing the CDB to do it.  The other option is to get rid of the requirement
// on the max number of sectors to transfer at any one time.
//
// Possible solution: Limit the sizes of the reads and writes, but allow
// any size for the other commands.
//
DWORD CPCIDiskAndCD::ScsiPassThroughDMA( CDB* pCdb,
                                         __inout_bcount(dwInBufSize) BYTE* pInBuffer,
                                         DWORD dwInBufSize,
                                         DWORD* pdwBytesReturned )
{
    DWORD dwError = ERROR_SUCCESS;

    BOOL fGetSense = FALSE;
    DWORD dwSectorSize = CDROM_SECTOR_SIZE;
    DWORD dwStartSector = 0;
    DWORD dwCurBuffer = 0;
    DWORD dwInBufOffset = 0;
    SG_BUF CurBuffer[MAX_SG_BUF];
    BOOL fRead = TRUE;
    BYTE bCommand = 0;

    ASSERT(pdwBytesReturned != NULL);
    ASSERT(pCdb != NULL);

    *pdwBytesReturned = 0;

    __try
    {
        DWORD dwNumSectors = 0;

        ZeroMemory( &m_SenseData, sizeof(SENSE_DATA));
        switch( ((BYTE*)pCdb)[0] )
        {
        case SCSIOP_READ:
            dwNumSectors = (pCdb->CDB10.TransferBlocksMsb << 8) +
                            pCdb->CDB10.TransferBlocksLsb;
            dwStartSector = (pCdb->CDB10.LogicalBlockByte0 << 24) +
                            (pCdb->CDB10.LogicalBlockByte1 << 16) +
                            (pCdb->CDB10.LogicalBlockByte2 << 8) +
                            (pCdb->CDB10.LogicalBlockByte3);
            break;

        case SCSIOP_READ12:
            dwNumSectors = (pCdb->READ12.TransferLength[0] << 24) +
                           (pCdb->READ12.TransferLength[1] << 16) +
                           (pCdb->READ12.TransferLength[2] << 8) +
                           (pCdb->READ12.TransferLength[3]);
            dwStartSector = (pCdb->READ12.LogicalBlock[0] << 24) +
                            (pCdb->READ12.LogicalBlock[1] << 16) +
                            (pCdb->READ12.LogicalBlock[2] << 8) +
                            (pCdb->READ12.LogicalBlock[3]);
            break;

        case SCSIOP_READ_CD:
            dwNumSectors = (pCdb->READ_CD.TransferBlocks[0] << 16) +
                           (pCdb->READ_CD.TransferBlocks[1] << 8) +
                           (pCdb->READ_CD.TransferBlocks[2]);
            dwStartSector = (pCdb->READ_CD.StartingLBA[0] << 24) +
                            (pCdb->READ_CD.StartingLBA[1] << 16) +
                            (pCdb->READ_CD.StartingLBA[2] << 8) +
                            (pCdb->READ_CD.StartingLBA[3]);

            // We need to be really careful with this command.  Since the sector
            // size is variable here, we might run into conditions where we
            // don't use the entire PRD, or the PRD isn't large enough for our
            // data.
            //
            if( pCdb->READ_CD.ExpectedSectorType != 2 &&
                pCdb->READ_CD.ExpectedSectorType != 4 &&
                dwNumSectors * CDROM_SECTOR_SIZE != dwInBufSize )
            {
                dwSectorSize = CDROM_RAW_SECTOR_SIZE;
            }
            break;

        case SCSIOP_WRITE12:
            fRead = FALSE;
            dwNumSectors = (pCdb->WRITE12.TransferLength[0] << 24) +
                           (pCdb->WRITE12.TransferLength[1] << 16) +
                           (pCdb->WRITE12.TransferLength[2] << 8) +
                           (pCdb->WRITE12.TransferLength[3]);
            dwStartSector = (pCdb->WRITE12.LogicalBlock[0] << 24) +
                            (pCdb->WRITE12.LogicalBlock[1] << 16) +
                            (pCdb->WRITE12.LogicalBlock[2] << 8) +
                            (pCdb->WRITE12.LogicalBlock[3]);
            break;

        default:
            ASSERT(FALSE);
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdma;
        }

        if( dwNumSectors * dwSectorSize > dwInBufSize )
        {
            dwError = ERROR_INVALID_PARAMETER;
            goto exit_scsipassthroughdma;
        }

        //
        // Process the SG buffers in blocks of MAX_CD_SECT_PER_COMMAND.  Each
        // DMA request will have a new SG_BUF array which will be a subset of
        // the original request, and may start/stop in the middle of the
        // original buffer.
        //
        while( dwNumSectors )
        {
            DWORD dwSectorsToTransfer = (dwNumSectors > MAX_CD_SECT_PER_COMMAND) ? MAX_CD_SECT_PER_COMMAND
                                                                                 : dwNumSectors;
            DWORD dwBufferLeft = dwSectorsToTransfer * dwSectorSize;

            CurBuffer[dwCurBuffer].sb_buf = pInBuffer + dwInBufOffset;
            CurBuffer[dwCurBuffer].sb_len = dwBufferLeft;
            dwInBufOffset += dwBufferLeft;

            //
            // This sets up the Physical Region Descriptor (PRD) table.  Each
            // entry is just an address to use, a size, and a flag to identify
            // the last entry in the table.  This function ensures that the
            // addresses specified are aligned (buffering when necessary) and
            // that they can be accessed from the device.
            //
            if( !SetupDMA( CurBuffer, 1, fRead ) )
            {
                dwError = ERROR_READ_FAULT;
                goto exit_scsipassthroughdma;
            }

            //
            // This will setup the DMA registers for a transfer.  This includes
            // the address of the PRD table that we setup, the direction of data
            // transfer, and then starts the transfer.
            //
            PrepareDMA();
            bIsBMIRQ = 0;
            switch( ((BYTE*)pCdb)[0] )
            {
            case SCSIOP_READ:
                pCdb->CDB10.TransferBlocksMsb = (BYTE)(dwSectorsToTransfer & 0xFF);
                pCdb->CDB10.TransferBlocksMsb = (BYTE)((dwSectorsToTransfer >> 8) & 0xFF);

                pCdb->CDB10.LogicalBlockByte0 = (BYTE)((dwStartSector >> 24) & 0xFF);
                pCdb->CDB10.LogicalBlockByte1 = (BYTE)((dwStartSector >> 16) & 0xFF);
                pCdb->CDB10.LogicalBlockByte2 = (BYTE)((dwStartSector >> 8) & 0xFF);
                pCdb->CDB10.LogicalBlockByte3 = (BYTE)((dwStartSector) & 0xFF);
                break;

            case SCSIOP_READ12:
                pCdb->READ12.TransferLength[0] =  (BYTE)((dwSectorsToTransfer >> 24) & 0xFF);
                pCdb->READ12.TransferLength[1] =  (BYTE)((dwSectorsToTransfer >> 16) & 0xFF);
                pCdb->READ12.TransferLength[2] =  (BYTE)((dwSectorsToTransfer >> 8) & 0xFF);
                pCdb->READ12.TransferLength[3] =  (BYTE)((dwSectorsToTransfer) & 0xFF);

                pCdb->READ12.LogicalBlock[0] = (BYTE)((dwStartSector >> 24) & 0xFF);
                pCdb->READ12.LogicalBlock[1] = (BYTE)((dwStartSector >> 16) & 0xFF);
                pCdb->READ12.LogicalBlock[2] = (BYTE)((dwStartSector >> 8) & 0xFF);
                pCdb->READ12.LogicalBlock[3] = (BYTE)((dwStartSector) & 0xFF);
                break;

            case SCSIOP_READ_CD:
                pCdb->READ_CD.TransferBlocks[0] = (BYTE)((dwSectorsToTransfer >> 16) & 0xFF);
                pCdb->READ_CD.TransferBlocks[1] = (BYTE)((dwSectorsToTransfer >> 8) & 0xFF);
                pCdb->READ_CD.TransferBlocks[2] = (BYTE)((dwSectorsToTransfer) & 0xFF);

                pCdb->READ_CD.StartingLBA[0] = (BYTE)((dwStartSector >> 24) & 0xFF);
                pCdb->READ_CD.StartingLBA[1] = (BYTE)((dwStartSector >> 16) & 0xFF);
                pCdb->READ_CD.StartingLBA[2] = (BYTE)((dwStartSector >> 8) & 0xFF);
                pCdb->READ_CD.StartingLBA[3] = (BYTE)((dwStartSector) & 0xFF);
                break;

            case SCSIOP_WRITE12:
                pCdb->WRITE12.TransferLength[0] =  (BYTE)((dwSectorsToTransfer >> 24) & 0xFF);
                pCdb->WRITE12.TransferLength[1] =  (BYTE)((dwSectorsToTransfer >> 16) & 0xFF);
                pCdb->WRITE12.TransferLength[2] =  (BYTE)((dwSectorsToTransfer >> 8) & 0xFF);
                pCdb->WRITE12.TransferLength[3] =  (BYTE)((dwSectorsToTransfer) & 0xFF);

                pCdb->WRITE12.LogicalBlock[0] = (BYTE)((dwStartSector >> 24) & 0xFF);
                pCdb->WRITE12.LogicalBlock[1] = (BYTE)((dwStartSector >> 16) & 0xFF);
                pCdb->WRITE12.LogicalBlock[2] = (BYTE)((dwStartSector >> 8) & 0xFF);
                pCdb->WRITE12.LogicalBlock[3] = (BYTE)((dwStartSector) & 0xFF);
                break;

            default:
                ASSERT(FALSE);
                dwError = ERROR_INVALID_PARAMETER;
                goto exit_scsipassthroughdma;
            }

            if (ZONE_CELOG) CeLogData(TRUE, CELID_ATAPI_DMASPTCOMMAND, pCdb, sizeof(CDB), 0, CELZONE_ALWAYSON, 0, FALSE);

            //
            // We should not have an interrupt at this point or things will get very bad.
            //
            ASSERT( WaitForSingleObject( m_pPort->m_hIRQEvent, 0 ) != WAIT_OBJECT_0 );

            if( !AtapiSendCommand( pCdb, 0, TRUE ) )
            {
                dwError = ERROR_IO_DEVICE;

                goto exit_scsipassthroughdma;
            }

            if( fRead )
            {
                bCommand = 0x08 | 0x01;
            }
            else
            {
                bCommand = 0x00 | 0x01;
            }

            WriteBMCommand( bCommand );

            if( m_fInterruptSupported )
            {
                BYTE BMStatus = 0;
                BYTE BaseStatus = 0;
scsipassthroughdma_waitfordmainterrupt:
                if( !WaitForInterruptDMA(10000, &BMStatus) )
                {
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:ScsiPassThroughDMA- WaitforInterrupt failed (DevId %x) \r\n"),m_dwDeviceId));
                    dwError = ERROR_READ_FAULT;
                    goto exit_scsipassthroughdma;
                }

                //
                // ---------------------------------------------------------------------------
                // | Interrupt | Error | Active | Description
                // ---------------------------------------------------------------------------
                // | 0         | 0     | 1      | DMA transfer in progress, has not set INTRQ.
                // | 1         | 0     | 0      | INTRQ asserted, PRDs exhausted.
                // | 1         | 0     | 1      | INTRQ asserted, PRDs not exhausted.
                // | 0         | 0     | 0      | PRDs smaller than ATA transfer size.
                // | x         | 1     | x      | An error has occurred.
                // ---------------------------------------------------------------------------
                //

                if( !( (BMStatus & 0x04) || (1 == bIsBMIRQ) ) )
                {
                    //
                    // Where did this interrupt come from?
                    //
                    ASSERT(FALSE);
                    goto scsipassthroughdma_waitfordmainterrupt;
                }
                if( !WaitForDisc( WAIT_TYPE_NOT_BUSY, 1000000, 5000 ) )
                {
                    //
                    // We got the interrupt, and it was for this device, but we're still busy.
                    // This should probably end up in a device reset.
                    ASSERT(FALSE);
                    dwError = ERROR_IO_DEVICE;
                    goto exit_scsipassthroughdma;
                }

                WriteBMCommand(0);

                if( BaseStatus & ATA_STATUS_ERROR )
                {
                    fGetSense = TRUE;
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:ScsiPassThroughDMA- Command failed (%x/%x) \r\n"),m_dwDeviceId, BaseStatus));
                    dwError = ERROR_READ_FAULT;
                    goto exit_scsipassthroughdma;
                }

                if( (GetReason() & 0x3) != 0x3 )
                {
                    //
                    // We received an interrupt before the device completed the DMA transfer.
                    ASSERT(FALSE);
                    dwError = ERROR_IO_DEVICE;
                    goto exit_scsipassthroughdma;
                }

                if( (BMStatus & 0x03) != 0 )
                {
                    if( BMStatus & BM_STATUS_ACTIVE )
                    {
                        dwError = ERROR_BUFFER_OVERFLOW;
                        goto exit_scsipassthroughdma;
                    }
                    else
                    {
                        dwError = ERROR_IO_DEVICE;
                        goto exit_scsipassthroughdma;
                    }
                }

                if( !WaitForDisc( WAIT_TYPE_NOT_DRQ, 500, 100 ) )
                {
                    //
                    // The device still thinks there is data to transfer... not good.
                    ASSERT(FALSE);
                    dwError = ERROR_IO_DEVICE;
                    goto exit_scsipassthroughdma;
                }
            }
#ifdef DEBUG
            else
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("ATAPI:ScsiPassThroughDMA - DMA without interrupts is bad.\r\n")));
                ASSERT(FALSE);
            }
#endif

            //
            // Read the DMA status registers and stop the bus master.
            //
            if( EndDMA() )
            {
                WaitOnBusy( FALSE );

                //
                // Clean up the memory that we used for DMA.
                //
                CompleteDMA( &CurBuffer[dwCurBuffer], 1, fRead );
            }
            else
            {
                dwError = ERROR_READ_FAULT;
                goto exit_scsipassthroughdma;
            }

            *pdwBytesReturned += dwSectorsToTransfer * dwSectorSize;

            dwCurBuffer++;
            dwStartSector += dwSectorsToTransfer;
            dwNumSectors -= dwSectorsToTransfer;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto exit_scsipassthroughdma;
    }

exit_scsipassthroughdma:
    if( dwError != ERROR_SUCCESS )
    {
        //
        // This will delete any physical pages that we allocated beyond the
        // minimum amount and will disable DMA.
        //
        AbortDMA();

        if( !WaitForDisc( WAIT_TYPE_NOT_BUSY, 1000000, 50000 ) )
        {
            ASSERT(FALSE);
        }

        if( !WaitForDisc( WAIT_TYPE_NOT_DRQ, 1000000, 50000 ) )
        {
            //
            // We received an interrupt before the device completed the DMA transfer.
            ASSERT(FALSE);
        }
    }

    if( fGetSense )
    {
        AtapiGetSenseInfo( &m_SenseData, _T("ScsiPassThroughDMA") );
    }

    if (ZONE_CELOG) CeLogData(TRUE, CELID_ATAPI_DMASPTRESULT, &m_SenseData, sizeof(SENSE_DATA), 0, CELZONE_ALWAYSON, 0, FALSE);

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// SetupCdRomRead
//
DWORD CPCIDiskAndCD::SetupCdRomRead( BOOL bRawMode,
                                     DWORD dwLBAAddr,
                                     DWORD dwTransferLength,
                                     PCDB pCdb )
{
    BOOL fIsDVD = (m_dwDeviceFlags & DFLAGS_DEVICE_ISDVD);

    if( fIsDVD && bRawMode )
    {
        return ERROR_INVALID_PARAMETER;
    }

    memset( pCdb, 0, sizeof(CDB) );

    if( fIsDVD )
    {
        pCdb->READ12.OperationCode = SCSIOP_READ12;

        pCdb->READ12.LogicalBlock[0] = (BYTE)((dwLBAAddr >> 24) & 0xFF);
        pCdb->READ12.LogicalBlock[1] = (BYTE)((dwLBAAddr >> 16) & 0xFF);
        pCdb->READ12.LogicalBlock[2] = (BYTE)((dwLBAAddr >> 8) & 0xFF);
        pCdb->READ12.LogicalBlock[3] = (BYTE)(dwLBAAddr & 0xFF);

        pCdb->READ12.TransferLength[0] = (BYTE)((dwTransferLength >> 24) & 0xFF);
        pCdb->READ12.TransferLength[1] = (BYTE)((dwTransferLength >> 16) & 0xFF);
        pCdb->READ12.TransferLength[2] = (BYTE)((dwTransferLength >> 8) & 0xFF);
        pCdb->READ12.TransferLength[3] = (BYTE)(dwTransferLength & 0xFF);
    }
    else if( !bRawMode )
    {
        if( dwTransferLength > 0xFFFF )
        {
            return ERROR_INVALID_PARAMETER;
        }

        pCdb->CDB10.OperationCode = SCSIOP_READ;

        pCdb->CDB10.LogicalBlockByte0 = (BYTE)((dwLBAAddr >> 24) & 0xFF);
        pCdb->CDB10.LogicalBlockByte1 = (BYTE)((dwLBAAddr >> 16) & 0xFF);
        pCdb->CDB10.LogicalBlockByte2 = (BYTE)((dwLBAAddr >> 8) & 0xFF);
        pCdb->CDB10.LogicalBlockByte3 = (BYTE)(dwLBAAddr & 0xFF);

        pCdb->CDB10.TransferBlocksMsb = (BYTE)((dwTransferLength >> 8) & 0xFF);
        pCdb->CDB10.TransferBlocksLsb = (BYTE)(dwTransferLength & 0xFF);
    }
    else
    {
        //
        // The user has requested RAW data.  I'm not sure that this ever worked
        // correctly.
        //
        if( dwTransferLength > 0xFFFFFF )
        {
            return ERROR_INVALID_PARAMETER;
        }

        pCdb->READ_CD.OperationCode = SCSIOP_READ_CD;

        pCdb->READ_CD.HeaderCode = 2;

        pCdb->READ_CD.StartingLBA[0] = (BYTE)((dwLBAAddr >> 24) & 0xFF);
        pCdb->READ_CD.StartingLBA[1] = (BYTE)((dwLBAAddr >> 16) & 0xFF);
        pCdb->READ_CD.StartingLBA[2] = (BYTE)((dwLBAAddr >> 8) & 0xFF);
        pCdb->READ_CD.StartingLBA[3] = (BYTE)(dwLBAAddr & 0xFF);

        pCdb->READ_CD.TransferBlocks[0] = (BYTE)((dwTransferLength >> 16) & 0xFF);
        pCdb->READ_CD.TransferBlocks[1] = (BYTE)((dwTransferLength >> 8) & 0xFF);
        pCdb->READ_CD.TransferBlocks[2] = (BYTE)(dwTransferLength & 0xFF);
    }

    return ERROR_SUCCESS;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadCdRom
//
DWORD CPCIDiskAndCD::ReadCdRom( CDROM_READ *pReadInfo, PDWORD pBytesReturned )
{
    CDB Cdb = {0};
    CDROM_ADDR CurAddr = {0};
    WORD wSectorSize = 0;
    DWORD dwError = ERROR_SUCCESS;
    PSGX_BUF pSgBuf = NULL;

    GetBaseStatus(); // Clear Interrupt if it is already set

    CurAddr = pReadInfo->StartAddr;

    // The request must either be in MSF format or LBA format
    DEBUGCHK(pReadInfo->StartAddr.Mode == CDROM_ADDR_MSF || pReadInfo->StartAddr.Mode == CDROM_ADDR_LBA);

    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:ReadCdRom Address=%ld Mode=%02X Length=%ld TrackMode=%02X\r\n"), CurAddr.Address, CurAddr.Mode, pReadInfo->TransferLength, pReadInfo->TrackMode));

    //
    // If in MSF format then convert it to LBA
    //
    if( CurAddr.Mode == CDROM_ADDR_MSF )
    {
        CDROM_MSF_TO_LBA( &CurAddr );
    }

    //
    // Verify that the transfer count is not 0
    //
    if( (pReadInfo->TransferLength == 0) ||
        (pReadInfo->sgcount == 0))
    {
       return ERROR_INVALID_PARAMETER;
    }

    if( pReadInfo->bRawMode )
    {
        wSectorSize = CDROM_RAW_SECTOR_SIZE;
    }
    else
    {
        wSectorSize = CDROM_SECTOR_SIZE;
    }

    pSgBuf = &(pReadInfo->sglist[0]);

    if( IsDMASupported() )
    {
        __try
        {
            dwError = ReadCdRomDMA( CurAddr.Address.lba,
                                    pReadInfo->TransferLength,
                                    wSectorSize,
                                    pReadInfo->sgcount,
                                    pSgBuf );
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            AbortDMA();
            dwError = ERROR_INVALID_PARAMETER;
        }

        if( dwError == ERROR_SUCCESS )
        {
            *(pBytesReturned) = pReadInfo->TransferLength * wSectorSize;
        }
    }
    else
    {
        SetupCdRomRead( pReadInfo->bRawMode,
                        CurAddr.Address.lba,
                        pReadInfo->TransferLength,
                        &Cdb );

        if( AtapiSendCommand( &Cdb, wSectorSize, IsDMASupported() ) )
        {
            if( !AtapiReceiveData( pSgBuf,
                                   pReadInfo->sgcount,
                                   pBytesReturned ) )
            {
                dwError = ERROR_READ_FAULT;
            }
        }
        else
        {
             dwError = ERROR_READ_FAULT;
        }
    }

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadCdRomDMA
//
DWORD CPCIDiskAndCD::ReadCdRomDMA( DWORD dwLBAAddr,
                                   DWORD dwTransferLength,
                                   WORD wSectorSize,
                                   DWORD dwSgCount,
                                   SGX_BUF *pSgBuf )
{
    CDB Cdb = {0};
    WORD wCount = 0;
    DWORD dwError=ERROR_SUCCESS;
    DWORD dwSectorsToTransfer = 0;
    SG_BUF CurBuffer[MAX_SG_BUF];

    DWORD dwStartBufferNum = 0;
    DWORD dwEndBufferNum = 0;
    DWORD dwEndBufferOffset = 0;
    DWORD dwNumSectors = dwTransferLength;
    DWORD dwStartSector = dwLBAAddr;

    DEBUGMSG( ZONE_IO, (TEXT("ATAPI:ReadCdRomDMA - Transfer Length: %d\r\n"),dwTransferLength));

    //
    // Process the SG buffers in blocks of MAX_CD_SECT_PER_COMMAND.  Each DMA
    // request will have a new SG_BUF array which will be a subset of the
    // original request, and may start/stop in the middle of the original buffer.
    //
    while( dwNumSectors )
    {
        dwSectorsToTransfer = (dwNumSectors > MAX_CD_SECT_PER_COMMAND)
                                            ? MAX_CD_SECT_PER_COMMAND
                                            : dwNumSectors;

        DWORD dwBufferLeft = dwSectorsToTransfer * wSectorSize;
        DWORD dwNumSg = 0;

        //
        // We are simply breaking the SG buffers we received into smaller chunks
        // when necessary.  If each of the buffers is less than
        // MAX_CD_SECT_PER_COMMAND * wSectorSize, then the buffers will be the
        // same as the original.  If an SG buffer is larger than this size, it
        // will be broken into multiple smaller SG buffers.
        //
        while( dwBufferLeft )
        {
            DWORD dwCurBufferLen = pSgBuf[dwEndBufferNum].sb_len - dwEndBufferOffset;

            if (dwBufferLeft < dwCurBufferLen)
            {
                //
                // The buffer left for this block is less than the current SG
                // buffer length
                //
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_buf = pSgBuf[dwEndBufferNum].sb_buf + dwEndBufferOffset;
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_len = dwBufferLeft;
                dwEndBufferOffset += dwBufferLeft;
                dwBufferLeft = 0;
            }
            else
            {
                //
                // The buffer left for this block is greater than or equal to
                // the current SG buffer length.  Move on to the next SG buffer.
                //
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_buf = pSgBuf[dwEndBufferNum].sb_buf + dwEndBufferOffset;
                CurBuffer[dwEndBufferNum - dwStartBufferNum].sb_len = dwCurBufferLen;
                dwEndBufferOffset = 0;
                dwEndBufferNum++;
                dwBufferLeft -= dwCurBufferLen;
            }

            dwNumSg++;
        }

        if( !SetupDMA( CurBuffer, dwNumSg, TRUE ) )
        {
            dwError = ERROR_READ_FAULT;
            goto exit_readcdromdma;
        }

        BeginDMA(TRUE);

        SetupCdRomRead( wSectorSize == CDROM_RAW_SECTOR_SIZE ? TRUE : FALSE,
                        dwStartSector,
                        dwSectorsToTransfer,
                        &Cdb );

        wCount = (SHORT)((dwSectorsToTransfer * wSectorSize) >> 1);

        if( AtapiSendCommand( &Cdb, wCount, TRUE ) )
        {
            if( m_fInterruptSupported )
            {
                if( !WaitForInterrupt( m_dwDiskIoTimeOut ) )
                {
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:ReadCdRomDMA- WaitforInterrupt failed (DevId %x) \r\n"),m_dwDeviceId));
                    dwError = ERROR_READ_FAULT;
                    goto exit_readcdromdma;
                }

                if( GetAltStatus() & ATA_STATUS_ERROR )
                {
                    DEBUGMSG( ZONE_ERROR, (TEXT("ATAPI:ReadCdRomDMA- Command failed (%x) \r\n"),GetError()));
                    dwError = ERROR_READ_FAULT;
                    goto exit_readcdromdma;
                }
            }

            if( EndDMA() )
            {
                WaitOnBusy( FALSE );
                CompleteDMA( (PSG_BUF)pSgBuf, dwSgCount, TRUE );
            }
            else
            {
                dwError = ERROR_READ_FAULT;
                goto exit_readcdromdma;
            }
        }

        dwStartSector += dwSectorsToTransfer;
        dwStartBufferNum = dwEndBufferNum;
        dwNumSectors -= dwSectorsToTransfer;
    }


exit_readcdromdma:
    if( dwError != ERROR_SUCCESS )
    {
        AbortDMA();
    }

    return dwError;
}

// /////////////////////////////////////////////////////////////////////////////
// WaitForInterrupt
//
// I've copied the function from the CPCIDisk here so that we could safely
// remove the check on the ERR bit after reading the BaseStatus register.  We
// need to know the difference between not receiving an interrupt, and receiving
// an interrupt but the command failed in the device.
//
BOOL CPCIDiskAndCD::WaitForInterrupt( DWORD dwTimeOut )
{
    BOOL fRet = TRUE;
    DWORD dwRet = 0;

    //
    // Wait for an interrupt.
    //
    dwRet = WaitForSingleObject( m_pPort->m_hIRQEvent, dwTimeOut );

    if( dwRet == WAIT_TIMEOUT )
    {
        fRet = FALSE;
    }
    else if( dwRet != WAIT_OBJECT_0 )
    {
        //
        // Possible reasons for WAIT_FAILED are:
        //
        // ERROR_OUT_OF_MEMORY
        //  A) Out of memory
        // ERROR_OPERATION_ABORTED
        //  A) Thread is being terminated
        // ERROR_INVALID_HANDLE
        //  A) Object is deleted
        //  B) 2 Threads are waiting on the interrupt
        //

        if( GetLastError() == ERROR_INVALID_HANDLE )
        {
            fRet = FALSE;
        }
        else
        {
            //
            // If we were out of memory or the thread is being terminated, then
            // we should try to leave the device in a good state.
            //
            if( !WaitForDisc( WAIT_TYPE_DRQ, dwTimeOut, 10 ) )
            {
                DEBUGMSG(ZONE_ERROR, (_T("CPCIDisk::WaitForInterrupt> Wait for disk failed - timeout(%u)\r\n"), dwTimeOut));
                fRet = FALSE;
            }
        }
    }

    //
    // Reading the base status register clears the interrupt.  This function is
    // only interested in getting the interrupt, not in the status register.
    // We will check for other errors later, so we throw this return value away.
    //
    GetBaseStatus();

    //
    // NOTE: There could be a 400ns delay after reading the base register before
    // the device actually releases the interrupt signal.  If we re-enable
    // interrupts before this has been released then we will receive another
    // interrupt.  Since I don't know a way to poll the device to see if the
    // interrupt signal has been released, I'm putting in the smallest delay I
    // can to ensure this timing is met, 1 microsecond.
    //
    ::StallExecution(1);

    //
    // Signal interrupt done, re-enabling interrupts.
    //
    InterruptDone( m_pPort->m_dwSysIntr );

    return fRet;
}

// /////////////////////////////////////////////////////////////////////////////
// WaitForInterruptDMA
//
// This is the same as WaitForInterrupt but that it read the BM status register
// before clearing the interrupt.  I don't know that this is necessary, but as
// the documentation I've read it vague I feel that it is safer.
//
BOOL CPCIDiskAndCD::WaitForInterruptDMA( DWORD dwTimeOut, BYTE* pBMStatus )
{
    BOOL fRet = TRUE;
    DWORD dwRet = 0;

    ASSERT( pBMStatus != NULL );

    //
    // Wait for an interrupt.
    //
    dwRet = WaitForSingleObject( m_pPort->m_hIRQEvent, dwTimeOut );

    if( dwRet == WAIT_TIMEOUT )
    {
        fRet = FALSE;
    }
    else if( dwRet != WAIT_OBJECT_0 )
    {
        //
        // Possible reasons for WAIT_FAILED are:
        //
        // ERROR_OUT_OF_MEMORY
        //  A) Out of memory
        // ERROR_OPERATION_ABORTED
        //  A) Thread is being terminated
        // ERROR_INVALID_HANDLE
        //  A) Object is deleted
        //  B) 2 Threads are waiting on the interrupt
        //

        if( GetLastError() == ERROR_INVALID_HANDLE )
        {
            fRet = FALSE;
        }
        else
        {
            //
            // If we were out of memory or the thread is being terminated, then
            // we should try to leave the device in a good state.
            //
            if( !WaitForDisc( WAIT_TYPE_DRQ, dwTimeOut, 10 ) )
            {
                DEBUGMSG(ZONE_ERROR, (_T("CPCIDisk::WaitForInterrupt> Wait for disk failed - timeout(%u)\r\n"), dwTimeOut));
                fRet = FALSE;
            }
        }
    }

    *pBMStatus = ReadBMStatus();

    //
    // Reading the base status register clears the interrupt.  This function is
    // only interested in getting the interrupt, not in the status register.
    // We will check for other errors later, so we throw this return value away.
    //
    GetBaseStatus();

    //
    // NOTE: There could be a 400ns delay after reading the base register before
    // the device actually releases the interrupt signal.  If we re-enable
    // interrupts before this has been released then we will receive another
    // interrupt.  Since I don't know a way to poll the device to see if the
    // interrupt signal has been released, I'm putting in the smallest delay I
    // can to ensure this timing is met, 1 microsecond.
    //
    ::StallExecution(1);

    //
    // Signal interrupt done, re-enabling interrupts.
    //
    InterruptDone( m_pPort->m_dwSysIntr );

    return fRet;
}


// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// ValidatePassThrough
//
// This functions checks the fields of the SCSI_PASS_THROUGH structure to
// validate that they make sense.
//
// dwInBufSize: This is the size of the entire pPassThrough buffer.
//
BOOL ValidatePassThrough( SCSI_PASS_THROUGH* pPassThrough, DWORD dwInBufSize )
{
    BOOL fResult = FALSE;

    //
    // Validate that the data buffer looks correct.
    //
    if( pPassThrough->DataTransferLength &&
        pPassThrough->DataBufferOffset < sizeof(SCSI_PASS_THROUGH) )
    {
        goto exit_validatepassthrough;
    }

    if( pPassThrough->DataTransferLength >
        dwInBufSize - sizeof(SCSI_PASS_THROUGH) )
    {
        goto exit_validatepassthrough;
    }

    if( dwInBufSize - pPassThrough->DataTransferLength <
        pPassThrough->DataBufferOffset )
    {
        goto exit_validatepassthrough;
    }

    //
    // Validate that the sense buffer and the data buffer don't
    // overlap.
    //
    if( pPassThrough->DataBufferOffset < pPassThrough->SenseInfoOffset &&
        pPassThrough->DataBufferOffset + pPassThrough->DataTransferLength > pPassThrough->SenseInfoOffset )
    {
        goto exit_validatepassthrough;
    }

    if( pPassThrough->SenseInfoOffset < pPassThrough->DataBufferOffset &&
        pPassThrough->SenseInfoOffset + pPassThrough->SenseInfoLength > pPassThrough->DataBufferOffset )
    {
        goto exit_validatepassthrough;
    }

    if( pPassThrough->SenseInfoOffset == pPassThrough->DataBufferOffset &&
        pPassThrough->SenseInfoOffset != 0  &&
        pPassThrough->DataTransferLength != 0 )
    {
        goto exit_validatepassthrough;
    }

    if( dwInBufSize -
        sizeof(SCSI_PASS_THROUGH) -
        pPassThrough->DataTransferLength <
        pPassThrough->SenseInfoLength )
    {
        goto exit_validatepassthrough;
    }

    if( pPassThrough->SenseInfoLength &&
        pPassThrough->SenseInfoOffset < sizeof(SCSI_PASS_THROUGH) )
    {
        goto exit_validatepassthrough;
    }

    fResult = TRUE;

exit_validatepassthrough:

    return fResult;
}
