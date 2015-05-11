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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//  
// -----------------------------------------------------------------------------
//
// -- Intel Copyright Notice --
// 
// @par
// Copyright (c) 2002-2008 Intel Corporation All Rights Reserved.
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


#define INSTANTIATE_DEBUG
#include <wavdrv.h>


// protect critical section
CRITICAL_SECTION g_csMain;

// -----------------------------------------------------------------------------
// DllMain
// -----------------------------------------------------------------------------
BOOL __stdcall
DllMain (
    HANDLE  hinstDLL,
    DWORD   Op,
    LPVOID  lpvReserved
    )
{
    switch (Op) {

        case DLL_PROCESS_ATTACH :
            DEBUGREGISTER((HINSTANCE)hinstDLL);
			DisableThreadLibraryCalls((HMODULE) hinstDLL);
            break;

        case DLL_PROCESS_DETACH :
            break;
            
        case DLL_THREAD_DETACH :
            break;
            
        case DLL_THREAD_ATTACH :
            break;
            
        default :
            break;
    }
    return TRUE;
}


// OPENHANDLES are created on WAV_Open and get passed in to WAV_IoControl, WAV_Write, etc.
typedef struct _tag_OPENHANDLE
{
    HDRIVER hDriver;
    DWORD   dwTrust; // for tracking trust level of owning process
} OPENHANDLE, *POPENHANDLE;

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
EXTERN_C HDRIVER
WAV_Init(
    PCTSTR pszActiveKey
    )
{ 
    HDRIVER pDriver = NULL;
    HKEY hActiveKey = NULL;
    HKEY hDevKey = NULL;
    PTSTR szDevKey = NULL; 

	InitializeCriticalSection(&g_csMain);

    DEBUGMSG (ZONE_DRIVER, (TEXT("WAV_Init(%s)\r\n"), pszActiveKey));
    
	EnterCriticalSection( &g_csMain);

    DWORD dwError = RegOpenKeyEx( HKEY_LOCAL_MACHINE, pszActiveKey, 0, 0, &hActiveKey);
    
    if ( (dwError == ERROR_SUCCESS) && hActiveKey )
    {
        if ( (hDevKey = WavLoadRegKey( hActiveKey, &szDevKey)) && szDevKey)
		{
			//	CalibrateStallCounter();
			pDriver = new CDriverContext(hDevKey);
			if (pDriver != NULL) 
            {

        	    if (! pDriver->Initialize(pszActiveKey)) 
              {
                DEBUGMSG(ZONE_ERROR, (TEXT("WAV_Init FAILED")));
                delete pDriver;
                pDriver = NULL;
				}
			}
        }
    }

    if (hActiveKey)
    {
        RegCloseKey(hActiveKey);
    }
 
	LeaveCriticalSection( &g_csMain);

    return pDriver;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
EXTERN_C BOOL
WAV_Deinit(
    HDRIVER pDriver
    )
{
    DEBUGMSG (ZONE_DRIVER, (TEXT("WAV_Deinit(0x%X)\r\n"), pDriver));

	DeleteCriticalSection(&g_csMain);

    delete pDriver;
    return TRUE;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
EXTERN_C POPENHANDLE
WAV_Open(
    HDRIVER pDriver, 
    DWORD dwAccess, 
    DWORD dwShareMode
    )
{ POPENHANDLE pHandle;

    DEBUGMSG (ZONE_DRIVER, (TEXT("WAV_Open(0x%X)\r\n"), pDriver));
    pHandle = new OPENHANDLE;
    if (pHandle != NULL) {
        pHandle->hDriver = pDriver;
        pHandle->dwTrust = NULL; // assume untrusted. can't tell for sure until WAV_IoControl
    }
    return pHandle;
}


// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
EXTERN_C BOOL
WAV_Close(
    POPENHANDLE pOpenHandle
    )
{
    DEBUGMSG (ZONE_DRIVER, (TEXT("WAV_Close(0x%X)\r\n"), pOpenHandle));
    LocalFree(pOpenHandle);
    return TRUE;
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
EXTERN_C BOOL
WAV_IOControl(
    POPENHANDLE pOpenHandle, 
    DWORD  dwCode, 
    PVOID pBufIn,
    DWORD  dwLenIn, 
    PDWORD  pdwBufOut, 
    DWORD  dwLenOut,
    PDWORD pdwActualOut
    )
{ 
	DWORD dwResult;
#if 1
    // check caller trust. if context hasn't been initialized, load from CeGetCallerTrust.
    if (pOpenHandle->dwTrust != OEM_CERTIFY_TRUST) {
        if (OEM_CERTIFY_TRUST != (pOpenHandle->dwTrust = CeGetCallerTrust())) {
            DEBUGMSG(ZONE_ERROR, (TEXT("WAV_IoControl: untrusted process\r\n")));
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
    }

    HDRIVER pDriver = pOpenHandle->hDriver;

    switch (dwCode) 
	{
		case IOCTL_WAV_MESSAGE:         
		case IOCTL_MIX_MESSAGE:
		case IOCTL_POWER_CAPABILITIES:
		case IOCTL_POWER_SET:
		case IOCTL_POWER_QUERY:
		case IOCTL_REGISTER_POWER_RELATIONSHIP:
		case IOCTL_POWER_GET:
			// fall through and handle the message
			break;
		default:
			// Unrecognized IOCTL
			return FALSE;
	}

    // make sure we can send back the return code
    if (dwLenOut < sizeof(DWORD) || pdwBufOut == NULL) 
	{
        SetLastError (MMSYSERR_INVALPARAM);
        return FALSE;
    }

    switch (dwCode) 
	{
		case IOCTL_WAV_MESSAGE:
		{	
			dwResult = pDriver->WaveMessage((PMMDRV_MESSAGE_PARAMS) pBufIn);
			break;
		}
		case IOCTL_MIX_MESSAGE:
		{
			dwResult = pDriver->MixerMessage((PMMDRV_MESSAGE_PARAMS) pBufIn);
			break;
		}
		case IOCTL_POWER_CAPABILITIES:
		case IOCTL_POWER_SET:
		case IOCTL_POWER_QUERY:
		case IOCTL_REGISTER_POWER_RELATIONSHIP:
		case IOCTL_POWER_GET:
		{
			EnterCriticalSection( &g_csMain);
			dwResult = pDriver->PowerMessage( dwCode, pBufIn, dwLenIn, pdwBufOut, 
												  dwLenOut, pdwActualOut );
			LeaveCriticalSection( &g_csMain);
			return (dwResult);
			break;
		}
    	}
#endif

	*pdwBufOut = dwResult;
	if (pdwActualOut != NULL) 
	{
		*pdwActualOut = sizeof(DWORD);
	}

	return TRUE;
}

