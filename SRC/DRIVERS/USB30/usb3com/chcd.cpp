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
// Copyright (c) 2013 Intel Corporation All Rights Reserved. 
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
// File: chcd.cpp
//
// Description:
// 
//     This file contains the Chcd object, which is the main entry
//     point for all HCDI calls by USBD
//
//----------------------------------------------------------------------------------

#include "globals.hpp"
#include "chcd.hpp"

//----------------------------------------------------------------------------------
// Function: CHcd
//
// Description: Constructor for CHcd
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
CHcd::CHcd()
{
    m_pCRootHub = NULL;
    m_fDevicePowerDown = FALSE;
    m_pOpenedPipesCache = NULL;
    m_dwOpenedPipeCacheSize = 0;
    m_DevPwrState = D0;
}

//----------------------------------------------------------------------------------
// Function: ~CHcd
//
// Description: Destructor for CHcd
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
CHcd::~CHcd()
{
    Lock();
    CRootHub *pRoot = m_pCRootHub;
    m_pCRootHub = NULL;
    Unlock();

     // signal root hub to close
    if(pRoot)
    {
        pRoot->HandleDetach();
        delete pRoot;
    }
}

//----------------------------------------------------------------------------------
// Function: SetRootHub
//
// Description: Set new root hub context
//
// Parameters: pRootHub - pointer to root hub context
//
// Returns: pointer to previous root hub context
//----------------------------------------------------------------------------------
CRootHub* CHcd::SetRootHub(CRootHub* pRootHub)
{
    Lock();
    CRootHub* pReturn=m_pCRootHub;
    m_pCRootHub=pRootHub;
    Unlock();
    return pReturn;
}

//----------------------------------------------------------------------------------
// Function: OpenPipe
//
// Description: Create a logical communication pipe to the endpoint described
//              by lpEndpointDescriptor for device address.
//              This needs to be implemented forHCDI
//
// Parameters:  address - address of device to open pipe for
//
//              lpEndpointDescriptor - describes endpoint to open
//
//              lpPipeIndex - out param, indicating index of opened pipe
//
// Returns: TRUE if pipe opened ok, FALSE otherwise
//----------------------------------------------------------------------------------
BOOL CHcd::OpenPipe(IN UINT uAddress,
                     IN LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                     OUT LPUINT lpPipeIndex,
                     OUT LPVOID* plpCPipe)
{
    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: +CHcd::OpenPipe fordevice on addr %d\n"),
        GetControllerName(),
        uAddress));

    BOOL fSuccess = FALSE;

    Lock();
    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL &&
         lpEndpointDescriptor != NULL &&
         lpEndpointDescriptor->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
         lpEndpointDescriptor->bLength >= sizeof(USB_ENDPOINT_DESCRIPTOR) &&
         lpPipeIndex != NULL)
    {
        // root hub will send the request to the appropriate device
        fSuccess =(REQUEST_OK == pRoot->OpenPipe(uAddress,
                                                   lpEndpointDescriptor,
                                                   lpPipeIndex,
                                                   plpCPipe));
    }

    Unlock();
    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: -CHcd::OpenPipe foraddress %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: ClosePipe
//
// Description: Close the logical communication pipe to this device's endpoint
//              This needs to be implemented for HCDI
//
// Parameters:  address - address of device to close pipe for
//
//              uPipeIndex - indicating index of pipe to close
//
// Returns: TRUE if call to m_pCRootHub succeeds, elseFALSE
//----------------------------------------------------------------------------------
BOOL CHcd::ClosePipe(IN UINT uAddress, IN UINT uPipeIndex)
{
    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: +CHcd::ClosePipe - address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex));

    BOOL fSuccess = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL)
    {
        fSuccess =(REQUEST_OK == pRoot->ClosePipe(uAddress, uPipeIndex));
    }

    Unlock();

    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: -CHcd::ClosePipe - \
              address = %d, uPipeIndex = %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: IssueTransfer
