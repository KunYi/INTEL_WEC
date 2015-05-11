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
#ifndef _HELPER_H_
#define _HELPER_H_

HKEY WavLoadRegKey(HKEY hActiveKey, TCHAR **pszDevKey);
BOOL WavGetRegistryValue(HKEY hKey, PTSTR szValueName, PDWORD pdwValue);
BOOL WavGetRegistryString( HKEY hKey, PTSTR szValueName, PTSTR *pszValue, DWORD dwSize=0);
BOOL WavGetRegistryString2( HKEY hKey, PTSTR szValueName, PTSTR *pszValue, DWORD dwSize=0);
BOOL WavSetRegistryValue(HKEY hKey, PTSTR szValueName, DWORD dwValue);
BOOL WavSetRegistryValue2(HKEY hKey, PTSTR szValueName, DWORD dwValue);
BOOL WavSetRegistryString( HKEY hKey, PTSTR szValueName, PTSTR szValue);
BOOL WavParseIdString(BYTE *str, int len, DWORD *pdwOffset,BYTE **ppDest, DWORD *pcBytesLeft);

#endif // _HELPER_H_
