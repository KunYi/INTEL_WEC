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
// File: cdevice.hpp Implements classes for managing USB devices
//----------------------------------------------------------------------------------

#ifndef CDEVICE_HPP
#define CDEVICE_HPP

#include "globals.hpp"
#include "sync.hpp"
#include "cpipeabs.hpp"

// 1.0 second to accommodate slow control pipes
// address array variables - initially set to have address 0 used
// because it is reserved for the root hub(besides, no real device
// can permanently use addr0, since it is used at set address time)
#define STANDARD_REQUEST_TIMEOUT (1000)
#define STANDARD_STATUS_TIMEOUT (100)
#define ORIENT_TIMEOUT (1800)

/* Unknown Speed mode */
#define USB_UNKNOWN_SPEED       ((UCHAR)0x00)
/* Full Speed mode */
#define USB_FULL_SPEED          ((UCHAR)0x01)
/* Low Speed mode */
#define USB_LOW_SPEED           ((UCHAR)0x02)
/* High Speed mode */
#define USB_HIGH_SPEED          ((UCHAR)0x03)
/* Super Speed mode */
#define USB_SUPER_SPEED         ((UCHAR)0x04)

//log base 2 of 32
#define LOG_BASE2_OF32           5
//bits in DWORD
#define BITS_IN_DWORD            32

#define MAX_ADDR_ARRAY           8

#define MAX_PORT_NUMBER (32)

//
// bits for <CDevice::m_dwDevState>
//
#define FXN_DEV_STATE_OPERATIONAL    (0)

#define FXN_DEV_STATE_FUNCTION       (0x01)
#define FXN_DEV_STATE_UNDETERMINED   (0x02)
#define FXN_DEV_STATE_ATTACHMENT     (0x04)
#define FXN_DEV_STATE_ENABFAILED     (0x10)
#define FXN_DEV_STATE_DETACHMENT     (0x40)
#define FXN_DEV_STATE_DETACHUSBD     (0x80)

#define FXN_DEV_STATE_PROGR_BITS     (0x0F)
#define FXN_DEV_STATE_DETACH_BITS    (0xF0)

// this enum is used in AttachNewDevice
typedef enum
{
    //
    // KEEP THIS ARRAY IN SYNC WITH cszCfgStateStrings array below!
    //

    // these steps are common for both hubs and functions
    DEVICE_CONFIG_STATUS_OPENING_ENDPOINT0_PIPE = 0,
    DEVICE_CONFIG_RESET_AND_ENABLEPORT_FOR_SPEEDINFO,
    DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT,
    DEVICE_CONFIG_STATUS_SCHEDULING_ENABLE_SLOT,
    DEVICE_CONFIG_STATUS_SCHEDULING_RESET_DEVICE,
    DEVICE_CONFIG_STATUS_SCHEDULING_ADDRESS_DEVICE,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR_TEST,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_DEVICE_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_SETUP_CONFIGURATION_DESCRIPTOR_ARRAY,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_CONFIG_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_CONFIG_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_DETERMINE_CONFIG_TO_CHOOSE,
    DEVICE_CONFIG_STATUS_ADD_ENDPOINTS,
    DEVICE_CONFIG_STATUS_BANDWIDTH,
    DEVICE_CONFIG_STATUS_SCHEDULING_SET_CONFIG,
    // if(device is a hub) hub is not supported
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_SCHEDULING_GET_HUB_DESCRIPTOR,
    DEVICE_CONFIG_STATUS_CREATE_NEW_HUB,
    DEVICE_CONFIG_STATUS_SET_HUB_DEPTH,
    // } else {
    DEVICE_CONFIG_STATUS_CREATE_NEW_FUNCTION,
    // }
    DEVICE_CONFIG_STATUS_INSERT_NEW_DEVICE_INTO_UPSTREAM_HUB_PORT_ARRAY,
    DEVICE_CONFIG_STATUS_SIGNAL_NEW_DEVICE_ENTER_OPERATIONAL_STATE,
    DEVICE_CONFIG_STATUS_FAILED,
    DEVICE_CONFIG_STATUS_DONE,

    DEVICE_CONFIG_STATUS_INVALID // must be last item in list
} DEVICE_CONFIG_STATUS;