//
// Description: Issue a USB transfer. This needs to be implemented for HCDI
//
// Parameters:  pITP->address - address of device
//
//              pITP->uPipeIndex - index of pipe to issue transfer on(NOT the endpoint address)
//
//              pITP->lpStartAddress - ptr to function to callback when transfer completes
//                              (this can be NULL)
//
//              pITP->lpvNotifyParameter - parameter to pass to lpStartAddress in callback
//
//              pITP->dwFlags - combination of
//                              USB_IN_TRANSFER
//                              USB_OUT_TRANSFER
//                              USB_NO_WAIT
//                              USB_SHORT_TRANSFER_OK
//                              USB_START_ISOCH_ASAP
//                              USB_COMPRESS_ISOCH
//                              USB_SEND_TO_DEVICE
//                              USB_SEND_TO_INTERFACE
//                              USB_SEND_TO_ENDPOINT
//                              USB_DONT_BLOCK_FOR_MEM
//                         defined in usbtypes.h
//
//              pITP->lpvControlHeader - forcontrol transfers, a pointer to the
//                                 USB_DEVICE_REQUEST structure
//
//
//              pITP->dwStartingFrame - forIsoch transfers, this indicates the
//                                first frame of the transfer. If the
//                                USB_START_ISOCH_ASAP flag is set in
//                                dwFlags, the dwStartingFrame is ignored
//                                and the transfer is scheduled As Soon
//                                As Possible
//
//              pITP->dwFrames - indicates over how many frames to conduct this
//                         Isochronous transfer. Also should be the # of
//                         entries in aLengths, lpdwIsochErrors, lpdwIsochLengths
//
//
//              pITP->aLengths - array of dwFrames long. aLengths[i] is how much
//                         isoch data to transfer in the i'th frame. The
//                         sum of all entries should be dwBufferSize
//
//              pITP->dwBufferSize - size of data buffer passed in lpvBuffer
//
//              pITP->lpvBuffer - data buffer(may be NULL)
//
//              pITP->uPaBuffer - physical address of data buffer(may be 0)
//
//              pITP->lpvCancelId - identifier used to refer to this transfer
//                            in case it needs to be canceled later
//
//              pITP->lpdwIsochErrors - array of dwFrames long. lpdwIsochErrors[ i ]
//                               is set on transfer complete to be the status
//                               of the i'th frame of the isoch transfer
//
//              pITP->lpdwIsochLengths - array of dwFrames long. lpdwIsochLengths[ i ]
//                                is set on transfer complete to be the amount
//                                of data transferred in the i'th frame of
//                                the isoch transfer
//
//              pITP->lpfComplete - pointer to BOOL indicating TRUE/FALSE on
//                            whether this transfer is complete
//
//              pITP->lpdwBytesTransfered - pointer to DWORD indicating total # of
//                                    bytes successfully transferred
//
//              pITP->lpdwError - pointer to DWORD set to error status on
//                          transfer complete
//
// Returns: TRUE if transfer scheduled ok, otherwise FALSE
//----------------------------------------------------------------------------------
BOOL CHcd::IssueTransfer(ISSUE_TRANSFER_PARAMS* pITP)
{
   DEBUGMSG(ZONE_XHCD | ZONE_TRANSFER,
      (TEXT("%s: +CHcd::IssueTransfer - \
             address = %d, pipe = %d, dwFlags = 0x%x, \
             lpvControlHeader = 0x%x, lpvBuffer = 0x%x, \
             dwBufferSize = %d\n"),
       GetControllerName(), 
       pITP->uAddress,
       pITP->uPipeIndex,
       pITP->dwFlags,
       pITP->lpcvControlHeader,
       pITP->lpvBuffer,
       pITP->dwBufferSize));

   BOOL fSuccess = FALSE;

   Lock();

   CRootHub *pRoot = m_pCRootHub;
   
   if(pRoot != NULL)
   {
       HCD_REQUEST_STATUS status = REQUEST_IGNORED;

       status = pRoot->IssueTransfer(pITP);

       if(status == REQUEST_OK)
       {
           fSuccess = TRUE;
       }
       else if(status == REQUEST_IGNORED)
       {
           SetLastError(ERROR_DEV_NOT_EXIST);
       }       
   }

   Unlock();

   DEBUGMSG(ZONE_XHCD | ZONE_TRANSFER,
      (TEXT("%s: -CHcd::IssueTransfer - returing BOOL %d\n"),
       GetControllerName(),
       fSuccess));

   return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: AbortTransfer
//
// Description: Abort a previously issued transfer
//              This needs to be implemented for HCDI
//
// Parameters:  address - address of device on which transfer was issued
//
//              uPipeIndex - index of pipe on which transfer was issued
//
//              lpCancelAddress - function to callback when this transfer has aborted
//
//              lpvNotifyParameter - parameter to callback lpCancelAddress
//
//              lpvCancelId - used to identify the transfer to abort. This was passed
//                            in when IssueTransfer was called
//
// Returns: TRUE if call to m_pCRootHub succeeds, elseFALSE
//----------------------------------------------------------------------------------
BOOL CHcd::AbortTransfer(IN UINT uAddress,
                           IN UINT uPipeIndex,
                           IN LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                           IN LPVOID lpvNotifyParameter,
                           IN LPCVOID lpvCancelId)
{
    DEBUGMSG(ZONE_XHCD | ZONE_TRANSFER,
       (TEXT("%s: +CHcd::AbortTransfer - \
              address = %d, uPipeIndex = %d, lpvCancelId = 0x%x\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex,
        lpvCancelId));

    BOOL fSuccess = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL)
    {
        fSuccess =(REQUEST_OK == pRoot->AbortTransfer(uAddress,
                                                       uPipeIndex,
                                                       lpCancelAddress,
                                                       lpvNotifyParameter,
                                                       lpvCancelId));
    }

    Unlock();

    DEBUGMSG(ZONE_XHCD | ZONE_TRANSFER,
       (TEXT("%s: -CHcd::AbortTransfer - \
              address = %d, uPipeIndex = %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: IsPipeHalted
//
// Description: Check whether the pipe indicated by address/uPipeIndex is
//              halted(stalled) and return result in *lpfHalted
//              This needs to be implemented for HCDI
//
// Parameters:  address - address of device to check pipe for
//
//              uPipeIndex - indicating index of pipe to check
//
//              lpfHalted - out param, indicating TRUE if pipe is halted,
//                          elseFALSE
//
// Returns: TRUE ifcorrect status placed in *lpfHalted, elseFALSE
//----------------------------------------------------------------------------------
BOOL CHcd::IsPipeHalted(IN UINT uAddress,
                          IN UINT uPipeIndex,
                          OUT LPBOOL lpfHalted)
{
    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: +CHcd::IsPipeHalted - address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex));

    BOOL fRetval = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL && lpfHalted != NULL)
    {
        fRetval  =(REQUEST_OK == pRoot->IsPipeHalted(uAddress,
                                                     uPipeIndex,
                                                     lpfHalted));
    }

    Unlock();

    INT iHalted = -1;

    if(lpfHalted)
    {
        iHalted = *lpfHalted;
    }

    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: -CHcd::IsPipeHalted - \
              address = %d, uPipeIndex = %d, *lpfHalted = %d, retval = %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex,
        iHalted,
        fRetval ));

    return fRetval ;
}

