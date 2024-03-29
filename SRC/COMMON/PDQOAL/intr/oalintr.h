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
/*
 *    Module Name:  
 *       oalintr.h
 *
 *    Abstract:
 *          System Interrupt definitions for OAL layer.  SYSINTR_FIRMWARE is
 *       defined in nkintr.h.  The OAL can use any values in the range
 *       SYSINTR_FIRMWARE thru SYSINTR_MAXIMUM.
 *
 */

// Note : The CEPC is a bit unusual since we allow various
// cards to be a different addresses without actually defining
// a new platform.  So, for those drivers there is no need
// to define an actual SYSINTR value.  If the driver is loaded
// by a bus driver, the SYSINTR value will be provided (in the
// registry) by the bus driver otherwise OALIntrRequestSysIntr may
// be used to request a SYSINTR <-> IRQ mapping.
//
// Define SYSINTR values for statically-mapped (SYSINTR <-> IRQ map) devices.
//
#define SYSINTR_KEYBOARD    (SYSINTR_FIRMWARE+1)
#define SYSINTR_TOUCH       (SYSINTR_FIRMWARE+12)

// And Audio hasn't been modified yet, but should be.
#define SYSINTR_AUDIO       (SYSINTR_FIRMWARE+4)



