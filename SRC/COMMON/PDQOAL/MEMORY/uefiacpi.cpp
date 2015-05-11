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
//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2012 Intel Corporation All Rights Reserved. 
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
#include <windows.h>
#include <oal.h>
#include <acpi.h>
#include <bootarg.h>
#include "acpitable.h"


typedef struct  {
    char      sSignature[8];
    BYTE      bChecksum;
    BYTE      sOemId[6];
    BYTE      bRevision;
    DWORD     dwRsdtAddress;

    DWORD     dwLength;         // v2
    ULARGE_INTEGER XsdtAddress; // v2
    BYTE      bExtendedCheckSum;// v2
    BYTE      rgbReserved[3];   // v2
} AcpiTableRsdp;

//typedef enum {
//    UEFI_ACPI_TABLE_APIC = 'CIPA',
//    UEFI_ACPI_TABLE_FACS = 'FCAF',
//    UEFI_ACPI_TABLE_RSDT = 'TDSR',
//    UEFI_ACPI_TABLE_FACP = 'PCAF',
//    UEFI_ACPI_TABLE_DSDT = 'TDSD',
//    UEFI_ACPI_TABLE_NULL = 0
//} UEFI_ACPI_TABLE;

// if your platform has invalid checksums on the tables set this to FALSE
static BOOL s_fAcpiCalcCheckSum = TRUE;

static AcpiInfo s_AcpiInfo = { sizeof (AcpiInfo), FALSE, 0, (DWORD)-1, 0 };

DWORD                   g_dwRsdtPA = 0;
const AcpiTableRsdp*    g_pRsdp;
volatile AcpiTable*     g_pRsdt = NULL;
volatile AcpiTable*     g_pFacp = NULL;
volatile FADT*          g_pFadt = NULL;
volatile AcpiTable*     g_pDsdt = NULL;


//Debug Table Content
#undef DUMP_TABLE_CONTENT

// Disable DEBUG print by #undef
#undef BSP_AML_DEBUG

#ifdef BSP_AML_DEBUG
#define OAL_AML_DBG   1
#else
#define OAL_AML_DBG   0
#endif


// reserved this pysical memroy in config.bib
static BYTE*    g_pACPIMemory = (BYTE *)0x80220000; 
ROUTING_INFO    *g_PRTInfo    = NULL;

// reserved memory boundary
#define ACPI_MEM_RANGE          (0x80320000)

// DSDT header length
#define DEF_BLOCK_HEADER_SIZE   (36)


// following ACPI spec.
#define OP_ZEOR        (0x00)
#define OP_ONE         (0x01)
#define OP_BUFFER      (0x11)
#define OP_PACKAGE     (0x12)
#define OP_METHOD      (0x14)
#define OP_RETURN      (0xA4)
#define OP_DEVICE      (0x82)

#define PREFIX_BYTE    (0x0A)
#define PREFIX_WORD    (0x0B)
#define PREFIX_DWORD   (0x0C)

#define PREFIX_EXTOP   (0x5B)

// following PCI spec.
#define PRIMARY_PCI_BUS   (0x00)

typedef enum {
    NONE = 0,
    INTA = 1,
    INTB = 2,
    INTC = 3,
    INTD = 4
};

typedef enum {
    PIRQA = 1,
    PIRQB = 2,
    PIRQC = 3,
    PIRQD = 4,
    PIRQE = 5,
	PIRQF = 6,
	PIRQG = 7,
	PIRQH = 8
};
// Root Bus indicator - an unique AML steam "Name(_HID,EISAID ("PNP0A08"))"
static BYTE RootBusID[10] = { 0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x41, 0xD0, 0x0A, 0x08 };

// Interrrupt link device indicator - an unique AML steam "Name(_HID,EISAID ("PNP0C0F"))"
static BYTE IntLinkID[10] = { 0x08, 0x5F, 0x48, 0x49, 0x44, 0x0C, 0x41, 0xD0, 0x0C, 0x0F };

// "_PRT object prefix"
static BYTE PRT[4]       = { 0x5F, 0x50, 0x52, 0x54 }; 

// _ADR object of Name Op
static BYTE ADR[5]       = { 0x08, 0x5F, 0x41, 0x44, 0x52 };

// _PRS object of Name Op
static BYTE PRS[5]       = { 0x08, 0x5F, 0x50, 0x52, 0x53 };


