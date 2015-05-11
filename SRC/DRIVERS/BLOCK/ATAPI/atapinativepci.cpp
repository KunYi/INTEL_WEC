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
//  

/*++

Module Name:
    AtapiNativePCI.cpp

Abstract:
    Base ATA/ATAPI Native PCI controller (IRQ sharing) support.

Revision History:

--*/

#include <atamain.h>
#include <giisr.h>
#include <atapinativepci.h>

// ----------------------------------------------------------------------------
// Function: CreateNativePCIHD
//     Spawn function called by IDE/ATA controller enumerator
//
// Parameters:
//     hDevKey -
// ----------------------------------------------------------------------------

EXTERN_C
CDisk *
CreateNativePCIHD(
    HKEY hDevKey
    )
{
    return new CNativePCIDisk(hDevKey);
}

// ----------------------------------------------------------------------------
// Function: CNativePCIDisk
//     Constructor
//
// Parameters:
//     hKey -
// ----------------------------------------------------------------------------

CNativePCIDisk::CNativePCIDisk(
    HKEY hKey
    ) : CPCIDisk(hKey), CPCISharedIRQ()
{
}

// ----------------------------------------------------------------------------
// Function: CNativePCIDiskAndCD
//     Destructor
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

CNativePCIDisk::~CNativePCIDisk(
    )
{
}

// ----------------------------------------------------------------------------
// Function: ConfigPort
//     Initialize IST/ISR
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

BOOL
CNativePCIDisk::ConfigPort(
    )
{
    // if the port's IRQ event has already been created, then we are a slave
    if ( (m_pPort->m_hIRQEvent) && (0 == bIsNativeMasterDeviceDeleted) ) {
        return CPCIDisk::ConfigPort();
    }

    if (!CPCIDisk::ConfigPort())
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CNativePCIDisk::ConfigPort> Failed in CNativePCIDisk::ConfigPort()\r\n"
            )));
        goto cleanUp;
    }

    if (!HookSharedIrq(this))
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CNativePCIDisk::ConfigPort> Failed in CPCISharedIRQ::HookSharedIrq()\r\n"
            )));
        goto cleanUp;
    }
    return TRUE;

cleanUp:
    return FALSE;
}
