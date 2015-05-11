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

#include "SDHCPci.h"
#include <cebuscfg.h>


#ifndef SHIP_BUILD
#define STR_MODULE _T("CSDHCPci::")
#define SETFNAME(name) LPCTSTR pszFname = STR_MODULE name _T(":"); DBG_UNREFERENCED_LOCAL_VARIABLE(pszFname)
#else
#define SETFNAME(name)
#endif


// Read the PCI "slot info" register.
// Returns FALSE if device is not PCI or read failure
BOOL
CSDHCPci::ReadPciSlotInfoReg(
                             PDDKWINDOWINFO  pwini,
                             PBYTE           pbSlotInfo
                             )
{
    PREFAST_DEBUGCHK(pwini);
    PREFAST_DEBUGCHK(pbSlotInfo);

    BOOL fRet = FALSE;

    // Read PCI information (if available)
    if (pwini->dwInterfaceType == PCIBus) {        
        DDKPCIINFO pcii;
        pcii.cbSize = sizeof(pcii);
        if (DDKReg_GetPciInfo(m_regDevice, &pcii) == ERROR_SUCCESS) {
            PCI_SLOT_NUMBER pciSlotNumber;
            pciSlotNumber.u.bits.DeviceNumber = pcii.dwDeviceNumber;
            pciSlotNumber.u.bits.FunctionNumber = pcii.dwFunctionNumber;

            // Pass a DWORD since it will write at minimum 4 bytes to the buffer.
            DWORD dwSlotInfo;
            DWORD cbRead = GetDeviceConfigurationData(m_hBusAccess, 
                PCI_WHICHSPACE_CONFIG, pwini->dwBusNumber, 
                pciSlotNumber.u.AsULONG, SDHC_PCI_SLOT_INFO, 
                sizeof(dwSlotInfo), &dwSlotInfo);

            if (cbRead) {
                *pbSlotInfo = LOBYTE(dwSlotInfo);
                fRet = TRUE;
            }
        }
    }

    return fRet;
}


DWORD 
CSDHCPci::DetermineFirstSlotWindow(
                                   PDDKWINDOWINFO pwini
                                   )
{
    PREFAST_DEBUGCHK(pwini);

    DWORD dwSlotZeroWindow;
    BYTE bSlotInfo;

    if (ReadPciSlotInfoReg(pwini, &bSlotInfo)) {
        dwSlotZeroWindow = SDHC_PCI_FIRST_WINDOW(bSlotInfo);
        DEBUGCHK((DWORD)SDHC_PCI_NUMBER_OF_SLOTS(bSlotInfo) >= m_cSlots);
    }
    else {
        // Use the default method
        dwSlotZeroWindow = CSDHCBase::DetermineFirstSlotWindow(pwini);
    }

    return dwSlotZeroWindow; 
}


// Determine the number of slots on this controller.
DWORD
CSDHCPci::DetermineSlotCount(
                             )
{
    SETFNAME(_T("DetermineSlotCount"));

    DWORD cSlots = 0;

    // Read window information
    DDKWINDOWINFO wini;
    wini.cbSize = sizeof(wini);
    DWORD dwStatus = DDKReg_GetWindowInfo(m_regDevice, &wini);
    if (dwStatus == ERROR_SUCCESS) {
        BYTE bSlotInfo;
        if (ReadPciSlotInfoReg(&wini, &bSlotInfo)) {
            cSlots = SDHC_PCI_NUMBER_OF_SLOTS(bSlotInfo);
        }
        else {
            // Use the default method
            cSlots = CSDHCBase::DetermineSlotCount();
        }
    }
    else {
        DEBUGMSG(SDCARD_ZONE_ERROR, (_T("%s Error getting window information\r\n"),
            pszFname));
    }

    return cSlots;
}


extern "C"
PCSDHCBase
CreateSDHCPciObject(
                    )
{
    return new CSDHCPci;
}


VOID 
CSDHCPci::DestroySDHCPciObject(
                     PCSDHCBase pSDHC
                     )
{
    DEBUGCHK(pSDHC);
    delete pSDHC;
}

