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
// File: system.c
//
// Description:
// 
//     Device dependant part of the USB eXtensible Host Controller Driver(XHCD).
//----------------------------------------------------------------------------------

#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>
#include <ddkreg.h>
#include <devload.h>
#include <giisr.h>
#include <hcdddsi.h>
#include <cebuscfg.h>

#define REG_PHYSICAL_PAGE_SIZE TEXT("PhysicalPageSize")
#define REG_HSIC_PORT_NUNMBER  (TEXT("HSICPortNumber"))

// Amount of memory to use for HCD buffer
#define GC_TOTAL_AVILABLE_MEMORY (0x50000) // 320K
// 1/4 as high priority
#define GC_HIGH_PRIORITY_MEMORY (GC_TOTAL_AVILABLE_MEMORY / 4)

#pragma warning(push)
#pragma warning(disable:4214)
typedef struct
{
    DWORD dwAddr64Bit:1;
    DWORD dwBWNeg:1;
    DWORD dwCSZ:1;
    DWORD dwPPC:1;
    DWORD dwPIND:1;
    DWORD dwLHRC:1;
    DWORD dwLTC:1;
    DWORD dwNSS:1;
    DWORD dwReserved1:4;
    DWORD dwNaxPSASize:4;
    DWORD dwXHCIExtCapPointer:16;
} HCCP_CAP_BIT;

typedef union
{
    volatile HCCP_CAP_BIT   bit;
    volatile DWORD          dwReg;
} HCCP_CAP;

typedef struct
{
    DWORD dwCapID:8;
    DWORD dwNextCapPointer:8;
    DWORD dwHcBIOSOwned:1;
    DWORD dwReserved1:7;
    DWORD dwHcOSOwned:1;
    DWORD dwReserved2:7;
} USBLEGSUP_BIT;

#pragma warning(pop)
typedef union
{
    volatile USBLEGSUP_BIT  bit;
    volatile DWORD          dwReg;
} USBLEGSUP;

typedef struct _SXHCDPDD
{
    LPVOID              lpvMemoryObject;
    LPVOID              lpvXHCDMddObject;
    PVOID               pvVirtualAddress;   // DMA buffers as seen by the CPU
    DWORD               dwPhysicalMemSize;
    PHYSICAL_ADDRESS    paLogicalAddress;   // DMA buffers as seen by the DMA controller and bus interfaces
    DMA_ADAPTER_OBJECT  daoAdapterObject;
    TCHAR               szDriverRegKey[MAX_PATH];
    PUCHAR              pIoPortBase;
    DWORD               dwSysIntr;
    CRITICAL_SECTION    csPdd;              // serializes access to the PDD object
    HANDLE              hIsrHandle;
    HANDLE              hParentBusHandle;    
} SXHCDPDD;

#define HOST_OWNED    (1 << 16)
#define MAX_TIMEOUT   (1000)

#define INTEL_VENDOR_ID   (0x8086)
#define INTEL_DEVICE_ID_1 (0x1e31)
#define INTEL_DEVICE_ID_2 (0x0f35)

#define USB_XUSB2_PORT_ROUTING      (0xD0)
#define USB_USB2_PORT_ROUTING_MASK  (0xD4)
#define USB_USB3_PORT_SS_ENABLE     (0xD8)
#define USB3_PORT_ROUTING_MASK      (0xDC)

//Offset of the HCCPARAMS register 
#define USB_XHCD_HCCPARAMS    0x10   

#define BITS_IN_DWORD         32

//----------------------------------------------------------------------------------
// Function: HcdPdd_DllMain
//
// Description: DLL Entry point
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
// Returns: TRUE
//----------------------------------------------------------------------------------
extern BOOL HcdPdd_DllMain(HANDLE hInstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(hInstDLL);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpvReserved);
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: GetRegistryPhysicalMemSize
//
// Description: Get physical memory from registry
//
// Parameters:  RegKeyPath - driver registry key path
//
//              lpdwPhyscialMemSize - base address
//
// Returns: TRUE if operation is succeeded, else FALSE
//----------------------------------------------------------------------------------
static BOOL GetRegistryPhysicalMemSize(LPCWSTR RegKeyPath,
                                        DWORD * lpdwPhyscialMemSize)
{
    HKEY hKey;
    DWORD dwData;
    DWORD dwSize;
    DWORD dwType;
    BOOL  fRet = FALSE;
    DWORD dwRet;

    // Open key
    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegKeyPath, 0, 0, &hKey);
    if(dwRet != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("!XHCD:GetRegistryConfig RegOpenKeyEx(%s) failed %d\r\n"),
            RegKeyPath,
            dwRet));
        return FALSE;
    }

    // Read base address, range from registry and determine IOSpace
    dwSize = sizeof(dwData);
    dwRet = RegQueryValueEx(hKey,
                            REG_PHYSICAL_PAGE_SIZE,
                            0,
                            &dwType,
                            (PUCHAR)&dwData,
                            &dwSize);
    if(dwRet == ERROR_SUCCESS)
    {
        if(lpdwPhyscialMemSize)
        {
            *lpdwPhyscialMemSize = dwData;
        }
        fRet=TRUE;
    }
    RegCloseKey(hKey);
    return fRet;
}

