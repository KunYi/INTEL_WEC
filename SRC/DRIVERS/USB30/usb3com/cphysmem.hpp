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
// File: cphysmem.hpp Definitions for physical memory manager
//----------------------------------------------------------------------------------

#ifndef CPHYSMEM_HPP
#define CPHYSMEM_HPP

#include "globals.hpp"

typedef ULONG FAR * LPULONG;

// All the memory that this CPhysMem class returns to callers
// will have its physical address divisible by CPHYSMEM_MEMORY_ALIGNMENT.
// This allows us to satisfy the HCD alignment requirements. For
// instance, in XHCI, TDs must be aligned on 64 byte boundaries.
//
// This number should be 1) A power of two and 2) a common multiple
// of any required alignment criteria of the HCD.
#define CPHYSMEM_MEMORY_ALIGNMENT DWORD(64)
#define MAX_UNALIGNED_BUFFER    (64*1024)

//
// Hook these if you want to track local memory allocations.
//
#define CPHYS_MEM_ALLOC  LocalAlloc
#define CPHYS_MEM_FREE   LocalFree

#define MEMLIST_NEXT_PA(pnode)(pnode->dwPhysAddr + pnode->dwSize)

//
// Linked list helper functions
//
#define IS_LIST_EMPTY(_ListHead)         ((BOOL)((_ListHead)->pNext == _ListHead))
#define END_OF_LIST(_ListHead, _Entry)   ((BOOL)(_ListHead == _Entry))

#ifdef DEBUG
#define VALIDATE_HEAPS(fHighPri)  ValidateHeaps(fHighPri)
#else
#define VALIDATE_HEAPS(fHighPri)
#endif

//
// Flag defines for AllocateMemory / FreeMemory
//
#define CPHYSMEM_FLAG_HIGHPRIORITY ((DWORD)(1 << 0))  // Allocates from High Priority region first
#define CPHYSMEM_FLAG_NOBLOCK      ((DWORD)(1 << 1))  // Call doesn't block if no memory available.

#ifdef DEBUG
    #define CPHYSMEM_BLOCK_FOR_MEM_INTERVAL    1000
    #define CPHYSMEM_DEBUG_MAXIMUM_BLOCK_TIME  5000
#else
    #define CPHYSMEM_BLOCK_FOR_MEM_INTERVAL INFINITE
#endif // DEBUG

typedef struct tMEMLIST   MEMLIST;
typedef struct tMEMLIST* PMEMLIST;

struct tMEMLIST
{
    DWORD       dwVirtAddr;
    DWORD       dwPhysAddr;
    DWORD       dwSize;
    PMEMLIST    pNext;
    PMEMLIST    pPrev;

#ifdef DEBUG
    #define CPHYSMEM_MAX_DEBUG_NODE_DESCRIPTION_LENGTH 80 
    TCHAR szDescription[CPHYSMEM_MAX_DEBUG_NODE_DESCRIPTION_LENGTH];
#endif // DEBUG
};

inline VOID InitializeListHead(PMEMLIST pListHead)
{
    pListHead->pNext = pListHead->pPrev = pListHead;
}

inline PMEMLIST FirstNode(PMEMLIST pListHead) 
{
    return(pListHead->pNext); 
}

inline VOID RemoveNode(PMEMLIST pNode)
{
    pNode->pPrev->pNext = pNode->pNext;
    pNode->pNext->pPrev = pNode->pPrev;
    pNode->pNext = NULL;
    pNode->pPrev = NULL;
}

inline VOID InsertNodeBefore(PMEMLIST pNodeNew, PMEMLIST pNodeExisting)
{
    pNodeExisting->pPrev->pNext = pNodeNew;
    pNodeNew->pPrev = pNodeExisting->pPrev;
    pNodeNew->pNext = pNodeExisting;
    pNodeExisting->pPrev = pNodeNew;
}

class CPhysMem
{
public:
    CPhysMem(DWORD dwSize, DWORD dwHighPrioritySize, PUCHAR pVirtAddr, PUCHAR pPhysAddr);
    ~CPhysMem();
    BOOL ReInit();

