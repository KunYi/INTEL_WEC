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
// File: ctransfer.h -  This file implements the CTransfer class for managing transfers
//----------------------------------------------------------------------------------

#ifndef TRANSFER_H
#define TRANSFER_H

#include "cphysmem.hpp"
#include "cpipe.h"
#include "hal.h"
#include "datastructures.h"

class CXhcd;

typedef struct _STRANSFER
{
    // These are the IssueTransfer parameters
    IN LPTRANSFER_NOTIFY_ROUTINE lpStartAddress;
    IN LPVOID lpvNotifyParameter;
    IN DWORD dwFlags;
    IN LPCVOID lpvControlHeader;
    IN DWORD dwStartingFrame;
    IN DWORD dwFrames;
    IN LPCDWORD aLengths;
    IN DWORD dwBufferSize;
    IN OUT LPVOID lpvBuffer;
    IN ULONG uPaBuffer;
    IN LPCVOID lpvCancelId;
    OUT LPDWORD lpdwIsochErrors;
    OUT LPDWORD lpdwIsochLengths;
    OUT LPBOOL lpfComplete;
    OUT LPDWORD lpdwBytesTransferred;
    OUT LPDWORD lpdwError;
} STRANSFER;

class CTransfer
{
    friend class CPipe;

public:

    CTransfer(IN CPipe * const cPipe,
                IN CPhysMem * const pCPhysMem,
                STRANSFER sTransfer);

    virtual ~CTransfer();

    CPipe * const m_pCPipe;
    CPhysMem * const m_pCPhysMem;
    CTransfer * GetNextTransfer(VOID)
    {
        return  m_pNextTransfer;
    };
    VOID SetNextTransfer(CTransfer * pNext)
    {
        m_pNextTransfer= pNext;
    };
    virtual BOOL Init(VOID);
    virtual BOOL AddTransfer() = 0;
    STRANSFER GetSTransfer()
    {
        return m_sTransfer;
    };
    VOID DoNotCallBack()
    {
        m_sTransfer.lpfComplete = NULL;
        m_sTransfer.lpdwError = NULL;
        m_sTransfer.lpdwBytesTransferred = NULL;
        m_sTransfer.lpStartAddress = NULL;
    }
    LPCTSTR GetControllerName(VOID) const;

    BOOL m_fDoneTransferCalled;

    PBYTE m_pAllocatedForClient;
    
protected:
    CTransfer&operator=(CTransfer&);
   
    CTransfer * m_pNextTransfer;
    STRANSFER   m_sTransfer;
    PBYTE   m_pAllocatedForControl;
    DWORD   m_dwPaControlHeader;
    DWORD   m_dwDataTransferred;
    DWORD   m_dwTransferID;

    TRANSFER_DATA* m_pTd;
    static  DWORD m_dwGlobalTransferID;
};

class CPhysMem;
class CXhcd;
class CQueuedPipe;

class CQTransfer : public CTransfer
{
    friend class CQueuedPipe;

#define STATUS_CQT_NONE        (0)
#define STATUS_CQT_PREPARED    (1)
#define STATUS_CQT_ACTIVATED   (2)
//terminal state
#define STATUS_CQT_CANCELED    (3)
#define STATUS_CQT_HALTED      (4)
//got event but nod handled
#define STATUS_CQT_DONE        (5)
//terminal state
#define STATUS_CQT_RETIRED     (7)
#define STATUS_CQT_SHORT       (8)

public:

    CQTransfer(IN CQueuedPipe * const pCQPipe,
                IN CPhysMem * const pCPhysMem,
                STRANSFER sTransfer) : CTransfer((CPipe*)pCQPipe,
                                                    pCPhysMem,
                                                    sTransfer)
    {
        m_dwStatus             = STATUS_CQT_NONE; 
        m_dwDataNotTransferred = 0; 
        m_dwUsbError           = USB_NO_ERROR;
        m_dwSoftRetryCnt       = 0;
    }

    ~CQTransfer()
    {
        ASSERT(m_dwStatus == STATUS_CQT_RETIRED || m_dwStatus == STATUS_CQT_CANCELED);

        if(m_pTd != NULL)
        {
            delete m_pTd;
        }

        return;
    }

    BOOL Activate(VOID);
    BOOL AddTransfer(VOID);
    BOOL AbortTransfer(VOID);
    BOOL DoneTransfer(VOID);
    BOOL IsTransferDone(VOID);
    BOOL IsHalted(VOID)
    {
        return(m_dwStatus == STATUS_CQT_HALTED);
    }
    VOID SetStatus(DWORD dwStatus){
        m_dwStatus = dwStatus;
    }
    DWORD GetStatus(VOID)
    {
        return m_dwStatus;
    }
    DWORD GetTransferLength(VOID)
    {
        return m_sTransfer.dwBufferSize;
    }
    VOID SetDataToTransfer(DWORD dwDataNotTransferred)
    {
        m_dwDataNotTransferred = dwDataNotTransferred;
    }

private:

    CQTransfer&operator=(CQTransfer&); 

    // data members prepared for copying into TD of the round-robin at activation time
    DWORD   m_dwDataNotTransferred;
    DWORD   m_dwUsbError;

protected:
    // these members are populated at activation time - they indicate the chain slot this transfer is placed in
    DWORD   m_dwStatus;
    DWORD   m_dwSoftRetryCnt;
};




#endif /* TRANSFER_H */