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

#include <atamain.h>

BOOL AtaGetRegistryValue(HKEY hKey, const TCHAR* szValueName, PDWORD pdwValue)
{

    DWORD               dwValType, dwValLen;
    LONG                lStatus;

    dwValLen = sizeof(DWORD);

    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)pdwValue, &dwValLen);

    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_DWORD)) {
        DEBUGMSG( ZONE_HELPER , (TEXT("ATAConfig: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pdwValue = (DWORD)-1;
        return FALSE;
    }
    DEBUGMSG( ZONE_HELPER, (TEXT("ATAPI: AtaGetRegistryValue(%s) Value(%x) hKey: %x\r\n"), szValueName,*pdwValue,hKey));
    return TRUE;
}

BOOL AtaGetRegistryString( HKEY hKey, const TCHAR* szValueName, PTSTR *pszValue, DWORD dwSize)
{
    DWORD             dwValType, dwValLen;
    LONG                lStatus;

    dwValLen = 0;
    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, NULL, &dwValLen);

    if (dwSize && (dwValLen > dwSize)) {
        DEBUGMSG( ZONE_HELPER, (TEXT("ATAConfig: AtaGetRegistryString size specified is too small!!!\r\n")));
        return FALSE;
    }

    if ((lStatus != ERROR_SUCCESS) || (dwValType != REG_SZ)) {
        DEBUGMSG( ZONE_HELPER , (TEXT("ATAConfig: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        *pszValue = NULL;
        return FALSE;
    }

    if (!dwSize)
        *pszValue = (PTSTR)LocalAlloc( LPTR, dwValLen);

    lStatus = RegQueryValueEx( hKey, szValueName, NULL, &dwValType, (PBYTE)*pszValue, &dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_HELPER , (TEXT("ATAConfig: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        LocalFree( *pszValue);
        *pszValue = NULL;
        return FALSE;
    }
    DEBUGMSG( ZONE_HELPER, (TEXT("ATAPI: AtaGetRegistryString(%s) Value(%s) hKey: %x\r\n"), szValueName,*pszValue, hKey));
    return TRUE;
}


BOOL AtaSetRegistryValue(HKEY hKey, const TCHAR* szValueName, DWORD dwValue)
{

    DWORD              dwValLen;
    LONG                lStatus;

    dwValLen = sizeof(DWORD);

    lStatus = RegSetValueEx( hKey, szValueName, 0, REG_DWORD, (PBYTE)&dwValue, dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_HELPER , (TEXT("ATAConfig: RegQueryValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        return FALSE;
    }
    DEBUGMSG( ZONE_HELPER, (TEXT("ATAPI: AtaSetRegistryValue(%s) Value(%x) hKey: %x\r\n"), szValueName, dwValue,hKey));
    return TRUE;
}


BOOL AtaSetRegistryString( HKEY hKey, const TCHAR* szValueName, PTSTR szValue)
{
    DWORD             dwValLen;
    LONG               lStatus;

    dwValLen = (wcsnlen_s( szValue, (4*1024/sizeof(WCHAR)))+1)*sizeof(WCHAR);
    lStatus = RegSetValueEx( hKey, szValueName, 0, REG_SZ, (PBYTE)szValue, dwValLen);

    if (lStatus != ERROR_SUCCESS) {
        DEBUGMSG( ZONE_HELPER , (TEXT("ATAConfig: RegSetValueEx(%s) failed -returned %d  Error=%08X\r\n"), szValueName, lStatus, GetLastError()));
        return FALSE;
    }

    DEBUGMSG( ZONE_HELPER, (TEXT("ATAPI: AtaSetRegistryString(%s) Value(%s) hKey: %x\r\n"), szValueName, szValue, hKey));
    return TRUE;
}


 BOOL ATAParseIdString(const BYTE *str, int len, DWORD *pdwOffset,BYTE **ppDest, DWORD *pcBytesLeft)
{
    const BYTE *p;
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
            memcpy(pDest, str, cCopied-1);
            pDest[cCopied-1] = '\0';
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
    };

    *pcBytesLeft -= cCopied;
    return TRUE;
}

// This function is used to fetch an IDE/ATA channel's I/O window from its instance
// key; this function recovers gracefully if an OEM has a proprietary registry
// configuration that doesn't specify bus type or bus number
BOOL
AtaGetRegistryResources(
    HKEY hDevKey,
    PDDKWINDOWINFO pdwi
    )
{
    DEBUGCHK(pdwi != NULL);

    if (!pdwi) {
        return FALSE;
    }

    // fetch I/O window information
    pdwi->cbSize = sizeof(*pdwi);
    if (ERROR_SUCCESS != ::DDKReg_GetWindowInfo(hDevKey, pdwi)) {
        return FALSE;
    }

    // if interface not specified, then assume PCI
    if (pdwi->dwInterfaceType == InterfaceTypeUndefined) {
        DEBUGMSG(ZONE_WARNING, (_T(
            "Atapi!AtaGetRegistryResources> bus type not specified, using PCI as default\r\n"
            )));
        pdwi->dwInterfaceType = PCIBus;
    }

    return TRUE;
}

#ifdef I830M4
BOOL
AtaGetRegistryPciInfo(
    HKEY hDevKey,
    PDDKPCIINFO ppi
    )
{
    DEBUGCHK(ppi != NULL);

    if (!ppi) {
        return FALSE;
    }

    // fetch I/O window information
    ppi->cbSize = sizeof(*ppi);
    if (ERROR_SUCCESS != ::DDKReg_GetPciInfo(hDevKey, ppi)) {
        return FALSE;
    }

    return TRUE;
}
#endif
// This function translates a bus address in an I/O window to a virtual address
// Set *pisIOMapped to TRUE if you want to use the ioWindows array
// Set *pisIOMapped to FALSE if you want to use the memWindows array
// On return *pisIOMapped tells you if the address was IOmapped

DWORD
DoIoTranslation(
    PDDKWINDOWINFO pdwi,
    DWORD dwWindowIndex,
    IN OUT PBOOL pisIOMapped
    )
{
    ASSERT(pisIOMapped);
    PHYSICAL_ADDRESS PhysicalAddress; // bus address
    DWORD AddressSpace = (*pisIOMapped) ? 1 : 0 ;
    LPVOID pAddress;                  // return

    DEBUGCHK(pdwi != NULL);
    DEBUGCHK(dwWindowIndex < MAX_DEVICE_WINDOWS);
    if (!pdwi) {
        return NULL;
    }

    if (*pisIOMapped) {

        DEBUGCHK(dwWindowIndex < pdwi->dwNumIoWindows);
        // extract the target bus address
        PhysicalAddress.HighPart = 0;
        PhysicalAddress.LowPart = pdwi->ioWindows[dwWindowIndex].dwBase;

        // translate the target bus address to a virtual address
        if (!TransBusAddrToVirtual(
            (INTERFACE_TYPE)pdwi->dwInterfaceType,
            pdwi->dwBusNumber,
            PhysicalAddress,
            pdwi->ioWindows[dwWindowIndex].dwLen,
            &AddressSpace,
            &pAddress
         )) {
            return NULL;
        }
    }
    else {

        DEBUGCHK(dwWindowIndex < pdwi->dwNumMemWindows);
        // extract the target bus address
        PhysicalAddress.HighPart = 0;
        PhysicalAddress.LowPart = pdwi->memWindows[dwWindowIndex].dwBase;

        // translate the target bus address to a virtual address
        if (!TransBusAddrToVirtual(
            (INTERFACE_TYPE)pdwi->dwInterfaceType,
            pdwi->dwBusNumber,
            PhysicalAddress,
            pdwi->memWindows[dwWindowIndex].dwLen,
            &AddressSpace,
            &pAddress
         )) {
            return NULL;
        }
    }

    *pisIOMapped = AddressSpace ? TRUE : FALSE ;

    return (DWORD)pAddress;
}

// This function translates a bus address to a static physical address usable by giisr.dll
// Set *pisIOMapped to TRUE if you want to use the ioWindows array
// Set *pisIOMapped to FALSE if you want to use the memWindows array
// On return *pisIOMapped tells you if the address was IOmapped

DWORD
DoStaticTranslation(
    PDDKWINDOWINFO pdwi,
    DWORD dwWindowIndex,
    IN OUT PBOOL pisIOMapped    )
{
    ASSERT(pisIOMapped);
    PHYSICAL_ADDRESS PhysicalAddress; // bus address
    DWORD AddressSpace = (*pisIOMapped) ? 1 : 0 ;
    LPVOID pAddress;                  // return

    DEBUGCHK(pdwi != NULL);
    DEBUGCHK(dwWindowIndex < MAX_DEVICE_WINDOWS);

    if (!pdwi) {
        return NULL;
    }

    if (*pisIOMapped) {

        DEBUGCHK(dwWindowIndex < pdwi->dwNumIoWindows);
        // extract bus address
        PhysicalAddress.HighPart = 0;
        PhysicalAddress.LowPart = pdwi->ioWindows[dwWindowIndex].dwBase;

        // translate the target bus address to a statically mapped physical address
        if (!TransBusAddrToStatic(
            (INTERFACE_TYPE)pdwi->dwInterfaceType,
            pdwi->dwBusNumber,
            PhysicalAddress,
            pdwi->ioWindows[dwWindowIndex].dwLen,
            &AddressSpace,
            &pAddress
        )) {
            return NULL;
        }
    }
    else {

        DEBUGCHK(dwWindowIndex < pdwi->dwNumMemWindows);
        // extract the target bus address
        PhysicalAddress.HighPart = 0;
        PhysicalAddress.LowPart = pdwi->memWindows[dwWindowIndex].dwBase;

        // translate the target bus address to a virtual address
        if (!TransBusAddrToStatic(
            (INTERFACE_TYPE)pdwi->dwInterfaceType,
            pdwi->dwBusNumber,
            PhysicalAddress,
            pdwi->memWindows[dwWindowIndex].dwLen,
            &AddressSpace,
            &pAddress
         )) {
            return NULL;
        }
    }

    *pisIOMapped = AddressSpace ? TRUE : FALSE ;
    return (DWORD)pAddress;
}