//----------------------------------------------------------------------------------
// Function: GetRegistryHSICPortNumber
//
// Description: Get HSIC port number from registry
//
// Parameters:  RegKeyPath - driver registry key path
//
//              lpdwHSICPortNumber - HSIC Port Number
//
// Returns: TRUE if succeeded, else FALSE
//----------------------------------------------------------------------------------
static BOOL GetRegistryHSICPortNumber(LPCWSTR RegKeyPath,
                                        DWORD * lpdwHSICPortNumber)
{
    HKEY hKey;
    DWORD dwData;
    DWORD dwSize;
    DWORD dwType;
    BOOL  fRet = FALSE;
    DWORD dwRet;

    // Open key
    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegKeyPath, 0, 0, &hKey);
    if(dwRet != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("!XHCD:GetRegistryConfig RegOpenKeyEx(%s) failed %d\r\n"),
            RegKeyPath,
            dwRet));
        return FALSE;
    }

    // Read HSIC port number from registry
    dwSize = sizeof(dwData);
    dwRet = RegQueryValueEx(hKey,
                            REG_HSIC_PORT_NUNMBER,
                            0,
                            &dwType,
                            (PUCHAR)&dwData,
                            &dwSize);
    if(dwRet == ERROR_SUCCESS)
    {
        if(lpdwHSICPortNumber)
        {
            *lpdwHSICPortNumber = dwData;
        }
        fRet=TRUE;
    } else {
        if(lpdwHSICPortNumber)
        {
            *lpdwHSICPortNumber = 0;
        }
    }
    RegCloseKey(hKey);
    return fRet;
}


