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
    atapipci.cpp

Abstract:
    Base ATA/ATAPI PCI device abstraction.

Revision History:

--*/

#include <atapipci.h>

LONG CPCIDisk::m_lDeviceCount = 0;

static TCHAR *g_szPCICDRomDisk = TEXT("CD-ROM");
static TCHAR *g_szPCIHardDisk = TEXT("Hard Disk");

// ----------------------------------------------------------------------------
// Function: CreatePCIHD
//     Spawn function called by IDE/ATA controller enumerator
//
// Parameters:
//     hDevKey -
// ----------------------------------------------------------------------------

EXTERN_C
CDisk *
CreatePCIHD(
    HKEY hDevKey
    )
{
    return new CPCIDisk(hDevKey);
}

// ----------------------------------------------------------------------------
// Function: CPCIDisk
//     Constructor
//
// Parameters:
//     hKey -
// ----------------------------------------------------------------------------

CPCIDisk::CPCIDisk(
    HKEY hKey
    ) : CDisk(hKey)
{
    m_pStartMemory = NULL;
    m_pPort = NULL;
    m_pPhysList = NULL;
    m_pSGCopy = NULL;
    m_pPFNs = NULL;
    m_pPRDPhys.QuadPart = 0;
    m_pPRD = NULL;
    m_dwPhysCount = 0;
    m_dwSGCount = 0;
    m_dwPRDSize = 0;
    m_pBMCommand = NULL;

    InterlockedIncrement(&m_lDeviceCount);

    DEBUGMSG(ZONE_PCI, (_T(
        "Atapi!CPCIDisk::CPCIDisk> device count(%d)\r\n"
        ), m_lDeviceCount));
}

// ----------------------------------------------------------------------------
// Function: ~CPCIDisk
//     Destructor
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

CPCIDisk::~CPCIDisk(
    )
{
    FreeDMABuffers();

    InterlockedDecrement(&m_lDeviceCount);

    DEBUGMSG(ZONE_PCI, (_T(
        "Atapi!CPCIDisk::~CPCIDisk> device count(%d)\r\n"
        ), m_lDeviceCount));
}

// ----------------------------------------------------------------------------
// Function: FreeDMABuffers
//     Deallocate DMA buffers
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

void
CPCIDisk::FreeDMABuffers(
    )
{
    if (m_pPRD) {
        FreePhysMem(m_pPRD);
        m_pPRDPhys.QuadPart = 0;
        m_pPRD = NULL;
    }

#pragma prefast(push)
#pragma prefast(disable:6250, "VirtualFree without MEM_RELEASE.  Memory is released as m_pStartMemory")

    if (m_pPhysList) {
        // free the fixed pages; the variable pages should already be free
        for (DWORD i = 0; i < MIN_PHYS_PAGES; i++) {
            FreePhysMem(m_pPhysList[i].pVirtualAddress);
        }
        VirtualFree(m_pPhysList, UserKInfo[KINX_PAGESIZE], MEM_DECOMMIT);
        m_pPhysList = NULL;
    }

    if (m_pSGCopy) {
        VirtualFree(m_pSGCopy, UserKInfo[KINX_PAGESIZE], MEM_DECOMMIT);
        m_pSGCopy = NULL;
    }

    if (m_pPFNs) {
        VirtualFree(m_pPFNs, UserKInfo[KINX_PAGESIZE], MEM_DECOMMIT);
        m_pSGCopy = NULL;
    }
#pragma prefast(pop)

    if (m_pStartMemory) {
        VirtualFree(m_pStartMemory, 0, MEM_RELEASE);
        m_pStartMemory = NULL;
    }

    m_dwPhysCount = 0;
    m_dwSGCount = 0;
}

// ----------------------------------------------------------------------------
// Function: CopyDiskInfoFromPort
//     This function is not used
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

