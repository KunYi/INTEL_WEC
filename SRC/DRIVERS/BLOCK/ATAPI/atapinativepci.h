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
// AtapiNativePCI.h
//
// Base ATA/ATAPI Native PCI controller (IRQ sharing) support.
//

#pragma once

#include <atapipci.h>
#include <atapisharedirq.h>

class CNativePCIDisk : public CPCIDisk, public CPCISharedIRQ
{
public:
    //
    // AtapiNativePci.cpp
    //
    CNativePCIDisk( HKEY hKey );
    virtual ~CNativePCIDisk();
    virtual BOOL ConfigPort();
};