    BOOL AllocateMemory(DEBUG_PARAM(IN const TCHAR* pszMemDescription)
                            IN const DWORD dwPassedInSize,
                            OUT PUCHAR* const pVirtAddr,
                            IN const DWORD dwFlags,
                            IN const DWORD dwBoundary,
                            IN BOOL* pfRequestingAbort = NULL);

    VOID FreeMemory(IN const PUCHAR pVirtAddr,
                        IN const ULONG ulPhysAddr,
                        IN const DWORD dwFlags);

    inline ULONG VaToPa(UCHAR const*const pVirtAddr);

    inline PUCHAR PaToVa(ULONG ulPhysAddr);

    inline BOOL InittedOK()
    {
        return m_fInitted;
    };

    PMEMLIST GetFREELIST (BOOL fHighPri);
    PMEMLIST GetINUSELIST (BOOL fHighPri);



    const DWORD  m_dwTotal;
    const DWORD  m_dwHighPri;
    const PUCHAR m_pVirtBase;
    const PUCHAR m_pPhysBase;

private:

    BOOL AddNodeToInUseList(PMEMLIST pNode, BOOL fHighPri);
    BOOL AddNodeToFreeList(PMEMLIST pNode, BOOL fHighPri);

    BOOL DeleteNode(PMEMLIST pNode);

    PMEMLIST CreateNewNode(DWORD dwSize, DWORD dwVirtAddr, DWORD dwPhysAddr);

    PMEMLIST FindFreeBlock(DWORD dwSize, DWORD dwBoundary, BOOL fHighPri);
    BOOL RemoveNodeFromList(PMEMLIST pNode, PMEMLIST* pListHead);
    BOOL FreeList(PMEMLIST *ppHead);

#ifdef DEBUG
    BOOL ValidateHeaps(BOOL fHighPri);
#endif
    CPhysMem(const CPhysMem&);     // NOT IMPLEMENTED default copy constructor
    CPhysMem& operator=(CPhysMem&); 

    CRITICAL_SECTION    m_csLock;
    BOOL                m_fInitted;
    BOOL                m_fPhysFromPlat;
    HANDLE              m_hFreeMemEvent;
    BOOL                m_fHasBlocked;
    DWORD               m_dwPaVaConversion;
    DWORD               m_dwTotalPhysMemSize;
    PUCHAR              m_pPhysicalBufferAddr;

    PMEMLIST            m_pNodeFreeListHead;
    
    DWORD               m_dwNormalPA;
    DWORD               m_dwNormalVA;
    DWORD               m_dwNormalSize;
    PMEMLIST            m_pFreeListHead;
    PMEMLIST            m_pInUseListHead;
    
    DWORD               m_dwHighPriorityPA;
    DWORD               m_dwHighPriorityVA;
    DWORD               m_dwHighPrioritySize;
    PMEMLIST            m_pHighPriorityInUseListHead;
    PMEMLIST            m_pHighPriorityFreeListHead;
};

inline ULONG CPhysMem::VaToPa(UCHAR const*const pVirtAddr)
{
    DEBUGCHK(pVirtAddr != NULL);
    ASSERT(((DWORD)m_pPhysicalBufferAddr <=(DWORD)pVirtAddr) &&
       ((DWORD)pVirtAddr <(DWORD)m_pPhysicalBufferAddr + m_dwTotal));
    return ULONG(ULONG(pVirtAddr) + m_dwPaVaConversion);
}

inline PUCHAR CPhysMem::PaToVa(ULONG ulPhysAddr)
{
    DEBUGCHK(ulPhysAddr != 0);
    PUCHAR pVirtAddr = PUCHAR(ulPhysAddr - m_dwPaVaConversion);
    ASSERT(((DWORD)m_pPhysicalBufferAddr <=(DWORD)pVirtAddr) &&
       ((DWORD)pVirtAddr <(DWORD) m_pPhysicalBufferAddr + m_dwTotal));
    return pVirtAddr;
}

#endif /* CPHYSMEM_HPP */