void
CPCIDisk::CopyDiskInfoFromPort(
    )
{
    ASSERT(m_pPort->m_dwRegBase != 0);
    m_pATAReg = (PBYTE)m_pPort->m_dwRegBase;
    m_pATARegAlt = (PBYTE)m_pPort->m_dwRegAlt;

    ASSERT(m_pPort->m_dwBMR != 0);
    m_pBMCommand = (LPBYTE)m_pPort->m_dwBMR;
}

// ----------------------------------------------------------------------------
// Function: WaitForInterrupt
//     Wait for interrupt
//
// Parameters:
//     dwTimeOut -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::WaitForInterrupt(
    DWORD dwTimeOut
    )
{
    BYTE bStatus;
    BOOL fRet = TRUE;
    DWORD dwRet;

    // wait for interrupt
    dwRet = WaitForSingleObject(m_pPort->m_hIRQEvent, dwTimeOut);

    if (dwRet == WAIT_TIMEOUT) {
        fRet = FALSE;
    }
    else {
        if (dwRet != WAIT_OBJECT_0) {
            if (!WaitForDisc(WAIT_TYPE_DRQ, dwTimeOut, 10)) {
                DEBUGMSG(ZONE_ERROR, (_T(
                    "CPCIDisk::WaitForInterrupt> Wait for disk failed - timeout(%u)\r\n"
                    ), dwTimeOut));
                fRet = FALSE;
            }
        }
    }

    // read status; acknowledge interrupt
    bStatus = GetBaseStatus();
    if (bStatus & ATA_STATUS_ERROR) {
        bStatus = GetError();
        fRet = FALSE;
    }

    // signal interrupt done
    InterruptDone(m_pPort->m_dwSysIntr);

    return fRet;
}

// ----------------------------------------------------------------------------
// Function: EnableInterrupt
//     Enable channel interrupt
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

void
CPCIDisk::EnableInterrupt(
    )
{
    GetBaseStatus(); // acknowledge interrupt, if pending

    // signal interrupt done
    InterruptDone(m_pPort->m_dwSysIntr);
}

// ----------------------------------------------------------------------------
// Function: ConfigureRegisterBlock
//     This function is called by DSK_Init before any other CDisk function to
//     set up the register block.
//
// Parameters:
//     dwStride -
// ----------------------------------------------------------------------------

VOID
CPCIDisk::ConfigureRegisterBlock(
    DWORD dwStride
    )
{
    m_dwStride = dwStride;
    m_dwDataDrvCtrlOffset = ATA_REG_DATA * dwStride;
    m_dwFeatureErrorOffset = ATA_REG_FEATURE * dwStride;
    m_dwSectCntReasonOffset = ATA_REG_SECT_CNT * dwStride;
    m_dwSectNumOffset = ATA_REG_SECT_NUM * dwStride;
    m_dwDrvHeadOffset = ATA_REG_DRV_HEAD * dwStride;
    m_dwCommandStatusOffset = ATA_REG_COMMAND * dwStride;
    m_dwByteCountLowOffset = ATA_REG_BYTECOUNTLOW * dwStride;
    m_dwByteCountHighOffset = ATA_REG_BYTECOUNTHIGH * dwStride;

    // PCI ATA implementations don't assign I/O resources for the first four
    // bytes, as they are unused

    m_dwAltStatusOffset = ATA_REG_ALT_STATUS * dwStride;
    m_dwAltDrvCtrl = ATA_REG_DRV_CTRL * dwStride;
}

