//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
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
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************


Module Name:

	Debug.h

Abstract:

	Header file for debug registration.

Revision History:

	mmaguire 02/13/99 - based losely on older WinCE DirectSound implementation
						which used PSL's to separate client and server code.


*****************************************************************************/
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// BEGIN INCLUDES
//
// standard includes:
//
#include <windows.h>
//
// where we can find declaration for main class in this file:
//
//
//
// where we can find declarations needed in this file:
//

//
// END INCLUDES
//////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_H_
#define _DEBUG_H_

//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
Setup up debug zones
*****************************************************************************/
//////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG

// Uncomment this to turn on debugging for latency.
//#define DEBUG_LATENCY

// Uncomment this to debug play position.
//#define DEBUG_PLAY_POSITION

// This is a suggestion which came from Kent Lottis out of a code
// review 06/24/99.  In debug builds, we will add a member variable "m_myThis"
// to each class in which we will save the value of "this" at creation time.
// Each time an object's method is accessed, we will check 
// that the current "this" pointer matches the object's "m_myThis" pointer.
// If it doesn't it means somebody is attempting to perform operations
// on a pointer they think is a pointer to a valid object, but which 
// is actually garbage.
// We will also scramble the m_myThis pointer in our destructor to make
// sure we catch usage of already deleted objects.

// Put this in the private part of each class definition.
#define MYTHIS_DECLARATION void * m_myThis;

// Put this at the beginning of each constructor.
#define MYTHIS_CONSTRUCTOR m_myThis=this;

// Put this at the end of each destructor.
#define MYTHIS_DESTRUCTOR m_myThis=(void*)0xcdcdcdcd;

// Put this at the beginning of each method.
#define _MYTHIS_CHECK(action)\
    if( m_myThis != this ) { \
        DEBUGMSG(ZONE_ERROR, (TEXT("method call on a bogus interface pointer\r\n"))); \
        ASSERT(FALSE); \
        action;\
    }

// Put this at the beginning of each method.
#define MYTHIS_CHECK _MYTHIS_CHECK(0)

//#define MYTHIS_CHECK ASSERT( m_myThis == this );

void __cdecl
ZoneDbgPrintf(PTCHAR pszFormat, ...);

#define MODNAME TEXT("HDA")

#define DBG_BIT(bitno) (1<<(bitno))

#define ZONE_ERROR		DEBUGZONE(0)
#define ZONE_WARNING	DEBUGZONE(1)
#define ZONE_ENTER		DEBUGZONE(2)
#define ZONE_INIT		DEBUGZONE(3)
#define ZONE_INFO		DEBUGZONE(4)
#define ZONE_IOCTL		DEBUGZONE(5)
#define ZONE_INTERRUPT  DEBUGZONE(6)
#define ZONE_CALLBACK	DEBUGZONE(7)
#define ZONE_PLAYCURSOR	DEBUGZONE(8)
#define ZONE_VOLUME     DEBUGZONE(9)
#define ZONE_DRIVER     DEBUGZONE(10)
#define ZONE_HDA	DEBUGZONE(11)
#define ZONE_VERBOSE	DEBUGZONE(12)
#define ZONE_REFCOUNT   DEBUGZONE(13)
#define ZONE_PERF		DEBUGZONE(14)
#define ZONE_TEMP		DEBUGZONE(15)

#ifdef INSTANTIATE_DEBUG
DBGPARAM dpCurSettings =
{
	MODNAME,
	{
		TEXT("Errors"),			// 0
		TEXT("Warnings"),		// 1
		TEXT("Enter,Exit"),		// 2
		TEXT("Initialize"),		// 3
		TEXT("Info"),			// 4
		TEXT("IOCTL"),			// 5
		TEXT("Interrupt"),		// 6
		TEXT("Callback"),		// 7
		TEXT("Play Cursor"),  	// 8
		TEXT("Volume"), 		// 9
		TEXT("Driver"),		    // 10
		TEXT("HDA"),		// 11
		TEXT("Verbose"), 		// 12
		TEXT("Refcount"),		// 13
		TEXT("Performance"),	// 14
		TEXT("Temporary tests")	// 15
	},
//	0x0000      // nothing
//  0xffff      // everything
      (1<< 0)   // Errors
    | (1<< 1)   // Warnings
//   | (1<< 4)   // Info
//	| (1<< 11)
//	| (1 << 12)
/*

    | (1<< 2)   // Enter/Exit
    | (1<< 3)   // Initialize
    | (1<< 4)   // Info
    | (1<< 5)   // IOCTL
    | (1<< 6)   // Interrupt
    | (1<< 7)   // Callback
    | (1<< 8)   // Play Cursor
    | (1<< 9)   // Undefined
    | (1<<10)   // Driver
    | (1<<11)   // HDA
    | (1<<12)   // Verbose
    | (1<<13)   // Refcount
    | (1<<14)   // Performance
    | (1<<15)   // Temporary Tests
*/
};
#endif // INSTANTIATE_DEBUG

#else // NOT DEBUG

// We want these to compile away to nothing:
#define MYTHIS_DECLARATION
#define MYTHIS_CONSTRUCTOR
#define MYTHIS_DESTRUCTOR
#define MYTHIS_CHECK
#define _MYTHIS_CHECK(x)

#define ZONE_ERROR	0
#define ZONE_WARNING	0
#define ZONE_ENTER	0
#define ZONE_INIT	0
#define ZONE_INFO	0
#define ZONE_IOCTL	0
#define ZONE_VOLUME 0
#define ZONE_REFCOUNT	0
#define ZONE_RESOURCES	0
#define ZONE_CRITSEC	0
#define ZONE_MIXER	0
#define ZONE_PERF	0
#define ZONE_TEMP	0

#endif //DEBUG

#endif // _DEBUG_H_
