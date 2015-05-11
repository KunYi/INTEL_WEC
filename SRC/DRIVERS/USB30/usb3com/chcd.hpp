//
// Copyright(c) Microsoft Corporation.  All rights reserved.
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
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.

//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2011-2012 Intel Corporation All Rights Reserved. 
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

//----------------------------------------------------------------------------------
// File: chcd.hpp Chcd implements the abstract HCDI interface. It mostly
//                  just passes requests on to other objects, which
//                  do the real work.
//----------------------------------------------------------------------------------

#ifndef HCD_HPP
#define HCD_HPP

#include "cdevice.hpp"
#include "cphysmem.hpp"

class CHcd;



VOID InvalidateOpenedPipesCacheEntries(UINT uDevice, CHcd* pHcd);

DWORD XhcdProcessException (DWORD dwCode);

CHcd * CreateHCDObject(IN LPVOID pvXhcdPddObject,
                       IN CPhysMem * pCPhysMem,
                       IN LPCWSTR szDriverRegistryKey,
                       IN REGISTER portBase,
                       IN DWORD dwSysIntr);

extern "C"
{

static BOOL HcdGetFrameNumber(LPVOID lpvHcd, LPDWORD lpdwFrameNumber);
static BOOL HcdGetFrameLength(LPVOID lpvHcd, LPUSHORT lpuFrameLength);
static BOOL HcdSetFrameLength(LPVOID lpvHcd, HANDLE hEvent, USHORT wFrameLength);
static BOOL HcdStopAdjustingFrame(LPVOID lpvHcd);

static BOOL HcdOpenPipe(LPVOID lpvHcd,
                        UINT uDevice,
                        LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                        LPUINT lpiEndpointIndex);
static BOOL HcdClosePipe(LPVOID lpvHcd, UINT uDevice, UINT uEndpointIndex);
static BOOL HcdResetPipe(LPVOID lpvHcd, UINT uDevice, UINT uEndpointIndex);
static BOOL HcdIsPipeHalted(LPVOID lpvHcd,
                            UINT uDevice,
                            UINT uEndpointIndex,
                            LPBOOL lpfHalted);

static BOOL HcdIssueTransfer(LPVOID lpvHcd,
                             UINT uDevice,
                             UINT uEndpointIndex,
                             LPTRANSFER_NOTIFY_ROUTINE lpStartAddress,
                             LPVOID lpvNotifyParameter,
                             DWORD dwFlags,
                             LPCVOID lpvControlHeader,
                             DWORD dwStartingFrame,
                             DWORD dwFrames,
                             LPCDWORD aLengths,
                             DWORD dwBufferSize,
                             LPVOID lpvBuffer,
                             ULONG uPaBuffer,
                             LPCVOID lpvCancelId,
                             LPDWORD lpdwIsochErrors,
                             LPDWORD lpdwIsochLengths,
                             LPBOOL lpfComplete,
                             LPDWORD lpdwBytesTransfered,
                             LPDWORD lpdwError);

static BOOL HcdAbortTransfer(LPVOID lpvHcd,
                             UINT uDevice,
                             UINT uEndpointIndex,
                             LPTRANSFER_NOTIFY_ROUTINE lpStartAddress,
                             LPVOID lpvNotifyParameter,
                             LPCVOID lpvCancelId);
}

class CHcd : public LockObject, public CDeviceGlobal
{
public:

    CHcd();

    // These functions are called by the HCDI interface
    virtual ~CHcd();
    virtual BOOL InitializeDevice(VOID) = 0;
    virtual VOID DeInitializeDevice(VOID) = 0;   

    virtual BOOL GetFrameNumber(OUT LPDWORD lpdwFrameNumber) = 0;
    virtual BOOL GetFrameLength(OUT LPUSHORT lpuFrameLength) = 0;
    virtual BOOL SetFrameLength(IN HANDLE hEvent, IN USHORT wFrameLength) = 0;
    virtual BOOL StopAdjustingFrame(VOID) = 0;

    BOOL OpenPipe(IN UINT uAddress,
                    IN LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    OUT LPUINT lpPipeIndex,
                    OUT LPVOID* const plpCPipe = NULL);

    BOOL ClosePipe(IN UINT uAddress, IN UINT uPipeIndex);

    BOOL IssueTransfer(ISSUE_TRANSFER_PARAMS* pITP);

    BOOL AbortTransfer(IN UINT uAddress,
                        IN UINT uPipeIndex,
                        IN LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                        IN LPVOID lpvNotifyParameter,
                        IN LPCVOID lpvCancelId);

    BOOL IsPipeHalted(IN UINT uAddress,
                        IN UINT uPipeIndex,
                        OUT LPBOOL lpfHalted);

    BOOL ResetPipe(IN UINT uAddress, IN UINT uPipeIndex);

    virtual VOID PowerMgmtCallback(IN BOOL fOff) = 0;

    CRootHub* GetRootHub()
    {
        return m_pCRootHub;
    };

    CRootHub* SetRootHub(CRootHub* pRootHub);
    virtual BOOL DisableDevice(IN const UINT uAddress, IN const BOOL fReset);
    virtual BOOL SuspendResume(IN const UINT uAddress, IN const BOOL fSuspend);

    // Abstract for RootHub Function.
    virtual BOOL GetPortStatus(IN const UCHAR bPort,
                                OUT USB_HUB_AND_PORT_STATUS& rStatus) const = 0;
    virtual PVOID EnableSlot(UCHAR* pbSlotId) = 0;
    virtual DWORD GetStartTime() const = 0;
    virtual INT AddressDevice(UCHAR bSlotId,
                              UINT uPortId,
                              UCHAR bSpeed) = 0;
    virtual INT ResetDevice(UCHAR bSlotId) const = 0;
    virtual INT DoConfigureEndpoint(UCHAR bSlotId) const = 0;
    virtual INT AddEndpoint(UCHAR bSlotId, USB_ENDPOINT_DESCRIPTOR* peptDescr) const = 0;

    virtual BOOL RootHubFeature(IN const UCHAR bPort,
                                IN const UCHAR bSetOrClearFeature,
                                IN const USHORT wFeature) = 0;
    virtual BOOL ResetAndEnablePort(IN const UCHAR bPort) const = 0;
    virtual VOID DisablePort(IN const UCHAR bPort) const = 0;
    virtual BOOL WaitForPortStatusChange(HANDLE /*m_hHubChanged*/) const
    {
        return FALSE;
    };

    virtual LPCTSTR GetControllerName(VOID) const = 0;
    virtual DWORD SetCapability(DWORD dwCap) = 0; 
    virtual DWORD GetCapability() = 0;


    // Default does not support it function.
    virtual BOOL SuspendHC()
    {
        return FALSE;
    };

    CEDEVICE_POWER_STATE m_DevPwrState;

    BOOL  m_fDevicePowerDown;
    PVOID m_pOpenedPipesCache;
    DWORD m_dwOpenedPipeCacheSize;

protected:
    virtual BOOL ResumeNotification()
    {
        Lock();
        BOOL fReturn = FALSE;
        if(m_pCRootHub)
        {
            fReturn = m_pCRootHub->ResumeNotification();
            m_pCRootHub->NotifyOnSuspendedResumed(FALSE);
            m_pCRootHub->NotifyOnSuspendedResumed(TRUE);
        }

        Unlock();
        return fReturn;
    }

private:

    // pointer to CRootHub object, which represents the built-in hardware USB ports
    CRootHub* m_pCRootHub;
};

#endif /* HCD_HPP */