// ----------------------------------------------------------------------------
// Function: Init
//     Initialize channel
//
// Parameters:
//     hActiveKey -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::Init(
    HKEY hActiveKey
    )
{
    BOOL bRet = FALSE;

    m_f16Bit = TRUE; // PCI is 16-bit

    // configure port
    if (!ConfigPort()) {
        DEBUGMSG(ZONE_INIT, (_T(
            "Atapi!CPCIDisk::Init> Failed to configure port; device(%u)\r\n"
            ), m_dwDeviceId));
        goto exit;
    }

    // assign the appropriate folder name
    m_szDiskName = (IsCDRomDevice() ? g_szPCICDRomDisk : g_szPCIHardDisk);

    // reserve memory for DMA buffers
    m_pStartMemory = (LPBYTE)VirtualAlloc(NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    if (!m_pStartMemory) {
        bRet = FALSE;
    }

    // finish intialization; i.e., initialize device
    bRet = CDisk::Init(hActiveKey);
    if (!bRet) {
        goto exit;
    }

exit:;
    //
    // If this function fails, then the disk object will be deleted.  However,
    // for the case of the Promise card, the port's m_Disk object will be set
    // to this disk, and the interrupt will try to access this memory.  We need
    // to clean this up or we end up with a nasty heap corruption.
    //
    if( !bRet )
    {
        if( m_pPort->m_pDisk[0] == this )
        {
            m_pPort->m_pDisk[0] = NULL;
        }
        else
        {
            m_pPort->m_pDisk[1] = NULL;
        }
    }

    return bRet;
}

// ----------------------------------------------------------------------------
// Function: MainIoctl
//     This is redundant
//
// Parameters:
//     pIOReq -
// ----------------------------------------------------------------------------

DWORD
CPCIDisk::MainIoctl(
    PIOREQ pIOReq
    )
{
    DEBUGMSG(ZONE_IOCTL, (_T(
        "Atapi!CPCIDisk::MainIoctl> IOCTL(%d), device(%d) \r\n"
        ), pIOReq->dwCode, m_dwDeviceId));

    return CDisk::MainIoctl(pIOReq);
}

// ----------------------------------------------------------------------------
// Function: ConfigPort
//     Initialize IST/ISR
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::ConfigPort(
    )
{
    m_pATAReg = (PBYTE)m_pPort->m_dwRegBase;
    m_pATARegAlt = (PBYTE)m_pPort->m_dwRegAlt;
    m_pBMCommand = (PBYTE)m_pPort->m_dwBMR;

    // this function is called for the master and slave on this channel; if
    // this has already been called, then exit
    if (m_pPort->m_hIRQEvent) {
        m_dwDeviceFlags |= DFLAGS_DEVICE_INITIALIZED;
        return TRUE;
    }
#ifdef I830M4
    unsigned char bStatus = ReadBMStatus();
    bStatus |= 0x86;  // clear the interrupt status per ICH4M spec & addendum
    WriteBMStatus(bStatus);
#endif
    // create interrupt event
    if (NULL == (m_pPort->m_hIRQEvent = CreateEvent(NULL, FALSE, FALSE, NULL))) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCIDisk::ConfigPort> Failed to create interrupt event for device(%d)\r\n"
            ), m_dwDeviceId));
        return FALSE;
    }

    // associate interrupt event with IRQ
    if (!InterruptInitialize(
        m_pPort->m_dwSysIntr,
        m_pPort->m_hIRQEvent,
        NULL,
        0)
    ) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR, (_T(
            "Atapi!CPCIDisk::ConfigPort> Failed to initialize interrupt(SysIntr(%d)) for device(%d)\r\n"
            ), m_pPort->m_dwSysIntr, m_dwDeviceId));
        return FALSE;
    }

    return TRUE;
}

// ----------------------------------------------------------------------------
// Function: TranslateAddress
//     Translate a system address to a bus address for the DMA controller
//
// Parameters:
//     pdwAddr -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::TranslateAddress(
    PDWORD pdwAddr
    )
{
    // translate a system address to a bus address for the DMA bus controller

    PHYSICAL_ADDRESS SystemLogicalAddress, TransLogicalAddress;
    DWORD dwBus;

    // fetch bus number/type
    // if (m_pPort->m_pCNTRL != NULL) {
    //     dwBus = m_pPort->m_pCNTRL->m_dwBus;
    // }
    // else {
    //     dwBus = 0;
    // }

    dwBus = m_pPort->m_pController->m_dwi.dwBusNumber;

    // translate address
    SystemLogicalAddress.HighPart = 0;
    SystemLogicalAddress.LowPart = *pdwAddr;
    if (!HalTranslateSystemAddress(PCIBus, dwBus, SystemLogicalAddress, &TransLogicalAddress)) {
        return FALSE;
    }

    *pdwAddr = TransLogicalAddress.LowPart;

    return TRUE;
}

