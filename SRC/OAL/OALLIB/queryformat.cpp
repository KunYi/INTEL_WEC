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

#include <windows.h>
#include <oal.h>
#include <storemgr.h>
#include <bootarg.h>
#include <x86boot.h>

//------------------------------------------------------------------------------
//
//  Function:  OALIoCtlHalQueryFormatPartition
//
//  This function is called by Filesys.exe to allow an OEM to specify whether a specific
//  partition is to be formatted on mount. Before Filesys.exe calls this IOCTL, it checks
//  the CheckForFormat registry value in the storage profile for your block driver.
//
extern "C"
BOOL x86IoCtlHalQueryFormatPartition(
                               UINT32 code, 
                               __in_bcount(nInBufSize) void *lpInBuf, 
                               UINT32 nInBufSize, 
                               __out_bcount(nOutBufSize) void *lpOutBuf, 
                               UINT32 nOutBufSize, 
                               __out UINT32 *lpBytesReturned
                               )
{
    UNREFERENCED_PARAMETER(code);
    if (  !lpInBuf  || (nInBufSize != sizeof(STORAGECONTEXT))
       || !lpOutBuf || (nOutBufSize < sizeof(BOOL)))
    {
        return FALSE;
    }
    else
    {
        STORAGECONTEXT *pStore = (STORAGECONTEXT *)lpInBuf;
        BOOL  *pfClean = (BOOL*)lpOutBuf;
        WORD* pwRebootSig = (WORD *) BOOT_ARG_REBOOT_SIG_LOCATION;

        __try
        {
            // Indicate that the RAMFMD needs a format on non-reboots.
            if ((NKwcscmp(pStore->StoreInfo.szDeviceName, L"RAMFMD") == 0) &&
                *pwRebootSig != BOOTARG_REBOOT_SIG)
            {
                *pfClean = TRUE;
            }
            else if (g_pX86Info->fFormatUserStore)
            {
                OALMSG(1, (TEXT("OEM: formatting %s\r\n"), pStore->StoreInfo.szDeviceName));
                *pfClean = TRUE;
            }
        }
        __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            // We don't know for sure whether or not we've rebooted, assume we need a format.
            *pfClean = TRUE;
        }

        if (*pfClean)
        {
            if(pStore->dwFlags & AFS_FLAG_BOOTABLE) 
            {
                OALMSG(OAL_WARN, (TEXT("OEM: Clearing storage registry hive\r\n")));
            }
            else
            {
                OALMSG(OAL_WARN, (TEXT("OEM: Clearing storage\r\n")));
            }
        }
        else
        {
            OALMSG(OAL_INFO, (TEXT("OEM: Not clearing storage\r\n")));
        }
    }

    if (lpBytesReturned)
    {
        *lpBytesReturned = sizeof(UINT32);
    }

    return TRUE;
}
