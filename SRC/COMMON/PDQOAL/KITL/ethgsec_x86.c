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
#include <nkintr.h>
#include <oal.h>
#include <oal_kitl.h>

extern BOOL IsAfterPostInit();

DWORD OEMKitlGetSecs()
{
    SYSTEMTIME st;
    DWORD dwRet;
    static DWORD dwLastTime;  // For making sure we aren't running backward
    static DWORD dwBias;

    // after post-init, it's possible that someone is holding the RTC CS, and we cannot safely call OEMGetRealTime while in KITL.
    // So we use CurMSec directly after post init.
    // NOTE: if this function is called while interrupts are off, the time will not advance.
    // 
    if (!IsAfterPostInit()) 
    {
        OEMGetRealTime( &st );
        dwRet = ((60UL * (60UL * (24UL * (31UL * st.wMonth + st.wDay) + st.wHour) + st.wMinute)) + st.wSecond);
        dwBias = dwRet;
    } 
    else 
    {
        dwRet = (CurMSec/1000) + dwBias;
    }

    if (dwRet < dwLastTime) 
    {
        KITLOutputDebugString ("! Time went backwards (or wrapped): cur: %u, last %u\n",
                              dwRet,dwLastTime);
    }

    dwLastTime = dwRet;
    return (dwRet);
}


UINT32 OALGetTickCount()
{
    return OEMKitlGetSecs () * 1000;
}


