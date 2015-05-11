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
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef ICH_IOCTL_H_
#define ICH_IOCTL_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#define IOCTL_HAL_POWERSHUTDOWN		CTL_CODE(FILE_DEVICE_HAL, 3000, METHOD_BUFFERED, FILE_ANY_ACCESS)


#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // ICH_IOCTL_H_
