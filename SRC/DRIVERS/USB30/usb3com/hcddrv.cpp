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


//----------------------------------------------------------------------------------
// File: hcddrv.cpp
//
// Description:
//
//----------------------------------------------------------------------------------

#include <windows.h>

#include <usb200.h>
#include <devload.h>
#include "globals.hpp"
#include "chcd.hpp"
#include "cphysmem.hpp"

#include "../xhci/chw.h"

// Debug Zones.
#ifdef DEBUG

#define DBG_XHCD        (1 << 0)
#define DBG_INIT        (1 << 1)
#define DBG_REGISTERS   (1 << 2)
#define DBG_HUB         (1 << 3)
#define DBG_ATTACH      (1 << 4)
#define DBG_DESCRIPTORS (1 << 5)
#define DBG_FUNCTION    (1 << 6)
#define DBG_PIPE        (1 << 7)
#define DBG_TRANSFER    (1 << 8)
#define DBG_COMPLIANCE  (1 << 9)
#define DBG_TD          (1 << 10)
#define DBG_CPHYSMEM    (1 << 11)
#define DBG_VERBOSE     (1 << 12)
#define DBG_WARNING     (1 << 13)
#define DBG_ERROR       (1 << 14)

DBGPARAM dpCurSettings =
{
    TEXT("USB HCD"),
    {
        TEXT("Xhcd"),
        TEXT("Init"),
        TEXT("Registers"),
        TEXT("Hub"),
        TEXT("Attach"),
        TEXT("Descriptors"),
        TEXT("Function"),
        TEXT("Pipe"),
        TEXT("Transfer"),
        TEXT("Compliance"),
        TEXT("TD"),
        TEXT("CPhysMem"),
        TEXT("Verbose"),
        TEXT("Warning"),
        TEXT("Error")
    },
    DBG_ERROR | DBG_WARNING
};
#endif // DEBUG

#define POWER_DOWN_CHECK  if(pHcd->m_fDevicePowerDown)\
                            {\
                            SetLastError(USB_NOT_ACCESSED_ALT);\
                            return FALSE;\
                            }

DWORD               g_dwContext = 0;
DWORD               g_dwIstThreadPriority = 101;
CRITICAL_SECTION    g_CSection;

HCD_FUNCS gc_HcdFuncs =
{
    sizeof(HCD_FUNCS),      //DWORD                   dwCount;

    &HcdGetFrameNumber,     //LPHCD_GET_FRAME_NUMBER      lpGetFrameNumber;
    &HcdGetFrameLength,     //LPHCD_GET_FRAME_LENGTH      lpGetFrameLength;
    &HcdSetFrameLength,     //LPHCD_SET_FRAME_LENGTH      lpSetFrameLength;
    &HcdStopAdjustingFrame, //LPHCD_STOP_ADJUSTING_FRAME  lpStopAdjustingFrame;
    &HcdOpenPipe,           //LPHCD_OPEN_PIPE             lpOpenPipe;
    &HcdClosePipe,          //LPHCD_CLOSE_PIPE            lpClosePipe;
    &HcdResetPipe,          //LPHCD_RESET_PIPE            lpResetPipe;
    &HcdIsPipeHalted,       //LPHCD_IS_PIPE_HALTED        lpIsPipeHalted;
    &HcdIssueTransfer,      //LPHCD_ISSUE_TRANSFER        lpIssueTransfer;
    &HcdAbortTransfer,      //LPHCD_ABORT_TRANSFER        lpAbortTransfer;
    &HcdDisableDevice,      //LPHCD_DISABLE_DEVICE        lpDisableDevice;
    &HcdSuspendResume       //LPHCD_SUSPEND_RESUME        lpSuspendResume;
};


// OpenedPipesCache holds pointers to opened Pipes.
// This allows HcdIssueTransfer() to call CPipeAbs::IssueTransfer()
// directly, instead of calling down through several C++ classes.
//
// This optimization is purely to aVOID the C++ function call overhead
// introduced by too many classes.  The optimization is useful for
// small transfers.

typedef struct
{
    CPipeAbs*       pCPipeAbs;
    DWORD           dwPipeId;
    volatile LONG   lInUse;
    DWORD           dwReservedUnusedPad;
} OPENED_PIPE_CACHE_ENTRY;

//----------------------------------------------------------------------------------
// Function: InvalidateOpenedPipesCacheEntries
//
// Description: Clear cached shortcuts to CPipe classes
//
// Parameters: uDevice - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD. 
//
//             pHcd - pointer to the Host Controller Driver object which we
//             pass to USBD
//
// Returns: None
//----------------------------------------------------------------------------------
VOID InvalidateOpenedPipesCacheEntries(UINT uDevice, CHcd* pHcd)
{
    OPENED_PIPE_CACHE_ENTRY* pCache =
       (OPENED_PIPE_CACHE_ENTRY*)pHcd->m_pOpenedPipesCache;

    if(pCache)
    {
        UINT uKey = uDevice<<16;

        pHcd->Lock();

        for(UINT i = 0; i < pHcd->m_dwOpenedPipeCacheSize; i++)
        {
            if((0xFFFF0000 & pCache[i].dwPipeId) == uKey)
            {
                DEBUGMSG(ZONE_ATTACH,
                   (TEXT("XHCD!InvalidateOpenedPipesCacheEntries() \
                          #%u: deviceId=%u, pipeId=%u\r\n"),
                    i,
                    uDevice,
                   (pCache[i].dwPipeId >> 8) & 0xff));

                //
                // Do not proceed whileCPipe is in use -
                // CPipe class will be destroyed right after this fxn exits
                //
                while(pCache[i].lInUse)
                {
                    pHcd->Unlock();
                    Sleep(1);
                    pHcd->Lock();
                }

                pCache[i].pCPipeAbs = NULL;
                pCache[i].dwPipeId = 0;
            }
        }

        pHcd->Unlock();
    }
}

