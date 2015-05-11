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
// File: cxhcd.cpp
//
// Description:
// 
//     This file contains the CXhcd object, which is the main entry
//            point for all HCDI calls by USBD 
//----------------------------------------------------------------------------------

#include <windows.h>

#include "globals.hpp"
#include "cxhcd.h"
#include "cpipe.h"

//----------------------------------------------------------------------------------
// Function: CXhcd
//
// Description: Initialize variables associated with this class
//              *All* initialization which could possibly fail should be done
//              via the Initialize() routine, which is called right after
//              the constructor
//
// Parameters: pvXhcdPddObject - Pointer to PDD specific data for this instance.
//
//             pCPhysMem       - Memory object created by <f HcdMdd_CreateMemoryObject>.
//
//             lpRegPath       - Registry key where XHCI configuration settings are stored.
//
//             portBase        - Pointer to XHCI controller registers.
//
//             dwSysIntr       - Logical value for XHCI interrupt (SYSINTR_xx).
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcd::CXhcd(IN LPVOID pvXhcdPddObject,
                IN CPhysMem * pCPhysMem,
                IN LPCWSTR lpRegPath, // szDriverRegistryKey ignored for now
                IN REGISTER portBase,
                IN DWORD dwSysIntr) : CHW(portBase,
                                        dwSysIntr,
                                        pCPhysMem,
                                        pvXhcdPddObject,
                                        lpRegPath){
    DEBUGMSG(ZONE_XHCD && ZONE_VERBOSE,(TEXT("+CXhcd::CXhcd\n")));
}

//----------------------------------------------------------------------------------
// Function: ~CXhcd
//
// Description: Destroy all memory and objects associated with this class
//
// Parameters: None
//
// Returns: None
//----------------------------------------------------------------------------------
CXhcd::~CXhcd()
{
    DEBUGMSG(ZONE_XHCD && ZONE_VERBOSE,(TEXT("+CXhcd::~CXhcd\n")));

    // make the API set inaccessible
    CRootHub *pRoot = SetRootHub(NULL);
    // signal root hub to close
    if(pRoot)
    {
        pRoot->HandleDetach();
        delete pRoot;
    }

    CHW::StopHostController();

    DEBUGMSG(ZONE_XHCD && ZONE_VERBOSE,(TEXT("-CXhcd::~CXhcd\n")));
}

//----------------------------------------------------------------------------------
// Function: DeInitializeDevice
//
// Description: prepare device for delete
//
// Parameters: none
//
// Returns: none
//----------------------------------------------------------------------------------
VOID CXhcd::DeInitializeDevice(VOID)
{
    DEBUGMSG(ZONE_XHCD && ZONE_VERBOSE,(TEXT("+CXhcd::DeInitialize\n")));

    CHW::StopHostController();

    // make the API set inaccessible
    CRootHub *pRoot = SetRootHub(NULL);
    // signal root hub to close
    if(pRoot)
    {
        pRoot->HandleDetach();
        delete pRoot;
    }

    CHW::DeInitialize(TRUE);
    CDeviceGlobal::DeInitialize();

    DEBUGMSG(ZONE_XHCD && ZONE_VERBOSE,(TEXT("-CXhcd::DeInitialize\n")));
}