// ----------------------------------------------------------------------------
// Function: SetupDMA
//     Prepare DMA transfer
//
// Parameters:
//     pSgBuf -
//     dwSgCount -
//     fRead -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::SetupDMA(
    PSG_BUF pSgBuf,
    DWORD dwSgCount,
    BOOL fRead
    )
{
    DWORD dwAlignMask = m_dwDMAAlign - 1;
    DWORD dwPageMask = (DWORD)(UserKInfo[KINX_PAGESIZE] - 1);

    DWORD iPage = 0, iPFN, iBuffer;
    BOOL fUnalign = FALSE;

    DMA_ADAPTER_OBJECT Adapter;

    Adapter.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
    Adapter.InterfaceType = (INTERFACE_TYPE)m_pPort->m_pController->m_dwi.dwInterfaceType;
    Adapter.BusNumber = m_pPort->m_pController->m_dwi.dwBusNumber;

    DEBUGMSG(ZONE_DMA, (_T(
        "Atapi!CPCIDisk::SetupDMA> Request(%s), SgCount(%d)\r\n"
        ), fRead ? (_T("Read")) : (_T("Write")), dwSgCount));

    // stop bus master
    WriteBMCommand(0);

    if (!m_pPRD) {
        m_pPRD = (PDMATable)HalAllocateCommonBuffer(&Adapter,
            UserKInfo[KINX_PAGESIZE], &m_pPRDPhys, FALSE);
        if (!m_pPRD) {
            goto ExitFailure;
        }
    }

    // m_pPhysList tracks pages used for DMA buffers when the scatter/gather
    // buffer is unaligned
    if (!m_pPhysList) {
        m_pPhysList = (PPhysTable)VirtualAlloc(m_pStartMemory, UserKInfo[KINX_PAGESIZE], MEM_COMMIT, PAGE_READWRITE);
        if (!m_pPhysList) {
            goto ExitFailure;
        }
        // allocate the minimum number of fixed pages
        for (DWORD i = 0; i < MIN_PHYS_PAGES; i++) {
            PHYSICAL_ADDRESS PhysicalAddress = {0};
            m_pPhysList[i].pVirtualAddress = (LPBYTE)HalAllocateCommonBuffer(&Adapter,
                (DWORD)UserKInfo[KINX_PAGESIZE], &PhysicalAddress, FALSE);
            m_pPhysList[i].pPhysicalAddress = (LPBYTE)PhysicalAddress.QuadPart;
            if (!m_pPhysList[i].pVirtualAddress) {
                goto ExitFailure;
            }
        }
    }
    m_dwPhysCount = 0;

    // m_pSGCopy tracks the mapping between scatter/gather buffers and DMA
    // buffers when the scatter/gather buffer is unaligned and we are reading,
    // so we can copy the read data back to the scatter/gather buffer; when the
    // scatter/gather buffer is aligned, m_pSGCopy tracks the scatter/gather
    // buffers of a particular DMA transfer, so we can unlock the buffers at
    // completion

    if (!m_pSGCopy) {
        m_pSGCopy = (PSGCopyTable)VirtualAlloc(
            m_pStartMemory + UserKInfo[KINX_PAGESIZE],
            UserKInfo[KINX_PAGESIZE],
            MEM_COMMIT,
            PAGE_READWRITE);
        if (!m_pSGCopy) {
            goto ExitFailure;
        }
    }
    m_dwSGCount = 0;

    if (!m_pPFNs) {
        m_pPFNs = (PDWORD)VirtualAlloc(
            m_pStartMemory + 2*UserKInfo[KINX_PAGESIZE],
            UserKInfo[KINX_PAGESIZE],
            MEM_COMMIT,
            PAGE_READWRITE);
        if (!m_pPFNs) {
            goto ExitFailure;
        }
    }

    // determine whether the a buffer or the buffer length is unaligned
    for (iBuffer = 0; iBuffer < dwSgCount; iBuffer++) {
        if (
            ((DWORD)pSgBuf[iBuffer].sb_buf & dwAlignMask) ||
            ((DWORD)pSgBuf[iBuffer].sb_len & dwAlignMask)
        ) {
            fUnalign = TRUE;
            break;
        }
    }

    if (fUnalign) {

        DWORD dwCurPageOffset = 0;

        for (iBuffer = 0; iBuffer < dwSgCount; iBuffer++) {

            LPBYTE pBuffer = (LPBYTE)pSgBuf[iBuffer].sb_buf;

            DWORD dwBufferLeft = pSgBuf[iBuffer].sb_len;
            while (dwBufferLeft) {

                DWORD dwBytesInCurPage = UserKInfo[KINX_PAGESIZE] - dwCurPageOffset;
                DWORD dwBytesToTransfer = (dwBufferLeft > dwBytesInCurPage) ? dwBytesInCurPage : dwBufferLeft;

                // allocate a new page, if necessary
                if ((dwCurPageOffset == 0) && (m_dwPhysCount >= MIN_PHYS_PAGES)) {
                    PHYSICAL_ADDRESS PhysicalAddress = {0};
                    m_pPhysList[m_dwPhysCount].pVirtualAddress = (LPBYTE)HalAllocateCommonBuffer(
                        &Adapter, UserKInfo[KINX_PAGESIZE], &PhysicalAddress, FALSE);
                    m_pPhysList[m_dwPhysCount].pPhysicalAddress = (LPBYTE)PhysicalAddress.QuadPart;
                    if (!m_pPhysList[m_dwPhysCount].pVirtualAddress) {
                        goto ExitFailure;
                    }
                }

                if (fRead) {

                    // prepare a scatter/gather copy entry on read, so we can
                    // copy data from the DMA buffer to the scatter/gather
                    // buffer after this DMA transfer is complete

                    m_pSGCopy[m_dwSGCount].pSrcAddress = m_pPhysList[m_dwPhysCount].pVirtualAddress + dwCurPageOffset;
                    m_pSGCopy[m_dwSGCount].pDstAddress = pBuffer;
                    m_pSGCopy[m_dwSGCount].dwSize = dwBytesToTransfer;
                    m_dwSGCount++;

                }
                else {
                    memcpy(m_pPhysList[m_dwPhysCount].pVirtualAddress + dwCurPageOffset, pBuffer, dwBytesToTransfer);
                }

                // if this buffer is larger than the space remaining on the page,
                // then finish processing this page by setting @dwCurPageOffset<-0

                if (dwBufferLeft >= dwBytesInCurPage) {
                    dwCurPageOffset = 0;
                }
                else {
                    dwCurPageOffset += dwBytesToTransfer;
                }

                // have we finished a page? (i.e., offset was reset or this is the last buffer)
                if ((dwCurPageOffset == 0) || (iBuffer == (dwSgCount - 1))) {
                    // add this to the PRD table
                    m_pPRD[m_dwPhysCount].physAddr = (DWORD)m_pPhysList[m_dwPhysCount].pPhysicalAddress;
                    m_pPRD[m_dwPhysCount].size = dwCurPageOffset ? (USHORT)dwCurPageOffset : (USHORT)UserKInfo[KINX_PAGESIZE];
                    m_pPRD[m_dwPhysCount].EOTpad = 0;
                    m_dwPhysCount++;
                }

                // update transfer
                dwBufferLeft -= dwBytesToTransfer;
                pBuffer += dwBytesToTransfer;
           }
        }

        m_pPRD[m_dwPhysCount - 1].EOTpad = 0x8000;

    }
    else {

        DWORD dwTotalBytes = 0;

        for (iBuffer = 0; iBuffer < dwSgCount; iBuffer++) {

            LPBYTE pBuffer = (LPBYTE)pSgBuf[iBuffer].sb_buf;

            // determine the number of bytes remaining to be placed in PRD
            dwTotalBytes = pSgBuf[iBuffer].sb_len;
            if (!LockPages (
                pBuffer,
                dwTotalBytes,
                m_pPFNs,
                fRead ? LOCKFLAG_WRITE : LOCKFLAG_READ)
            ) {
                goto ExitFailure;
            }

            // add a scatter/gather copy entry for the area we lock, so that
            // we can unlock it when we are finished
            m_pSGCopy[m_dwSGCount].pSrcAddress = pBuffer;
            m_pSGCopy[m_dwSGCount].pDstAddress = 0;
            m_pSGCopy[m_dwSGCount].dwSize = dwTotalBytes;
            m_dwSGCount++;

            iPFN = 0;
            while (dwTotalBytes) {

                DWORD dwBytesToTransfer = UserKInfo[KINX_PAGESIZE];

                if ((DWORD)pBuffer & dwPageMask) {
                    // the buffer is not page aligned; use up the next page
                    // boundary
                    dwBytesToTransfer = UserKInfo[KINX_PAGESIZE] - ((DWORD)pBuffer & dwPageMask);
                }

                if (dwTotalBytes < dwBytesToTransfer) {
                    // use what remains
                    dwBytesToTransfer = dwTotalBytes;
                }

                m_pPRD[iPage].physAddr = (m_pPFNs[iPFN] << UserKInfo[KINX_PFN_SHIFT]) + ((DWORD)pBuffer & dwPageMask);

                if (!TranslateAddress(&m_pPRD[iPage].physAddr)) {
                    goto ExitFailure;
                }

                m_pPRD[iPage].size = (USHORT)dwBytesToTransfer;
                m_pPRD[iPage].EOTpad = 0;

                iPage++;
                iPFN++;

                // update transfer
                pBuffer += dwBytesToTransfer;
                dwTotalBytes -= dwBytesToTransfer;
            }
        }

        m_dwPhysCount = 0;
        m_pPRD[iPage-1].EOTpad = 0x8000;
    }

    return TRUE;

ExitFailure:

    DEBUGCHK(0);

    // clean up
    // FreeDMABuffers();

    return FALSE;
}

