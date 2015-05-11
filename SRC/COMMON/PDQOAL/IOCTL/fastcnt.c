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
#include <oal.h>


BOOL x86IoCtlGetFastCounter(
                               UINT32 code, 
                               __in_bcount(nInBufSize) void *lpInBuf, 
                               UINT32 nInBufSize, 
                               __out_bcount(nOutBufSize) void *lpOutBuf, 
                               UINT32 nOutBufSize, 
                               __out UINT32 *lpBytesReturned
                               ) 
{
    BOOL  rc = FALSE;
    LARGE_INTEGER perfCounter = {0};

    UNREFERENCED_PARAMETER(nInBufSize);
    UNREFERENCED_PARAMETER(lpInBuf);
    UNREFERENCED_PARAMETER(code);
    
    if ((lpOutBuf == NULL) || (lpBytesReturned == NULL))
    {
        NKSetLastError(ERROR_INVALID_PARAMETER);
        goto cleanUp;
    }
    if (nOutBufSize < sizeof(DWORD))
    {
        NKSetLastError(ERROR_INSUFFICIENT_BUFFER);
        goto cleanUp;
    }

    // Check the boot arg structure for the default display settings.
    __try 
    {
        if (g_pOemGlobal && g_pOemGlobal->pfnQueryPerfCounter)
            rc = g_pOemGlobal->pfnQueryPerfCounter(&perfCounter);
        if (rc == TRUE)
        {
            ((DWORD) *((PDWORD) lpOutBuf)) = perfCounter.LowPart;
            *lpBytesReturned = sizeof (DWORD);
        }
    } 
    __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        rc = FALSE;
        NKSetLastError(ERROR_INVALID_PARAMETER);
        if (lpBytesReturned) 
        {
            *lpBytesReturned = 0;
        }
        goto cleanUp;
    }
     
cleanUp:
    return rc;
}

BOOL x86IoCtlGetFastCounterInfo(
                               UINT32 code, 
                               __in_bcount(nInBufSize) void *lpInBuf, 
                               UINT32 nInBufSize, 
                               __out_bcount(nOutBufSize) void *lpOutBuf, 
                               UINT32 nOutBufSize, 
                               __out UINT32 *lpBytesReturned
                               ) 
{
    BOOL rc = FALSE;
    PFASTCOUNTERINFO pFastCounterInfo = (PFASTCOUNTERINFO) lpOutBuf;
    LARGE_INTEGER freq;

    UNREFERENCED_PARAMETER(nInBufSize);
    UNREFERENCED_PARAMETER(lpInBuf);
    UNREFERENCED_PARAMETER(code);

    if ((lpOutBuf == NULL) || (lpBytesReturned == NULL))
    {
        NKSetLastError(ERROR_INVALID_PARAMETER);
        goto cleanUp;
    }

    if (nOutBufSize < sizeof(FASTCOUNTERINFO))
    {
        NKSetLastError(ERROR_INSUFFICIENT_BUFFER);
        goto cleanUp;
    }


    // Check the boot arg structure for the default display settings.
    __try 
    {
        if (g_pOemGlobal && g_pOemGlobal->pfnQueryPerfFreq 
            && g_pOemGlobal->pfnQueryPerfFreq(&freq) == TRUE)
        {
            pFastCounterInfo->dwFrequency = freq.LowPart;
            pFastCounterInfo->eMode = FAST_COUNTER_MODE_CONTINUOUS;
            *lpBytesReturned = sizeof(FASTCOUNTERINFO);

            rc = TRUE;
        }
        NKSetLastError(ERROR_GEN_FAILURE);
        
    } 
    __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        NKSetLastError(ERROR_INVALID_PARAMETER);
        if (lpBytesReturned) 
        {
            *lpBytesReturned = 0;
        }
        return FALSE;
    }

cleanUp:
    return rc;
}