//----------------------------------------------------------------------------------
// Function: XhcdProcessException
//
// Description: checks for a particular exception before handling it
//
// Parameters: dwCode - exception code
//
// Returns: EXCEPTION_EXECUTE_HANDLER or EXCEPTION_CONTINUE_SEARCH
//----------------------------------------------------------------------------------
DWORD XhcdProcessException (DWORD dwCode)
{
    if (dwCode == STATUS_ACCESS_VIOLATION) {
            return EXCEPTION_EXECUTE_HANDLER;
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

//----------------------------------------------------------------------------------
// Function: DllMain
//
// Description: DLL Entry point.
//
// Parameters:
//      hInstDLL
//          [in] Handle to the DLL. The value is the base address of the DLL.
//
//      dwReason
//          [in] Specifies a flag indicating why the DLL entry-point function
//          is being called.
//
//      lpvReserved
//          [in] Specifies further aspects of DLL initialization and cleanup.
//          If dwReason is DLL_PROCESS_ATTACH, lpvReserved is NULL for
//          dynamic loads and nonnull for static loads. If dwReason is
//          DLL_PROCESS_DETACH, lpvReserved is NULL if DllMain is called
//          by using FreeLibrary and nonnull if DllMain is called during
//          process termination.
//
// Returns:
//      When the system calls the DllMain function with the
//      DLL_PROCESS_ATTACH value, the function returns TRUE if it
//      succeeds or FALSE if initialization fails.
//
//      If the return value is FALSE when DllMain is called because the
//      process uses the LoadLibrary function, LoadLibrary returns NULL.
//
//      If the return value is FALSE when DllMain is called during
//      process initialization, the process terminates with an error.
//
//      When the system calls the DllMain function with a value other
//      than DLL_PROCESS_ATTACH, the return value is ignored.
//
//----------------------------------------------------------------------------------
BOOL WINAPI DllMain(HANDLE hInstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        DEBUGREGISTER((HINSTANCE)hInstDLL);
        DEBUGMSG(ZONE_INIT,(TEXT("HCD driver DLL attach\r\n")));
        DisableThreadLibraryCalls((HMODULE) hInstDLL);
        g_dwContext = 0;
        InitializeCriticalSection(&g_CSection);
    }
    else if(dwReason == DLL_PROCESS_DETACH)
    {
        DeleteCriticalSection(&g_CSection);
    }

    return HcdPdd_DllMain(hInstDLL, dwReason, lpvReserved);
}

//----------------------------------------------------------------------------------
// Function: HcdGetFrameNumber
//
// Description: Get the current microframe number
//
// Parameters:  lpvHcd - pointer to the Host Controller Driver object which we
//                       pass to USBD
//
//              lpdwFrameNumber - microframe number
//
// Returns: TRUE if frame number is correct, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdGetFrameNumber(LPVOID lpvHcd, LPDWORD lpdwFrameNumber)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->GetFrameNumber(lpdwFrameNumber);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdGetFrameLength
//
// Description: Get the microframe length
//
// Parameters:  lpvHcd - pointer to the Host Controller Driver object which we
//                       pass to USBD
//
//              lpuFrameLength - microframe length
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdGetFrameLength(LPVOID lpvHcd, LPUSHORT lpuFrameLength)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->GetFrameLength(lpuFrameLength);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdSetFrameLength
//
// Description: Set the microframe length
//
// Parameters:  lpvHcd - pointer to the Host Controller Driver object which we
//                       pass to USBD
//
//              hEvent - event to set when frame has reached required
//                       length
//
//              wFrameLength - microframe length
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdSetFrameLength(LPVOID lpvHcd, HANDLE hEvent, USHORT wFrameLength)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->SetFrameLength(hEvent, wFrameLength);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdStopAdjustingFrame
//
// Description: Stop modifying the host controller frame length
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdStopAdjustingFrame(LPVOID lpvHcd)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->StopAdjustingFrame();
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdOpenPipe
//
// Description: Open pipe
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD. 
//
//             lpEndpointDescriptor - pointer to endpoint descriptor for
//                                    this pipe (assumed non-NULL)
//
//             lpiEndpointIndex - endpoint index
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdOpenPipe(LPVOID lpvHcd,
                        UINT uDevice,
                        LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                        LPUINT lpiEndpointIndex)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;

    OPENED_PIPE_CACHE_ENTRY* pCache =
       (OPENED_PIPE_CACHE_ENTRY*)pHcd->m_pOpenedPipesCache;

    CPipeAbs* pCPipe =(CPipeAbs*)0xFFFFFFFF;
    BOOL fRetVal = pHcd->OpenPipe(uDevice,
                                    lpEndpointDescriptor,
                                    lpiEndpointIndex,
                                   (LPVOID*)(&pCPipe));

    if(fRetVal && pCPipe && pCache &&
       (lpEndpointDescriptor->bmAttributes&USB_ENDPOINT_TYPE_MASK) !=
        USB_ENDPOINT_TYPE_CONTROL)
    {
        UINT uKey =(uDevice << 16)|((*lpiEndpointIndex & 0xFF) << 8) | 0x80;

        // We don't want to cache Endpoint 0, which is dedicated to control
        // transfers. We only cache Bulk, Interrupt, and Isochronous pipes.
        ASSERT(*lpiEndpointIndex != 0);

        pHcd->Lock();

        // Is the pipe already in our cache?
        for(UINT i = 0; i < pHcd->m_dwOpenedPipeCacheSize; i++)
        {
            if(pCache[i].dwPipeId ==(uKey|i) && pCache[i].pCPipeAbs != NULL)
            {
                    goto alreadyOpened;
            }
        }

        // Add the pipe to our cache.
        BOOL fSuccess = FALSE;
        for(UINT i = 0; i < pHcd->m_dwOpenedPipeCacheSize; i++)
        {
            if(pCache[i].pCPipeAbs == NULL)
            {
                ASSERT(pCache[i].lInUse == 0);
                pCache[i].lInUse = 0;
                pCache[i].pCPipeAbs = pCPipe;
                pCache[i].dwPipeId = uKey | i;
                fSuccess = TRUE;
                break;
            }
        }

        if(!fSuccess)
        {
            DEBUGMSG(ZONE_WARNING,
               (TEXT("WARNING: HcdOpenPipe(): OpenedPipesCache full; \
                      increase 'HcdPipeCache' value in Registry.\r\n")));
        }

alreadyOpened:
        pHcd->Unlock();
    }

    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdClosePipe
//
// Description: Close pipe
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD. 
//
//             lpiEndpointIndex - endpoint index
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdClosePipe(LPVOID lpvHcd, UINT uDevice, UINT uEndpointIndex)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;

    OPENED_PIPE_CACHE_ENTRY* pCache =
       (OPENED_PIPE_CACHE_ENTRY*)pHcd->m_pOpenedPipesCache;

    //
    // Cache lookups & mods are prevented whileCPipe class may be destroyed
    //
    if(uEndpointIndex != 0 && pCache != NULL)
    {
        pHcd->Lock();
    }

    BOOL fRetVal = pHcd->ClosePipe(uDevice, uEndpointIndex);

    if(uEndpointIndex != 0 && pCache != NULL) {

        UINT uKey =(uDevice << 16) |((uEndpointIndex & 0xFF) << 8) | 0x80;

        for(UINT i = 0; i < pHcd->m_dwOpenedPipeCacheSize; i++)
        {
            if(pCache[i].dwPipeId ==(uKey | i))
            {
                //
                // CPipe class is already destroyed, just remove Cache entry
                // If use count is non-zero, then CPipe is destroyed while
                // doing transfer!
                //
                ASSERT(pCache[i].lInUse == 0);
                pCache[i].pCPipeAbs = NULL;
                pCache[i].dwPipeId = 0;
                pCache[i].lInUse = 0;
                break;
            }
        }

        pHcd->Unlock();
    }

    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdResetPipe
//
// Description: Reset pipe
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD. 
//
//             lpiEndpointIndex - endpoint index
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdResetPipe(LPVOID lpvHcd, UINT uDevice, UINT uEndpointIndex)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->ResetPipe(uDevice, uEndpointIndex);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdIsPipeHalted
//
// Description: Return whether or not this pipe is halted (stalled)
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of this device. This will also be used as the
//                       device's index number when communicating with USBD. 
//
//             lpiEndpointIndex - endpoint index
//
//             lpfHalted - pipe state
//
// Returns: TRUE if operation passed successfully, else FALSE 
//----------------------------------------------------------------------------------
static BOOL HcdIsPipeHalted(LPVOID lpvHcd,
                            UINT uDevice,
                            UINT uEndpointIndex,
                            LPBOOL lpfHalted)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->IsPipeHalted(uDevice, uEndpointIndex, lpfHalted);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdIssueTransfer
//
// Description: Issue a Transfer on this pipe
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address to send transfer to
//
//             lpiEndpointIndex - endpoint index
//
//             lpStartAddress - routine to callback after transfer completes
//
//             lpParameter - parameter for lpStartAddress callback
//
//             OTHER PARAMS - see comment in CUhcd::IssueTransfer
//
// Returns: requestOK if transfer issued ok, else requestFailed
//----------------------------------------------------------------------------------
static BOOL HcdIssueTransfer(LPVOID lpvHcd,
                                UINT uDevice,
                                UINT uEndpointIndex,
                                LPTHREAD_START_ROUTINE lpStartAddress,
                                LPVOID lpParameter,
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
                                LPDWORD lpdwError)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;

    OPENED_PIPE_CACHE_ENTRY* pCache = NULL;

    if(!pHcd->m_fDevicePowerDown)
    {
       (OPENED_PIPE_CACHE_ENTRY*)pHcd->m_pOpenedPipesCache;
    }

    BOOL fRetVal;
    volatile LONG* plInUse = NULL;
    CPipeAbs* pCCPipe = NULL;

    // We don't cache pipes to Endpoint 0, which is always a
    // Control Transfer endpoint.
    // Control Transfers require different handling than Bulk,
    // Interrupt, and Isochronous.
    if(uEndpointIndex != 0 && pCache)
    {
        UINT uKey =(uDevice << 16) |((uEndpointIndex & 0xFF) << 8) | 0x80;

        //
        // prevent Cache modifications during lookup then release
        // global CS as fast as possible
        //
        pHcd->Lock();

        for(UINT i = 0; i < pHcd->m_dwOpenedPipeCacheSize; i++)
        {
            if(pCache[i].dwPipeId ==(uKey | i))
            {
                //
                // entry found, mark it in use to prevent untimely removal
                //
                plInUse = &(pCache[i].lInUse);
                if (plInUse != NULL) 
                {
                    InterlockedIncrement(plInUse);
                }
                pCCPipe = pCache[i].pCPipeAbs;
                break;
            }
        }

        pHcd->Unlock();

        //
        // if cached CPipe found, then invoke it directly
        //
        if(pCCPipe != NULL)
        {
            HCD_REQUEST_STATUS status = pCCPipe->IssueTransfer(uDevice,
                                                                lpStartAddress,
                                                                lpParameter,
                                                                dwFlags,
                                                                lpvControlHeader,
                                                                dwStartingFrame,
                                                                dwFrames,
                                                                aLengths,
                                                                dwBufferSize,
                                                                lpvBuffer,
                                                                uPaBuffer,
                                                                lpvCancelId,
                                                                lpdwIsochErrors,
                                                                lpdwIsochLengths,
                                                                lpfComplete,
                                                                lpdwBytesTransfered,
                                                                lpdwError);
            //
            // let it go - CPipe class is not used any more
            //
            InterlockedDecrement(plInUse);

            if(status == REQUEST_OK)
            {
                fRetVal = TRUE;
            }
            else
            {
                fRetVal = FALSE;
                if(status == REQUEST_IGNORED)
                {
                    SetLastError(ERROR_DEV_NOT_EXIST);
                }
            }

            goto doneHcdIssueTransfer;
        }
    }

    // Pipe was not in the cache; take the non-cached route through the C++ methods
    //(more function-call overhead)
    // This is expected forControl Transfers(transfers to Endpoint 0).

    // bounce back all transfers when device is powered down
    POWER_DOWN_CHECK;

    // We package the parameters into a struct and pass the pointer down.
    // This prevents pushing/popping of all 16 individual arguments as we call down
    // into the driver.  This is an optimization to reduce function call overhead
    // for very small transfers.
    ISSUE_TRANSFER_PARAMS* pITP = NULL;

#if defined x86 || defined _X86_
    // NOTE: we are taking A POINTER TO THE ARGUMENTS ON THE STACK, treating it as a
    // struct ISSUE_TRANSFER_PARAMS*, and passing that into pHcd->IssueTransfer().
    // Only x86 platforms will always put all parms on the stack
    pITP =(ISSUE_TRANSFER_PARAMS*)(&uDevice);
#else
    #pragma warning(disable: 4533)
    ISSUE_TRANSFER_PARAMS itp =
    {uDevice,
    uEndpointIndex,
    lpStartAddress,
    lpParameter,
    dwFlags,
    lpvControlHeader,
    dwStartingFrame,
    dwFrames,
    aLengths,
    dwBufferSize,
    lpvBuffer,
    uPaBuffer,
    lpvCancelId,
    lpdwIsochErrors,
    lpdwIsochLengths,
    lpfComplete,
    lpdwBytesTransfered,
    lpdwError
    };

    pITP = &itp;
#endif
    ASSERT(lpdwError == pITP->lpdwError);

    fRetVal = pHcd->IssueTransfer(pITP);

doneHcdIssueTransfer:

    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdAbortTransfer
//
// Description: Abort any transfer on this pipe if its cancel ID matches
//          that which is passed in.
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address to send transfer to
//
//             lpiEndpointIndex - endpoint index
//
//             lpStartAddress - routine to callback after aborting transfer
//
//             lpParameter - parameter for lpStartAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: TRUE if operation is succeeded, else FALSE
//----------------------------------------------------------------------------------
static BOOL HcdAbortTransfer(LPVOID lpvHcd,
                             UINT uDevice,
                             UINT uEndpointIndex,
                             LPTHREAD_START_ROUTINE lpStartAddress,
                             LPVOID lpParameter,
                             LPCVOID lpvCancelId)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->AbortTransfer(uDevice,
                                        uEndpointIndex,
                                        lpStartAddress,
                                        lpParameter,
                                        lpvCancelId);

    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdDisableDevice
//
// Description: Disable attached device
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of device to be disabled
//
//             fReset - reattach device if true
//
// Returns: TRUE if operation is succeeded, else FALSE
//----------------------------------------------------------------------------------
static BOOL HcdDisableDevice(LPVOID lpvHcd, UINT uDevice, BOOL fReset)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    POWER_DOWN_CHECK;
    BOOL fRetVal = pHcd->DisableDevice(uDevice,fReset);
    return fRetVal;
}