// ----------------------------------------------------------------------------
// Function: BeginDMA
//     Begin DMA transfer
//
// Parameters:
//     fRead -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::BeginDMA(
    BOOL fRead
    )
{
    BYTE bStatus, bCommand;

    CacheSync(CACHE_SYNC_DISCARD);

    WriteBMCommand(0);
    ASSERT(0 == m_pPRDPhys.HighPart);
    WriteBMTable(m_pPRDPhys.LowPart);

    bStatus = ReadBMStatus();
    bStatus |= 0x06;
    // bStatus |= 0x66;

    WriteBMStatus(bStatus);

    if (fRead) {
        bCommand = 0x08 | 0x01;
    }
    else {
        bCommand = 0x00 | 0x01;
    }

    WriteBMCommand(bCommand);

    bStatus = ReadBMStatus();

    return TRUE;
}

// ----------------------------------------------------------------------------
// Function: PrepareDMA
//     Begin DMA transfer
//
// Parameters:
//     fRead -
// ----------------------------------------------------------------------------

BOOL CPCIDisk::PrepareDMA()
{
    BYTE bStatus;

    CacheSync(CACHE_SYNC_DISCARD);

    WriteBMCommand(0);
    ASSERT(0 == m_pPRDPhys.HighPart);
    WriteBMTable(m_pPRDPhys.LowPart);

    bStatus = ReadBMStatus();
    bStatus |= 0x06;
    // bStatus |= 0x66;

    WriteBMStatus(bStatus);

    return TRUE;
}

