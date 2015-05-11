//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2011-2012 Intel Corporation All Rights Reserved. 
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
// File: ceventhandler.h - event handler class for XHCI
//       This contains interrupt routines which handle the XHCI interrupts.
//----------------------------------------------------------------------------------

#ifndef CXHCDEVENTHANDLER_H
#define CXHCDEVENTHANDLER_H

class CPhysMem;
class CHW;
class CXhcdRing;

class CXhcdEventHandler
{
    friend class CHW;

public:

    CXhcdEventHandler(IN CHW* pCHW, IN CPhysMem *pCPhysMem);

    ~CXhcdEventHandler();
    BOOL Initialize();
    VOID DeInitialize();

    BOOL WaitForPortStatusChange(HANDLE m_hHubChanged) const;

    // parse events
    VOID HandleEvent();
    // command event handler
    VOID HandleCommandCompletion(COMMAND_EVENT_TRB *pEvent);
    // port change event handler
    VOID HandlePortStatus(PORT_EVENT_TRB *pEvent) const;
    BOOL HandleCommandWaitList(LOGICAL_DEVICE *pVirtDev,
                                COMMAND_EVENT_TRB *pEvent) const;
    VOID HandleSetDequeueCompletion(COMMAND_EVENT_TRB *pEvent,
                                TRB *pTrb) const;
    VOID HandleResetEpCompletion(TRB *pTrb) const;
    INT HandleTransferEvent(TRANSFER_EVENT_TRB *pEvent);

    CPhysMem * const m_pMem;
    CHW*       const m_pCHW;    
    CXhcdRing* m_pXhcdRing;




private:

    HANDLE  m_hUsbHubChangeEvent;
    BOOL    m_fEventHandlerClosing;

    INT     m_iNoopsHandled;

    INT CheckCompCode(UINT uTrbCompCode);
};

#endif /* CXHCDEVENTHANDLER_H */