//----------------------------------------------------------------------------------
// Function: HcdSuspendResume
//
// Description: Suspend or resume device
//
// Parameters: lpvHcd - pointer to the Host Controller Driver object 
//
//             uDevice - USB address of device to be disabled
//
//             fSuspend - suspend device if true
//
// Returns: TRUE if operation is succeeded, else FALSE
//----------------------------------------------------------------------------------
static BOOL HcdSuspendResume(LPVOID lpvHcd, UINT uDevice, BOOL fSuspend)
{
    CHcd * const pHcd =(CHcd *)lpvHcd;
    DEBUGMSG(ZONE_INIT,
       (_T("+++ HcdSuspendResume(%x,%x,%u)\r\n"),
        lpvHcd,
        uDevice,
        fSuspend));
    BOOL fRetVal = pHcd->SuspendResume(uDevice, fSuspend);
    DEBUGMSG(ZONE_INIT,
       (_T("--- HcdSuspendResume(%x,%x,%u) = %u\r\n"),
        lpvHcd,
        uDevice,
        fSuspend,
        fRetVal));

    return fRetVal;
}


//----------------------------------------------------------------------------------
// Function: GetInterruptThreadPriority
//
// Description: Function to read the interrupt thread priority from the registry.
//              If it is not in the registry then a default value is returned.
//
// Parameters: lpszActiveKey - registry key
//
// Returns: Priority value
//----------------------------------------------------------------------------------
static DWORD GetInterruptThreadPriority(LPCWSTR lpszActiveKey)
{
    HKEY hDevKey;
    DWORD dwValType;
    DWORD dwValLen;
    DWORD dwPrio;

    dwPrio = DEFAULT_XHCD_IST_PRIORITY;

    hDevKey = OpenDeviceKey(lpszActiveKey);

    if(hDevKey)
    {
        dwValLen = sizeof(DWORD);

        RegQueryValueEx(hDevKey,
                        TEXT("Priority256"),
                        NULL,
                        &dwValType,
                       (PUCHAR)&dwPrio,
                        &dwValLen);

        RegCloseKey(hDevKey);
    }

    return dwPrio;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_CreateMemoryObject
//
// Description: Create aligned memory object
//
// Parameters: dwSize -  memory size
//
//              dwHighPrioritySize - high priority memory size
//
//              pVirtAddr - virtual address
//
//              pPhysAddr - physical address
//
// Returns: Object address
//----------------------------------------------------------------------------------
extern "C" LPVOID HcdMdd_CreateMemoryObject(DWORD dwSize,
                                            DWORD dwHighPrioritySize,
                                            PUCHAR pVirtAddr,
                                            PUCHAR pPhysAddr)
{
    //
    // We need at least a USBPAGE forSpecial allocation and a PAGE for normal
    // allocation.
    //
    ASSERT((dwHighPrioritySize +(2 * USBPAGESIZE)) < dwSize);

    CPhysMem * pobMem = new CPhysMem(dwSize,
                                        dwHighPrioritySize,
                                        pVirtAddr,
                                        pPhysAddr);
    if(pobMem)
    {
        if(!pobMem->InittedOK())
        {
            delete pobMem;
            pobMem = 0;
        }
    }

    return pobMem;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_CreateHcdObject
//
// Description: Create host controller driver object
//
// Parameters: lpvXhcdPddObject - Pointer to PDD specific data for this instance.
//
//             lpvMemoryObject - Memory object created by <f HcdMdd_CreateMemoryObject>.
//
//             szRegKey        - Registry key where XHCI configuration settings are stored.
//
//             pIoPortBase     - Pointer to XHCI controller registers.
//
//             dwSysIntr       - Logical value for XHCI interrupt (SYSINTR_xx).
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" LPVOID HcdMdd_CreateHcdObject(LPVOID lpvXhcdPddObject,
                                         LPVOID lpvMemoryObject,
                                         LPCWSTR szRegKey,
                                         PUCHAR pIoPortBase,
                                         DWORD dwSysIntr)
{
    CHcd * pobXhcd;

    pobXhcd = CreateHCDObject(lpvXhcdPddObject,
                               (CPhysMem *)lpvMemoryObject,
                                szRegKey,
                                pIoPortBase,
                                dwSysIntr);

    if(pobXhcd != NULL)
    {
        if(!pobXhcd->InitializeDevice())
        {
            delete pobXhcd;
            pobXhcd = NULL;
        }
        else
        {
            //
            // Allocate opened pipe cache - if it fails, will not be critical
            // we expect <m_dwOpenedPipeCacheSize> to be set by CHW initialization
            //
            if(pobXhcd->m_dwOpenedPipeCacheSize)
            {
                // size  constrained to >= MIN_PIPE_CACHE_SIZE && <= MAX_PIPE_CACHE_SIZE
                // when registry is read
                PREFAST_ASSUME(pobXhcd->m_dwOpenedPipeCacheSize >= 4 &&
                    pobXhcd->m_dwOpenedPipeCacheSize <= 120);
                OPENED_PIPE_CACHE_ENTRY* pCache =
                    new OPENED_PIPE_CACHE_ENTRY[pobXhcd->m_dwOpenedPipeCacheSize];

                if(pCache)
                {
                    pobXhcd->m_pOpenedPipesCache = pCache;

                    memset(pCache,
                            0,
                            sizeof(OPENED_PIPE_CACHE_ENTRY) * pobXhcd->m_dwOpenedPipeCacheSize);

                    DEBUGMSG(ZONE_INIT,
                       (TEXT("HcdMdd_CreateHcdObject() pipe cache of size \
                              %u(%u bytes) allocated\r\n"),
                        pobXhcd->m_dwOpenedPipeCacheSize,
                        sizeof(OPENED_PIPE_CACHE_ENTRY) * pobXhcd->m_dwOpenedPipeCacheSize));
                }
                else
                {
                    DEBUGMSG(ZONE_INIT,
                       (TEXT("WARNING! HcdMdd_CreateHcdObject() failed to allocate \
                              pipe cache of size %u\r\n"),
                        pobXhcd->m_dwOpenedPipeCacheSize));
                }
            }
        }
    }

    return pobXhcd;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_DestroyHcdObject
//
// Description: This function should be called when an XHCI controller is removed, or if an error
//        occurs during initialization, to allow the MDD to clean up internal data structures.
//
// Parameters: lpvXhcdObject - Pointer to OHCI driver object.
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HcdMdd_DestroyHcdObject(LPVOID lpvXhcdObject)
{
    CHcd * pobXhcd =(CHcd *)lpvXhcdObject;

    if(pobXhcd->m_pOpenedPipesCache)
    {
        OPENED_PIPE_CACHE_ENTRY* pCache =
           (OPENED_PIPE_CACHE_ENTRY*)pobXhcd->m_pOpenedPipesCache;
        delete [] pCache;
    }

    delete pobXhcd;

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_DestroyMemoryObject
//
// Description: Clean up an XHCI memory object
//
// Parameters: lpvMemoryObject - Pointer to XHCD memory object.
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HcdMdd_DestroyMemoryObject(LPVOID lpvMemoryObject)
{
    CPhysMem * pobMem =(CPhysMem *)lpvMemoryObject;
    delete pobMem;
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_PowerUp
//
// Description: XHCD MDD power up handler.
//
// Parameters: lpvXhcdObject - Pointer to OHCI driver object.
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HcdMdd_PowerUp(LPVOID lpvXhcdObject)
{
    CHcd * pobXhcd =(CHcd *)lpvXhcdObject;
    if (pobXhcd != NULL) 
    {
        pobXhcd->PowerMgmtCallback(FALSE);
        pobXhcd->m_fDevicePowerDown = FALSE;
    } 
    else 
    {
        return FALSE;
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_PowerDown
//
// Description: XHCD MDD power down handler.
//
// Parameters: lpvXhcdObject - Pointer to OHCI driver object.
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HcdMdd_PowerDown(LPVOID lpvXhcdObject)
{
    CHcd * pobXhcd =(CHcd *)lpvXhcdObject;

    if (pobXhcd != NULL) 
    {
        pobXhcd->m_fDevicePowerDown = TRUE;
        pobXhcd->PowerMgmtCallback(TRUE);
        OPENED_PIPE_CACHE_ENTRY* pCache =
            (OPENED_PIPE_CACHE_ENTRY*)pobXhcd->m_pOpenedPipesCache;

        if(pCache && pobXhcd->m_dwOpenedPipeCacheSize)
        {
            memset(pCache,
                0,
                sizeof(OPENED_PIPE_CACHE_ENTRY)*pobXhcd->m_dwOpenedPipeCacheSize);
        }
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdMdd_SetCapability
//
// Description: HCD MDD Set Capability.
//
// Parameters: lpvXhcdObject - Pointer to HCI driver object.
//
//             dwCapability - Bitmap for Capabliity
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C"  DWORD   HcdMdd_SetCapability(LPVOID lpvXhcdObject, DWORD dwCapability)
{
    CHcd * pobXhcd =(CHcd *)lpvXhcdObject;
    DWORD dwRet = pobXhcd->SetCapability(dwCapability);
    return dwRet;
}

//----------------------------------------------------------------------------------
// Function: HCD_Init
//
// Description: This function initializes a host controller. 
//              It is called by Device Manager.
//              This function is required by drivers loaded by 
//              ActivateDeviceEx or ActivateDevice.
//
// Parameters: dwContext - registry key
//
// Returns: status result
//----------------------------------------------------------------------------------
extern "C" DWORD HCD_Init(DWORD dwContext)
{
    HKEY hActiveKey;
    WCHAR szRegKeyPath[DEVKEY_LEN];
    DWORD dwStatus;
    DWORD dwValType;
    DWORD dwValLen;

    if (dwContext == 0) 
    {
        return NULL;
    }

    // Open driver's ACTIVE key
    dwStatus = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
               (LPCWSTR)dwContext,
                0,
                0,
                &hActiveKey);

    if(dwStatus != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,
           (TEXT("XHCD!HCD_Init RegOpenKeyEx(%s) returned %d.\r\n"),
           (LPCWSTR)dwContext,
            dwStatus));

        return NULL;
    }

    // Get Key value, which points to driver's key
    dwValLen = sizeof(szRegKeyPath);

    dwStatus = RegQueryValueEx(
                hActiveKey,
                DEVLOAD_DEVKEY_VALNAME,
                NULL,
                &dwValType,
               (PUCHAR)szRegKeyPath,
                &dwValLen);

    if(dwStatus != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,
           (TEXT("XHCD!HCD_Init RegQueryValueEx(%s\\%s) returned %d\r\n"),
           (LPCWSTR)dwContext,
            DEVLOAD_DEVKEY_VALNAME,
            dwStatus));

        RegCloseKey(hActiveKey);

        return NULL;
    }

    RegCloseKey(hActiveKey);
    g_dwIstThreadPriority = GetInterruptThreadPriority((LPTSTR)dwContext);
    szRegKeyPath[DEVKEY_LEN-1] = 0 ;

    EnterCriticalSection(&g_CSection);
    g_dwContext = dwContext;
    DWORD dwReturn = HcdPdd_Init((DWORD)szRegKeyPath);
    g_dwContext = 0;
    LeaveCriticalSection(&g_CSection);

    return dwReturn;
}

//----------------------------------------------------------------------------------
// Function: HCD_Deinit
//
// Description: deinitialize host controller driver
//
// Parameters: hDeviceContext - device handler
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HCD_Deinit(DWORD hDeviceContext)
{
    DEBUGMSG(ZONE_INIT,(TEXT("XHCD!HCD_Deinit\r\n")));
    EnterCriticalSection(&g_CSection);
    DWORD dwReturn = HcdPdd_Deinit(hDeviceContext);
    LeaveCriticalSection(&g_CSection);
    return dwReturn;
}

//----------------------------------------------------------------------------------
// Function: HCD_PowerUp
//
// Description: Power up handler
//
// Parameters: hDeviceContext - device handler
//
// Returns: none
//----------------------------------------------------------------------------------
extern "C" VOID HCD_PowerUp(DWORD hDeviceContext)
{
    HcdPdd_PowerUp(hDeviceContext);
}

//----------------------------------------------------------------------------------
// Function: HCD_PowerDown
//
// Description: Power down handler
//
// Parameters: hDeviceContext - device handler
//
// Returns: none
//----------------------------------------------------------------------------------
extern "C" VOID HCD_PowerDown(DWORD hDeviceContext)
{
    HcdPdd_PowerDown(hDeviceContext);
}

//----------------------------------------------------------------------------------
// Function: HCD_Open
//
// Description: HCD open routine. This is the standard stream interface 
//              driver open routine.  It is called when CreateFile function 
//              is called.
//
// Parameters:  dwDeviceContext - Pointer to device context returned from <f HcdPdd_Init>
//
//              dwAccessCode - Unused
//
//              dwShareMode - Unused
//
// Returns: pointer to MDD object
//----------------------------------------------------------------------------------
extern "C" DWORD HCD_Open(DWORD hDeviceContext,
                          DWORD dwAccessCode,
                          DWORD dwShareMode)
{
    return HcdPdd_Open(hDeviceContext, dwAccessCode, dwShareMode);
}

//----------------------------------------------------------------------------------
// Function: HCD_Close
//
// Description: HCD close routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HCD_Close(DWORD hOpenContext)
{
    return HcdPdd_Close(hOpenContext);
}

//----------------------------------------------------------------------------------
// Function: HCD_Read
//
// Description: HCD read routine. This is the standard stream interface driver read routine.  
//              It is currently unused.For future compatibility, drivers should return -1.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             pBuffer - addres of destination buffer 
//
//             dwCount - number of read bytes
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" DWORD HCD_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD dwCount)
{
    return HcdPdd_Read(hOpenContext, pBuffer, dwCount);
}

//----------------------------------------------------------------------------------
// Function: HCD_Write
//
// Description: HCD write routine. This is the standard stream interface 
//      driver write routine.  It is currently unused. For future compatibility, drivers should return -1.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             pSourceBytes - address of source buffer 
//
//             dwNumberOfBytes - number of bytes to write
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" DWORD HCD_Write(DWORD hOpenContext,
                            LPCVOID pSourceBytes,
                            DWORD dwNumberOfBytes)
{
    return HcdPdd_Write(hOpenContext, pSourceBytes, dwNumberOfBytes);
}

//----------------------------------------------------------------------------------
// Function: HCD_Seek
//
// Description: HCD seek routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             lAmount - position to seek to (relative to type)
//
//             dwType - type of seek
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" DWORD HCD_Seek(DWORD hOpenContext, LONG lAmount, DWORD dwType)
{
    return HcdPdd_Seek(hOpenContext, lAmount, dwType);
}

//----------------------------------------------------------------------------------
// Function: HCD_IOControl
//
// Description: HCD ioctl routine
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             dwCode - io control code to be performed
//
//             pBufIn - input data to the device
//
//             dwLenIn - number of bytes being passed in
//
//             pBufOut - output data from the device
//
//             dwLenOut - maximum number of bytes to receive from device
//
//             dwActualOut - actual number of bytes received from device
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern "C" BOOL HCD_IOControl(DWORD hOpenContext,
                              DWORD dwCode,
                              BYTE const*const pBufIn,
                              DWORD dwLenIn,
                              BYTE const*const pBufOut,
                              DWORD dwLenOut,
                              DWORD const*const pdwActualOut)
{
    BOOL fRetVal = FALSE;
    SetLastError(ERROR_INVALID_PARAMETER);

    __try
    {
        switch(dwCode){
        case IOCTL_POWER_CAPABILITIES:
            if(pBufOut != NULL && dwLenOut >= sizeof(POWER_CAPABILITIES))
            {
                PPOWER_CAPABILITIES pPwrCap =(PPOWER_CAPABILITIES)pBufOut;
                // full power & OFF are always advertised
                pPwrCap->DeviceDx   = DX_MASK(D0) | DX_MASK(D4);
                pPwrCap->WakeFromDx = DX_MASK(D0) | DX_MASK(D4);
                pPwrCap->InrushDx   = DX_MASK(D0) | DX_MASK(D4);
                pPwrCap->Flags      = 0;
                // HC drains little power; connected devices must specify
                // their own consumption
                pPwrCap->Power[0]   = 200;
                pPwrCap->Power[1]   = PwrDeviceUnspecified;
                pPwrCap->Power[2]   = PwrDeviceUnspecified;
                pPwrCap->Power[3]   = PwrDeviceUnspecified;
                pPwrCap->Power[4]   = 0;
                pPwrCap->Latency[0] = 0;
                pPwrCap->Latency[1] = PwrDeviceUnspecified;
                pPwrCap->Latency[2] = PwrDeviceUnspecified;
                pPwrCap->Latency[3] = PwrDeviceUnspecified;
                // half a sec to complete power up
                pPwrCap->Latency[4] = 500;

                // sleep is only supported if bit 0 of "HcdCapability" is set
                if((HCD_SUSPEND_RESUME&((CHcd*)hOpenContext)->GetCapability()) != 0)
                {
                    pPwrCap->DeviceDx   = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
                    pPwrCap->WakeFromDx = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
                    pPwrCap->InrushDx   = DX_MASK(D0) | DX_MASK(D3) | DX_MASK(D4);
                    // trickle of power only
                    pPwrCap->Power[3]   = 10;
                    // half a sec to complete power up
                    pPwrCap->Latency[3] = 500;
                }

                if(pdwActualOut != NULL)
                {
                    *(DWORD*) pdwActualOut = sizeof(POWER_CAPABILITIES);
                }

                fRetVal = TRUE;
                SetLastError(ERROR_SUCCESS);
                DEBUGMSG(ZONE_INIT,
                   (TEXT("XHCD!HCD_IOControl() %x: \
                          IOCTL_POWER_CAPABILITIES = %x success\r\n"),
                    hOpenContext,
                    pPwrCap->DeviceDx));
            }

            break;

        case IOCTL_POWER_GET:
            if(pBufOut != NULL && dwLenOut >= sizeof(CEDEVICE_POWER_STATE))
            {
                CHcd* pHcd =(CHcd*)hOpenContext;
                *((CEDEVICE_POWER_STATE*)pBufOut) = pHcd->m_DevPwrState;
                DEBUGMSG(ZONE_INIT,
                   (TEXT("XHCD!HCD_IOControl() %x: IOCTL_POWER_GET is %u\r\n"),
                    pHcd,
                    pHcd->m_DevPwrState));
                fRetVal = TRUE;
                SetLastError(ERROR_SUCCESS);
            }

            break;

        case IOCTL_POWER_SET:
            if(pBufOut != NULL && dwLenOut >= sizeof(CEDEVICE_POWER_STATE))
            {
                CHcd* pHcd =(CHcd*)hOpenContext;
                LONG lPwrState = *((LONG*)pBufOut);
                CEDEVICE_POWER_STATE dx = *((CEDEVICE_POWER_STATE*)pBufOut);

                // sleep is only supported ifbit 0 of "HcdCapability" is set
                if((HCD_SUSPEND_RESUME&((CHcd*)hOpenContext)->GetCapability()) != 0)
                {
                    //supported states D0, D3, D4(check IOCTL_POWER_CAPABILITIES)
                    switch(dx)
                    {
                    case D0:
                    case D1:
                    case D2:
                        dx = D0;
                        break;
                    case D3:
                        dx = D3;
                        break;
                    case D4:
                        dx = D4;
                        break;
                    default:
                        dx = D0;
                        break;
                    }
                }
                else
                {
                    //supported states D0, D4(check IOCTL_POWER_CAPABILITIES)
                    switch(lPwrState)
                    {
                    case D0:
                    case D1:
                    case D2:
                        dx = D0;
                        break;
                    case D3:
                    case D4:
                        dx = D4;
                        break;
                    default:
                        dx = D0;
                        break;
                    }
                }
                lPwrState =(LONG)dx;
                pHcd->Lock();

                if(*((CEDEVICE_POWER_STATE*)pBufOut) > D2)
                {
                    TCHAR szGetPowerState[MAX_SIZE_PWR_STATE_NAME] = {0};
                    DWORD dwGetFlag = 0;
                    DWORD dwStatus = GetSystemPowerState(szGetPowerState,
                                                        _countof(szGetPowerState),
                                                        &dwGetFlag);
                    if(dwStatus == ERROR_SUCCESS)
                    {
                        DEBUGMSG(ZONE_INIT,
                           (TEXT("XHCD!HCD_IOControl() Power State: %s flag: %x"),
                            szGetPowerState,
                            dwGetFlag));

                        if((dwGetFlag &
                           (POWER_STATE_SUSPEND |
                            POWER_STATE_OFF |
                            POWER_STATE_CRITICAL)) == 0)
                        {
                            //Setting the flag to TRUE ifit is *not* a suspend state
                            pHcd->m_fDevicePowerDown = TRUE;
                        }
                    }
                    else
                    {
                        pHcd->Unlock();
                        SetLastError(dwStatus);
                        DEBUGMSG(ZONE_ERROR,
                           (TEXT("ERROR!XHCD!HCD_IOControl() %x: \
                                  IOCTL_POWER_SET Error Code:%x\r\n"),
                            pHcd,
                            dwStatus));
                        fRetVal = FALSE;
                        break;
                    }

                }
                else
                {
                    pHcd->m_fDevicePowerDown = FALSE;
                }

                //
                // set the new power state and disable pipe cache ifHC is powering down
                //
                lPwrState =
                    InterlockedExchange((volatile LONG*)(&(pHcd->m_DevPwrState)),
                                        lPwrState);

                pHcd->Unlock();

                DEBUGMSG(ZONE_INIT,
                   (TEXT("XHCD!HCD_IOControl() %x: \
                          IOCTL_POWER_SET to %u from %u\r\n"),
                    pHcd,
                    pHcd->m_DevPwrState,
                    lPwrState));

                fRetVal = TRUE;
                SetLastError(ERROR_SUCCESS);
            }
            break;

        default:
            fRetVal = HcdPdd_IOControl(hOpenContext,
                                        dwCode,
                                       (PBYTE)pBufIn,
                                        dwLenIn,
                                       (PBYTE)pBufOut,
                                        dwLenOut,
                                       (PDWORD)pdwActualOut);
            if(fRetVal)
            {
                SetLastError(ERROR_SUCCESS);
            }

            break;
        }
    }
    __except(::XhcdProcessException (GetExceptionCode()))
    {
        DEBUGMSG(ZONE_INIT,
           (TEXT("XHCD!HCD_IOControl(%x) BAD PARAMETER(s) \
                  0x%x/%u, 0x%x/u, 0x%x!!!\r\n"),
            dwCode,
            pBufIn,
            dwLenIn,
            pBufOut,
            dwLenOut,
            pdwActualOut));
    }

    return fRetVal;
}