//----------------------------------------------------------------------------------
// Function: ConfigureXHCICard
//
// Description: translate a bus address to a virtual system address
//
// Parameters:  pPddObject - contains PDD reference pointer.
//
//              pIoPortBase - IN: contains physical address of register base
//                            OUT: contains virtual address of register base
//
//              dwAddrLen - Number of bytes to map on the device
//
//              dwIOSpace - Flag to indicate whether what this function maps to is in I/O space or 
//                          memory space.
//
//              ifcType   - Bus type specified by an element of INTERFACE_TYPE.
//
//              dwBusNumber - Bus number where the device resides.
//
// Returns: TRUE if successful. Otherwise, it returns FALSE.
//----------------------------------------------------------------------------------
static BOOL ConfigureXHCICard(SXHCDPDD * pPddObject,
                        PUCHAR *pIoPortBase,
                        DWORD dwAddrLen,
                        DWORD dwIOSpace,
                        INTERFACE_TYPE ifcType,
                        DWORD dwBusNumber
                        )
{
    ULONG               dwInIoSpace = dwIOSpace;
    ULONG               dwPortBase;
    PHYSICAL_ADDRESS    paIoPhysicalBase = {0, 0};

    dwPortBase = (ULONG)*pIoPortBase;
    paIoPhysicalBase.LowPart = dwPortBase;

    if(!BusTransBusAddrToVirtual(pPddObject->hParentBusHandle,
                                    ifcType,
                                    dwBusNumber,
                                    paIoPhysicalBase,
                                    dwAddrLen,
                                    &dwInIoSpace,
                                    (PPVOID)pIoPortBase))
    {
        DEBUGMSG(ZONE_ERROR, (L"XHCD: Failed TransBusAddrToVirtual\r\n"));
        return FALSE;
    }

    DEBUGMSG(ZONE_INIT,
            (TEXT("XHCD: ioPhysicalBase 0x%X, IoSpace 0x%X\r\n"),
              paIoPhysicalBase.LowPart,
              dwInIoSpace));
    DEBUGMSG(ZONE_INIT,
            (TEXT("XHCD: pIoPortBase 0x%X, portBase 0x%X\r\n"),
              *pIoPortBase,
              dwPortBase));

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: EnableSuperSpeed
//
// Description: Ports can be switched from EHCI to XHCI controller.
//
// Parameters: hBus - Handle to the bus
//
//             slot - Pci slot number
//
//             dwi - address window information
//
//             dwHSICPortNumber - HSCI controller port number
//
// Returns: none
//----------------------------------------------------------------------------------
static VOID EnableSuperSpeed(HANDLE hBus, PCI_SLOT_NUMBER slot, DDKWINDOWINFO dwi, DWORD dwHSICPortNumber)
{
    UINT    uPortsAvailable;
    DWORD   dwLength;

    dwLength = GetDeviceConfigurationData(hBus,
                                          PCI_WHICHSPACE_CONFIG,
                                          dwi.dwBusNumber,
                                          slot.u.AsULONG,
                                          USB3_PORT_ROUTING_MASK,
                                          sizeof(uPortsAvailable),
                                          &uPortsAvailable);
    if (dwLength != sizeof(uPortsAvailable)) 
    {
        return;
    }

    dwLength = SetDeviceConfigurationData(hBus,
                                            PCI_WHICHSPACE_CONFIG,
                                            dwi.dwBusNumber,
                                            slot.u.AsULONG,
                                            USB_USB3_PORT_SS_ENABLE,
                                            sizeof(uPortsAvailable),
                                            &uPortsAvailable);
    if (dwLength != sizeof(uPortsAvailable)) 
    {
        return;
    }

    dwLength = GetDeviceConfigurationData(hBus,
                                            PCI_WHICHSPACE_CONFIG,
                                            dwi.dwBusNumber,
                                            slot.u.AsULONG,
                                            USB_USB3_PORT_SS_ENABLE,
                                            sizeof(uPortsAvailable),
                                            &uPortsAvailable);
    if (dwLength != sizeof(uPortsAvailable)) 
    {
        return;
    }

    dwLength = GetDeviceConfigurationData(hBus,
                                            PCI_WHICHSPACE_CONFIG,
                                            dwi.dwBusNumber,
                                            slot.u.AsULONG,
                                            USB_USB2_PORT_ROUTING_MASK,
                                            sizeof(uPortsAvailable),
                                            &uPortsAvailable);
    if (dwLength != sizeof(uPortsAvailable)) 
    {
        return;
    }

    if ((dwHSICPortNumber != 0) && (dwHSICPortNumber < BITS_IN_DWORD)) {
        uPortsAvailable &= ~(1 << (dwHSICPortNumber - 1));
    }

    dwLength = SetDeviceConfigurationData(hBus,
                                          PCI_WHICHSPACE_CONFIG,
                                          dwi.dwBusNumber,
                                          slot.u.AsULONG,
                                          USB_XUSB2_PORT_ROUTING,
                                          sizeof(uPortsAvailable),
                                          &uPortsAvailable);
    if (dwLength != sizeof(uPortsAvailable)) 
    {
        return;
    }

    GetDeviceConfigurationData(hBus,
                               PCI_WHICHSPACE_CONFIG,
                               dwi.dwBusNumber,
                               slot.u.AsULONG,
                               USB_XUSB2_PORT_ROUTING,
                               sizeof(uPortsAvailable),
                               &uPortsAvailable);
}

//----------------------------------------------------------------------------------
// Function: HandshakeHandle
//
// Description: Reading  a register until handshake completes
//
// Parameters: uAddr - address of register to be read
//
//             uMask - mask of read result
//
//             uDone - expected result 
//
//             iSec - timeout
//
// Returns:  0 if successful. Otherwise < 0.
//----------------------------------------------------------------------------------
static INT HandshakeHandle(UINT uAddr, UINT uMask, UINT uDone, INT iSec)
{
    UINT    uResult;
    UINT    uCount = iSec;

    if(uCount == 0)
    {
        uCount = 1;
    }

    do
    {
        uResult = READ_REGISTER_ULONG((PULONG)uAddr);

        // card removed
        if(uResult == ~(UINT)0)
        {
            return -2;
        }
        uResult &= uMask;
        if(uResult == uDone)
        {
            return 0;
        }
        Sleep(1);
        uCount--;
    } while(uCount > 0);
    return -1;
}

//----------------------------------------------------------------------------------
// Function: InitializeXHCI
//
// Description: Configure and initialize XHCI card
//
// Parameters:  pPddObject - Pointer to PDD structure
//
//              szDriverRegKey - Pointer to active registry key string
//
// Returns: Return TRUE if card could be located and configured, otherwise FALSE
//----------------------------------------------------------------------------------
static BOOL InitializeXHCI( SXHCDPDD * pPddObject, LPCWSTR szDriverRegKey)
{
    PUCHAR  pIoPortBase = NULL;
    DWORD   dwAddrLen;
    DWORD   dwIOSpace;
    BOOL    fResult = FALSE;
    LPVOID  pobMem = NULL;
    LPVOID  pobXHCD = NULL;
    DWORD   dwPhysAddr;
    DWORD   dwHPPhysicalMemSize = 0;
    HKEY    hKey;
    DWORD   dwLength = 0;

    PCI_SLOT_NUMBER slot;
    HANDLE          hBus;

    DWORD       dwOffset;
    HCCP_CAP    hccParam;
    USBLEGSUP   usbLegsUp;
    DWORD*      pdwAddr;
    DWORD       dwHSICPortNumber;

    DDKWINDOWINFO dwi;
    DDKISRINFO dii;
    DDKPCIINFO dpi;

    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                    szDriverRegKey,
                    0,
                    0,
                    &hKey) != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("InitializeXHCI:GetRegistryConfig RegOpenKeyEx(%s) failed\r\n"),
            szDriverRegKey));
        return FALSE;
    }

    dwi.cbSize = sizeof(dwi);
    dii.cbSize = sizeof(dii);
    dpi.cbSize = sizeof(dpi);

    if(DDKReg_GetWindowInfo(hKey, &dwi) != ERROR_SUCCESS)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("InitializeXHCI:DDKReg_GetWindowInfo failed\r\n")));
        goto InitializeXHCIError;
    }
    
    if(dwi.dwNumMemWindows != 0)
    {
        dwPhysAddr = dwi.memWindows[0].dwBase;
        dwAddrLen= dwi.memWindows[0].dwLen;
        dwIOSpace = 0;
    }
    else if(dwi.dwNumIoWindows != 0)
    {
        dwPhysAddr= dwi.ioWindows[0].dwBase;
        dwAddrLen = dwi.ioWindows[0].dwLen;
        dwIOSpace = 1;
    }
    else
    {
        goto InitializeXHCIError;
    }
        
    pIoPortBase = (PUCHAR)dwPhysAddr;

    fResult = ConfigureXHCICard(pPddObject,
                                &pIoPortBase,
                                dwAddrLen,
                                dwIOSpace,
                                dwi.dwInterfaceType,
                                dwi.dwBusNumber);
    if(!fResult)
    {
        goto InitializeXHCIError;
    }
    
    if(DDKReg_GetIsrInfo(hKey, &dii) == ERROR_SUCCESS  &&
                                    dii.szIsrDll[0] != 0 &&
                                    dii.szIsrHandler[0] !=0 &&
                                    dii.dwIrq < 0xff && dii.dwIrq > 0)
    {
        // Install ISR handler
        pPddObject->hIsrHandle = LoadIntChainHandler(dii.szIsrDll,
                                                        dii.szIsrHandler,
                                                        (BYTE)dii.dwIrq);

        if(!pPddObject->hIsrHandle)
        {
            DEBUGMSG(ZONE_ERROR,
                (L"XHCD: Couldn't install ISR handler\r\n"));
        }
        else
        {
            GIISR_INFO info;
            PHYSICAL_ADDRESS paPortAddress = {0};
            paPortAddress.LowPart = dwPhysAddr;
            
            DEBUGMSG(ZONE_INIT,
                (L"XHCD: Installed ISR handler, Dll = '%s', Handler = '%s', Irq = %d\r\n",
                dii.szIsrDll,
                dii.szIsrHandler,
                dii.dwIrq));
            
            if(!BusTransBusAddrToStatic(pPddObject->hParentBusHandle,
                                        dwi.dwInterfaceType,
                                        dwi.dwBusNumber,
                                        paPortAddress,
                                        dwAddrLen,
                                        &dwIOSpace,
                                        &(PVOID)dwPhysAddr))
            {
                DEBUGMSG(ZONE_ERROR,(L"XHCD: Failed TransBusAddrToStatic\r\n"));
                goto InitializeXHCIError;
            }
        
            // Set up ISR handler
            info.SysIntr = dii.dwSysintr;
            info.CheckPort = TRUE;
            if(dwIOSpace)
            {
                info.PortIsIO = TRUE;
            }
            else
            {
                info.PortIsIO = FALSE;
            }
            info.UseMaskReg = FALSE;
            info.PortAddr = dwPhysAddr + *pIoPortBase + 0x04;
            info.PortSize = sizeof(DWORD);
            info.Mask = 0x08;

            if(!KernelLibIoControl(pPddObject->hIsrHandle,
                                    IOCTL_GIISR_INFO,
                                    &info,
                                    sizeof(info),
                                    NULL,
                                    0,
                                    NULL))
            {
                DEBUGMSG(ZONE_ERROR,
                    (L"XHCD: KernelLibIoControl call failed.\r\n"));
            }
        }
    }

    DEBUGMSG(ZONE_INIT,
        (TEXT("XHCD: Read config from registry: \
              Base Address: 0x%X, Length: 0x%X, \
              SysIntr: 0x%X, Interface Type: %u, Bus Number: %u\r\n"),
              dwPhysAddr,
              dwAddrLen,
              dii.dwSysintr,dwi.dwInterfaceType,
              dwi.dwBusNumber));

    if(!GetRegistryHSICPortNumber (szDriverRegKey, &dwHSICPortNumber))
    {
        dwHSICPortNumber = 0;
    }

    // Ask pre-OS software to release the control of the XHCI hardware.
    if(dwi.dwInterfaceType == PCIBus)
    {
        if(DDKReg_GetPciInfo(hKey, &dpi) == ERROR_SUCCESS)
        {
            slot.u.AsULONG = 0;
            slot.u.bits.DeviceNumber = dpi.dwDeviceNumber;
            slot.u.bits.FunctionNumber = dpi.dwFunctionNumber;

            hccParam.dwReg =
                READ_REGISTER_ULONG((PULONG)(pIoPortBase + USB_XHCD_HCCPARAMS));
            dwOffset = hccParam.bit.dwXHCIExtCapPointer;

            pdwAddr = (DWORD*)pIoPortBase;
            pdwAddr += dwOffset;
            
            while(1)
            {
                if (((PUCHAR)pdwAddr < pIoPortBase) || ((PUCHAR)pdwAddr > (pIoPortBase + dwAddrLen)))
                {
                    goto InitializeXHCIError;
                }

                usbLegsUp.dwReg = READ_REGISTER_ULONG((PULONG) pdwAddr);

                if(usbLegsUp.bit.dwCapID == 1)
                {
                    break;
                }

                dwOffset = usbLegsUp.bit.dwNextCapPointer;
                if(!dwOffset)
                {
                    break;
                }

                pdwAddr += dwOffset;
            }

            if(usbLegsUp.bit.dwHcBIOSOwned == 1)
            {
                INT iTimeout;

                usbLegsUp.bit.dwHcOSOwned = 1;
                
                WRITE_REGISTER_ULONG((PULONG)pdwAddr, usbLegsUp.dwReg);

                iTimeout = HandshakeHandle((UINT)pdwAddr,
                                            HOST_OWNED,
                                            0,
                                            MAX_TIMEOUT);    

                if(iTimeout)
                {
                    DEBUGMSG(ZONE_INIT,
                        (TEXT("xHCI handoff failed %08x\n"),
                        usbLegsUp.dwReg));
                    usbLegsUp.bit.dwHcBIOSOwned = 0;
                    WRITE_REGISTER_ULONG((PULONG)pdwAddr, usbLegsUp.dwReg);
                }
            }

            usbLegsUp.dwReg = READ_REGISTER_ULONG((PULONG)pdwAddr);

            hBus = CreateBusAccessHandle(szDriverRegKey);
            if(hBus)
            {
                USHORT  wVendor = 0;
                USHORT  wDevice = 0;

                dwLength = GetDeviceConfigurationData(hBus,
                                            PCI_WHICHSPACE_CONFIG,
                                            dwi.dwBusNumber,
                                            slot.u.AsULONG,
                                            0x2,
                                            sizeof(wDevice),
                                            &wDevice);
                if (dwLength == sizeof(wDevice)) 
                {
                    dwLength = GetDeviceConfigurationData(hBus,
                                            PCI_WHICHSPACE_CONFIG,
                                            dwi.dwBusNumber,
                                            slot.u.AsULONG,
                                            0x0,
                                            sizeof(wVendor),
                                            &wVendor);

                    if (dwLength == sizeof(wVendor) && 
                        wVendor == INTEL_VENDOR_ID &&
                        (wDevice == INTEL_DEVICE_ID_1 || 
                         wDevice == INTEL_DEVICE_ID_2))
                    {
                        EnableSuperSpeed(hBus, slot, dwi, dwHSICPortNumber);
                    }
                }
                CloseBusAccessHandle(hBus);
            }
        }
    }

    // The PDD can supply a buffer of contiguous physical memory here, or can let
    // the MDD try to allocate the memory from system RAM.  We will use the
    // HalAllocateCommonBuffer() API to allocate the memory and bus controller
    // physical addresses and pass this information into the MDD.
    if(GetRegistryPhysicalMemSize(szDriverRegKey, &pPddObject->dwPhysicalMemSize))
    {
        // A quarter for High priority Memory.
        dwHPPhysicalMemSize = pPddObject->dwPhysicalMemSize / 4;
        // Align with page size.        
        pPddObject->dwPhysicalMemSize =
            (pPddObject->dwPhysicalMemSize + PAGE_SIZE -1) & ~(PAGE_SIZE -1);
        dwHPPhysicalMemSize =
            ((dwHPPhysicalMemSize +  PAGE_SIZE -1) & ~(PAGE_SIZE -1));
    }
    else
    {
        pPddObject->dwPhysicalMemSize=0;
    }

    // Setup Minimun requirement.
    if(pPddObject->dwPhysicalMemSize < GC_TOTAL_AVILABLE_MEMORY)
    {
        pPddObject->dwPhysicalMemSize = GC_TOTAL_AVILABLE_MEMORY;
        dwHPPhysicalMemSize = GC_HIGH_PRIORITY_MEMORY;
    }

    pPddObject->daoAdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
    pPddObject->daoAdapterObject.InterfaceType = dwi.dwInterfaceType;
    pPddObject->daoAdapterObject.BusNumber = dwi.dwBusNumber;

    if((pPddObject->pvVirtualAddress =
        HalAllocateCommonBuffer(&pPddObject->daoAdapterObject,
                                pPddObject->dwPhysicalMemSize,
                                &pPddObject->paLogicalAddress,
                                FALSE)) == NULL)
    {
        goto InitializeXHCIError;
    }

    pobMem =
        HcdMdd_CreateMemoryObject(pPddObject->dwPhysicalMemSize,
                                dwHPPhysicalMemSize,
                                (PUCHAR)pPddObject->pvVirtualAddress,
                                (PUCHAR)pPddObject->paLogicalAddress.LowPart);
    if(!pobMem)
    {
        goto InitializeXHCIError;
    }

    pobXHCD = HcdMdd_CreateHcdObject(pPddObject,
                                    pobMem,
                                    szDriverRegKey,
                                    pIoPortBase,
                                    dii.dwSysintr);
    if(!pobXHCD)
    {
        goto InitializeXHCIError;
    }

    pPddObject->lpvMemoryObject = pobMem;
    pPddObject->lpvXHCDMddObject = pobXHCD;
    VERIFY(SUCCEEDED(StringCchCopy(pPddObject->szDriverRegKey,
                                    MAX_PATH,
                                    szDriverRegKey)));
    pPddObject->pIoPortBase = pIoPortBase;
    pPddObject->dwSysIntr = dii.dwSysintr;
    
    if(hKey != NULL)
    {
        DWORD dwCapability;
        DWORD dwType;
        dwLength = sizeof(DWORD);
        if(RegQueryValueEx(hKey,
                            HCD_CAPABILITY_VALNAME,
                            0,
                            &dwType,
                            (PUCHAR)&dwCapability,
                            &dwLength) == ERROR_SUCCESS)
        {
            HcdMdd_SetCapability(pobXHCD,dwCapability);
        }
        RegCloseKey(hKey);
    }

    return TRUE;

