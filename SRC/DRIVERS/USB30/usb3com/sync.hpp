//
// Copyright(c) Microsoft Corporation.  All rights reserved.
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
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.

//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2013 Intel Corporation All Rights Reserved. 
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


//----------------------------------------------------------------------------------
// File: sync.hpp
//      Implements Enhanced Critical Section API
//----------------------------------------------------------------------------------

#ifndef SYNC_HPP
#define SYNC_HPP

typedef enum _CRYT_SEC_STATUS
{
    CSS_SUCCESS = 0,
    CSS_DESTROYED,
    CSS_TIMEOUT
} CRYT_SEC_STATUS;

class Countdown
{
private:

        CRITICAL_SECTION cs;
        HANDLE hev;
        BOOL fLock;
        DWORD dwCount;
        VOID LockCountdown();

public:

    Countdown(DWORD);
    ~Countdown();
    BOOL IncrCountdown();
    VOID DecrCountdown();
    VOID WaitForCountdown(BOOL fKeepLocked);
    VOID UnlockCountdown()
    {
        EnterCriticalSection(&cs);
        fLock = FALSE;
        LeaveCriticalSection(&cs);
    }
};

class LockObject
{
    friend class CHcd;  // CHcd:CDeviceGlobal
    friend class CHW;   // CHW:CHcd
    friend class CXHcd; // CXHcd:CHW
    friend class CHub;

private:
    CRITICAL_SECTION m_CSection;
    CRITICAL_SECTION m_CESection;

public:
    LockObject()
    {
        InitializeCriticalSection(&m_CSection);
        InitializeCriticalSection(&m_CESection);
    };

    ~LockObject()
    {
        DeleteCriticalSection(&m_CSection);
        DeleteCriticalSection(&m_CESection);
    };

    VOID Lock(VOID) 
    {
        EnterCriticalSection(&m_CSection);
    };

    VOID Unlock(VOID)
    {
        LeaveCriticalSection(&m_CSection);
    };

    VOID EventLock(VOID)
    {
        EnterCriticalSection(&m_CESection);
    };

    VOID EventUnlock(VOID)
    {
        LeaveCriticalSection(&m_CESection);
    };

    BOOL TryLock(VOID)
    {
        return TryEnterCriticalSection(&m_CSection);
    };
};

#endif /* SYNC_HPP */