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

/*++

Module Name:
    atapipcicd.h

Abstract:
    Base ATA/ATAPI PCI CD-ROM/DVD device support.

Revision History:

--*/

#include <atamain.h>
#include <atapipcicd.h>

// ----------------------------------------------------------------------------
// Function: CreatePCIHDCD
//     Spawn function called by IDE/ATA controller enumerator
//
// Parameters:
//     hDevKey -
// ----------------------------------------------------------------------------

EXTERN_C
CDisk *
CreatePCIHDCD(
    HKEY hDevKey
    )
{
    return new CPCIDiskAndCD(hDevKey);
}

// ----------------------------------------------------------------------------
// Function: CPCIDiskAndCD
//     Constructor
//
// Parameters:
//     hKey -
// ----------------------------------------------------------------------------

CPCIDiskAndCD::CPCIDiskAndCD(
    HKEY hKey
    ) : CPCIDisk(hKey)
{
    m_pSterileCdRomReadRequest = NULL;
    m_cbSterileCdRomReadRequest = 0;
    m_dwMediaChangeCount = 0;
    m_dwAtapiState = 0;
}

// ----------------------------------------------------------------------------
// Function: CPCIDiskAndCD
//     Destructor
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

CPCIDiskAndCD::~CPCIDiskAndCD(
    )
{
    if (NULL != m_pSterileCdRomReadRequest) {
        LocalFree((HLOCAL)m_pSterileCdRomReadRequest);
        m_pSterileCdRomReadRequest = NULL;
    }
}

// ----------------------------------------------------------------------------
// Function: SterilizeCdRomReadRequest
//     Sterilize user-supplied CDROM_READ request.  If *ppSafe is NULL or not
//     sufficiently large enough, then re-allocate.  If ppSafe is re-allocated,
//     then return the updated number of SGX_BUF-s through lpcSafeSgX.  This
//     function must be called within a __try/__except block.
//
// Parameters:
//     ppSafe -
//     lpcSafeSgX -
//     pUnsafe -
//     cbUnsafe -
// ----------------------------------------------------------------------------

BOOL CPCIDiskAndCD::SterilizeCdRomReadRequest(
    PCDROM_READ* ppSafe,
    LPDWORD      lpcbSafe,
    PCDROM_READ  pUnsafe,
    DWORD        cbUnsafe,
    DWORD        dwArgType,
    __out_ecount(dwOldPtrCount) PUCHAR * saveoldptrs,
    __in DWORD dwOldPtrCount
    )
{
    DWORD       i = 0, mappedbuffers;
    PCDROM_READ pSafe = NULL;
    DWORD       cbSafe = 0;
    PUCHAR      ptemp;

    // ppSafe, lpcSafeSgX, and pUnsafe are required.  However, *ppSafe may
    // be NULL.
    if ((NULL == ppSafe)|| (NULL == lpcbSafe) || (NULL == pUnsafe)) {
        return FALSE;
    }
    // Extract args so we don't have to continually dereference.
    pSafe = *ppSafe;
    cbSafe = *lpcbSafe;
    // Is unsafe request smaller than minimum?
    if (cbUnsafe < sizeof(CDROM_READ)) {
        return FALSE;
    }
    // Is unsafe request correctly sized?
    if (cbUnsafe < (sizeof(CDROM_READ) + (sizeof(SGX_BUF) * (pUnsafe->sgcount - 1)))) {
        return FALSE;
    }
    // Is unsafe request larger than safe request container?
    if (cbUnsafe > cbSafe) {
        // Deallocate safe request container, if applicable.
        if (NULL != pSafe) {
            LocalFree((HLOCAL)pSafe);
            pSafe = NULL;
        }
        // Reallocate safe request container.
        pSafe = (PCDROM_READ)LocalAlloc(LPTR, cbUnsafe);
        if (NULL == pSafe) {
            // Failed to reallocate safe request container.  Fail.
            *ppSafe = NULL;
            *lpcbSafe = 0;
            return FALSE;
        }
        // Update safe request container.
        *ppSafe = pSafe;
        *lpcbSafe = cbUnsafe;
        // Update extracted size arg.
        cbSafe = cbUnsafe;
    }
    // Safely copy unsafe request to safe request.
    if (0 == CeSafeCopyMemory((LPVOID)pSafe, (LPVOID)pUnsafe, cbUnsafe)) {
        return FALSE;
    }
    if( pSafe->sgcount > dwOldPtrCount )
    {
        return FALSE;
    }
    // Map unsafe embedded pointers to safe request.
    for (i = 0; i < pSafe->sgcount; i += 1) {

        if (
            (NULL == pSafe->sglist[i].sb_buf) ||
            (0 == pSafe->sglist[i].sb_len)
        ) {
            goto CleanUpLeak;
        }

        // Verify embedded pointer access and map user mode pointers

        if (FAILED(CeOpenCallerBuffer(
                    (PVOID *)&ptemp,
                    (LPVOID)pSafe->sglist[i].sb_buf,
                    pSafe->sglist[i].sb_len,
                    dwArgType,
                    FALSE)))
        {
            goto CleanUpLeak;
        }

        ASSERT(ptemp);
        saveoldptrs[i] = pSafe->sglist[i].sb_buf;
        pSafe->sglist[i].sb_buf = ptemp;
    }
    return TRUE;

CleanUpLeak:

    mappedbuffers = i;
    for (i = 0; i < mappedbuffers; i++) {

        ASSERT((NULL != pSafe->sglist[i].sb_buf) &&
               (0 == pSafe->sglist[i].sb_len));

        // Close previously mapped pointers

        if (FAILED(CeCloseCallerBuffer(
                    (LPVOID)pSafe->sglist[i].sb_buf,
                    (LPVOID)saveoldptrs[i],
                    pSafe->sglist[i].sb_len,
                    dwArgType)))
        {
            ASSERT(!"Cleanup call to CeCloseCallerBuffer failed unexpectedly");
            return FALSE;
        }
    }

    return FALSE;

}

