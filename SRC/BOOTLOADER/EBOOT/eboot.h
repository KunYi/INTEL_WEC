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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

    eboot.h
    
Abstract:  

    Header file for ethernet bootloader.
    
Notes: 

--*/

#ifndef _EBOOT_H
#define _EBOOT_H


//------------------------------------------------------------------------------
//
//  Define Name:    EBOOT_VERSION
//  Description:    Major and Minor version numbers for bootloader.
//
//------------------------------------------------------------------------------

#define EBOOT_VERSION_MAJOR  3
#define EBOOT_VERSION_MINOR  7


//------------------------------------------------------------------------------
//
//  Section Name:   Memory Configuration
//  Description.:   The constants defining the memory layout below must match
//                  those defined in the .bib files.
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  Define Name:    RAM_START/END
//  Description:    Defines the start/end address of RAM.
//
//------------------------------------------------------------------------------

#define RAM_START               (0x00000000)
#define RAM_SIZE                (64 * 1024 * 1024)                  // 64MB
#define RAM_END                 (RAM_START + RAM_SIZE - 1)


#endif // _EBOOT_H