#ifdef DEBUG
__declspec(selectany) extern const TCHAR *cszCfgStateStrings[] =
{
    //
    // KEEP THIS ARRAY IN SYNC WITH DEVICE_CONFIG_STATUS enum above!
    //
    TEXT("DEVICE_CONFIG_STATUS_OPENING_ENDPOINT0_PIPE"),
    TEXT("DEVICE_CONFIG_RESET_AND_ENABLEPORT_FOR_SPEEDINFO"),
    TEXT("DEVICE_CONFIG_STATUS_RESET_AND_ENABLE_PORT"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_ENABLE_SLOT"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_RESET_DEVICE"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_ADDRESS_DEVICE"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR_TEST"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_DEVICE_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_DEVICE_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_SETUP_CONFIGURATION_DESCRIPTOR_ARRAY"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_CONFIG_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_CONFIG_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_DETERMINE_CONFIG_TO_CHOOSE"),
    TEXT("DEVICE_CONFIG_STATUS_ADD_ENDPOINTS"),
    TEXT("DEVICE_CONFIG_STATUS_BANDWIDTH"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_SET_CONFIG"),
    // if(device is a hub) {
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_INITIAL_HUB_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_SCHEDULING_GET_HUB_DESCRIPTOR"),
    TEXT("DEVICE_CONFIG_STATUS_CREATE_NEW_HUB"),
    TEXT("DEVICE_CONFIG_STATUS_SET_HUB_DEPTH"),
    // } else {
    TEXT("DEVICE_CONFIG_STATUS_CREATE_NEW_FUNCTION"),
    // }
    TEXT("DEVICE_CONFIG_STATUS_INSERT_NEW_DEVICE_INTO_UPSTREAM_HUB_PORT_ARRAY"),
    TEXT("DEVICE_CONFIG_STATUS_SIGNAL_NEW_DEVICE_ENTER_OPERATIONAL_STATE"),
    TEXT("DEVICE_CONFIG_STATUS_FAILED"),
    TEXT("DEVICE_CONFIG_STATUS_DONE"),

    TEXT("DEVICE_CONFIG_STATUS_INVALID")
};

inline LPCTSTR StatusToString (UINT status)
{
    if (status < DEVICE_CONFIG_STATUS_INVALID) 
    {
        return cszCfgStateStrings [status];
    } else {
        return TEXT("Invalid");
    }
}

#endif // DEBUG

class CHcd;
class CDevice;
class CFunction;
class CHub;
class CRootHub;
class CExternalHub;

//
// CDeviceGlobal description
//
class CDeviceGlobal
{
public:
    CDeviceGlobal();
    virtual ~CDeviceGlobal();
    BOOL Initialize(IN PVOID pHcd);
    VOID DeInitialize(VOID);

    //Object Countdown
    BOOL IncObjCountdown()
    {
        return  m_objCountdown.IncrCountdown();
    };

    VOID DecObjCountdown()
    {
        m_objCountdown.DecrCountdown();
    };

    virtual LPCTSTR GetControllerName(VOID) const = 0;

public:

    LPUSBD_SELECT_CONFIGURATION_PROC GetpUSBDSelectConfigurationProc()
    {
        return m_pUSBDSelectConfigurationProc;
    };

    LPUSBD_SUSPEND_RESUME_PROC GetpUSBDSuspendedResumed()
    {
        return m_pUSBDSuspendResumed;
    };

    LPUSBD_ATTACH_PROC GetpUSBDAttachProc()
    {
        return m_pUSBDAttachProc;
    };

    LPUSBD_DETACH_PROC GetpUSBDDetachProc()
    {
        return m_pUSBDDetachProc;
    };

    LPVOID GetpHcdContext()
    {
        return m_pvHcdContext;
    };

private:

    PVOID                            m_pHcd;
    // counts stray detach threads
    Countdown                        m_objCountdown;
    // USBD.dll related variables
    HINSTANCE                        m_hUSBDInstance;
    LPUSBD_SELECT_CONFIGURATION_PROC m_pUSBDSelectConfigurationProc;
    // this procedure is called when new USB devices(functions) are attached
    LPUSBD_ATTACH_PROC               m_pUSBDAttachProc;
    // this procedure is called when USB devices(functions) are detached
    LPUSBD_DETACH_PROC               m_pUSBDDetachProc;
    LPUSBD_SUSPEND_RESUME_PROC       m_pUSBDSuspendResumed;
    // OUT param for lpHcdAttachProc call
    LPVOID                           m_pvHcdContext;

#ifdef DEBUG
    BOOL g_fAlreadyCalled;
#endif // DEBUG

};

//
// abstract base class for devices
//
class CDevice
{
    friend class CHub;

public:

    CDevice(IN const UCHAR bAddress,
            IN const USB_DEVICE_INFO& rDeviceInfo,
            IN const UCHAR bSpeed,
            IN const UCHAR bTierNumber,
            IN CDeviceGlobal * const pDeviceGlobal,
            IN CHub * const pAttachedHub=NULL,
            const UCHAR bAttachedPort = 0);

    virtual ~CDevice();
    virtual DWORD EnterOperationalState(IN CPipeAbs* const pStartEndpointPipe) = 0;

    virtual HCD_REQUEST_STATUS OpenPipe(IN const UINT uAddress,
                                        IN LPCUSB_ENDPOINT_DESCRIPTOR const lpEndpointDescriptor,
                                        OUT LPUINT const lpPipeIndex,
                                        OUT LPVOID* const plpCPipe = NULL) = 0;

    virtual HCD_REQUEST_STATUS ClosePipe(IN const UINT uAddress,
                                            IN const UINT uPipeIndex) = 0;

    virtual HCD_REQUEST_STATUS IssueTransfer(PISSUE_TRANSFER_PARAMS pITP) = 0;

    virtual HCD_REQUEST_STATUS AbortTransfer(
                                    IN const UINT uAddress,
                                    IN const UINT uPipeIndex,
                                    IN LPTRANSFER_NOTIFY_ROUTINE const lpCancelAddress,
                                    IN LPVOID const lpvNotifyParameter,
                                    IN LPCVOID const lpvCancelId) = 0;

    virtual HCD_REQUEST_STATUS IsPipeHalted(IN const UINT uAddress,
                                            IN const UINT uPipeIndex,
                                            OUT LPBOOL const lpfHalted) = 0;

    virtual HCD_REQUEST_STATUS ResetPipe(IN const UINT uAddress, IN const UINT uPipeIndex) = 0;
    virtual HCD_REQUEST_STATUS DisableDevice(IN const UINT uAddress, IN const BOOL fReset);
    virtual HCD_REQUEST_STATUS SuspendResume(IN const UINT uAddress, IN const BOOL fSuspend);

    virtual BOOL ResumeNotification()
    {
        return FALSE;
    };

    virtual BOOL NotifyOnSuspendedResumed(BOOL /*fResumed*/)
    {
        return FALSE;
    };

    virtual VOID HandleDetach(VOID) = 0;

    virtual CHub * GetUSB2TT(UCHAR& pTTAddr, UCHAR& pTTPort, PVOID& pTTContext);

    LPCTSTR GetControllerName(VOID) const
    {
        return m_pDeviceGlobal->GetControllerName();
    }

#ifdef DEBUG
    VOID DumpDeviceDescriptor(IN const PUSB_DEVICE_DESCRIPTOR pDescriptor)const;
    VOID DumpConfigDescriptor(IN const PUSB_CONFIGURATION_DESCRIPTOR pDescriptor)const;
    VOID DumpInterfaceDescriptor(IN const PUSB_INTERFACE_DESCRIPTOR pDescriptor)const;
    VOID DumpEndpointDescriptor(IN const PUSB_ENDPOINT_DESCRIPTOR pDescriptor)const;
    VOID DumpExtendedBytes(IN BYTE const*const pByteArray, IN const DWORD dwSize)const;
#endif // DEBUG

private:

public:

    // utility functions for managing NON_CONST_USB_CONFIGURATION structures
    BOOL CreateUsbConfigurationStructure(IN NON_CONST_USB_CONFIGURATION& rConfig,
                                            __in_bcount(uDataBufferLen) const PUCHAR pDataBuffer,
                                            IN const UINT uDataBufferLen) const;

    VOID DeleteUsbConfigurationStructure(IN NON_CONST_USB_CONFIGURATION& rConfig) const;

protected:

    static DWORD CALLBACK TransferDoneCallbackSetEvent(PVOID pContext);
    BOOL AllocatePipeArray(VOID);

#ifdef DEBUG
    virtual const TCHAR* GetDeviceType(VOID) const = 0;
#endif // DEBUG

    CDevice(const CDevice&);              // NOT IMPLEMENTED default copy constructor
    CDevice&operator=(CDevice&);

    CHub * const          m_pAttachedHub; // Attached Hub.
    const UCHAR           m_bAttachedPort; // Port number of this hub.
    CRITICAL_SECTION      m_csDeviceLock; // critical section for device

    const UCHAR           m_bAddress; // address/deviceIndex of this device
    USB_DEVICE_INFO       m_deviceInfo; // holds device's USB descriptors
    const UCHAR           m_bSpeed; // indicates device speed
    const UCHAR           m_bTierNumber; // indicates tier # of device(i.e. how far
                                        // it is from the root hub) See the USB spec
                                        // 1.1, section 4.1.1
    CDeviceGlobal * const m_pDeviceGlobal;
    UCHAR                 m_bMaxNumPipes; // size of m_ppCPipe array
    union {
        CPipeAbs*         m_pCtrlPipe0; // used only during ATTACH
        BOOL              m_fIsSuspend; // Is this device suspend or not.
    };
    volatile DWORD        m_dwDevState; // progress during attach - 0 is fully operational
                                        // used to sync ATTACH and DETACH threads

#define ENDPOINT0_CONTROL_PIPE          UCHAR(0) // control pipe is at m_ppCPipe[0]
#define STATUS_CHANGE_INTERRUPT_PIPE    UCHAR(1) // for hubs, status change pipe is m_ppCPipe[1]

    CPipeAbs**            m_ppCPipe; // array of pipes for this device

public:
    UCHAR GetDeviceAddress()
    {
        return m_bAddress;
    };
};

//
// abstract base class for hubs
//
class CHub : public CDevice
{
    friend class CDevice;
public:

    CHub(IN const UCHAR bAddress,
        IN const USB_DEVICE_INFO& rDeviceInfo,
        IN const UCHAR bSpeed,
        IN const UCHAR bTierNumber,
        IN const USB_HUB_DESCRIPTOR& rUsbHubDescriptor,
        IN CHcd * const pCHcd,
        IN CHub * const pAttachedHub = NULL,
        const UCHAR bAttachedPort = 0);

    virtual ~CHub();

    VOID HandleDetach(VOID);
    UCHAR         m_uRootHubPort;
    UINT          m_uHubRoute;
    BOOL          m_DeviceProtocol;
    HCD_REQUEST_STATUS OpenPipe(IN const UINT uAddress,
                                IN LPCUSB_ENDPOINT_DESCRIPTOR const lpEndpointDescriptor,
                                OUT LPUINT const lpPipeIndex,
                                OUT LPVOID* const plpCPipe = NULL);

    HCD_REQUEST_STATUS ClosePipe(IN const UINT uAddress, IN const UINT uPipeIndex);

    HCD_REQUEST_STATUS IssueTransfer(PISSUE_TRANSFER_PARAMS pITP);

    HCD_REQUEST_STATUS AbortTransfer(IN const UINT uAddress,
                                        IN const UINT uPipeIndex,
                                        IN LPTRANSFER_NOTIFY_ROUTINE const lpCancelAddress,
                                        IN LPVOID const lpvNotifyParameter,
                                        IN LPCVOID const lpvCancelId);

    HCD_REQUEST_STATUS IsPipeHalted(IN const UINT uAddress,
                                    IN const UINT uPipeIndex,
                                    OUT LPBOOL const lpfHalted);

    HCD_REQUEST_STATUS ResetPipe(IN const UINT uAddress, IN const UINT uPipeIndex);

    virtual BOOL DisableOffStreamDevice(IN const UINT uAddress, IN const BOOL fReset);
    virtual BOOL SuspendResumeOffStreamDevice(IN const UINT uAddress, IN const BOOL fSuspend);

    virtual HCD_REQUEST_STATUS DisableDevice(IN const UINT uAddress, IN const BOOL fReset);
    virtual HCD_REQUEST_STATUS SuspendResume(IN const UINT uAddress,IN const BOOL fSuspend);

    // Notification when this hub is resumed.
    virtual BOOL ResumeNotification()
    {
        DEBUGMSG(ZONE_ATTACH,
               (TEXT("CHub(%s tier %d):: ResumeNotification(%d) !\n"),
                GetDeviceType(),
                m_bTierNumber,
                m_bAddress));

        m_fIsSuspend = FALSE;

        return SetEvent(m_hHubSuspendBlockEvent);
    };

    virtual BOOL NotifyOnSuspendedResumed(BOOL fResumed);

#ifdef DEBUG
    static VOID DumpHubDescriptor(IN const PUSB_HUB_DESCRIPTOR pDescriptor);
#endif // DEBUG

private:

    DWORD HubStatusChangeThread(VOID);

protected:

    static DWORD CALLBACK HubStatusChangeThreadStub(IN PVOID pContext);

    VOID AttachDevice(IN const UCHAR bPort,
                        IN UCHAR bSpeed = USB_UNKNOWN_SPEED,
                        IN const BOOL fSyncronously = FALSE,
                        IN const BOOL fResetForSpeedInfo = TRUE);

    BOOL GetDescriptor(IN CPipeAbs* const pControlPipe,
                        IN const UINT uAddress,
                        IN const UCHAR bDescriptorType,
                        IN const UCHAR bDescriptorIndex,
                        IN const USHORT wDescriptorSize,
                        OUT PVOID pBuffer);

    DWORD GetStartTime() const;

    PVOID EnableSlot(OUT UCHAR& bSlotId);

    BOOL ResetDevice(IN const UCHAR bAddress);

    BOOL AddressDevice(IN UCHAR bSlotId, UINT uPortId, IN UCHAR bSpeed, UCHAR bRootHubPort, UCHAR bHubSID, UINT uDevRoute);

    BOOL DoConfigureEndpoint(UCHAR bSlotId, USB_DEVICE_DESCRIPTOR* pDevDesc, USB_HUB_DESCRIPTOR* pHubDesc);

    BOOL AddEndpoints(UCHAR bSlotId, USB_DEVICE_INFO* deviceInfo);

    BOOL ResetTT(IN const UCHAR bPort);

    VOID DelayForFirstConnect (UCHAR bPort);

    virtual BOOL PowerAllHubPorts(VOID) = 0;

    virtual BOOL WaitForPortStatusChange(OUT UCHAR& rPort,
                                            OUT USB_HUB_AND_PORT_STATUS& rStatus) = 0;

    virtual BOOL SetOrClearFeature(IN const WORD wPort,
                                    IN const UCHAR bSetOrClearFeature,
                                    IN const USHORT wFeature) = 0;

    virtual BOOL SetOrClearRemoteWakup(BOOL fSet) = 0;

    virtual BOOL GetStatus(IN const UCHAR bPort,
                            OUT USB_HUB_AND_PORT_STATUS& rStatus) = 0;
    virtual UCHAR  GetDeviceSpeed(IN USB_HUB_AND_PORT_STATUS& rStatus ) = 0;

    virtual BOOL ResetAndEnablePort(IN const UCHAR bPort) = 0;

    virtual VOID DisablePort(IN const UCHAR bPort) = 0;

    static DWORD CALLBACK DetachDownstreamDeviceThreadStub(IN PVOID pContext);
    DWORD CALLBACK DetachDownstreamDeviceThread(CDevice* pDevToDetach);

    static DWORD CALLBACK AttachDownstreamDeviceThreadStub(IN PVOID pContext);
    DWORD CALLBACK AttachDownstreamDeviceThread(CDevice* pDevToAttach);

    VOID DetachDevice(IN const UCHAR bPort, IN const BOOL fSyncronously = FALSE);

    BOOL AllocateDeviceArray(VOID);
    CHub(const CHub&);        // NOT IMPLEMENTED default copy constructor
    CHub&operator=(CHub&);   

    USB_HUB_DESCRIPTOR    m_usbHubDescriptor; // the hub's USB descriptor
    CDevice**             m_ppCDeviceOnPort;  // array of pointers to the devices on this hub's ports
    PVOID *               m_pAddedTT;
    BOOL                  m_fHubThreadClosing;     // indicates to threads that this device is being closed

    HANDLE                m_hHubStatusChangeEvent; // indicates status change on one of the hub's ports
    HANDLE                m_hHubStatusChangeThread; // thread for checking port status change
    HANDLE                m_hHubSuspendBlockEvent;

    CHcd * const          m_pCHcd;
    BOOL                  m_fPortFirstConnect [MAX_PORT_NUMBER];
};

class CRootHub : public CHub
{
public:

    CRootHub(IN const USB_DEVICE_INFO& rDeviceInfo,
            IN const UCHAR bSpeed,
            IN const USB_HUB_DESCRIPTOR& rUsbHubDescriptor,
            IN CHcd * const pCHcd);

    ~CRootHub();

    DWORD EnterOperationalState(IN CPipeAbs* const pStartEndpointPipe);

private:

    BOOL  SetOrClearFeature(IN const WORD wPort,
                            IN const UCHAR bSetOrClearFeature,
                            IN const USHORT wFeature);

    virtual BOOL SetOrClearRemoteWakup(BOOL fSet);

    BOOL GetStatus(IN const UCHAR bPort, OUT USB_HUB_AND_PORT_STATUS& rStatus);
    UCHAR GetDeviceSpeed(IN USB_HUB_AND_PORT_STATUS& rStatus );

#ifdef DEBUG
    const TCHAR* GetDeviceType(VOID) const
    {
        static const TCHAR* cszDeviceType = TEXT("Root");
        return cszDeviceType;
    }
#endif // DEBUG

    BOOL PowerAllHubPorts(VOID);

    BOOL WaitForPortStatusChange(OUT UCHAR& rPort,
                                OUT USB_HUB_AND_PORT_STATUS& rStatus);

    BOOL ResetAndEnablePort(IN const UCHAR bPort);

    VOID DisablePort(IN const UCHAR bPort);

    CRootHub&operator=(CRootHub&);
};

class CExternalHub : public CHub
{
public:
    // ****************************************************
    // Public Functions for CExternalHub
    // ****************************************************
    CExternalHub( IN const UCHAR address,
                  IN const USB_DEVICE_INFO& rDeviceInfo,
                  IN const UCHAR bSpeed,
                  IN const UCHAR tierNumber,
                  IN const USB_HUB_DESCRIPTOR& rUsbHubDescriptor,
                  IN CHcd * const pCHcd,
                  IN CHub * const pAttachedHub,const UCHAR uAttachedPort);

    ~CExternalHub();

    DWORD EnterOperationalState( IN CPipeAbs* const pEndpoint0Pipe );
    BOOL         m_uHubSuperSpeed;
    // ****************************************************
    // Public Variables for CExternalHub
    // ****************************************************

private:
#ifdef DEBUG
    const TCHAR* GetDeviceType( void ) const
    {
        static const TCHAR* cszDeviceType = TEXT("External");
        return cszDeviceType;
    }
#endif // DEBUG

    BOOL  PowerAllHubPorts( void );

    BOOL  GetStatusChangeBitmap( OUT DWORD& rdwHubBitmap );

    BOOL  WaitForPortStatusChange( OUT UCHAR& rPort,
                                   OUT USB_HUB_AND_PORT_STATUS& rStatus );

    BOOL  ResetAndEnablePort( IN const UCHAR port );

    void  DisablePort( IN const UCHAR port );

    BOOL  SetOrClearFeature( IN const WORD port,
                             IN const UCHAR setOrClearFeature,
                             IN const USHORT feature );

    virtual BOOL  SetOrClearRemoteWakup(BOOL bSet);

    BOOL  GetStatus( IN const UCHAR port,
                     OUT USB_HUB_AND_PORT_STATUS& rStatus );
    UCHAR  GetDeviceSpeed(IN USB_HUB_AND_PORT_STATUS& rStatus );

    CExternalHub&operator=(CExternalHub&){ASSERT(FALSE);}

    // ****************************************************
    // Private Variables for CExternalHub
    // ****************************************************

};


// class for USB functions(i.e. mice, keyboards, etc)
class CFunction : public CDevice
{
    friend class CHub;
public:

    CFunction(IN const UCHAR bAddress,
                IN const USB_DEVICE_INFO& rUhcaddress,
                IN const UCHAR bSpeed,
                IN const UCHAR bTierNumber,
                IN CHcd * const pCHcd,
                IN CHub * const pAttachedHub,
                const UCHAR bAttachedPort,
                IN PVOID pDevicePointer);

    ~CFunction();

    DWORD EnterOperationalState(IN CPipeAbs* const pStartEndpointPipe);

    HCD_REQUEST_STATUS OpenPipe(IN const UINT uAddress,
                                IN LPCUSB_ENDPOINT_DESCRIPTOR const lpEndpointDescriptor,
                                OUT LPUINT const lpPipeIndex,
                                OUT LPVOID* const plpCPipe = NULL);

    HCD_REQUEST_STATUS ClosePipe(IN const UINT uAddress, IN const UINT uPipeIndex);

    HCD_REQUEST_STATUS IssueTransfer(PISSUE_TRANSFER_PARAMS pITP);

    HCD_REQUEST_STATUS AbortTransfer(IN const UINT uAddress,
                                        IN const UINT uPipeIndex,
                                        IN LPTRANSFER_NOTIFY_ROUTINE const lpCancelAddress,
                                        IN LPVOID const lpvNotifyParameter,
                                        IN LPCVOID const lpvCancelId);

    HCD_REQUEST_STATUS IsPipeHalted(IN const UINT uAddress,
                                    IN const UINT uPipeIndex,
                                    OUT LPBOOL const lpfHalted);

    HCD_REQUEST_STATUS ResetPipe(IN const UINT uAddress, IN const UINT uPipeIndex);

    virtual BOOL NotifyOnSuspendedResumed(BOOL fResumed);

private:

    VOID HandleDetach(VOID);

    BOOL SetOrClearFeature(IN const UCHAR bRecipient,
                            IN const WORD wIndex,
                            IN const UCHAR bSetOrClearFeature,
                            IN const USHORT wFeature);

#ifdef DEBUG
    const TCHAR* GetDeviceType(VOID) const
    {
        static const TCHAR* cszDeviceType = TEXT("Function");
        return cszDeviceType;
    }
#endif // DEBUG
    CFunction&operator=(CFunction&);

    PVOID           m_lpvDetachId;
    // for blocking on implicit transfers
    HANDLE          m_hFunctionFeatureEvent;
    CHcd * const    m_pCHcd;
    PVOID           m_pLogicalDevice;
};

#endif /* CDEVICE_HPP */