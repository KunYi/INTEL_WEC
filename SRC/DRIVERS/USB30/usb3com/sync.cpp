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

//----------------------------------------------------------------------------------
// File: sync.cpp
//
// Description:
// 
//      Implements Enhanced Critical Section API
//
//----------------------------------------------------------------------------------

#include "globals.hpp"
#include "sync.hpp"

//----------------------------------------------------------------------------------
// Function: Countdown
//
// Description: constructor for Countdown
//
// Parameters: dwInitial - initial state
//
// Returns: none
//----------------------------------------------------------------------------------
Countdown::Countdown(DWORD dwInitial)
{
    dwCount = dwInitial;
    if(dwInitial)
    {
        hev = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
    else
    {
        hev = CreateEvent(NULL, TRUE, TRUE, NULL);
    }
    
    InitializeCriticalSection(&cs);
    fLock = FALSE;
}

//----------------------------------------------------------------------------------
// Function: IncrCountdown
//
// Description: Increments counter
//
// Parameters: none
//
// Returns: TRUE - is success, FALSE - elsewhere
//----------------------------------------------------------------------------------
BOOL Countdown::IncrCountdown()
{
    BOOL fRet = TRUE;

    EnterCriticalSection(&cs);
    if(fLock)
    {
        fRet = FALSE;
    }
    else
    {
        if(dwCount++ == 0)
        {
            ResetEvent(hev);
        }
    }
    LeaveCriticalSection(&cs);

    return fRet;
}

//----------------------------------------------------------------------------------
// Function: DecrCountdown
//
// Description: Decrements counter
//
// Parameters: none
//
// Returns: TRUE - is success, FALSE - elsewhere
//----------------------------------------------------------------------------------

VOID Countdown::DecrCountdown()
{
    EnterCriticalSection(&cs);
    ASSERT(dwCount > 0);
    if(--dwCount == 0)
    {
        SetEvent(hev);
    }
    LeaveCriticalSection(&cs);
}

//----------------------------------------------------------------------------------
// Function: LockCountdown
//
// Description: Sets lock flag
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID Countdown::LockCountdown()
{
    EnterCriticalSection(&cs);
    fLock = TRUE;
    LeaveCriticalSection(&cs);
}

//----------------------------------------------------------------------------------
// Function: WaitForCountdown
//
// Description: Waiting for countdown event
//
// Parameters: fKeepLocked - is locked
//
// Returns: none
//----------------------------------------------------------------------------------
VOID Countdown::WaitForCountdown(BOOL fKeepLocked)
{
    LockCountdown();
    
    WaitForSingleObject(hev, INFINITE);
    ASSERT(dwCount == 0);

    fLock = fKeepLocked;
}

//----------------------------------------------------------------------------------
// Function: ~Countdown
//
// Description: Destructor for Countdown
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
Countdown::~Countdown()
{
    WaitForCountdown(TRUE);

    CloseHandle(hev);
    DeleteCriticalSection(&cs);
}