InitializeXHCIError:
    if(pPddObject->hIsrHandle)
    {
        FreeIntChainHandler(pPddObject->hIsrHandle);
        pPddObject->hIsrHandle = NULL;
    }
    
    if(pobXHCD)
    {
        HcdMdd_DestroyHcdObject(pobXHCD);
    }
    if(pobMem)
    {
        HcdMdd_DestroyMemoryObject(pobMem);
    }
    if(pPddObject->pvVirtualAddress)
    {
        HalFreeCommonBuffer(&pPddObject->daoAdapterObject,
                            pPddObject->dwPhysicalMemSize,
                            pPddObject->paLogicalAddress,
                            pPddObject->pvVirtualAddress,
                            FALSE);
    }

    pPddObject->lpvMemoryObject = NULL;
    pPddObject->lpvXHCDMddObject = NULL;
    pPddObject->pvVirtualAddress = NULL;
    if(hKey != NULL)
    {
        RegCloseKey(hKey);
    }

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Init
//
// Description: PDD Entry point - called at system init to detect and configure
//              XHCI card.
//
// Parameters: dwContext - Pointer to context value. For device.exe, this is
//              a string indicating our active registry key.
//
// Returns: Return pointer to PDD specific data structure, or NULL if error.
//----------------------------------------------------------------------------------
extern DWORD HcdPdd_Init(DWORD dwContext)
{
    SXHCDPDD *  pPddObject = malloc(sizeof(SXHCDPDD));
    BOOL        fRet = FALSE;

    if(pPddObject)
    {
        pPddObject->pvVirtualAddress = NULL;
        InitializeCriticalSection(&pPddObject->csPdd);
        pPddObject->hIsrHandle = NULL;
        pPddObject->hParentBusHandle = CreateBusAccessHandle((LPCWSTR)g_dwContext);
        
        fRet = InitializeXHCI(pPddObject, (LPCWSTR)dwContext);

        if(!fRet)
        {
            if(pPddObject->hParentBusHandle)
            {
                CloseBusAccessHandle(pPddObject->hParentBusHandle);
            }

            DeleteCriticalSection(&pPddObject->csPdd);
            free(pPddObject);
            pPddObject = NULL;
        }
    }

    return(DWORD)pPddObject;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_CheckConfigPower
//
// Description: Check power required by specific device configuration and return
//              whether it can be supported on this platform.
//              For CEPC, this is trivial, just limit to the 500mA requirement
//              of USB. For battery powered devices, this could be more
//              sophisticated, taking into account current battery status or
//              other info.
//
// Parameters:  bPort - Port number
//
//              dwCfgPower - Power required by configuration
//
//              dwTotalPower - Total power currently in use on port
//
// Returns: Return TRUE if configuration can be supported, FALSE if not.
//----------------------------------------------------------------------------------
extern BOOL HcdPdd_CheckConfigPower(UCHAR bPort,
                                    DWORD dwCfgPower,
                                    DWORD dwTotalPower)
{
    UNREFERENCED_PARAMETER(bPort);
    if((dwCfgPower + dwTotalPower) > 500)
    {
        return FALSE;
    }

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_PowerUp
//
// Description: power up handler
//
// Parameters: hDeviceContext - Pointer to context value.
//
// Returns: none
//----------------------------------------------------------------------------------
extern VOID HcdPdd_PowerUp(DWORD hDeviceContext)
{
    SXHCDPDD * pPddObject =(SXHCDPDD *)hDeviceContext;
    if (pPddObject != NULL) 
    {
        HcdMdd_PowerUp(pPddObject->lpvXHCDMddObject);
    }
    return;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_PowerDown
//
// Description: power down handler
//
// Parameters: hDeviceContext - Pointer to context value.
//
// Returns: none
//----------------------------------------------------------------------------------
extern VOID HcdPdd_PowerDown(DWORD hDeviceContext)
{
    SXHCDPDD * pPddObject =(SXHCDPDD *)hDeviceContext;
    if (pPddObject != NULL) 
    {
        HcdMdd_PowerDown(pPddObject->lpvXHCDMddObject);
    }
    return;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Deinit
//
// Description: deinitialize host controller driver
//
// Parameters: hDeviceContext - Pointer to context value.
//
// Returns: TRUE if successful, FALSE if error occurs.
//----------------------------------------------------------------------------------
extern BOOL HcdPdd_Deinit(DWORD hDeviceContext)
{
    SXHCDPDD * pPddObject =(SXHCDPDD *)hDeviceContext;

    if (pPddObject == NULL) 
    {
        return FALSE;
    }
    if(pPddObject->lpvXHCDMddObject)
    {
        HcdMdd_DestroyHcdObject(pPddObject->lpvXHCDMddObject);
    }
    if(pPddObject->lpvMemoryObject)
    {
        HcdMdd_DestroyMemoryObject(pPddObject->lpvMemoryObject);
    }
    if(pPddObject->pvVirtualAddress)
    {
        HalFreeCommonBuffer(&pPddObject->daoAdapterObject,
                            pPddObject->dwPhysicalMemSize,
                            pPddObject->paLogicalAddress,
                            pPddObject->pvVirtualAddress,
                            FALSE);
    }

    if(pPddObject->hIsrHandle)
    {
        FreeIntChainHandler(pPddObject->hIsrHandle);
        pPddObject->hIsrHandle = NULL;
    }
    
    if(pPddObject->hParentBusHandle)
    {
        CloseBusAccessHandle(pPddObject->hParentBusHandle);
    }
    
    free(pPddObject);
    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Open
//
// Description: This is the standard stream interface driver open routine.
//
// Parameters: hDeviceContext - Pointer to context value.
//
//             dwAccessCode - Unused
//
//             dwShareMode - Unused
//
// Returns: pointer to MDD object
//----------------------------------------------------------------------------------
extern DWORD HcdPdd_Open(DWORD hDeviceContext,
                        DWORD dwAccessCode,
                        DWORD dwShareMode)
{
    UNREFERENCED_PARAMETER(hDeviceContext);
    UNREFERENCED_PARAMETER(dwAccessCode);
    UNREFERENCED_PARAMETER(dwShareMode);

    return(DWORD)(((SXHCDPDD*)hDeviceContext)->lpvXHCDMddObject);
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Close
//
// Description: This is the standard stream interface driver close routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
// Returns: TRUE.
//----------------------------------------------------------------------------------
extern BOOL HcdPdd_Close(DWORD hOpenContext)
{
    UNREFERENCED_PARAMETER(hOpenContext);

    return TRUE;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Read
//
// Description: HCD read routine. This is the standard stream interface driver read 
//              routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             pBuffer - addres of destination buffer 
//
//             dwCount - number of read bytes
//
// Returns: -1.
//----------------------------------------------------------------------------------
extern DWORD HcdPdd_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD dwCount)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(pBuffer);
    UNREFERENCED_PARAMETER(dwCount);

    // an error occured
    return(DWORD)-1;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Write
//
// Description: HCD write routine. This is the standard stream interface 
//      driver write routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             pSourceBytes - address of source buffer 
//
//             dwNumberOfBytes - number of bytes to write
//
// Returns: -1.
//
// Returns:
//----------------------------------------------------------------------------------
extern DWORD HcdPdd_Write(DWORD hOpenContext,
                        LPCVOID pSourceBytes,
                        DWORD dwNumberOfBytes)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(pSourceBytes);
    UNREFERENCED_PARAMETER(dwNumberOfBytes);

    return(DWORD)-1;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_Seek
//
// Description: HCD seek routine.
//
// Parameters: hOpenContext - Pointer to device context returned from <f HcdPdd_Open>
//
//             lAmount - position to seek to (relative to type)
//
//             dwType - type of seek
//
// Returns: -1
//----------------------------------------------------------------------------------
extern DWORD HcdPdd_Seek(DWORD hOpenContext, LONG lAmount, DWORD dwType)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(lAmount);
    UNREFERENCED_PARAMETER(dwType);

    return(DWORD)-1;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_IOControl
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
//             pdwActualOut - actual number of bytes received from device
//
// Returns: FALSE
//----------------------------------------------------------------------------------
extern BOOL HcdPdd_IOControl(DWORD hOpenContext,
                            DWORD dwCode,
                            PBYTE pBufIn,
                            DWORD dwLenIn,
                            PBYTE pBufOut,
                            DWORD dwLenOut,
                            PDWORD pdwActualOut)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(dwCode);
    UNREFERENCED_PARAMETER(pBufIn);
    UNREFERENCED_PARAMETER(dwLenIn);
    UNREFERENCED_PARAMETER(pBufOut);
    UNREFERENCED_PARAMETER(dwLenOut);
    UNREFERENCED_PARAMETER(pdwActualOut);

    return FALSE;
}

//----------------------------------------------------------------------------------
// Function: HcdPdd_InitiatePowerUp
//
// Description: Manage WinCE suspend/resume events
//              This gets called by the MDD's IST when it detects a power resume.
//              By default it has nothing to do.
//
// Parameters: hDeviceContext - Pointer to context value.
//
// Returns: None
//----------------------------------------------------------------------------------
extern VOID HcdPdd_InitiatePowerUp(DWORD hDeviceContext)
{
    UNREFERENCED_PARAMETER(hDeviceContext);
    return;
}