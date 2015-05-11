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
#include <SDHC.h>

#ifndef _SDHCPCI_H_
#define _SDHCPCI_H_

typedef class CSDHCPci : public CSDHCBase {
public:
    // Constructor
    CSDHCPci() : CSDHCBase() {}

    // Destructor
    virtual ~CSDHCPci() {}

    virtual LPSDHC_DESTRUCTION_PROC GetDestructionProc() {
        return &DestroySDHCPciObject;
    }

    static VOID DestroySDHCPciObject(PCSDHCBase pSDHC);

protected:
    // Read the SD-defined slot info register in PCI configuration space.
    virtual BOOL ReadPciSlotInfoReg(PDDKWINDOWINFO pwini, PBYTE pbSlotInfo);


    // ---- Overrides of the base class ----

    // Get the slot count from the PCI slot info register.
    virtual DWORD DetermineSlotCount();

    // Get the first slot window index from the PCI slot info register.
    virtual DWORD DetermineFirstSlotWindow(PDDKWINDOWINFO pwini);
} *PCSDHCPci;


// PCI Defines
#define SDHC_PCI_SLOT_INFO            0x40
#define SDHC_PCI_NUMBER_OF_SLOTS(x)   ((((x) & 0x70) >> 4) + 1)
#define SDHC_PCI_FIRST_WINDOW(x)      ((x) & 0x07)

#endif // _SDHCPCI_H_