//------------------------------------------------------------------------------
//
//  Function:  DumpAmlCode()
//
//  Dump 16 bytes AML code for debugging
//
inline VOID DumpAmlCode( BYTE *src )
{
    UINT8   loop = 0;

    if (src != NULL)
    {
        OALMSG(1, (L"\t"));
    
        while(loop < 16)
        {
            OALMSG(1, (L"0x%x ", *(src+loop)));
            loop++;

            if( !(loop%8) )
            {
                OALMSG(1, (L"\r\n\t"));
            }
        }
    OALMSG(1, (L"\r\n"));
    }
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiScanRsdp
//
//  Scans Physical Memory from 'start' for 'length' to find the Root System Description Pointer
//
static const AcpiTableRsdp* AcpiScanRsdp (DWORD physStart, DWORD cbLength)
{
    const char szSig[] = "RSD PTR ";
    const char* psVirtStart = reinterpret_cast<const char *>(OALPAtoCA(physStart));

    // interate though the memory range looking for the ACPI sig on 16byte boundries
    for (DWORD dwOffset = 0; dwOffset < cbLength; dwOffset += 16) 
    {
        if (0 == strncmp(psVirtStart + dwOffset, szSig, _countof(szSig) - 1)) 
        {
            return reinterpret_cast<const AcpiTableRsdp*>(psVirtStart + dwOffset);
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiTableIdToString
//
//  Converts a four 'char' identifier to a Unicode null terminated string
//  NOTE: not multithreading safe
//
static TCHAR* AcpiTableIdToString(ACPI_TABLE Id)
{
    static TCHAR wszBuffer[5] = {0};
    wszBuffer[0] = reinterpret_cast<BYTE*>(&Id)[0];
    wszBuffer[1] = reinterpret_cast<BYTE*>(&Id)[1];
    wszBuffer[2] = reinterpret_cast<BYTE*>(&Id)[2];
    wszBuffer[3] = reinterpret_cast<BYTE*>(&Id)[3];
    return wszBuffer;
}

#define MAX_ACPI_TABLE_LENGTH   0x20000
//------------------------------------------------------------------------------
//
//  Function:  AcpiCheckSum
//
//  Validates the checksum of a block of data
//
static BOOL AcpiCheckSum (const BYTE* pbData, DWORD cbLength)
{
    BYTE bSum = 0;

    if (pbData == NULL)
    {
        return FALSE;
    }

    // Tables shouldn't be longer than <large number=5MB>
    if (cbLength > 0x20000)
    {
        OALMSG(OAL_ERROR, (TEXT(" ACPI table looks too large Start:0x%x Length:%d"), pbData, cbLength));
        return FALSE;
    }

    if (s_fAcpiCalcCheckSum)
    {
        for (DWORD i = 0; i < cbLength; ++i)
            bSum += *pbData++;
    }

    OALMSG(bSum != 0 && OAL_ERROR, (TEXT(" Invalid checksum found on ACPI Table: %s"), 
        AcpiTableIdToString(static_cast<ACPI_TABLE>(*reinterpret_cast<const DWORD*>(pbData)))));

    return bSum == 0;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiTableRsdp
//
//  Finds the Root System Description Pointer
//
static const AcpiTableRsdp* AcpiFindRsdp()
{
    if(!g_pRsdp)
    {
        return NULL;
    }
    else
    {
        return g_pRsdp;
    }
}

// defined in x86 OEMAddressTable
extern "C" DWORD dwOEMTotalRAM;

//------------------------------------------------------------------------------
//
//  Function:  AcpiFindRsdt
//
//  Finds and verifies the Root System Description Table
//
static volatile AcpiTable* AcpiFindRsdt(__in const volatile AcpiTableRsdp* const pAcpiTableRsdp)
{
    if(!g_pRsdt)
    {
        return NULL;
    }
    else
    {
        return g_pRsdt;
    }
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiPhysicalAddressToValidTable
//
//  Takes a pointer to physical memory, validates its a ACPI table, and returns to pointer
//  in Virtual Memory
//
static volatile AcpiTable* AcpiPhysicalAddressToValidTable()
{ 
    if (g_pRsdt)
    {
        return g_pRsdt;
    }

    return NULL;
}

static volatile AcpiTable* AcpiFindTable(__in volatile AcpiTable* pAcpiTable, const ACPI_TABLE TableToFind);

//------------------------------------------------------------------------------
//
//  Function:  AcpiParseFacp
//
//  Parse the Fixed ACPI Description Table looking for 'TableToFind' in the DSDT and FACS
//
static volatile AcpiTable* AcpiParseFacp(
    __in_ecount(dwEntryCount) volatile DWORD * pdwEntries, 
    DWORD /*dwEntryCount*/, 
    const ACPI_TABLE TableToFind
    )
{
    const volatile AcpiTable* pRetVal = NULL;

    if (pdwEntries == NULL)
    {
        return NULL;
    }

    {
        // 'FACS': Firmware ACPI Control Structure 
        // NOTE: This table doesn't have the standard header or checksum

        if (g_pRsdt)
        {
            OALMSG(1, (TEXT(" %s Start:0x%x Length:%d"), AcpiTableIdToString(g_pRsdt->Signature), g_pRsdt, g_pRsdt->dwLength));
            pRetVal = AcpiFindTable(g_pRsdt, TableToFind);
        }

        ++pdwEntries;
    }

    if (!pRetVal) 
    {   
        if (g_pRsdt)
        {
            pRetVal = AcpiFindTable(g_pRsdt, TableToFind);
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiParseRsdt
//
//  Parse the Root System Description Table looking for 'TableToFind' in all the subtables
//
static volatile AcpiTable* AcpiParseRsdt(
    __in_ecount(dwEntryCount) volatile DWORD * pdwEntries, 
    DWORD dwEntryCount, 
    const ACPI_TABLE TableToFind
    )
{
    volatile AcpiTable* pRetVal = NULL;

    if (pdwEntries == NULL)
    {
        return pRetVal;
    }

    for (DWORD i = 0; i < dwEntryCount && pRetVal == NULL; ++i)
    {
        volatile AcpiTable* pRsdt = AcpiPhysicalAddressToValidTable();
        if (pRsdt)
        {
            pRetVal = AcpiFindTable(pRsdt, TableToFind);
        }
        ++pdwEntries;
    }

    return pRetVal;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiFindTable
//
//  Parse the given pAcpiTable looking for 'TableToFind' in all the subtables
//
static volatile AcpiTable* AcpiFindTable(__in volatile AcpiTable* pAcpiTable, const ACPI_TABLE TableToFind)
{   
    volatile AcpiTable* RetVal = NULL;
    
    if (pAcpiTable == NULL)
    {
        return NULL;
    }
    OALMSG(1 , (TEXT("AcpiFindTable(): TableToFind is 0x%X!!!\r\n"), TableToFind));
    
    // some special cases, which we want to recurse into
    switch (TableToFind)
    {
    case ACPI_TABLE_RSDT:
        RetVal = g_pRsdt;
        break;

    case ACPI_TABLE_FACP:
        // Fixed ACPI Description Table (FADT) 
        RetVal = g_pFacp;
        break;
        
    case ACPI_TABLE_DSDT:
        RetVal = g_pDsdt;
        break;
        
    default:
        break;
    }

    return RetVal;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiEnumTables
//
//  Enumerates all the ACPI tables, which fills in the s_AcpiInfo struct used by AcpiInfo()
//
static void AcpiEnumTables()
{
    static BOOL fACPIEnumerated = FALSE;
    if (!fACPIEnumerated)
    {
        if(g_pRsdt)
        {
            AcpiFindTable(g_pRsdt, ACPI_TABLE_NULL);
            fACPIEnumerated = TRUE;
        }
    }
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiFindATable
//
//  Finds a specific ACPI Table
//
BOOL AcpiFindATable(
                    const ACPI_TABLE TableToFind, 
                    __out volatile void** pvTableData, 
                    __out volatile AcpiTable** pHeader
                    )
{
    volatile AcpiTable* const pAcpiHeader = AcpiFindTable(g_pRsdt, TableToFind);

    if (pHeader != NULL)
    {
        *pHeader = pAcpiHeader;
    }

    if (pvTableData != NULL)
    {
        *pvTableData = reinterpret_cast<void*>(const_cast<AcpiTable*>(pAcpiHeader+1));
    }

    return (pAcpiHeader != NULL);
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiInfo
//
//  Returns information on ACPI. returns TRUE if ACPI was found, and was in a 
//  physical address that could be accessed and Info was valid (with cbSize correct)
//
BOOL AcpiGetInfo(__out AcpiInfo* pInfo)
{
    if ( (!pInfo) || (pInfo->cbSize != sizeof AcpiInfo))
    {
        return FALSE;
    }

    // for s_AcpiInfo to have valid values we must enumerate all the tables
    AcpiEnumTables();

    memcpy(pInfo, &s_AcpiInfo, sizeof(s_AcpiInfo) );

    return s_AcpiInfo.fAcpiFound;
}


//------------------------------------------------------------------------------
//
//  Function:  AcpiFindTablesStartAddress
//
//  Returns the start 32bit physical address of the ACPI tables. 
//  It doesn't try to access this address.
//
DWORD AcpiFindTablesStartAddress()
{
    if (g_pRsdp != NULL)
    {
        return g_pRsdp->dwRsdtAddress;
    }
    else
    {
        return NULL;
    }
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiStaticMapping
//
//  Converts a physical address to virtual
//  
//
DWORD AcpiPhysicalAddressToVirtual(DWORD dwPhyAddr, DWORD* pdwSize)
{
    DWORD dwOffset;
    DWORD dwSize;

    if (pdwSize == NULL)
    {
        return NULL;
    }
    // This offset is because NKCreateStaticMapping will shift 8 bits for making 
    // address is page aligned before allocate static memory, so we need to know
    // how many bytes will be offset, and then add it back before return.
    dwOffset = dwPhyAddr & 0xFF;
    OALMSG(OAL_MEMORY , (TEXT("AcpiStaticMapping(): Offset in Page is 0x%X!!!\r\n"),dwOffset));
    
    dwSize = *pdwSize + dwOffset;
    OALMSG(OAL_MEMORY , (TEXT("AcpiStaticMapping(): Size will be allocate is 0x%X!!!\r\n"), dwSize));
    
    *pdwSize = dwSize;
    return reinterpret_cast<DWORD>(NKCreateStaticMapping(dwPhyAddr >> 8, dwSize)) + dwOffset;
}

//------------------------------------------------------------------------------
//
//  Function:  AcpiTableRsdp
//
//  Finds the Root System Description Pointer
//
static const AcpiTableRsdp* UefiAcpiFindRsdp()
{
    const AcpiTableRsdp*    pRsdp;
    const char              szSig[] = "RSD PTR ";
    DWORD                   dwRSDPAddr;
    DWORD                   dwRtnSize;
    DWORD                   dwLength;
    
#ifdef DUMP_TABLE_CONTENT    
    BYTE*                   pTemp;
    DWORD                   i;
#endif
    
    pRsdp = AcpiScanRsdp(0, 0x400);

    if (!pRsdp)
    {
        // check the next memory range, as defined in the ACPI spec
        pRsdp = AcpiScanRsdp(0xE0000, 0xfffff-0xe0000);
        if (!(pRsdp && strncmp((const char*)pRsdp, szSig, _countof(szSig) - 1) == 0))
        {
            pRsdp = NULL;
        }
    }
    
    if (!pRsdp)
    {
        dwRSDPAddr = *(reinterpret_cast<DWORD *>(((*reinterpret_cast<DWORD *>(BOOT_ARG_PTR_LOCATION)) + (PAGE_SIZE - 4)) | 0x80000000));
        OALMSG(OAL_MEMORY, (L"UefiAcpiFindRsdp():RSDP PA address = 0x%X\r\n", dwRSDPAddr));
        dwRtnSize = sizeof(AcpiTableRsdp);
        pRsdp = reinterpret_cast<const AcpiTableRsdp*>(AcpiPhysicalAddressToVirtual (dwRSDPAddr, &dwRtnSize));
    
        if (pRsdp && strncmp((const char*)pRsdp, szSig, _countof(szSig) - 1) == 0 )
        {
            // To do this is because we need to reallocate actual lengh for this Table
            if(!NKDeleteStaticMapping((AcpiTableRsdp *)pRsdp, dwRtnSize))
            {
                OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdp(): Failed to release mapping RSDP!!!\r\n")));
                return NULL;
            }
            
            dwRtnSize = pRsdp->dwLength;
            if (pRsdp->bRevision == 0)
            {
                dwRtnSize = 20;
            }

            pRsdp = NULL;
            pRsdp = reinterpret_cast<const AcpiTableRsdp*>(AcpiPhysicalAddressToVirtual (dwRSDPAddr, &dwRtnSize));
            
            if(!pRsdp)
            {
                OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdp(): Can't reallocate actualy RSDP lenght 0x%X!!!\r\n"), dwRtnSize));
                return NULL; 
            }
            
#ifdef DUMP_TABLE_CONTENT
            pTemp = (BYTE*)pRsdp;
            OALMSG(1, (L"UefiAcpiFindRsdp():RSDP VA address = 0x%X\r\n", pRsdp));
            for(i = 1; i <= dwRtnSize; i++) {
                OALMSG(1, (L"0x%2X ", *pTemp));
                if(!(i % 10)) {
                   OALMSG(1, (L"\r\n"));
                } 
                pTemp++;
            }
            OALMSG(1, (L"\r\n"));
#endif //DUMP_TABLE_CONTENT
        }
    }
    if(pRsdp)
    {
        // we found the RSDP, make sure it's valid
        dwLength = pRsdp->dwLength;
        if (pRsdp->bRevision == 0)
        {
            // Rev 0 had a 20 byte struct, with no Length member
            dwLength = 20;
        }

        OALMSG(OAL_MEMORY, (TEXT(" RSDP Start:0x%x ACPI Rev:%d Length:%d\r\n"), 
            pRsdp, pRsdp->bRevision, dwLength));

        if (AcpiCheckSum(reinterpret_cast<const BYTE*>(pRsdp), dwLength))
        {
            s_AcpiInfo.fAcpiFound = TRUE;
            s_AcpiInfo.dwAcpiVersion = pRsdp->bRevision;
            return pRsdp;
        }
    }
    
    return NULL;
}

//------------------------------------------------------------------------------
//
//  Function:  UefiAcpiFindRsdt
//
//  Finds and verifies the Root System Description Table
//
volatile AcpiTable* UefiAcpiFindRsdt()
{
    volatile AcpiTable* pRsdt = NULL;
    DWORD               dwRSDTAddr;
    DWORD               dwRtnSize;

#ifdef DUMP_TABLE_CONTENT
    BYTE                *pTemp;
    DWORD               i;
#endif    
    
    if (!g_pRsdp)
    {
        return NULL;
    }

    if (g_pRsdp->dwRsdtAddress == 0)
    {
        OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdt(): RSDT Not found\r\n")));
        return NULL;
    }
    
    dwRSDTAddr = g_pRsdp->dwRsdtAddress;
    g_dwRsdtPA = dwRSDTAddr;
    
    dwRtnSize = sizeof(AcpiTable);
    pRsdt = reinterpret_cast<volatile AcpiTable*>(AcpiPhysicalAddressToVirtual(dwRSDTAddr, &dwRtnSize));
    OALMSG(OAL_MEMORY, (TEXT("UefiAcpiFindRsdt(): RSDT Start:0x%X\r\n"), pRsdt));
    
    if(pRsdt && (pRsdt->Signature == UEFI_ACPI_TABLE_RSDT))
    { 
        // To do this is because we need to reallocate actual lengh for this Table
        if(!NKDeleteStaticMapping((AcpiTable *)pRsdt, sizeof(AcpiTable)))
        {
            OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdp(): Failed to release mapping RSDT!!!\r\n")));
            return NULL;
        }
        
        dwRtnSize = pRsdt->dwLength;
        pRsdt = NULL;
        pRsdt = reinterpret_cast<volatile AcpiTable*>(AcpiPhysicalAddressToVirtual(dwRSDTAddr, &dwRtnSize));
        if(!pRsdt)
        {
            OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdt(): Can't reallocate actualy RSDT lenght 0x%X!!!\r\n"), dwRtnSize));
            return NULL; 
        }
        
#ifdef DUMP_TABLE_CONTENT
        OALMSG(1, (L"UefiAcpiFindRsdt():RSDT VA address = 0x%X\r\n", pRsdt));
        pTemp = (BYTE *)pRsdt;
        for(i = 1; i <= dwRtnSize; i++) {
            OALMSG(1, (L"0x%2X ", *pTemp));
            if(!(i % 10)) {
               OALMSG(1, (L"\r\n"));
            } 
            pTemp++;
        }
        OALMSG(1, (L"\r\n"));
#endif //DUMP_TABLE_CONTENT
    }
    else
    {
        OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdt(): Get RSDT failed, pRsdt = 0x%X\r\n"), pRsdt));
    }

    if (pRsdt && AcpiCheckSum(const_cast<BYTE*>(reinterpret_cast<volatile BYTE*>(pRsdt)), pRsdt->dwLength))
    {
        return pRsdt;
    }

    return NULL; 
}

//------------------------------------------------------------------------------
//
//  Function:  UefAcpiFindFadt
//
//  Finds and verifies the Root System Description Table
//
static volatile FADT* UefiAcpiFindFadt()
{
    UINT64              Index;
    UINT32              Data32;
    DWORD               dwRtnSize;
    volatile FADT*      pFadt;
    volatile AcpiTable* pFacp = NULL;
    
#ifdef DUMP_TABLE_CONTENT
    BYTE                *pTemp;
    DWORD               i;
#endif

    if (!g_pRsdt)
    {
        return NULL;
    }

    for (Index = sizeof (AcpiTable); Index < g_pRsdt->dwLength; Index = Index + sizeof (UINT32)) {
        Data32 = *(UINT32 *) ((UINT8 *) g_pRsdt + Index);
        dwRtnSize = sizeof(AcpiTable) + sizeof(FADT);
        pFacp = reinterpret_cast<volatile AcpiTable*>(AcpiPhysicalAddressToVirtual(Data32, &dwRtnSize));
        if (pFacp->Signature == UEFI_ACPI_TABLE_FACP) {
            OALMSG(OAL_MEMORY, (TEXT("UefAcpiFindFadt(): Found FACP at address 0x%X\r\n"), pFacp));
            break;
        }
    }
       
    if(pFacp && (pFacp->Signature == UEFI_ACPI_TABLE_FACP))
    {  
#ifdef DUMP_TABLE_CONTENT
        pTemp = (BYTE *)pFacp;
        for(i = 1; i <= (sizeof(AcpiTable) + sizeof(FADT)); i++) {
            OALMSG(1, (L"0x%2X ", *pTemp));
            if(!(i % 10)) {
               OALMSG(1, (L"\r\n"));
            } 
            pTemp++;
        }
        OALMSG(1, (L"\r\n"));
#endif //DUMP_TABLE_CONTENT
    }
    else
    {
        OALMSG(OAL_LOG_ERROR, (TEXT("UefAcpiFindFadt(): Get RSDT failed, pFacp = 0x%X\r\n"), pFacp));
    }
    
    if (pFacp && AcpiCheckSum(const_cast<BYTE*>(reinterpret_cast<volatile BYTE*>(pFacp)), pFacp->dwLength))
    {
        pFadt = reinterpret_cast<volatile FADT*>((DWORD)pFacp + sizeof(AcpiTable));
        OALMSG(OAL_MEMORY, (TEXT("UefAcpiFindFadt(): FADT VA is 0x%X\r\n"), pFadt));
        g_pFacp = pFacp;
        return pFadt;
    }

    return NULL;
}
  
//------------------------------------------------------------------------------
//
//  Function:  UefAcpiFindDsdt
//
//  Finds and verifies the Root System Description Table
//
static volatile AcpiTable* UefiAcpiFindDsdt()
{
    volatile AcpiTable* pDsdt;
    DWORD               dwDsdtPA;
    DWORD               dwRtnSize;
    
#ifdef DUMP_TABLE_CONTENT
    BYTE                *pTemp;
    UINT32              i;
#endif    
    
    if (!g_pFadt)
    {
        return NULL;
    }

    if(g_pFadt->DSDT == 0)
    {
        OALMSG(OAL_LOG_ERROR, (TEXT("UefAcpiFindDsdt(): DSDT  Not found\r\n")));
        return NULL;
    }
    
    dwDsdtPA = g_pFadt->DSDT;
    OALMSG(OAL_MEMORY, (TEXT("UefAcpiFindDsdt(): DSDT PA address is 0x%X\r\n"), dwDsdtPA));
    
    dwRtnSize = sizeof(AcpiTable);
    pDsdt = reinterpret_cast<volatile AcpiTable*>(AcpiPhysicalAddressToVirtual(dwDsdtPA, &dwRtnSize));
      
    if(pDsdt && (pDsdt->Signature == UEFI_ACPI_TABLE_DSDT))
    {  
        // To do this is because we need to reallocate actual lengh for this Table
        if(!NKDeleteStaticMapping((AcpiTable *)pDsdt, sizeof(AcpiTable)))
        {
            OALMSG(OAL_LOG_ERROR, (TEXT("UefAcpiFindDsdt(): Failed to release mapping DSDT!!!\r\n")));
            return NULL;
        }
        
        dwRtnSize = pDsdt->dwLength;
        pDsdt = NULL;       
        pDsdt = reinterpret_cast<volatile AcpiTable*>(AcpiPhysicalAddressToVirtual(dwDsdtPA, &dwRtnSize));
        if(!pDsdt)
        {
            OALMSG(OAL_LOG_ERROR, (TEXT("UefiAcpiFindRsdt(): Can't reallocate actualy DSDT lenght 0x%X!!!\r\n"), 
                dwRtnSize));
            return NULL; 
        }
        
#ifdef DUMP_TABLE_CONTENT
        OALMSG(1, (L"UefAcpiFindDsdt():DSDT pointer VA address = 0x%X\r\n", pDsdt));
        pTemp = (BYTE*)pDsdt;
        for(i = 1; i <= dwRtnSize; i++) {
            OALMSG(1, (L"0x%2X ", *pTemp));
            if(!(i % 10)) 
            {
               OALMSG(1, (L"\r\n"));
            } 
            pTemp++;
        }
        OALMSG(1, (L"\r\n"));
#endif //DUMP_TABLE_CONTENT

        return pDsdt; 
    }
    else
        OALMSG(OAL_LOG_ERROR, (TEXT("UefAcpiFindDsdt(): Get DSDT failed, pRsdt = 0x%X\r\n"), pDsdt));
        
    // This function Can not used check sum is because we only allocate AcpiTable size
    // of static virtual menory, but in this DSDT table, it lenght include AML code size,
    // So to call check sum with pDsdt->dwLength will access unmapping address and then
    // cause system hand up......
    //if (UefiAcpiCheckSum((BYTE*)((volatile BYTE*)(pDsdt)), pDsdt->dwLength))
    //    return pDsdt; 
           
    return NULL;
}



BOOL x86InitACPITables()
{  
    BOOL bRc = TRUE;
    
    g_pRsdp = UefiAcpiFindRsdp();
    if(g_pRsdp == NULL)
    {
        OALMSG(OAL_LOG_ERROR, (L"x86FindACPITables(): Failed to Get RDSP pointer!!\r\n"));
        bRc = FALSE;
    }

    g_pRsdt = UefiAcpiFindRsdt();
    if(g_pRsdt == NULL)
    {
        OALMSG(OAL_LOG_ERROR, (L"x86FindACPITables(): Failed to Get RDST table pointer!!\r\n"));
        bRc = FALSE;
    }
    
    g_pFadt = UefiAcpiFindFadt();
    if(g_pFadt == NULL)
    {
        OALMSG(OAL_LOG_ERROR, (L"x86FindACPITables(): Failed to Get FACP table pointer!!\r\n"));
        bRc = FALSE;
    }
    
    g_pDsdt = UefiAcpiFindDsdt();
    if(g_pDsdt == NULL)
    {
        OALMSG(OAL_LOG_ERROR, (L"x86FindACPITables(): Failed to Get DSDT table pointer!!\r\n"));
        bRc = FALSE;
    }
    
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): All Tables are: \r\n"));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): RSDP VA = 0x%X \r\n", g_pRsdp));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): RSDT VA = 0x%X \r\n", g_pRsdt));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): FACP VA = 0x%X \r\n", g_pFacp));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): FADT VA = 0x%X \r\n", g_pFadt));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): DSDT VA = 0x%X \r\n", g_pDsdt));
    OALMSG(OAL_MEMORY, (L"x86FindACPITables(): ACPI PA = 0x%X \r\n", g_dwRsdtPA));
    
    return bRc;
}


//------------------------------------------------------------------------------
//
//  Function:  x86ParseDsdtTables
//
//  Based on some known restrictions, refer to design doc, parse AML code in DSDT 
//  table in order to retrieve the PCI routing information for each PCI bus. 
//  And also, PCI routing infomation wile save in the structure g_PRTInfo.
//
//
VOID x86ParseDsdtTables()
{
    ACPI_PARSE_STATE    AmlParser;
    BOOL                status;
    ROUTING_INFO        *Prev;
    ROUTING_INFO        *Table=NULL;
    BYTE                *AmlCode;

    //  Init AmlParser
    //
    AmlInitParser( &AmlParser );

    //  Skip the Definition Block Header of AML code.
    //  Size of(DEF_BLOCK_HEADER_SIZE) = 36 bytes
    //
    //  DefBlockHeader := TableSignature TableLength SpecCompliance CheckSum OemID
    //                   OemTableID OemRevision CreatorID CreatorRevision
    AmlParser.AmlEnd   += ( DEF_BLOCK_HEADER_SIZE );
    AmlParser.Aml      += ( DEF_BLOCK_HEADER_SIZE ); 


    // ==================================================================== //
    // Parser Rules :  (By the ACPI spec.)                                  //
    //                                                                      //
    //      * Each device has , if any, *only one* _PRT information.        //
    //      * And, a _PRT can be found in the package of a DEVICE() op.     //
    //      * Child PCI bus can be found in the Root PCI bus scope.         //
    //                                                                      //
    // ===================================================================  //

    //    1. Search for a Root PCI bus routing table
    //
    status = AmlFindRootPCIBus( &AmlParser );
    if( !status )
    {
        OALMSG(1, (L"PCI Root bridge cann't be found!!\r\n"));
        return;
    }

    status = AmlFindPRTMethod( &AmlParser );
    if( !status )
    {
        OALMSG(1, (L"_PRT Method cann't be found!!\r\n"));
        return;
    }

    AmlGetPRTInfo( &AmlParser, &Table );

    if( NULL == Table) 
    {
        OALMSG(1, (L"Don't find any Routing Information!!\r\n"));
        return;
    }

    g_PRTInfo = Table;

    OALMSG(OAL_AML_DBG, (L"===== PCI Root Routing Table : =====\r\n"));
    Prev = Table;
    while( Prev )
    {
        OALMSG(OAL_AML_DBG, (L"Routing Table : Dev=0x%x, IntPin=0x%d, IntLine=%d\r\n"
            ,Prev->DevNum, Prev->IntPin, Prev->IntLine));

        Prev = Prev->next;
    }

    //  2. Collect child PCI bus routing table, if any.
    //
    OALMSG( OAL_AML_DBG, (L"Find child bus from here:\r\n" ));

    for( AmlCode = AmlParser.Aml ; AmlCode < AmlParser.RootPkgEnd ; AmlCode++ )
    {

        status = AmlFindADeviceOp( &AmlParser);
        if( !status )
        {
            OALMSG(OAL_LOG_ERROR, (L"Can't find a Device Op in this Root PCI Bus!!\r\n"));
            break;
        }

        status = AmlFindPRTMethod( &AmlParser );
        if( !status )
        {
            OALMSG(OAL_AML_DBG, (L"GO to next Device Op.\r\n"));
            continue;
        }

        AmlGetPRTInfo(  &AmlParser, &Table );

        OALMSG(OAL_AML_DBG, (L"Save PCI routing table for Bus 0x%x\r\n" , AmlParser.PkgADR));


        Prev = g_PRTInfo;
        while( (Prev->DevNum != AmlParser.PkgADR) ) //For BSP_PCH_EG20T, Bus 1 is under root bus device 0x17
        {
            if(NULL != Prev->next)
				Prev = Prev->next;
			else
				break;
            OALMSG(OAL_AML_DBG, (L"."));
        }
        Prev->child = Table;

        OALMSG(OAL_AML_DBG, (L"Bus 0x%x save done!\r\n" ,AmlParser.PkgADR));
        AmlParser.PkgADR = 0;

    }

    OALMSG(OAL_AML_DBG, (L"No more _PRT data need to be colldected.\r\n" ));


    //  3. Get PCI IRQ resource.
    //
    status = AmlFindIntLinkDevice( &AmlParser );
    if( !status )
    {
        OALMSG(1, (L"NO avaliable IRQ numbers data.\r\n"));
        return;
    }

    status = AmlGetIRQDescriptor( &AmlParser );
    if( !status )
    {
        OALMSG(1, (L"Get IRQ Descriptor failed.\r\n"));
        return;
    }

    OALMSG(OAL_AML_DBG, (L"Paser Dsdt to Get IRQ information is done!.\r\n" ));
}


//------------------------------------------------------------------------------
//
//  Function:  GetDevicePIRQ
//
//  Get the requested PCI device routing table by the specific Device number 
//  and the IntPin number from the g_PRTInfo. This is necessary as each [bus device function]
//  has a specific IntPin number.
//
BOOL GetDevicePIRQ( BYTE bBusNum, BYTE bDevNum, BYTE bIntPinNum, PRT_TABLE *PRT)
{
    ROUTING_INFO    *Parent = NULL;
    ROUTING_INFO    *Prev   = NULL;


    OALMSG(OAL_AML_DBG, (L"+GetDevicePIRQ() request DevNum = 0x%x\r\n", bDevNum));

    if( PRT == NULL)
    {
        OALMSG(1, (L"Invalid Buffer detected!!\r\n"));
        return FALSE;
    }

    if( g_PRTInfo == NULL)
    {
        OALMSG(1, (L"None _PRT Info provided!!\r\n"));
        return FALSE;
    }

    Parent = g_PRTInfo;

#ifdef ENABLE_NM10
		if(PRIMARY_PCI_BUS==bBusNum)
		{
			//Retrieve the information from the primary bus
		    while(TRUE)
            {
		        if(Parent->DevNum == bDevNum)
		        {
			        //IntPin in PCI config space start with 1,2,3,4 
			        //But IntPin in ACPI start with 0,1,2,3
			        if(Parent->IntPin == (bIntPinNum-1))
			        {
				        //Some PCI devices may share the same IntPin
				        //Check if the IRQ has been claimed by others
				        if(Parent->fClaimed == (FALSE))
				        {
					        Parent->fClaimed = TRUE;
				            break;
				        }
			        }
		        }

		        Parent = Parent->next;
                if( !Parent )
                {
                    OALMSG(1, (L"Can't find the 0x%x device.\r\n", bDevNum));
                    return FALSE;
                }
			}
			
			PRT->DevAssignedLinkValue = Parent->IntLine;
		}
		else
		{
			//Retrieve the information from the secondary bus
	        while(TRUE)
            {
                if(NULL != Parent->child )
			    {
                    Prev = Parent->child;
				    if(Prev->DevNum == bDevNum)
		            {
			            if(Prev->IntPin == (bIntPinNum-1))
			            {
				            if(Prev->fClaimed == (FALSE))
				            {
					            Prev->fClaimed = TRUE;
				                break;
				            }
			            }
		            }
			    }
		        Parent = Parent->next;
                if( !Parent )
                {
                    OALMSG(1, (L"Can't find the 0x%x device.\r\n", bDevNum));
                    return FALSE;
                }
			}
			
			PRT->DevAssignedLinkValue = Prev->IntLine;
		}

 
#else
	while( (Parent->DevNum!= bDevNum))//origin code
    {
        Parent = Parent->next;
        if( !Parent )
        {
            OALMSG(1, (L"Can't find the 0x%x device.\r\n", bDevNum));
            return FALSE;
        }
    }
	
	PRT->DevAssignedLinkValue = Parent->IntLine;
#endif

    if( Parent->child )
    {
        Prev = Parent->child;
        while( Prev )
        {
            switch( Prev->IntPin )
            {
            case 0:
                PRT->BridgeDevINTA_StartLinkValue = Prev->IntLine;
                break;

            case 1:
                PRT->BridgeDevINTB_StartLinkValue = Prev->IntLine;
                break;

            case 2:
                PRT->BridgeDevINTC_StartLinkValue = Prev->IntLine;
                break;

            case 3:
                PRT->BridgeDevINTD_StartLinkValue = Prev->IntLine;
                break;
            }
            Prev = Prev->next; 
        }
    }
    OALMSG(OAL_AML_DBG, (L"-GetDevicePIRQ()\r\n"));
    return TRUE;
}


//------------------------------------------------------------------------------
//
//  Function:  AmlMemAlloc
//
//  This function is used for memory allocation in OEMInit(). It need to reserved a 
//  pysical memory range 0x80220000 in config.bib
//
//
BYTE* AmlMemAlloc( UINT32 memSize )
{
    BYTE*    CurrentIdx = (BYTE*)g_pACPIMemory;

    OALMSG(OAL_AML_DBG, (L" +AmlMemAlloc(): Memory start  @  0x%x \r\n", g_pACPIMemory));

    // check the reserved memory boundary ( from 0x80220000 )
    if( (UINT32)( g_pACPIMemory + memSize ) > ( ACPI_MEM_RANGE ) ) // memory size 1MB
    {
        OALMSG(1, (L"AmlMemAlloc(): ACPI memory Out of Range \r\n"));
        return NULL;
    }

    // Update availabel memory ptr
    g_pACPIMemory = (BYTE*)(CurrentIdx + memSize);

    // initialize buffer
    //
    memset(CurrentIdx, 0, memSize);

    OALMSG(OAL_AML_DBG, (L" -AmlMemAlloc(): Allocate memory, 0x%x \r\n", CurrentIdx));

    return CurrentIdx;
}


//------------------------------------------------------------------------------
//
//  Function:  AmlInitParser
//
//  Initialize the ACPI_PARSE_STATE structure and load the DSDT table.
//
//
void AmlInitParser( ACPI_PARSE_STATE *Parser )
{
    if (Parser == NULL )
    {
        return;
    }

    OALMSG(OAL_AML_DBG, (L"AmlInitParser(): Parser address, 0x%x \r\n", Parser));

    memset( Parser, 0, sizeof(ACPI_PARSE_STATE) );

    Parser->AmlStart  = (UINT8 *)g_pDsdt;
    Parser->AmlEnd    = (UINT8 *)g_pDsdt;
    Parser->Aml       = (UINT8 *)g_pDsdt;
    Parser->AmlLength = (g_pDsdt->dwLength);

    OALMSG(OAL_AML_DBG, (L"AmlInitParser(): DSDT AML table length, 0x%x \r\n", Parser->AmlLength));
}



//------------------------------------------------------------------------------
//
//  Function:  AmlFindRootPCIBus
//
//  Search all AML to find out where the Root PCI bus is.
//
//
BOOL AmlFindRootPCIBus( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *aml;
    BYTE    ByteCount = 0;
    DWORD   PkgLength;
    BOOL    status;

    OALMSG(OAL_AML_DBG, (L"\r\n+AmlFindRootPCIBus()\r\n"));

    if (ParserState == NULL )
    {
        return FALSE;
    }

    aml = ParserState->Aml;

    // Find a Device Op
    while(  aml < (ParserState->Aml + ParserState->AmlLength) )
    {

        // DefDevice := DeviceOp PkgLength NameString ObjectList
        // 
        if( (*aml == PREFIX_EXTOP) && ( *(aml+1) == OP_DEVICE) )
        {
            OALMSG(OAL_AML_DBG, (L"Find a Device OP  @  0x%x\r\n", aml ));

            aml += 2; // DEVICE() op is a 2-byte op, offset to the next argument "PkgLeadByte"
            PkgLength = AmlDecodePkgLength( aml++, &ByteCount );

            OALMSG(OAL_AML_DBG, (L"IS Root Bus... PkgLength = 0x%x\r\n", PkgLength));

            status = AmlIsRootPCIBus( aml+ByteCount, PkgLength );
            if( status )
            {
                ParserState->Aml          = aml;
                ParserState->PkgStart     = aml;
                ParserState->RootPkgStart = aml;

                ParserState->RootPkgEnd = aml + PkgLength;
                ParserState->PkgEnd     = aml + PkgLength;

                ParserState->PkgLength = PkgLength;

                OALMSG(OAL_AML_DBG, (L"\r\nFound the Root PCI Bus.\r\n"));

                return TRUE;
            }

            // offset the package 
            aml += (PkgLength-1);

            OALMSG(1, (L"\r\n"));

        }
        aml++;
    }

    OALMSG(1, (L"DeviceOp doesn't exist in this DSDT table!!\r\n"));
    return FALSE;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlFindADeviceOp
//
//  Find a DEVICE() Op byte in the range of Root PCI bus, then decode its package
//  length and save this device info.
//  
//
BOOL AmlFindADeviceOp( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *src;
    BYTE    ByteCount = 0;
    DWORD   PkgLength;

    if (ParserState == NULL )
    {
        return FALSE;
    }

    src = ParserState->Aml;

    OALMSG(OAL_AML_DBG, (L"+AmlFindADeviceOp()\r\n"));

    // Find a Device Op in the range of Root PCI package
    while(  src < (ParserState->RootPkgEnd) )
    {
        // DefDevice := DeviceOp PkgLength NameString ObjectList
        // 
        if( (*src == PREFIX_EXTOP) && ( *(src+1) == OP_DEVICE) )
        {
            OALMSG(OAL_AML_DBG, (L"Find a Device OP  @  0x%x\r\n", src ));

            src += 2; // DEVICE() op is a 2-byte op, offset to the next argument "PkgLeadByte"

            PkgLength = AmlDecodePkgLength( src++, &ByteCount );

            // Save this device package info.
            //
            ParserState->Aml       = src;
            ParserState->PkgStart  = src;
            ParserState->PkgEnd    = src + PkgLength;
            ParserState->PkgLength = PkgLength;
            OALMSG(OAL_AML_DBG, (L"\r\nFound a Device Op.\r\n"));

            return TRUE;
        }
        src++;
    }

    OALMSG(OAL_AML_DBG, (L"DeviceOp doesn't exist in this DSDT table!!\r\n"));
    return FALSE;
}

//------------------------------------------------------------------------------
//
//  Function:  AmlFindIntLinkDevice
//
//  Find Interrupt Link device in the range of Root PCI bus by searching a IntLinkID.
//  
//  IntLinkID is a AML unique stream bytes: "Name(_HID,EISAID("PNP0C0F"))"
//
//
BOOL AmlFindIntLinkDevice( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *aml;

    if (ParserState == NULL )
    {
        return FALSE;
    }

    aml = ParserState->RootPkgStart;

    OALMSG(OAL_AML_DBG, (L"+AmlFindIntLinkDevice(), 0x%x\r\n", ParserState->RootPkgEnd));

    while(  aml < ParserState->RootPkgEnd )
    {
        //Search a _HID with "PNP0C0F" interrupt link device
        if( !memcmp( aml, IntLinkID, sizeof(IntLinkID)) )
        {
            // keep current position
            //
            ParserState->Aml = aml;

            return TRUE;
        }
        OALMSG(OAL_AML_DBG, (L"."));
        aml++;
    }

    return FALSE;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlFindPRTMethod
//
//  Find a Method() Op with the _PRT object in the range of DEVICE(), then check
//  whether this DEVICE() is requred or not by comparing its device name.
//  
//
BOOL AmlFindPRTMethod( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *aml;
    BYTE    ByteCount = 0;
    DWORD   PkgLength;
    BOOL    status;

    OALMSG(OAL_AML_DBG, (L"\r\n+AmlFindPRTMethod()\r\n"));

    if (ParserState == NULL )
    {
        return FALSE;
    }

    // Find a Method Op
    for (aml = ParserState->Aml ; aml < ( ParserState->PkgEnd ) ; aml++ )
    {
        // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
        //
        if( (*aml == OP_METHOD) )
        {
            OALMSG(OAL_AML_DBG, (L"Find a Method OP  @  0x%x\r\n", aml ));

            aml++; //offset to the next argument "PkgLeadByte"

            PkgLength = AmlDecodePkgLength( aml++ ,&ByteCount);
#ifdef ENABLE_NM10
			//|  1 byte | 1byte or more| 
			//|OP_METHOD| PkgLeadByte  | ...........PRT..........|
			//|<-------------------PkgLength-------------------->|
			//                   
			//Offset (PkgLength-2-ByteCount) to make sure it doesn't go out of the boundary
			status = AmlIs_PRTMethod( aml+ByteCount, (PkgLength-2-ByteCount) );
#else
            status = AmlIs_PRTMethod( aml+ByteCount, PkgLength );
#endif
            if( status )
            {
                ParserState->PkgADR = AmlGetDeviceADR( ParserState );
                ParserState->Aml = aml;
                ParserState->PkgStart = aml;
                ParserState->PkgEnd = aml + PkgLength;
                ParserState->PkgLength = PkgLength;

                OALMSG(OAL_AML_DBG, (L"\r\nFound the _PRT MethodOP. \r\n"));

                return TRUE;
            }

            // offset the package for next search
#ifdef ENABLE_NM10
			//Offset (PkgLength-2-ByteCount) to make sure it doesn't go out of the boundary
            aml += ( PkgLength-2-ByteCount );
#else
	        aml += ( PkgLength-1 );
#endif
            OALMSG(OAL_AML_DBG, (L"\r\n"));
        }
    }

    OALMSG(OAL_AML_DBG, (L"_PRT Method can't be found in this Device Op!!\r\n"));

    return FALSE;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlDecodePkgLength
//
//  Decode the Package Length of a AML OP code, get how many bytes occupied for length and 
//  return the package length(in Byte) for this AML Op code.
//
//
DWORD AmlDecodePkgLength( BYTE* aml ,BYTE *PkgByteCount )
{
    BYTE    *PkgLeadByte = aml;
    BYTE    ByteCount;
    BYTE    BitValid;
    DWORD   PkgLength = 0;

    OALMSG(OAL_AML_DBG, (L"AmlDecodePkgLength : PkgLeadByte = 0x%x\r\n", *PkgLeadByte));

    if (aml == NULL || PkgByteCount == NULL)
    {
        OALMSG(1, (L"AmlDecodePkgLength : invalid input pointer \r\n"));
        return 0;
    }

    // PkgLeadByte :=  <bit 7-6: ByteData count that follows (0-3)>
    //                 <bit 5-4: Only used if PkgLength < 63>
    //                 <bit 3-0: Least significant package length nybble>
    *PkgByteCount = ByteCount = (*PkgLeadByte) >> 6 ;

    // If the multiple bytes encoding is used, bits 0-3 of the PkgLeadByte 
    // become the least significant 4 bits of the resulting package length 
    // value. The next ByteData will become the next least significant 8 bits
    // of the resulting value and so on, up to 3 ByteData bytes.
    while( ByteCount )
    {
        PkgLength |= ( *(PkgLeadByte+ByteCount) << ( (ByteCount << 3) - 4 ));
        ByteCount--;
    }

    if( *PkgByteCount )
    {
        // If more than one byte bit 4 and 5 of the 
        // PkgLeadByte are reserved and must be zero.
        BitValid   = 0xF;
        PkgLength |= *PkgLeadByte & BitValid;
    }
    else
    {
        // If the PkgLength has only one byte, bit 0 through 5
        // are used to encode the package length.
        BitValid   = 0x3F;
        PkgLength |= *PkgLeadByte & BitValid;
    }
    return PkgLength;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlGetPRTInfo
//
//  In _PRT Method, Search for the Return Op to figure out how many entry of 
//  rouing record need to get, then, get rouing entery one-by-one.
//
// Mapping Fields of _PRT mapping packages :
//
//  Field |    [ Address ] [ PCI pin number ] [ Source ] [ Source Index ]
//  Type  |     (DWORD)           (BYTE)      (NamePath)     (DWORD)
//
//

VOID AmlGetPRTInfo( ACPI_PARSE_STATE *ParserState, ROUTING_INFO **RoutingTable )
{
    BYTE            *aml;
    BYTE            ByteCount;
    UINT32          PciAddr = 0;
    DWORD           PkgLength;
    BYTE            EntryCount=0;
    ROUTING_INFO    *Table;
    ROUTING_INFO    *PrevTable;
    BYTE            Entry;
    BYTE            LNK[3] = { 0x4C, 0x4E, 0x4B }; //"LNKx prefix"

    OALMSG(OAL_AML_DBG, (L"\r\n+AmlGetPRTInfo()\r\n"));

    if ( (ParserState == NULL) || (RoutingTable==NULL) )
    {
        OALMSG(1, (L"AmlGetPRTInfo() invalid input pointer \r\n"));
        return;
    }

    aml = ParserState->Aml;

    (*RoutingTable) = NULL;

    // traverse to Return op
    while( aml < ParserState->PkgEnd )
    {
        if( (*aml == OP_RETURN) && ( *(aml+1) == OP_PACKAGE ) )
        {
            aml += 2; //point to PkgLeadbyte of the Package() op

            AmlDecodePkgLength( aml++ ,&ByteCount);

            aml += ByteCount;  // point to NumElements of Package Op

            EntryCount = *aml; 

            aml++; //offset to the next Package() Op

            OALMSG(OAL_AML_DBG, (L"There are %d rouitng table entries.\r\n\r\n", EntryCount));
            break;
        }
        aml++;
    }

    if( !EntryCount )
    {
        OALMSG(1, (L"Can't find any rouitng table entries in this package.\r\n"));
        return;
    }

    OALMSG(OAL_AML_DBG, (L"Prepare to extract the table.\r\n"));

    // extract each device's routing table
    for( Entry = 0  ; Entry <  EntryCount ; Entry++ )
    {
        Table = (ROUTING_INFO *) AmlMemAlloc( sizeof(ROUTING_INFO) );
        if( !Table )
        {
            OALMSG(1, (L"Can't allocate memory for routing table.\r\n"));
            return;
        }

        aml++; //point to PkgLeadbyte of the Package() op

        PkgLength = AmlDecodePkgLength( aml++ ,&ByteCount);

        // 1st Element: Device address
        aml += (ByteCount + 1); //PkgLength + NumElements

        OALMSG(OAL_AML_DBG, (L"1st Element: 0x%x\r\n", *aml));

        switch( *aml )
        {
        case PREFIX_WORD: // 2-bytes element
            aml++; // move to the content

            memcpy( (UINT8*)&PciAddr, aml,  sizeof(WORD));
            OALMSG(OAL_AML_DBG, (L"Extract Device number= 0x%x \r\n", PciAddr));

            // PCI adress encoding : High word-Device #, Low word-Function # 
            //                           
            Table->DevNum = (UINT8)( PciAddr >> 16 );
			Table->fClaimed = FALSE;
            aml += sizeof(WORD);

            break;

        case PREFIX_DWORD: // 3-bytes element
            aml++; // move to the content

            memcpy( (UINT8*)&PciAddr, aml,  sizeof(DWORD) );

            OALMSG(OAL_AML_DBG, (L"Extract Device number= 0x%x \r\n", PciAddr));
            Table->DevNum = (UINT8)( PciAddr >> 16 );
            aml += sizeof(DWORD);

            break;

        default:
            OALMSG(1, (L"Extract Device number failed!! \r\n"));
        }

        OALMSG(OAL_AML_DBG, (L"Prepare to extract the interrupt line.\r\n"));

        // 2nd Element: Interrupt Line

        OALMSG(OAL_AML_DBG, (L"2nd Element: 0x%x\r\n", *aml));

        switch( *aml )
        {
        case OP_ZEOR: //
            Table->IntPin = 0;
            aml += 1;
            break;

        case OP_ONE: // 
            Table->IntPin = 1;
            aml += 1;
            break;

        case PREFIX_BYTE: // 

            aml++; // Offset this Prefix byte to the data

            Table->IntPin = *(aml);
            aml += 1;
            break;

        default:
            OALMSG(1, (L"Extract Device IntPin failed!! \r\n"));
        }

        OALMSG(OAL_AML_DBG, (L"Prepare to extract the IRQ.\r\n"));

        //"\_SB_.PCI0.LPC.LNKx"
        while( memcmp( aml, LNK, 3 ) )
        {
            aml++; 
        }
        aml += 3; // move to the x position

        OALMSG(OAL_AML_DBG, (L"3rd Element: 0x%x\r\n", *aml));

        switch( *aml )
        {
        case 'A': //
            Table->IntLine = PIRQA;
            aml += 1;
            break;

        case 'B': // 
            Table->IntLine = PIRQB;
            aml += 1;
            break;

        case 'C': // 
            Table->IntLine = PIRQC;
            aml += 1;
            break;

        case 'D': // 
            Table->IntLine = PIRQD;
            aml += 1;
            break;

        case 'E': // 
            Table->IntLine = PIRQE;
            aml += 1;
            break;

        case 'F': // 
            Table->IntLine = PIRQF;
            aml += 1;
            break;

        case 'G': // 
            Table->IntLine = PIRQG;
            aml += 1;
            break;

        case 'H': // 
            Table->IntLine = PIRQH;
            aml += 1;
            break;
        default:
            OALMSG(1, (L"Extract Device IntPin failed!! \r\n"));
        }

        aml++; //Pass throught the 4th Element to the next Package Op

        OALMSG(OAL_AML_DBG, (L"Routing Table : Dev=0x%x, IntPin=0x%d, IntLine=%d\r\n"
            ,Table->DevNum, Table->IntPin, Table->IntLine));

        if( (*RoutingTable) )
        {
            // Append to existing list
            PrevTable = (*RoutingTable);
            while( PrevTable->next )
            {
                PrevTable = PrevTable->next;
                OALMSG(OAL_AML_DBG, (L"."));
            }
            PrevTable->next = Table;
        }
        else
        {
            *RoutingTable = Table;
        }
    }

}


//------------------------------------------------------------------------------
//
//  Function:  AmlGetIRQDescriptor
//
//  Under _PRS object, parser the following BufferOp in order to get the PCI IRQ 
//  Descriptor, defined in ACPI spec., in which includes the available PIRQ number
//  for the PCI registers PIRQ[A-D] and PIRQ[E-H].
//
//
//  IntLinkID is a AML unique stream bytes: "Name(_HID,EISAID("PNP0C0F"))"
//
//
BOOL AmlGetIRQDescriptor( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *aml = NULL;
    BYTE    ByteCount = 0;
    DWORD   PkgLength;

    AML_PIRQ_IRQ_TBL    IrqTbl;
    BYTE                pirq;
    BYTE                IrqCount = 0;
    WORD                NumsIrq;
    BOOL                status;

    OALMSG(OAL_AML_DBG, (L"+AmlGetIRQDescriptor()\r\n"));

    // initialization
    IrqTbl.bPIRQX = 0;

    if ( ParserState == NULL )
    {
        OALMSG(1, (L"AmlGetIRQDescriptor() invalid input pointer \r\n"));
        return FALSE;
    }

    status = AmlSeekToPRS( ParserState );
    if( !status )
    {
        OALMSG(1, (L"Can't found any _PRS object.\r\n"));
        return FALSE;
    }

    aml = ParserState->Aml;

    //check if a BufferOp
    if( *aml == OP_BUFFER )
    {
        // DefBuffer := BufferOp PkgLength BufferSize <IRQ Descriptor>
        //

        aml++; //offset to the PkgLength

        // 1st "PkgLength" argument
        //
        PkgLength = AmlDecodePkgLength( aml++ ,&ByteCount);

        aml += ByteCount;

        // 2nd "BufferSize" argument
        //
        switch( *aml )
        {
        case PREFIX_BYTE: 
            aml += (sizeof(BYTE)+ 1); // move to the next content
            break;

        case PREFIX_WORD: 
            aml += (sizeof(BYTE)+ 1); // move to the next content
            break;

        case PREFIX_DWORD: 
            aml += (sizeof(BYTE)+ 1); // move to the next content
            break;
        }

        // 3rd "IRQ Descriptor" argument
        //
        memcpy( (BYTE *)(&IrqTbl.wNumsIrq), (aml+1), sizeof(WORD) );

        OALMSG(OAL_AML_DBG, (L"IRQ bit mask = 0x%x\r\n", IrqTbl.wNumsIrq ));

        // Count how many available IRQ number in this PIRQ
        NumsIrq = IrqTbl.wNumsIrq;

        while( NumsIrq )
        {
            if( NumsIrq & 0x01 )
            {
                IrqCount++;
            }
            NumsIrq = NumsIrq >> 1;
        }
        IrqTbl.bIrqNums = IrqCount;

        OALMSG(OAL_AML_DBG, (L"IRQ number = 0x%x\r\n", IrqCount ));


        // Fill in the IRQ resource into g_PirqTbl for getting up
        // PIRQ[A-D] and PIRQ[E-H] in the further.
        //
        for( pirq = 0; pirq < MAX_PIRQ_NUM ; pirq++)
        {
            memcpy( &g_PirqTbl[pirq], &IrqTbl, sizeof(IrqTbl) );
            g_PirqTbl[pirq].bPIRQX = pirq;

            OALMSG(OAL_AML_DBG, (L"#PIRQ=0x%x, #count=0x%x, Value= 0x%x\r\n", 
            g_PirqTbl[pirq].bPIRQX,g_PirqTbl[pirq].bIrqNums,g_PirqTbl[pirq].wNumsIrq));
        }
    }
    else
    {
        OALMSG(1, (L"Get IRQ descriptro failed.\r\n"));
        return FALSE;
    }

    return TRUE;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlGetDeviceADR
//
//  Find a Name OP with _ADR object under this Device package in order to get
//  the address of this PCI device.
//  
//
BYTE AmlGetDeviceADR( ACPI_PARSE_STATE *ParserState )
{
    BYTE    *aml;
    DWORD   PciAddr=0;

    if ( ParserState == NULL )
    {
        OALMSG(1, (L"AmlGetDeviceADR() invalid input pointer \r\n"));
        return NULL;
    }

    OALMSG(OAL_AML_DBG, (L"\r\n==== GetDeviceAddress ==== 0x%x\r\n", ParserState->PkgStart));

    // Find a Name Op with _ADR : Name( _ADR, xxxx )
    for (aml = ParserState->PkgStart; aml < ( ParserState->PkgEnd ) ; aml++ )
    {
        //Seek to a Name Op with _ADR : Name( _ADR, <PCI Device Address> )
        //
        if( !memcmp( aml, ADR, sizeof(ADR)) )
        {
            aml += sizeof(ADR);

            switch( *aml )
            {
            case OP_ZEOR :
                PciAddr = 0;
                break;

            case PREFIX_WORD : // 2-bytes element
                aml++; // move to the content

                memcpy( (UINT8*)&PciAddr, aml,  sizeof(WORD));

                OALMSG(OAL_AML_DBG, (L"Extract Device number= 0x%x \r\n", PciAddr));

                break;

            case PREFIX_DWORD : // 3-bytes element
                aml++; // move to the content

                memcpy( (UINT8*)&PciAddr, aml,  sizeof(DWORD) );

                OALMSG(OAL_AML_DBG, (L"Extract Device number= 0x%x \r\n", PciAddr));

                break;

            }

            // PCI adress encoding : High word-Device #, Low word-Function # 
            //
            return (BYTE)( PciAddr >> 16 );
        }
        OALMSG(OAL_AML_DBG, (L"."));
    }

    OALMSG(1, (L"_ADR object cann't be found in this Device Op!!\r\n"));

    return NULL;
}



//------------------------------------------------------------------------------
//
//  Function:  AmlIsRootPCIBus
//
//  Determined by checking " Name(_HID,EISAID ("PNP0A08"))" under this device OP.
//
//  "PNP0A08" is a index of Root PCI bridge.
//
BOOL AmlIsRootPCIBus( BYTE *aml, DWORD PkgLength )
{
    BYTE    *PkgEnd;

    if ( aml == NULL )
    {
        OALMSG(1, (L"AmlIsRootPCIBus() invalid input pointer \r\n"));
        return FALSE;
    }

    PkgEnd = aml + PkgLength;

    // Look though this package's _HID is "PNP0A08"
    while( aml < PkgEnd )
    {
        if( !memcmp( aml, RootBusID, sizeof(RootBusID)) )
        {
            return TRUE;
        }
        OALMSG(OAL_AML_DBG, (L"."));
        aml++;
    }
    return FALSE;
}


//------------------------------------------------------------------------------
//
//  Function:  AmlIs_PRTMethod
//
// Determined by checking "_PRT" prefix under this Method OP.
// 
//
BOOL AmlIs_PRTMethod( BYTE *aml, DWORD PkgLength )
{
    BYTE    *PkgEnd;

    if ( aml == NULL )
    {
        OALMSG(1, (L"AmlIs_PRTMethod() invalid input pointer \r\n"));
        return FALSE;
    }

    OALMSG(OAL_AML_DBG, (L"+AmlIs_PRTMethod()\r\n"));

    PkgEnd = aml + PkgLength;

    // look though this package's _HID is "PNP0A08"
    while( aml < PkgEnd )
    {
        if( !memcmp( aml, PRT, sizeof(PRT) ))
        {
            return TRUE;
        }

        OALMSG(OAL_AML_DBG, (L"."));
        aml++;
    }

    return FALSE;
}


//------------------------------------------------------------------------------
//
//  Function:  AmlSeekToPRS
//
//  Find a Name OP with _PRS object under this Device package.
//  
//
BOOL AmlSeekToPRS( ACPI_PARSE_STATE *ParserState )
{

    BYTE    *aml;

    if ( ParserState == NULL )
    {
        OALMSG(1, (L"AmlSeekToPRS() invalid input pointer \r\n"));
        return FALSE;
    }

    OALMSG(OAL_AML_DBG, (L"+AmlSeekToPRS()\r\n"));

    aml = ParserState->Aml;

    // Seek to a Name Op with _PRS : Name( _PRS, <IRQ resource> )
    while( aml < ParserState->RootPkgEnd )
    {
        if( !memcmp( aml, PRS, sizeof(PRS)) )
        {
            ParserState->Aml = aml + sizeof(PRS);
            return TRUE;
        }
        OALMSG(OAL_AML_DBG, (L"."));
        aml++;
    }

    return FALSE;
}

