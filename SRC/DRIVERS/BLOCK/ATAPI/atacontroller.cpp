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

/*++

Module Name:
    ATAController.cpp

Abstract:
    Generic IDE/ATA Controller device support.

Revision History:
    Created 21 June 2006 ZohebV

--*/

#include <atamain.h>
#include <giisr.h>
#include <ceddk.h>


class CATAController: public CIDEController
{
private:
    int m_numPorts;
public:
    virtual BOOL Init(PIDEREG p_idereg)
    {
        ASSERT(p_idereg);
        m_numPorts = -1;
        m_pPort[ATA_PRIMARY] = new CPort(this);
        if (!m_pPort[ATA_PRIMARY]) return FALSE;
        m_pIdeReg = p_idereg;
        m_pPort[ATA_SECONDARY] = new CPort(this);
        if (!m_pPort[ATA_SECONDARY]) {
            delete m_pPort[ATA_PRIMARY];
            m_pPort[ATA_PRIMARY] = NULL;
            return FALSE;
        }
        m_bisIOMapped = TRUE;
        return true;
    }

    // This function reads the I/O window data from the IDE instance key and builds
    // the I/O ports for the primary and secondary IDE controller channels

#ifdef DEBUG
    virtual BOOL MapIORegisters(HKEY hDevKey
                              , PTSTR szDevKey
                              )
#else
    virtual BOOL MapIORegisters(HKEY hDevKey
                              , PTSTR //szDevKey
                              )
#endif
    {
        CIDEBUS *pBus = this;
        BOOL fRet = FALSE;
        BOOL isIOMapped = TRUE;

        DEBUGCHK(pBus);
        DEBUGCHK(pBus->m_pPort[ATA_PRIMARY]);
        DEBUGCHK(pBus->m_pPort[ATA_SECONDARY]);

        // fetch the IDE/ATA channel's I/O window; a channel can contain a single device
        // or a master and a slave device; each device requires a device control I/O
        // window and an alternate status I/O window; if the IDE/ATA controller supports
        // bus mastering, then a bus master I/O window will be present

        if (
            (!AtaGetRegistryResources(hDevKey, &pBus->m_dwi)) ||
            (pBus->m_dwi.dwNumIoWindows < 2)
        ) {
            DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                "Atapi!BuildPortSet> Resource configuration missing or invalid in device key(%s)\r\n"
                ), szDevKey));
            goto exit;
        }

        // save the base virtual addresses of the device control (RegBase)
        // and alternate status (RegAlt) I/O windows
    #ifdef I830M4
        if (!AtaGetRegistryPciInfo(hDevKey, &m_ppi)) {
            DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                "Atapi!BuildPortSet> Resource configuration missing or invalid in device key(%s)\r\n"
                ), szDevKey));
            goto exit;
        }

        if (pBus->m_pIdeReg->dwLegacyIRQ != (BYTE)-1) {
            // ICH only uses BARs when in Native IDE mode, otherwise it uses legacy addressing
            pBus->m_dwi.ioWindows[0].dwBase = 0x1F0;
            pBus->m_dwi.ioWindows[1].dwBase = 0x3F4;
        }

    #endif

        isIOMapped = TRUE;
        pBus->m_pPort[ATA_PRIMARY]->m_dwRegBase   = DoIoTranslation(&pBus->m_dwi, 0,&isIOMapped);
        pBus->m_bisIOMapped                       = isIOMapped;
        isIOMapped = TRUE;
        pBus->m_pPort[ATA_PRIMARY]->m_dwRegAlt    = DoIoTranslation(&pBus->m_dwi, 1,&isIOMapped);
        DEBUGCHK(pBus->m_bisIOMapped == isIOMapped); // If one register is IO/memory mapped so are all others
        pBus->m_pPort[ATA_PRIMARY]->m_fInitialized = TRUE;

        if (pBus->m_dwi.dwNumIoWindows >= 4) {

            // this channel supports a primary and secondary IDE/ATA channels; save
            // the base virtual addresses of the secondary channel's devoce control
            // and alternate status I/O windows

    #ifdef I830M4
        if (pBus->m_pIdeReg->dwLegacyIRQ != (BYTE)-1) {
            // ICH only uses BARs when in Native IDE mode, otherwise it uses legacy addressing
            pBus->m_dwi.ioWindows[2].dwBase = 0x170;
            pBus->m_dwi.ioWindows[3].dwBase = 0x374;
        }
    #endif

            isIOMapped = TRUE;
            pBus->m_pPort[ATA_SECONDARY]->m_dwRegBase   = DoIoTranslation(&pBus->m_dwi, 2,&isIOMapped);
            DEBUGCHK(pBus->m_bisIOMapped == isIOMapped); // If one register is IO/memory mapped so are all others
            isIOMapped = TRUE;
            pBus->m_pPort[ATA_SECONDARY]->m_dwRegAlt    = DoIoTranslation(&pBus->m_dwi, 3,&isIOMapped);
            DEBUGCHK(pBus->m_bisIOMapped == isIOMapped); // If one register is IO/memory mapped so are all others
            isIOMapped = TRUE;
            pBus->m_pPort[ATA_SECONDARY]->m_fInitialized = TRUE;

        }
        if (pBus->m_dwi.dwNumIoWindows >= 5) {

            // the IDE/ATA controller supports bus mastering; save the base virtual
            // address of each channel's bus master I/O window

            DEBUGCHK(pBus->m_dwi.ioWindows[4].dwLen >= 16);

            isIOMapped = TRUE;
            pBus->m_pPort[ATA_PRIMARY]->m_dwBMR = DoIoTranslation(&pBus->m_dwi, 4,&isIOMapped);
            DEBUGCHK(pBus->m_bisIOMapped == isIOMapped); // If one register is IO/memory mapped so are all others
            isIOMapped = TRUE;
            pBus->m_pPort[ATA_PRIMARY]->m_dwBMRStatic= DoStaticTranslation(&pBus->m_dwi, 4,&isIOMapped);
            DEBUGCHK(pBus->m_bisIOMapped == isIOMapped); // If one register is IO/memory mapped so are all others
            pBus->m_pPort[ATA_SECONDARY]->m_dwBMR = pBus->m_pPort[ATA_PRIMARY]->m_dwBMR + 8;
            pBus->m_pPort[ATA_SECONDARY]->m_dwBMRStatic = pBus->m_pPort[ATA_PRIMARY]->m_dwBMRStatic + 8;
        }

        // the controller placed in native mode (ProgIf&0x05)==0x05; overwrite
        // dwLegacyIrq to -1
        DDKPCIINFO PciInfo;
        
        memset(&PciInfo, 0, sizeof(PciInfo));
        PciInfo.cbSize = sizeof(PciInfo);
        if (
            (DDKReg_GetPciInfo(hDevKey, &PciInfo) == ERROR_SUCCESS) &&
            ((PciInfo.dwWhichIds & PCIIDM_PROGIF) != 0) &&
            ((PciInfo.idVals[PCIID_PROGIF] & 0x05) == 0x05)
            ) {
                DEBUGMSG(ZONE_WARNING, (_T(
                    "Atapi!IDE_Init> MapIORegisters: WARN: Controller is placed in native mode\r\n"
                    ), _T(
                    "set %s\\%s to -1 to force driver operate in native mode\r\n"
                    ), szDevKey, REG_VAL_IDE_LEGACYIRQ));

                m_pIdeReg->dwLegacyIRQ = (DWORD)-1;
                // Write back the change to registry.
                if (!AtaSetRegistryValue(hDevKey, REG_VAL_IDE_LEGACYIRQ, m_pIdeReg->dwLegacyIRQ))
                {
                    DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                        "Atapi!IDE_Init> MapIORegisters: failed to set %s\\%s to -1\r\n"
                        ), szDevKey, REG_VAL_IDE_LEGACYIRQ));
                    return ERROR_GEN_FAILURE;
                }
            }

        fRet = TRUE;

    exit:
        return fRet;
    }

    virtual BOOL MapIrqtoSysIntr()
    {
        CIDEBUS *pBus = this;
        BOOL fRet = FALSE;

        // assign IRQs

    #ifdef I830M4
        if (pBus->m_pIdeReg->dwLegacyIRQ != (DWORD)-1) {
            pBus->m_pIdeReg->dwIrq = 14;
            pBus->m_pIdeReg->dwSysIntr = SYSINTR_NOP;    // force assignemnt
        }
    #endif
        pBus->m_pPort[ATA_PRIMARY]->m_dwIrq = pBus->m_pIdeReg->dwIrq;
        pBus->m_pPort[ATA_SECONDARY]->m_dwIrq = pBus->m_pIdeReg->dwIrq;

        // if IDE/ATA controller is legacy ISA/PCI, then IRQ of secondary channel is
        // (IRQ of primary channel + 1); otherwise, the primary and secondary
        // channels must share an interrupt and employ an ISR

        if (pBus->m_pIdeReg->dwLegacyIRQ == pBus->m_pPort[ATA_PRIMARY]->m_dwIrq) {
            pBus->m_pPort[ATA_SECONDARY]->m_dwIrq = pBus->m_pPort[ATA_PRIMARY]->m_dwIrq + 1;
        }

        // no SysIntr provided; we have to map IRQ to SysIntr ourselves; note that,
        // even if the primary and secondary channels share an IRQ, each channel is
        // required to have its own IRQ-SysIntr mapping

        if (!pBus->m_pIdeReg->dwSysIntr) {

            DWORD dwReturned = 0;
            UINT32 primaryIrqAndFlags[3];

            primaryIrqAndFlags[0] = (UINT32)-1;
            primaryIrqAndFlags[1] = 0x00000008; // translate statically mapped sysintr instead of requesting a new one
            primaryIrqAndFlags[2] = pBus->m_pPort[ATA_PRIMARY]->m_dwIrq;

            if (!KernelIoControl(
                IOCTL_HAL_REQUEST_SYSINTR,
                (LPVOID)primaryIrqAndFlags, sizeof(primaryIrqAndFlags),
                (LPVOID)&pBus->m_pPort[ATA_PRIMARY]->m_dwSysIntr, sizeof(DWORD),
                &dwReturned
            )) {
                DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                    "Atapi!IDE_Init> Failed to map IRQ(%d) to SysIntr for primary channel of device\r\n"
                    ), pBus->m_pPort[ATA_PRIMARY]->m_dwIrq));
                goto exit;
            }

            // even if primary and secondary channels use the same IRQ, we need two
            // separate SysIntr mappings

            if (pBus->m_pPort[ATA_SECONDARY]->m_fInitialized) {

                UINT32 secondaryIrqAndFlags[3];
                secondaryIrqAndFlags[0] = (UINT32)-1;
                secondaryIrqAndFlags[1] = 0x00000008; // translate statically mapped sysintr instead of requesting a new one
                secondaryIrqAndFlags[2] = pBus->m_pPort[ATA_SECONDARY]->m_dwIrq;

                if (!KernelIoControl(
                    IOCTL_HAL_REQUEST_SYSINTR,
                    (LPVOID)secondaryIrqAndFlags, sizeof(secondaryIrqAndFlags),
                    (LPVOID)&pBus->m_pPort[ATA_SECONDARY]->m_dwSysIntr, sizeof(DWORD),
                    &dwReturned
                )) {
                    DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                        "Atapi!IDE_Init> Failed to map IRQ(%d) to SysIntr for secondary channel on device\r\n"
                        ), pBus->m_pPort[ATA_SECONDARY]->m_dwIrq));
                    goto exit;
                }
            }
        }
        else {

            DWORD dwReturned = 0;

            // the SysIntr corresponds to the primary channel; we need to request
            // a separate SysIntr for the secondary channel

            pBus->m_pPort[ATA_PRIMARY]->m_dwSysIntr = pBus->m_pIdeReg->dwSysIntr;

            if (pBus->m_pPort[ATA_SECONDARY]->m_fInitialized) {

                UINT32 secondaryIrqAndFlags[3];
                secondaryIrqAndFlags[0] = (UINT32)-1;
                secondaryIrqAndFlags[1] = 0x00000008; // translate statically mapped sysintr instead of requesting a new one
                secondaryIrqAndFlags[2] = pBus->m_pPort[ATA_SECONDARY]->m_dwIrq;

                if (!KernelIoControl(
                    IOCTL_HAL_REQUEST_SYSINTR,
                    (LPVOID)secondaryIrqAndFlags, sizeof(secondaryIrqAndFlags),
                    (LPVOID)&pBus->m_pPort[ATA_SECONDARY]->m_dwSysIntr, sizeof(DWORD),
                    &dwReturned
                )) {
                    DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
                        "Atapi!IDE_Init> Failed to map IRQ(%d) to SysIntr for secondary channel on device\r\n"
                        ), pBus->m_pPort[ATA_SECONDARY]->m_dwIrq));
                    goto exit;
                }
            }
        }

        // if this IDE/ATA controller only has a single channel,
        // then destroy the secondary channel's port

        DEBUGCHK(pBus->m_pPort[ATA_PRIMARY]->m_fInitialized);
        m_numPorts = 2;
        if (!pBus->m_pPort[ATA_SECONDARY]->m_fInitialized) {
            delete pBus->m_pPort[ATA_SECONDARY];
            pBus->m_pPort[ATA_SECONDARY] = NULL;
            m_numPorts = 1;
        }

        fRet = TRUE;
exit:
        return fRet;
    }

    virtual BOOL IsMasterSlaveDevice()
    {
        return TRUE;
    }

    virtual DWORD GetNumPorts()
    {
        ASSERT(m_numPorts != -1);
        return DWORD(m_numPorts);
    }

    virtual CPort * GetPort(unsigned int i)
    {
        ASSERT(m_numPorts != -1);
        ASSERT(i < DWORD(m_numPorts));
        ASSERT(i < MAX_DEVICES_PER_CONTROLLER);
        return m_pPort[i];
    }


};

extern "C" CIDEController * NewCATAController()
{
    return new CATAController;
}
