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

// /////////////////////////////////////////////////////////////////////////////
// AtapiPCICD.h
//
// Base ATAPI PCI CD-ROM/DVD device support.
//

#pragma once

#include <atapipci.h>

class CPCIDiskAndCD : public CPCIDisk
{
protected:
    PCDROM_READ m_pSterileCdRomReadRequest;
    DWORD m_cbSterileCdRomReadRequest;
    SENSE_DATA m_SenseData;
    DWORD m_dwAtapiState;
    volatile DWORD m_dwMediaChangeCount;

public:
    //
    // AtapiPciCD.cpp
    //
    CPCIDiskAndCD( HKEY hKey );
    virtual ~CPCIDiskAndCD();
    virtual DWORD MainIoctl( PIOREQ pIOReq );

protected:
    static BOOL SterilizeCdRomReadRequest( PCDROM_READ* ppSafe,
                                           LPDWORD      lpcbSafe,
                                           PCDROM_READ  pUnsafe,
                                           DWORD        cbUnsafe,
                                           DWORD        dwArgType,
                                           __out_ecount(dwOldPtrCount) PUCHAR*  saveoldptrs,
                                           __in DWORD dwOldPtrCount );

    //
    // CDIO.cpp
    //
    DWORD AtapiIoctl( PIOREQ pIOReq );
    DWORD ReadCdRom( CDROM_READ *pReadInfo, PDWORD pBytesReturned );
    DWORD SetupCdRomRead( BOOL bRawMode,
                          DWORD dwLBAAddr,
                          DWORD dwTransferLength,
                          PCDB pCdb );
    virtual DWORD ReadCdRomDMA( DWORD dwLBAAddr,
                                DWORD dwTransferLength,
                                WORD wSectorSize,
                                DWORD dwSgCount,
                                SGX_BUF *pSgBuf );

    BOOL AtapiSendCommand( PCDB pCdb,
                           WORD wCount = 0,
                           BOOL fDMA = FALSE);
    BOOL AtapiReceiveData( PSGX_BUF pSgBuf,
                           DWORD dwSgCount,
                           LPDWORD pdwBytesRead,
                           BOOL fGetSense = TRUE);
    BOOL AtapiSendData( PSGX_BUF pSgBuf,
                        DWORD dwSgCount,
                        LPDWORD pdwBytesWritten );

    BOOL AtapiIsUnitReady( PIOREQ pIOReq = NULL );
    BOOL AtapiIsUnitReadyEx();
    BOOL AtapiGetSenseInfo( SENSE_DATA *pSenseData, const TCHAR* strFunctionName = NULL );
    void ReportSenseResults( SENSE_DATA* pSenseData, const TCHAR* strFunctionName );
    BOOL AtapiIssueInquiry( INQUIRY_DATA *pInqData );
    BOOL AtapiGetToc( CDROM_TOC *pTOC );
    DWORD AtapiGetDiscInfo( PIOREQ pIOReq );
    DWORD AtapiReadQChannel( PIOREQ pIOReq );
    DWORD AtapiLoadMedia( BOOL bEject = FALSE );
    void AtapiDumpSenseData( SENSE_DATA* pSenseData = NULL );

/*
    DWORD ScsiPassThrough( const SCSI_PASS_THROUGH& PassThrough,
                           SGX_BUF* pSgxBuf,
                           PSENSE_DATA pSenseData,
                           DWORD* pdwBytesReturned,
                           BOOL fAllowNoData = FALSE );
    DWORD ScsiPassThroughDMA( CDB* pCdb,
                              SGX_BUF* pSgxBuf,
                              PSENSE_DATA pSenseData,
                              DWORD* pdwBytesReturned,
                              BOOL fAllowNoData = FALSE );

    DWORD ScsiPassThrough( SCSI_PASS_THROUGH* pPassThrough,
                           DWORD dwInBufSize,
                           DWORD* pdwBytesReturned,
                           BOOL fInternalCall = FALSE );
*/
    DWORD ScsiPassThrough( PIOREQ pIOReq );
    DWORD ScsiPassThroughDirect( PIOREQ pIOReq );

    DWORD ScsiPassThroughDMA( CDB* pCdb,
                              BYTE* pInBuffer,
                              DWORD dwInBufSize,
                              DWORD* pdwBytesReturned );
    DWORD AtapiPIOTransfer( CDB* pCdb,
                            BOOL fDataIn,
                            PSGX_BUF pSgxBuf,
                            DWORD BufCount,
                            DWORD* pdwBytesReturned );
    BOOL WaitForInterrupt( DWORD dwTimeOut );
    BOOL WaitForInterruptDMA(DWORD dwTimeOut, BYTE* pBMStatus);

    //
    // CDAudio.cpp
    //
    DWORD ControlAudio( PIOREQ pIOReq );

    //
    // DVDIoctl.cpp
    //
    DWORD DVDReadKey( PIOREQ pIOReq );
    DWORD DVDGetRegion( PIOREQ pIOReq );
    DWORD DVDSendKey( PIOREQ pIOReq );
    DWORD DVDSetRegion( PIOREQ pIOReq );
    BOOL DVDGetCopySystem( LPBYTE pbCopySystem, LPBYTE pbRegionManagement );

};

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------


BOOL ValidatePassThrough( SCSI_PASS_THROUGH* pPassThrough, DWORD dwInBufSize );
