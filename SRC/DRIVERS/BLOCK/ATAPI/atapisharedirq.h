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
// AtapiSharedIrq.h
//
// Base ATA/ATAPI IRQ sharing support.
//

#pragma once

#include <atamain.h>
#include <diskmain.h>

class CPCISharedIRQ
{
protected:
    HANDLE m_hIsr;
    HANDLE m_hIst;
    HANDLE m_hSharedIRQEvent;
    BOOL   m_fTerminated;
    CPort *m_pIDEPort;

    enum
    {
        // Config port undo flags
        CP_CLNUP_IRQEVENT       = 0x01,
        CP_CLNUP_HEVENT         = 0x02,
        CP_CLNUP_INTSYSINTR     = 0x04,
        CP_CLNUP_INTCHNHDNLR    = 0x08,
        CP_CLNUP_IST            = 0x10
    };
    
    static DWORD IstMain(CPCISharedIRQ* pThis);
public:
    //
    // AtapiSharedIrq.cpp
    //
    CPCISharedIRQ();
    virtual ~CPCISharedIRQ();
    virtual BOOL HookSharedIrq( CDisk *pThis );
};
