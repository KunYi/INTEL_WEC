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

#ifndef _SDHC_PWR_CTRL_DEFINED
#define _SDHC_PWR_CTRL_DEFINED

#include <SDCardDDK.h>
#include <SDHCD.h>
#include <ceddk.h>

class CSDHCSlotBase;

//This classes default behavior will unload and reload the driver when necessary during suspend and resume
class CSDHCPowerCtrlBase {
public:
    // Constructor - only initializes the member data. True initialization
    // occurs in Init().
    CSDHCPowerCtrlBase();
    virtual ~CSDHCPowerCtrlBase();

    SD_API_STATUS   Init(CSDHCSlotBase*  psdhcSlotBase);
    BOOL            IsFakeCardRemoval() {return m_fFakeCardRemoval;}
    BOOL            KeepPower() {return m_fKeepPower;}

    // Called when the device is suspending.
    virtual SD_API_STATUS PowerDown();
    // Called when the device is resuming.
    virtual SD_API_STATUS PowerUp();

protected:
    CSDHCSlotBase*  m_pSDHCSlotBase;

private:
    CEDEVICE_POWER_STATE    m_cpsAtPowerDown;       // power state at PowerDown()
    BOOL                    m_fFakeCardRemoval : 1; // should we simulate card removal?
protected:
    BOOL m_fKeepPower;
};

//This classes default behavior will re initialize the hardware when necessary during suspend and resume
class CSDHCPowerCtrlPersist : public CSDHCPowerCtrlBase {
public:
    // Constructor - only initializes the member data. True initialization
    // occurs in Init().
    CSDHCPowerCtrlPersist();
    ~CSDHCPowerCtrlPersist();

    // Called when the device is suspending.
    SD_API_STATUS PowerDown();
    // Called when the device is resuming.
    SD_API_STATUS PowerUp();
};

#endif // _SDHC_PWR_CTRL_DEFINED
