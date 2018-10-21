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

#include <windows.h>
#include <x86ioctl.h>
#include <oal.h>
#include <pc_smp.h>
#include <x86boot.h>
#include "ioctl_atom.h"

#ifdef ENABLE_WEA
#include <autohal.h>
#endif

BOOL x86IoCtlHalQueryFormatPartition(
                               UINT32 code, 
                               __in_bcount(nInBufSize) void *lpInBuf, 
                               UINT32 nInBufSize, 
                               __out_bcount(nOutBufSize) void *lpOutBuf, 
                               UINT32 nOutBufSize, 
                               __out UINT32 *lpBytesReturned
                               ) ;

const OAL_IOCTL_HANDLER g_oalIoCtlTable [] = {
{ IOCTL_HAL_QUERY_FORMAT_PARTITION,         0,  x86IoCtlHalQueryFormatPartition },
#ifdef ENABLE_WEA
{ IOCTL_HAL_REBOOT_HANDLER,                 0,  OALIoCtlHalRebootHandler        }, // from <autohal.h>
#include <osaxs_ioctl_tab.h>
#endif
#include "ioctl_tab_atom.h"
};

LPCWSTR g_pPlatformManufacturer = L"Intel Corporation";     // OEM Name
LPCWSTR g_pPlatformName         = L"ATOM";          // Platform Name
LPCWSTR g_oalIoCtlPlatformType = L"ATOM";           // changeable by IOCTL
LPCWSTR g_oalIoCtlPlatformOEM  = L"ATOM";           // constant, should've never changed
LPCSTR  g_oalDeviceNameRoot    = "ATOM";            // ASCII, initial value

LPCWSTR g_pszDfltProcessorName = L"IntelAtom";

const DWORD g_dwBSPMsPerIntr = 1;           // interrupt every ms


#ifdef ENABLE_WEA
BOOL OALIoCtlHalRebootHandler (
    UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer,
    UINT32 outSize, UINT32 *pOutSize
) 
{
    BOOL rc = TRUE;
    return rc;
}
#endif // ENABLE_WEA