// ----------------------------------------------------------------------------
// Function: EndDMA
//     End DMA transfer
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::EndDMA(
    )
{
    BYTE bStatus = ReadBMStatus();

    if ((bStatus & BM_STATUS_INTR) && (bStatus & BM_STATUS_ACTIVE)) {
        DEBUGMSG(ZONE_DMA, (_T(
            "Atapi!CPCIDisk::EndDMA> Status: active; status(0x%x)\r\n"
            ), bStatus));
    }
    else if ((bStatus & BM_STATUS_INTR) && !(bStatus & BM_STATUS_ACTIVE)) {
        DEBUGMSG(ZONE_DMA, (_T(
            "Atapi!CPCIDisk::EndDMA> Status: inactive; status(0x%x)\r\n"
            ), bStatus));
    }
    else if (!(bStatus & BM_STATUS_INTR)&& (bStatus & BM_STATUS_ACTIVE)) {

        DEBUGMSG(ZONE_ERROR|ZONE_DMA, (_T(
            "Atapi!CPCIDisk::EndDMA> Interrupt delayed; status(0x%x)\r\n"
            ), bStatus));

        BOOL bCount = 0;

        for(;;) {

            StallExecution(100);

            bCount++;
            bStatus = ReadBMStatus();

            if ((bStatus & BM_STATUS_INTR) && !(bStatus & BM_STATUS_ACTIVE)) {
                DEBUGMSG(ZONE_DMA, (_T(
                    "Atapi!CPCIDisk::EndDMA> DMA complete after delay; status(0x%x)\r\n"
                    ), bStatus));
                break;
            }
            else {
                DEBUGMSG(ZONE_ERROR|ZONE_DMA, (_T(
                    "Atapi!CPCIDisk::EndDMA> Interrupt still delayed; status(0x%x)\r\n"
                    ), bStatus));
                if (bCount > 10) {
                    WriteBMCommand(0);
                    return FALSE;
                }
            }
        }
    }
    else {
        if (bStatus & BM_STATUS_ERROR) {
            DEBUGMSG(ZONE_ERROR|ZONE_DMA, (_T(
                "Atapi!CPCIDisk::EndDMA> Error; (0x%x)\r\n"
                ), bStatus));
            DEBUGCHK(0);
            return FALSE;
        }
    }

    WriteBMCommand(0);

    return TRUE;
}