// ----------------------------------------------------------------------------
// Function: MainIoctl
//     Process IOCTL_CDROM_ and IOCTL_DVD_ I/O controls
//
// Parameters:
//     pIOReq -
// ----------------------------------------------------------------------------

DWORD
CPCIDiskAndCD::MainIoctl(
    PIOREQ pIOReq
    )
{
    DWORD dwError;

    DEBUGMSG(ZONE_IOCTL, (_T(
        "Atapi!CPCIDiskAndCD::MainIoctl> IOCTL(0x%x), device(%d)\r\n"
        ),pIOReq->dwCode, m_dwDeviceId));

    dwError = CPCIDisk::MainIoctl(pIOReq);

    if (dwError == ERROR_NOT_SUPPORTED) {

        switch(pIOReq->dwCode) {

            // SCSI passthru commands
            case IOCTL_SCSI_PASS_THROUGH:
            case IOCTL_SCSI_PASS_THROUGH_DIRECT:

            // supported ATAPI commands
            case IOCTL_CDROM_READ_SG:
            case IOCTL_CDROM_TEST_UNIT_READY:
            case IOCTL_CDROM_DISC_INFO:
            case IOCTL_CDROM_EJECT_MEDIA:
            case IOCTL_CDROM_LOAD_MEDIA:

            // supported DVD commands
            case IOCTL_DVD_START_SESSION:
            case IOCTL_DVD_READ_KEY:
            case IOCTL_DVD_END_SESSION:
            case IOCTL_DVD_SEND_KEY:
            case IOCTL_DVD_GET_REGION:

            // supported audio commands
            case IOCTL_CDROM_READ_TOC:
            case IOCTL_CDROM_GET_CONTROL:
            case IOCTL_CDROM_PLAY_AUDIO_MSF:
            case IOCTL_CDROM_SEEK_AUDIO_MSF:
            case IOCTL_CDROM_STOP_AUDIO:
            case IOCTL_CDROM_PAUSE_AUDIO:
            case IOCTL_CDROM_RESUME_AUDIO:
            case IOCTL_CDROM_GET_VOLUME:
            case IOCTL_CDROM_SET_VOLUME:
            case IOCTL_CDROM_READ_Q_CHANNEL:
            case IOCTL_CDROM_GET_LAST_SESSION:
            case IOCTL_CDROM_RAW_READ:
            case IOCTL_CDROM_DISK_TYPE:
            case IOCTL_CDROM_SCAN_AUDIO:
            case IOCTL_CDROM_ISSUE_INQUIRY:

                if (IsAtapiDevice()) {
                    dwError = AtapiIoctl(pIOReq);
                }
                else {
                    dwError = ERROR_INVALID_OPERATION;
                }
                break;

            default:
                dwError = ERROR_NOT_SUPPORTED;
                break;
        }
    }

    return dwError;
}