//----------------------------------------------------------------------------------
// Function: ResetPipe
//
// Description: Reset a stalled pipe on device at address "address"
//              This needs to be implemented for HCDI
//
// Parameters:  address - address of device to reset pipe for
//
//              uPipeIndex - indicating index of pipe to check
//
//              lpfHalted - out param, indicating TRUE if pipe is halted,
//                          elseFALSE
//
// Returns: TRUE ifcorrect status placed in *lpfHalted, elseFALSE
//----------------------------------------------------------------------------------
BOOL CHcd::ResetPipe(IN UINT uAddress, IN UINT uPipeIndex)
{
    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: +CHcd::ResetPipe - address = %d, uPipeIndex = %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex));

    BOOL fSuccess = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL)
    {
        fSuccess =(REQUEST_OK == pRoot->ResetPipe(uAddress, uPipeIndex));
    }

    Unlock();

    DEBUGMSG(ZONE_XHCD,
       (TEXT("%s: -CHcd::ResetPipe - \
              address = %d, uPipeIndex = %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        uPipeIndex,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: DisableDevice
//
// Description: Disable on device at address "address"
//              This needs to be implemented forHCDI
//
// Parameters:  address - address of device to disable for
//
//              fReset - Reset it after disable.
//
// Returns: TRUE ifSusess.
//----------------------------------------------------------------------------------
BOOL CHcd::DisableDevice(IN const UINT uAddress,
                        IN const BOOL fReset)
{
    DEBUGMSG(ZONE_HCD,
       (TEXT("%s: +CHcd::DisableDevice - address = %d,fReset = %d\n"),
        GetControllerName(),
        uAddress,
        fReset));

    BOOL fSuccess = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL)
    {
        fSuccess =(REQUEST_OK == pRoot->DisableDevice(uAddress, fReset));
    }

    Unlock();

    DEBUGMSG(ZONE_HCD,
       (TEXT("%s: -CHcd::DisableDevice - address = %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        fSuccess));

    return fSuccess;
}

//----------------------------------------------------------------------------------
// Function: SuspendResume
//
// Description: Suspend or Resume on device at address "address"
//              This needs to be implemented for HCDI
//
// Parameters:  address - address of device to disable for
//
//              fSuspend  - Suspend, otherwise resume..
//
// Returns: TRUE ifSusess.
//----------------------------------------------------------------------------------
BOOL CHcd::SuspendResume(IN const UINT uAddress,
                            IN const BOOL fSuspend)
{
    DEBUGMSG(ZONE_HCD,
       (TEXT("%s: +CHcd::SuspendResume - address = %d,  fSuspend = %d\n"),
        GetControllerName(),
        uAddress,
        fSuspend));

    BOOL fSuccess = FALSE;

    Lock();

    CRootHub *pRoot = m_pCRootHub;

    if(pRoot != NULL)
    {
        fSuccess =(REQUEST_OK == pRoot->SuspendResume(uAddress,  fSuspend));
    }

    Unlock();

    DEBUGMSG(ZONE_HCD,
       (TEXT("%s: -CHcd::SuspendResume - \
              address = %d,fSuspend = %d, returning BOOL %d\n"),
        GetControllerName(),
        uAddress,
        fSuspend,
        fSuccess));

    return fSuccess;
}