// ----------------------------------------------------------------------------
// Function: AbortDMA
//     Abort DMA transfer
//
// Parameters:
//     None
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::AbortDMA(
    )
{
    DWORD i;

    WriteBMCommand(0);

    for (i = 0; i < m_dwSGCount; i++) {
        if (!m_pSGCopy[i].pDstAddress) {
            UnlockPages(m_pSGCopy[i].pSrcAddress, m_pSGCopy[i].dwSize);
        }
    }

    // free all but the first @MIN_PHYS_PAGES pages; these are fixed
    for (i = MIN_PHYS_PAGES; i < m_dwPhysCount; i++) {
        FreePhysMem(m_pPhysList[i].pVirtualAddress);
    }

    return FALSE;
}

// ----------------------------------------------------------------------------
// Function: CompleteDMA
//     Complete DMA transfer
//
// Parameters:
//     pSgBuf -
//     dwSgCount -
//     fRead -
// ----------------------------------------------------------------------------

BOOL
CPCIDisk::CompleteDMA(
      PSG_BUF           //pSgBuf
    , DWORD             //dwSgCount
    , BOOL              //fRead
    )
{
    DWORD i;

    for (i = 0; i < m_dwSGCount; i++) {
        if (m_pSGCopy[i].pDstAddress) {
            // this corresponds to an unaligned region; copy it back to the
            // scatter/gather buffer
            memcpy(m_pSGCopy[i].pDstAddress, m_pSGCopy[i].pSrcAddress, m_pSGCopy[i].dwSize);
        }
        else {
            // this memory region needs to be unlocked
            UnlockPages(m_pSGCopy[i].pSrcAddress, m_pSGCopy[i].dwSize);
        }
    }

    // free all but the first @MIN_PHYS_PAGES pages; the first @MIN_PHYS_PAGES
    // pages are fixed

    for (i = MIN_PHYS_PAGES; i < m_dwPhysCount; i++) {
        FreePhysMem(m_pPhysList[i].pVirtualAddress);
    }

    return TRUE;
}

