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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//  
// -----------------------------------------------------------------------------
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
#include <windows.h>
#include <x86boot.h>
#include <kitl.h>
#include <oal.h>


extern LPCWSTR g_oalIoCtlPlatformType;
extern LPCSTR  g_oalDeviceNameRoot;
extern UINT8 g_x86uuid[16];

static void
itoa10(
    int n,
    CHAR s[],
    int bufflen
    )
{
    int i = 0;

    // Get absolute value of number
    unsigned int val = (unsigned int)((n < 0) ? -n : n);

    // Extract digits in reverse order
    while (val)
    {
        // Make sure we don't step off the end of the character array (leave
        // room for the possible '-' sign and the null terminator).
        if (i < (bufflen - 2))
        {
            s[i++] = (val % 10) + '0';
        }

        val /= 10;
    }

    // Add sign if number negative
    if (n < 0) s[i++] = '-';

    s[i--] = '\0';

    // Reverse string
    for (n = 0; n < i; n++, i--) {
        char swap = s[n];
        s[n] = s[i];
        s[i] = swap;
    }
}


VOID CreateName(UINT16 mac[3], CHAR *pBuffer)
{
    if ( mac[0] || mac[1] || mac[2] ) {

        itoa10 (((mac[2]>>8) | ((mac[2] & 0x00ff) << 8)), (pBuffer + strlen (pBuffer)), (OAL_KITL_ID_SIZE - strlen(pBuffer)));
    }
}


VOID* OALArgsQuery(UINT32 type)
{
    switch(type)
    {
        case OAL_ARGS_QUERY_DEVID:
            // Update the string only if it is not already updated.
            // The buffer will already have the prefix (root) included.

            if ( strlen(g_pX86Info->szDeviceName) <= strlen(g_oalDeviceNameRoot) )
            {
                CreateName(g_pX86Info->wMac, g_pX86Info->szDeviceName);
            }

            return (VOID*)g_pX86Info->szDeviceName;
            break;
        case OAL_ARGS_QUERY_UUID:
            RETAILMSG(1, (TEXT("You are requesting a UUID from a platform which does not guarantee universal uniqueness.\r\n")));
            return (VOID*)(&(g_x86uuid));
            break;
        default:
            return NULL;
    }
}

BOOL x86IoCtlHalSetDeviceInfo (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
    static WCHAR szPlatformType[80];    // max of 80 chars
    
    if (lpBytesReturned) {
        *lpBytesReturned = 0;
    }

    if ((nInBufSize > sizeof (DWORD))
        && (SPI_GETPLATFORMTYPE == *(LPDWORD)lpInBuf)
        && ((nInBufSize -= sizeof (DWORD)) <= sizeof (szPlatformType))) {

        memcpy (szPlatformType, ((LPBYTE)lpInBuf)+sizeof(DWORD), nInBufSize);
        g_oalIoCtlPlatformType = szPlatformType;

        return TRUE;        
    }

    NKSetLastError (ERROR_INVALID_PARAMETER);
    return FALSE;
}

BOOL x86IoCtlHalGetUUID (
    UINT32 code, VOID *lpInBuf, UINT32 nInBufSize, VOID *lpOutBuf, 
    UINT32 nOutBufSize, UINT32 *lpBytesReturned
) {
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    UNREFERENCED_PARAMETER(nInBufSize);
    UNREFERENCED_PARAMETER(lpInBuf);
    UNREFERENCED_PARAMETER(code);

    if (lpBytesReturned) {
        *lpBytesReturned = sizeof(GUID);
    }
    if (lpOutBuf && (nOutBufSize >= sizeof(GUID))) {
        USHORT *wMac = g_pX86Info->wMac;
        // OEM's with unique ID hardware can return the value here.

        // The CEPC platform does not have any unique ID settings.
        // We'll use the Kernel Ethernet Debug address if non-zero
        if (wMac[0] | wMac[1] | wMac[2]) {
            memset(lpOutBuf, 0, sizeof(GUID));
            memcpy(lpOutBuf, (char *)wMac, sizeof(g_pX86Info->wMac));
            return TRUE;
        }
        dwErr = ERROR_NOT_SUPPORTED;
    }
    NKSetLastError (dwErr);
    return FALSE;

}