//----------------------------------------------------------------------------------
// Function: InitializeDevice
//
// Description: Set up the Host Controller hardware, associated data structures,
//              and threads so that schedule processing can begin.
//              This function is called by right after the constructor.
//              It is the starting point for all initialization.
//              This needs to be implemented for HCDI
//
// Parameters: none
//
// Returns: TRUE - if initializes successfully and is ready to process
//                 the schedule
//          FALSE - if setup fails
//----------------------------------------------------------------------------------
BOOL CXhcd::InitializeDevice()
{
    DEBUGMSG(ZONE_INIT,(TEXT("+CXhcd::Initialize. Entry\r\n")));

    // All Initialize routines must be called, so we can't write
    // if(!CDevice::Initialize() || !CPipe::Initialize() etc)
    // due to short circuit eval.
    {
        BOOL fCDeviceInitOK;
        BOOL fCHWInitOK;
        
        fCDeviceInitOK = CDeviceGlobal::Initialize(this);

        if (!m_pMem->ReInit())
        {
            return FALSE;
        }

        fCHWInitOK = CHW::Initialize();

        if(!fCDeviceInitOK || !fCHWInitOK)
        {
            DEBUGMSG(ZONE_ERROR,
                (TEXT("-CXhcd::Initialize. Error - \
                could not initialize device/pipe/hw classes\n")));
            ASSERT(FALSE);
            return FALSE;
        }
    }

    // set up the root hub object
    {
        USB_DEVICE_INFO deviceInfo;
        USB_HUB_DESCRIPTOR usbHubDescriptor;

        deviceInfo.dwCount = sizeof(USB_DEVICE_INFO);
        deviceInfo.pnonConstUsbConfigs = NULL;
        deviceInfo.pnonConstUsbActiveConfig = NULL;
        deviceInfo.usbDeviceDescriptor.bLength = sizeof(USB_DEVICE_DESCRIPTOR);
        deviceInfo.usbDeviceDescriptor.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
        // USB spec 300
        deviceInfo.usbDeviceDescriptor.bcdUSB = 0x300;
        deviceInfo.usbDeviceDescriptor.bDeviceClass = USB_DEVICE_CLASS_HUB;
        deviceInfo.usbDeviceDescriptor.bDeviceSubClass = 0xff;
        deviceInfo.usbDeviceDescriptor.bDeviceProtocol = 0xff;
        deviceInfo.usbDeviceDescriptor.bMaxPacketSize0 = 0;
        deviceInfo.usbDeviceDescriptor.bNumConfigurations = 0;

        usbHubDescriptor.bDescriptorType = USB_HUB_DESCRIPTOR_TYPE;
        usbHubDescriptor.bDescriptorLength = USB_HUB_DESCRIPTOR_MINIMUM_SIZE;
        usbHubDescriptor.bNumberOfPorts = GetNumberOfPort();
        usbHubDescriptor.wHubCharacteristics =
            USB_HUB_CHARACTERISTIC_NO_POWER_SWITCHING |
            USB_HUB_CHARACTERISTIC_NOT_PART_OF_COMPOUND_DEVICE |
            USB_HUB_CHARACTERISTIC_NO_OVER_CURRENT_PROTECTION;
        //This adds 100ms additional delay to the existing 100ms
        // delay to detect the attached device
        usbHubDescriptor.bPowerOnToPowerGood = 50;
        usbHubDescriptor.bHubControlCurrent = 0;
        DEBUGCHK(usbHubDescriptor.bNumberOfPorts <= 0xf &&
            usbHubDescriptor.bNumberOfPorts >=1 );
        // all devices on hub are removable
        usbHubDescriptor.bRemoveAndPowerMask[0] = 0;
        // must be 0xFF, USB spec 1.1, table 11-8
        usbHubDescriptor.bRemoveAndPowerMask[1] = 0xFF;

        // FALSE indicates root hub is not low speed
        //(though, this is ignored for hubs anyway)
        SetRootHub(new CRootHub(deviceInfo,
                                USB_SUPER_SPEED,
                                usbHubDescriptor,
                                this));
    }
    if(!GetRootHub())
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("-CXhcd::Initialize - unable to create root hub object\n")));
        return FALSE;
    }

    // Signal root hub to start processing port changes
    // The root hub doesn't have any pipes, so we pass NULL as the
    // endpoint0 pipe
    if(FXN_DEV_STATE_OPERATIONAL != GetRootHub()->EnterOperationalState(NULL))
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("-CXhcd::Initialize. Error initializing root hub\n")));
        return FALSE;
    }
    // Start processing frames
    CHW::EnterOperationalState();

    DEBUGMSG(ZONE_INIT,(TEXT("-CXhcd::Initialize. Success!!\r\n")));
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: CreateHCDObject
//
// Description: creates new CXhcd object
//
// Parameters: see CXhcd::CXhcd
//
// Returns: new CXhcd object
//----------------------------------------------------------------------------------
CHcd * CreateHCDObject(IN LPVOID pvXhcdPddObject,
                        IN CPhysMem * pCPhysMem,
                        IN LPCWSTR szDriverRegistryKey,
                        IN REGISTER portBase,
                        IN DWORD dwSysIntr)
{
    return new CXhcd(pvXhcdPddObject,
                    pCPhysMem,
                    szDriverRegistryKey,
                    portBase,
                    dwSysIntr);
}