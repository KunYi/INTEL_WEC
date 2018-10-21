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

#include "SDHC.h"
#include "SDHCPowerCtrlBase.hpp"
#include "SDHCSlot.h"

CSDHCPowerCtrlBase::CSDHCPowerCtrlBase()
{
    m_cpsAtPowerDown = D0;
    m_fFakeCardRemoval = FALSE;
    m_fKeepPower = TRUE;
}

CSDHCPowerCtrlBase::~CSDHCPowerCtrlBase()
{
    m_fFakeCardRemoval = FALSE;
    m_fKeepPower = TRUE;
    m_pSDHCSlotBase = NULL;
}

SD_API_STATUS 
CSDHCPowerCtrlBase::Init(
    CSDHCSlotBase*  psdhcSlotBase
)
{
    if (!m_pSDHCSlotBase)
    {
        return SD_API_STATUS_INVALID_PARAMETER;
    }
    m_pSDHCSlotBase = psdhcSlotBase;
    return SD_API_STATUS_SUCCESS;
}

SD_API_STATUS 
CSDHCPowerCtrlBase::PowerDown()
{
    SD_API_STATUS           status              = SD_API_STATUS_SUCCESS;
    BOOL                    bIsPowerManaged     = FALSE;
    BOOL                    bWakeupControl      = FALSE;
    BOOL                    bSleepsWithPower    = FALSE;
    CEDEVICE_POWER_STATE    currentPowerState   = D0;
    CEDEVICE_POWER_STATE    newPowerState       = D3;

    if(!m_pSDHCSlotBase)
    {
        status = SD_API_STATUS_NOT_IMPLEMENTED;
        return status;
    }

    bIsPowerManaged = m_pSDHCSlotBase->IsPowerManaged();
    bWakeupControl =  m_pSDHCSlotBase->SleepsWithPower();
    bSleepsWithPower = m_pSDHCSlotBase->WakeupControl();
    currentPowerState = m_pSDHCSlotBase->CurrentPowerState();
    m_cpsAtPowerDown = currentPowerState;

    if (!bIsPowerManaged) {
        if (bWakeupControl) {
            newPowerState = D3;
        }
        else {
            newPowerState = D4;
        }
        m_pSDHCSlotBase->SetHardwarePowerState(newPowerState);
    }
    m_fKeepPower = FALSE;
    if (bSleepsWithPower || currentPowerState == D0) {
        DEBUGCHK(!bSleepsWithPower || currentPowerState == D3);
        m_fKeepPower = TRUE;
    }
    else
        m_fFakeCardRemoval = TRUE;

    return status;
}

SD_API_STATUS 
CSDHCPowerCtrlBase::PowerUp()
{
    SD_API_STATUS           status              = SD_API_STATUS_SUCCESS;
    BOOL                    bIsPowerManaged     = FALSE;
    BOOL                    bSleepsWithPower    = FALSE;
    
    if(!m_pSDHCSlotBase)
    {
        status = SD_API_STATUS_NOT_IMPLEMENTED;
        return status;
    }

    bIsPowerManaged = m_pSDHCSlotBase->IsPowerManaged();
    bSleepsWithPower = m_pSDHCSlotBase->SleepsWithPower();

    if (!bIsPowerManaged) {
        m_pSDHCSlotBase->SetHardwarePowerState(m_cpsAtPowerDown);
    }
    else if (bSleepsWithPower) {
        m_pSDHCSlotBase->RestoreInterrupts(FALSE);
    }

    return status;
}

CSDHCPowerCtrlPersist::CSDHCPowerCtrlPersist()
    : CSDHCPowerCtrlBase()
{
}

CSDHCPowerCtrlPersist::~CSDHCPowerCtrlPersist()
{
}

SD_API_STATUS 
CSDHCPowerCtrlPersist::PowerDown()
{
    SD_API_STATUS           status          = SD_API_STATUS_SUCCESS;
    SD_ADAPTIVE_CONTROL     AdaptiveControl = {0};
    
    if (!m_pSDHCSlotBase)
    {
        status = SD_API_STATUS_NOT_IMPLEMENTED;
        return status;
    }

    m_fKeepPower = TRUE;
    m_pSDHCSlotBase->GetLocalAdaptiveControl(&AdaptiveControl);
#if (CE_MAJOR_VER < 8)
    m_pSDHCSlotBase->ControlCardPowerOff(&m_fKeepPower, 
            AdaptiveControl.bResetOnSuspendResume,
            AdaptiveControl.bPwrCtrlOnSuspendResume);
#else
    m_pSDHCSlotBase->ControlCardPowerOff(&m_fKeepPower, 
            AdaptiveControl.currentRegistryAdaptiveControl.bResetOnSuspendResume,
            AdaptiveControl.currentRegistryAdaptiveControl.bPwrCtrlOnSuspendResume);
#endif

    return status;
}

SD_API_STATUS 
CSDHCPowerCtrlPersist::PowerUp()
{
    SD_API_STATUS           status          = SD_API_STATUS_SUCCESS;
    SD_ADAPTIVE_CONTROL     AdaptiveControl = {0};
    
    if(!m_pSDHCSlotBase)
    {
        status = SD_API_STATUS_NOT_IMPLEMENTED;
        return status;
    }

    m_pSDHCSlotBase->GetLocalAdaptiveControl(&AdaptiveControl);
#if (CE_MAJOR_VER < 8)
    m_pSDHCSlotBase->ControlCardPowerOn(
            AdaptiveControl.bResetOnSuspendResume,
            AdaptiveControl.bPwrCtrlOnSuspendResume);
#else
    m_pSDHCSlotBase->ControlCardPowerOn(
            AdaptiveControl.currentRegistryAdaptiveControl.bResetOnSuspendResume,
            AdaptiveControl.currentRegistryAdaptiveControl.bPwrCtrlOnSuspendResume);
#endif

    return status;
}