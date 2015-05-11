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
#include <wavdrv.h>


HKEY WavLoadRegKey(HKEY hActiveKey, TCHAR **pszDevKey)
{
    DWORD dwType;
    PTSTR szDevKey = NULL;
    DWORD dwValueLen = 0;
    HKEY hDevKey = NULL;    
    // Find out how big the name of the device key is
    // Since dwValueLen is 0 and the name is NULL we expect a ERROR_SUCCESS although the doc's
    // say that it should return ERROR_MORE_DATA
    if (ERROR_SUCCESS == RegQueryValueEx( hActiveKey, DEVLOAD_DEVKEY_VALNAME, NULL, &dwType, NULL, &dwValueLen)) {
        szDevKey = (PTSTR)LocalAlloc( LPTR, dwValueLen);
        // Read in the KeyName of the device
        RegQueryValueEx( hActiveKey, DEVLOAD_DEVKEY_VALNAME, NULL, &dwType, (PBYTE)szDevKey, &dwValueLen);
            // Now do the actual open of the key
        if (ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, szDevKey, 0, 0, &hDevKey)) {
            hDevKey = NULL;
            DEBUGMSG( ZONE_INIT, (TEXT("Found a device key name %s but could not open it !!!\r\n"), szDevKey));
        }
    }
    if (!hDevKey) {
        LocalFree( szDevKey);
        *pszDevKey = NULL;
    } else {    
        *pszDevKey = szDevKey;
    }    
    return hDevKey;     
}

BOOL WavGetRegistryValue(HKEY hKey, PTSTR szValueName, PDWORD pdwValue)
{
    
    DWORD               dwValType, dwValLen;
    LONG                lStatus;
            
    dwValLen = sizeof(DWORD);
        
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)pdwValue, &dwValLen);
        
    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_DWORD)) {           
        DEBUGMSG( ZONE_ERROR, (TEXT("RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pdwValue = -1;
        return FALSE;
    } 
    DEBUGMSG( ZONE_INFO, (TEXT("WavGetRegistryValue(%s) Value(%x) hKey: %x\r\n"), szValueName,*pdwValue,hKey));
    return TRUE;
}

BOOL WavGetRegistryString( HKEY hKey, PTSTR szValueName, PTSTR *pszValue, DWORD dwSize)
{
    DWORD             dwValType, dwValLen;
    LONG                lStatus;
    
    dwValLen = 0;
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, NULL, &dwValLen);

    if (dwSize && (dwValLen > dwSize)) {
        DEBUGMSG( ZONE_ERROR, (TEXT("WavGetRegistryString size specified is too small!!!\r\n")));
        return FALSE;
    }   
        
    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_SZ)) {          
        DEBUGMSG( ZONE_ERROR, (TEXT("RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pszValue = NULL;
        return FALSE;
    }
    
    if (!dwSize) 
        *pszValue = (PTSTR)LocalAlloc( LPTR, dwValLen);
    
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)*pszValue, &dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_ERROR, (TEXT("RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        LocalFree( *pszValue);
        *pszValue = NULL;
        return FALSE;
    }    
    DEBUGMSG( ZONE_INFO, (TEXT("WavGetRegistryString(%s) Value(%s) hKey: %x\r\n"), szValueName,*pszValue, hKey));
    return TRUE;
}


BOOL WavSetRegistryValue(HKEY hKey, PTSTR szValueName, DWORD dwValue)
{
    
    DWORD              dwValLen;
    LONG                lStatus;
            
    dwValLen = sizeof(DWORD);
        
    lStatus = RegSetValueEx( hKey, szValueName, 0, REG_DWORD, (PBYTE)&dwValue, dwValLen);
        
    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_ERROR, (TEXT("RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        return FALSE;
    } 
    DEBUGMSG( ZONE_INFO, (TEXT("WavSetRegistryValue(%s) Value(%x) hKey: %x\r\n"), szValueName, dwValue,hKey));
    return TRUE;
}


BOOL WavSetRegistryString( HKEY hKey, PTSTR szValueName, PTSTR szValue)
{
    DWORD             dwValLen;
    LONG               lStatus;
    
    dwValLen = (wcslen( szValue)+1)*sizeof(WCHAR);
    lStatus = RegSetValueEx( hKey, szValueName, 0, REG_SZ, (PBYTE)szValue, dwValLen);
        
    if (lStatus != ERROR_SUCCESS) {          
        DEBUGMSG( ZONE_ERROR, (TEXT("RegSetValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        return FALSE;
    }
    
    DEBUGMSG( ZONE_INFO, (TEXT("WavSetRegistryString(%s) Value(%s) hKey: %x\r\n"), szValueName, szValue, hKey));
    return TRUE;
}


BOOL WavParseIdString(BYTE *str, int len, DWORD *pdwOffset,BYTE **ppDest, DWORD *pcBytesLeft)
{
    BYTE *p;
    DWORD cCopied;
    BYTE *pDest;
    
    // check validity (spec says ASCII, I assumed printable)
    for (p = str; p < &str[len]; ++p)
        if (*p < 0x20 || *p > 0x7F) {
            *pdwOffset = 0;
            return FALSE;
        }

    // find the last non-pad character
    for (p = &str[len]; p > str && p[-1] == ' '; --p)
        ;
    cCopied = p - str;

    // special case - empty string implies not present
    if (cCopied == 0) {
        *pdwOffset = 0;
        return TRUE;
    }
    ++cCopied; // this is a byte count so add a terminating null

    // always increment *ppDest because it counts the bytes that we want,
    // not just the bytes that we've actually written.
    pDest = *ppDest;
    *ppDest += cCopied;

    // if there has already been an error, then we needn't continue further
    if (GetLastError() != ERROR_SUCCESS)
        return TRUE;

    // make sure there's enough space to copy into
    if (cCopied > *pcBytesLeft) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        cCopied = *pcBytesLeft;
    }
    __try {
        if (cCopied) {
            CeSafeCopyMemory (pDest, str, cCopied-1);
            pDest[cCopied-1] = '\0';
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    };

    *pcBytesLeft -= cCopied;
    return TRUE;
}

