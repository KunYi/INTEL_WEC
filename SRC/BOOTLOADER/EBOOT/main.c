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
/*++

    Ethernet boot loader main module. This file contains the C main
    for the boot loader.    NOTE: The firmware "entry" point (the real
    entry point is _EntryPoint in init assembler file.

    The Windows CE boot loader is the code that is executed on a Windows CE
    development system at power-on reset and loads the Windows CE
    operating system. The boot loader also provides code that monitors
    the behavior of a Windows CE platform between the time the boot loader
    starts running and the time the full operating system debugger is 
    available. Windows CE OEMs are supplied with sample boot loader code 
    that runs on a particular development platform and CPU.

--*/

#include <windows.h>
#include <nkintr.h>
#include <bootarg.h>
#include <pc.h>
#include <ceddk.h>
#include <pehdr.h>
#include <romldr.h>
#include <blcommon.h>
#include "eboot.h"
#include <kitlprot.h>
#include "x86kitl.h"
#include <pci.h>

#define PLATFORM_STRING             "CEPC"

#define BOOT_ARG_PTR_LOCATION_NP    0x001FFFFC
#define BOOT_ARG_LOCATION_NP        0x001FFF00

#define ETHDMA_BUFFER_BASE      0x00200000
#define ETHDMA_BUFFER_SIZE      0x00020000

// Embed CE Version with which Eboot was compiled
// Important!! -- Must be at top of file to work
// Please do not add globals above this point!
#ifdef OS_VERSION
__declspec(align(4)) const char  OS_VER_STR1[] = "$@(";
__declspec(align(4)) const DWORD OS_VER        = OS_VERSION;    
__declspec(align(4)) const char  OS_VER_STR2[] = "$@(";
#endif

// FLASH backup support
BOOL g_bDownloadImage = TRUE;
// Multi-XIP
#define FLASH_BIN_START 0  // FLASH Offset.
MultiBINInfo g_BINRegionInfo;
DWORD g_dwMinImageStart;

const OAL_KITL_ETH_DRIVER *g_pEdbgDriver;

USHORT wLocalMAC[3];

BOOL PrintPCIConfig();

PVOID GetKernelExtPointer(DWORD dwRegionStart, DWORD dwRegionLength);

// file globals
static BOOT_ARGS *pBootArgs;    // bootarg pointer
static EDBG_ADDR MyAddr;        // CEPC address
static BOOL AskUser (EDBG_ADDR *pMyAddr, DWORD *pdwSubnetMask);

typedef void (*PFN_LAUNCH)();
extern PFN_OEMVERIFYMEMORY g_pOEMVerifyMemory;
DWORD   g_dwStartAddr, g_dwLength, g_dwOffset;
void DrawProgress (int Percent, BOOL fBackground);

// stub OAL functions
struct {
    void (*pfnAcquireOalSpinLock)();
}g_pNKGlobal;
BOOL IsAfterPostInit() { return FALSE; }


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SpinForever(void)
{
    KITLOutputDebugString("SpinForever...\r\n");
    while(1);
}

//------------------------------------------------------------------------------
//
//  Function Name:  OEMVerifyMemory (DWORD dwStartAddr, DWORD dwLength)
//
//  Description:    This function verifies that the passed in memory range
//                  falls within a valid address. It also sets the global
//                  Start and Length values - this should be moved elsewhere.
//
//------------------------------------------------------------------------------

BOOL OEMVerifyMemory (DWORD dwStartAddr, DWORD dwLength)
{
    BOOL    rc;                 // return code
    DWORD   Addr1;              // starting address
    DWORD   Addr2;              // ending   address

    // Setup address range for comparison

    Addr1 = dwStartAddr;
    Addr2 = Addr1 + (dwLength - 1);

    KITLOutputDebugString( "****** OEMVerifyMemory Checking Range [ 0x%x ==> 0x%x ]\r\n", Addr1, Addr2 );

    // Validate each range

    // since we don't have pTOC information yet and we don't know exactly how much memory is on the CEPC,
    // We'll just skip the end address testing.
    // if( (Addr1 >= RAM_START) && (Addr2 <= RAM_END) )
    if (Addr1 >= RAM_START)
    {
        KITLOutputDebugString("****** RAM Address ****** \r\n\r\n");

        rc = TRUE;
    }
    else
    {
        KITLOutputDebugString("****** OEMVerifyMemory FAILED - Invalid Memory Area ****** \r\n\r\n");

        rc = FALSE;
    }

    // If the range is verified set the start and length globals

    if( rc == TRUE )
    {
        // Update progress 

        DrawProgress (100, TRUE); 

        g_dwStartAddr = dwStartAddr;
        g_dwLength = dwLength;
    }

    // Indicate status

    return( rc );
}

//------------------------------------------------------------------------------

BOOL OEMDebugInit (void)
{
    // Initialize the monitor port (for development system)
    // This should be done first since otherwise we can not
    // print any message onto the monitor.
    
    OEMInitDebugSerial();

    // Set the pointer to our VerifyMemory routine

    g_pOEMVerifyMemory = OEMVerifyMemory;

    // Indicate success

    return( TRUE );
}

//------------------------------------------------------------------------------

BOOL OEMPlatformInit (void)
{
    extern void BootUp (void);
    PCKITL_NIC_INFO pNicInfo;

    KITLOutputDebugString("Microsoft Windows CE Ethernet Bootloader %d.%d for CE/PC (%s)\n",
                          EBOOT_VERSION_MAJOR,EBOOT_VERSION_MINOR, __DATE__);

    // init PCI config mechanism
    PCIInitConfigMechanism (1);
    
    //
    // Get pointer to Boot Args...
    //
    pBootArgs = (BOOT_ARGS *) ((ULONG)(*(PBYTE *)BOOT_ARG_PTR_LOCATION_NP));
    KITLOutputDebugString("Boot Args @ 0x%x and  ucLoaderFlags is %x \r\n", pBootArgs,pBootArgs->ucLoaderFlags);

    pBootArgs->dwEBootAddr = (DWORD) BootUp;
    pBootArgs->ucLoaderFlags &= LDRFL_FLASH_BACKUP ;

    //
    // What PCI hardware is available?
    //
    PrintPCIConfig();

    if (EDBG_ADAPTER_DEFAULT == pBootArgs->ucEdbgAdapterType) {
        pBootArgs->ucEdbgAdapterType = EDBG_ADAPTER_NE2000;
    }

    pNicInfo = InitKitlNIC (pBootArgs->ucEdbgIRQ, pBootArgs->dwEdbgBaseAddr, pBootArgs->ucEdbgAdapterType);

    if (!pNicInfo) {
        KITLOutputDebugString ("Unable to find NIC card, spin forever\r\n");
        return FALSE;
    }

    g_pEdbgDriver = pNicInfo->pDriver;

    // update bootarg
    pBootArgs->ucEdbgAdapterType = (UCHAR) pNicInfo->dwType;
    pBootArgs->ucEdbgIRQ         = (UCHAR) pNicInfo->dwIrq;
    pBootArgs->dwEdbgBaseAddr    = pNicInfo->dwIoBase;

    //
    // Initialize NIC DMA buffer, if required.
    //
    if (g_pEdbgDriver->pfnInitDmaBuffer
        && !g_pEdbgDriver->pfnInitDmaBuffer (ETHDMA_BUFFER_BASE, ETHDMA_BUFFER_SIZE)) {

        KITLOutputDebugString("ERROR: Failed to initialize Ethernet controller DMA buffer.\r\n");
        return FALSE;
    }

    // Call driver specific initialization.
    //
    if (!g_pEdbgDriver->pfnInit ((BYTE *) pBootArgs->dwEdbgBaseAddr, 1, MyAddr.wMAC)) {
        KITLOutputDebugString("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\r\n");
        KITLOutputDebugString("---------------------------------------------\r\n");
        KITLOutputDebugString(" Failed to initialize Ethernet board!\r\n");
        KITLOutputDebugString(" Please check that the Ethernet card is\r\n");
        KITLOutputDebugString(" properly installed and configured.\r\n");
        KITLOutputDebugString("---------------------------------------------\r\n");
        KITLOutputDebugString("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\r\n");
        return FALSE;
    }


    // Display the MAC address returned by the EDBG driver init function.
    //
    KITLOutputDebugString("Returned MAC Address:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                           MyAddr.wMAC[0] & 0x00FF, MyAddr.wMAC[0] >> 8,
                           MyAddr.wMAC[1] & 0x00FF, MyAddr.wMAC[1] >> 8,
                           MyAddr.wMAC[2] & 0x00FF, MyAddr.wMAC[2] >> 8);

    return(TRUE);
}

DWORD OEMPreDownload (void)
{
    DWORD dwSubnetMask, dwBootFlags = 0;
    DWORD DHCPLeaseTime = DEFAULT_DHCP_LEASE, *pDHCPLeaseTime = &DHCPLeaseTime;
    char szDeviceName[EDBG_MAX_DEV_NAMELEN], szNameRoot[EDBG_MAX_DEV_NAMELEN];
    BOOL fGotJumpImg = FALSE;

    //
    // set bootloader capability
    //
    // support passive-kitl
    KITLOutputDebugString("OEMPreDownload  ucLoaderFlags is %x \r\n",pBootArgs->ucLoaderFlags);
    dwBootFlags = EDBG_CAPS_PASSIVEKITL;

    // check if this is a cold boot. Force download if so.
    if (BOOTARG_SIG != pBootArgs->dwEBootFlag) {
        dwBootFlags |= EDBG_BOOTFLAG_FORCE_DOWNLOAD;
    }

    // Create device name based on Ethernet address
    // If a custom device name was specified on the loadcepc command line,
    // use it.
    //
    if ((pBootArgs->dwVersionSig == BOOT_ARG_VERSION_SIG) && 
        (pBootArgs->szDeviceNameRoot[0] != '\0'))
    {
        StringCbCopyA(szNameRoot, sizeof(szNameRoot), pBootArgs->szDeviceNameRoot);
    }
    else
    {
        StringCbCopyA(szNameRoot, sizeof(szNameRoot), PLATFORM_STRING);
        if (pBootArgs->dwVersionSig == BOOT_ARG_VERSION_SIG)
            StringCchCopyA(pBootArgs->szDeviceNameRoot, EDBG_MAX_DEV_NAMELEN, PLATFORM_STRING);
    }

    x86KitlCreateName (szNameRoot, MyAddr.wMAC, szDeviceName);
    KITLOutputDebugString("Using device name: %s\n", szDeviceName);

    // Assign a default subnet mask.
    dwSubnetMask = ntohl(0xffff0000);
    
    // IP address may have been passed in on loadcepc cmd line
    // give user a chance to enter IP address if not
    if (!pBootArgs->EdbgAddr.dwIP && AskUser (&MyAddr, &dwSubnetMask)) {
        // user entered static IP
        pBootArgs->EdbgAddr.dwIP = MyAddr.dwIP;
    }

    // use the static IP address if we have one
    if (pBootArgs->EdbgAddr.dwIP) {
        KITLOutputDebugString("Using static IP address: %X\r\n",pBootArgs->EdbgAddr.dwIP);
        MyAddr.dwIP = pBootArgs->EdbgAddr.dwIP;
        pDHCPLeaseTime = NULL;  // Skip DHCP
        pBootArgs->EdbgFlags |= EDBG_FLAGS_STATIC_IP;
    }

    // initialize TFTP transport
    if (!EbootInitEtherTransport (&MyAddr, &dwSubnetMask, &fGotJumpImg, pDHCPLeaseTime,
        EBOOT_VERSION_MAJOR, EBOOT_VERSION_MINOR, PLATFORM_STRING, szDeviceName, EDBG_CPU_i486, dwBootFlags)) {
        return BL_ERROR;
    }

    // update BOOTARG with the info we got so far
    memcpy(&pBootArgs->EdbgAddr,&MyAddr,sizeof(MyAddr));
    pBootArgs->ucLoaderFlags |= LDRFL_USE_EDBG | LDRFL_ADDR_VALID | LDRFL_JUMPIMG;
    pBootArgs->DHCPLeaseTime = DHCPLeaseTime;

    g_bDownloadImage=(fGotJumpImg?FALSE:TRUE);
    return fGotJumpImg? BL_JUMP : BL_DOWNLOAD;
}

void OEMLaunch (DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr)
{
    EDBG_OS_CONFIG_DATA *pCfgData;
    EDBG_ADDR EshellHostAddr;

    KITLOutputDebugString("OEMLaunch   ucLoaderFlags is %x \r\n",pBootArgs->ucLoaderFlags);

    // Pretend we're 100% done with download
    DrawProgress (100, FALSE);

    memset (&EshellHostAddr, 0, sizeof (EshellHostAddr));

    if (!dwLaunchAddr) {
        dwLaunchAddr = pBootArgs->dwLaunchAddr;
    } else {
        pBootArgs->dwLaunchAddr = dwLaunchAddr;
    }

    // wait for the jump command from eshell unless user specify jump directly
    KITLOutputDebugString ("Download successful! Jumping to image at %Xh...\r\n", dwLaunchAddr);
    if (!(pCfgData = EbootWaitForHostConnect (&MyAddr, &EshellHostAddr))) {
        KITLOutputDebugString ("EbootWaitForHostConenct failed, spin forever\r\n");
        SpinForever();
        return;
    }

    // update service information
    if (pCfgData->Flags & EDBG_FL_DBGMSG) {
        KITLOutputDebugString("Enabling debug messages over Ethernet, IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->DbgMsgIPAddr),ntohs(pCfgData->DbgMsgPort));
        memcpy(&pBootArgs->DbgHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->DbgHostAddr.dwIP  = pCfgData->DbgMsgIPAddr;
        pBootArgs->DbgHostAddr.wPort = pCfgData->DbgMsgPort;
    }
    if (pCfgData->Flags & EDBG_FL_PPSH) {
        KITLOutputDebugString("Enabling CESH over Ethernet,           IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->PpshIPAddr),ntohs(pCfgData->PpshPort));
        memcpy(&pBootArgs->CeshHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->CeshHostAddr.dwIP  = pCfgData->PpshIPAddr;
        pBootArgs->CeshHostAddr.wPort = pCfgData->PpshPort;
    }
    if (pCfgData->Flags & EDBG_FL_KDBG) {
        KITLOutputDebugString("Enabling KDBG over Ethernet,           IP: %s, port:%u\n",
                              inet_ntoa(pCfgData->KdbgIPAddr),ntohs(pCfgData->KdbgPort));
        memcpy(&pBootArgs->KdbgHostAddr.wMAC,&EshellHostAddr.wMAC,6);
        pBootArgs->KdbgHostAddr.dwIP  = pCfgData->KdbgIPAddr;
        pBootArgs->KdbgHostAddr.wPort = pCfgData->KdbgPort;
    }
    pBootArgs->ucEshellFlags = pCfgData->Flags;
    pBootArgs->dwEBootFlag = BOOTARG_SIG;
    pBootArgs->KitlTransport = pCfgData->KitlTransport;
    memcpy (&pBootArgs->EshellHostAddr, &EshellHostAddr, sizeof(EDBG_ADDR));

    KITLOutputDebugString("Lauch Windows CE from address 0x%x\r\n",dwLaunchAddr);
    ((PFN_LAUNCH)(dwLaunchAddr))();

    // never reached
    SpinForever ();
}
void
DrawProgress (int Percent, BOOL fBackground)
{
#if DRAW_PROGRESS_BAR
    int     i, j;
    int     iWidth = Percent*(pBootArgs->cyDisplayScreen/100);
    PWORD   pwData;
    PDWORD  pdwData;

    if ((pBootArgs->pvFlatFrameBuffer)) {
        // 20 rows high
        for (i=0; i < 20; i++) {
            switch (pBootArgs->bppScreen) {
            case 8 :
                memset ((PBYTE)pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20), fBackground ? 3 : 15,
                        iWidth);
                break;
            case 16 :
                pwData = (PWORD)(pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20));
                for (j=0; j < iWidth; j++) {
                    *pwData++ = fBackground ? 0x001f : 0xFFFF;
                }
                break;
            case 32 :
                pdwData = (PDWORD)(pBootArgs->pvFlatFrameBuffer + pBootArgs->cbScanLineLength*(i+20));
                for (j=0; j < iWidth; j++) {
                    *pdwData++ = fBackground ? 0x000000FF : 0xFFFFFFFF;
                }
                break;
            default :
                // Unsupported format, ignore
                break;
            }
        }
    }
#endif  // DRAW_PROGRESS_BAR.
}

// callback to receive data from transport
BOOL OEMReadData (DWORD cbData, LPBYTE pbData)
{
    BOOL fRetVal;
    int cPercent;
    if (fRetVal = EbootEtherReadData (cbData, pbData)) {
        g_dwOffset += cbData;
        if (g_dwLength) {
            cPercent = g_dwOffset*100/g_dwLength;
            DrawProgress (cPercent, FALSE);
        }
    }
    return fRetVal;
}

//
// Memory mapping related functions
//
LPBYTE OEMMapMemAddr (DWORD dwImageStart, DWORD dwAddr)
{
    // map address into physical address
    return (LPBYTE) (dwAddr & ~0x80000000);
}

//
// CEPC doesn't have FLASH, LED, stub the related functions
//
void OEMShowProgress (DWORD dwPacketNum)
{
}

BOOL OEMIsFlashAddr (DWORD dwAddr)
{
    return FALSE;
}

BOOL OEMStartEraseFlash (DWORD dwStartAddr, DWORD dwLength)
{
    return FALSE;
}

void OEMContinueEraseFlash (void)
{
}

BOOL OEMFinishEraseFlash (void)
{
    return FALSE;
}



//------------------------------------------------------------------------------
/* OEMEthGetFrame
 *
 *   Check to see if a frame has been received, and if so copy to buffer.
 *
 * Return Value:
 *    Return TRUE if frame has been received, FALSE if not.
 */
//------------------------------------------------------------------------------
BOOL
OEMEthGetFrame(
    BYTE *pData,       // OUT - Receives frame data
    UINT16 *pwLength)  // IN  - Length of Rx buffer
                       // OUT - Number of bytes received
{
    return g_pEdbgDriver->pfnGetFrame (pData, pwLength);
}



//------------------------------------------------------------------------------
/* OEMEthSendFrame
 *
 *   Send Ethernet frame.
 *
 *  Return Value:
 *   TRUE if frame successfully sent, FALSE otherwise.
 */
//------------------------------------------------------------------------------
BOOL
OEMEthSendFrame(
    BYTE *pData,     // IN - Data buffer
    DWORD dwLength)  // IN - Length of buffer
{
    int retries = 0;

    // Let's be persistant here
    while (retries++ < 4) {
        if (!g_pEdbgDriver->pfnSendFrame (pData, dwLength))
            return TRUE;
        else
            KITLOutputDebugString("!OEMEthSendFrame failure, retry %u\n",retries);
    }
    return FALSE;
}


//------------------------------------------------------------------------------
//
// rtc cmos functions, in oal_x86_rtc.lib
//
BOOL Bare_GetRealTime(LPSYSTEMTIME lpst);


//------------------------------------------------------------------------------
//  OEMGetRealTime
//
//  called by the kitl library to handle OEMKitlGetSecs
//
//------------------------------------------------------------------------------
BOOL OEMGetRealTime(LPSYSTEMTIME lpst)
{
    return Bare_GetRealTime(lpst);
}

//
// read a line from debug port, return FALSE if timeout
// (timeout value in seconds)
//
#define BACKSPACE   8
#define IPADDR_MAX  15      // strlen ("xxx.xxx.xxx.xxx") = 15
static char szbuf[IPADDR_MAX+1];

static BOOL ReadIPLine (char *pbuf, DWORD dwTimeout)
{
    DWORD dwCurrSec = OEMKitlGetSecs ();
    char ch;
    int nLen = 0;
    while (OEMKitlGetSecs () - dwCurrSec < dwTimeout) {

        switch (ch = OEMReadDebugByte ()) {
        case OEM_DEBUG_COM_ERROR:
        case OEM_DEBUG_READ_NODATA:
            // no data or error, keep reading
            break;

        case BACKSPACE:
            nLen --;
            OEMWriteDebugByte (ch);
            break;

        case '\r':
        case '\n':
            OEMWriteDebugByte ('\n');
            pbuf[nLen] = 0;
            return TRUE;

        default:
            if ((ch == '.' || (ch >= '0' && ch <= '9')) && (nLen < IPADDR_MAX)) {
                pbuf[nLen ++] = ch;
                OEMWriteDebugByte (ch);
            }
        }
    }

    return FALSE;   // timeout
}

DWORD
OEMKitlGetSecs ()
{
    SYSTEMTIME st;

    OEMGetRealTime( &st );
    return ((60UL * (60UL * (24UL * (31UL * st.wMonth + st.wDay) + st.wHour) + st.wMinute)) + st.wSecond);
}

DWORD
OALGetTickCount(
    )
{
    return OEMKitlGetSecs() * 1000;
}

// ask user to select transport options
static BOOL AskUser (EDBG_ADDR *pMyAddr, DWORD *pdwSubnetMask)
{

    KITLOutputDebugString ("\rHit ENTER within 3 seconds to enter static IP address!");
    szbuf[IPADDR_MAX] = 0;  // for safe measure, make sure null termination
    if (!ReadIPLine (szbuf, 3)) {
        return FALSE;
    }

    KITLOutputDebugString ("\rEnter IP address, or CR for default (%s): ", inet_ntoa (pMyAddr->dwIP));
    ReadIPLine (szbuf, INFINITE);
    if (szbuf[0]) {
        pMyAddr->dwIP = inet_addr (szbuf);
    }

    KITLOutputDebugString ("\rEnter Subnet Masks, or CR for default (%s): ", inet_ntoa (*pdwSubnetMask));
    ReadIPLine (szbuf, INFINITE);
    if (szbuf[0]) {
        *pdwSubnetMask = inet_addr (szbuf);
    }

    KITLOutputDebugString ( "\r\nUsing IP Address %s, ", inet_ntoa (pMyAddr->dwIP));
    KITLOutputDebugString ( "subnet mask %s\r\n", inet_ntoa (*pdwSubnetMask));
    return TRUE;
}
// Dummmy
LPVOID NKCreateStaticMapping(DWORD dwPhysBase, DWORD dwSize)
{
    return (LPVOID)dwPhysBase;
}


/*
    @func   void | OEMDownloadFileNotify | Receives/processed download file manifest.
    @rdesc  
    @comm    
    @xref   
*/
void OEMDownloadFileNotify(PDownloadManifest pInfo)
{
    BYTE nCount;

    if (!pInfo || !pInfo->dwNumRegions)
    {
        KITLOutputDebugString("WARNING: OEMDownloadFileNotify - invalid BIN region descriptor(s).\r\n");
        return;
    }

    g_dwMinImageStart = pInfo->Region[0].dwRegionStart;

    KITLOutputDebugString("\r\nDownload file information:\r\n");
    KITLOutputDebugString("-----------------------------------------------------\r\n");
    for (nCount = 0 ; nCount < pInfo->dwNumRegions ; nCount++)
    {
        KITLOutputDebugString("[%d]: Address=0x%x  Length=0x%x  Name=%s\r\n", nCount, 
                                                                              pInfo->Region[nCount].dwRegionStart, 
                                                                              pInfo->Region[nCount].dwRegionLength, 
                                                                              pInfo->Region[nCount].szFileName);

        if (pInfo->Region[nCount].dwRegionStart < g_dwMinImageStart)
        {
            g_dwMinImageStart = pInfo->Region[nCount].dwRegionStart;
            if (g_dwMinImageStart == 0)
            {
                KITLOutputDebugString("WARNING: OEMDownloadFileNotify - bad start address for file (%s).\r\n", pInfo->Region[nCount].szFileName);
                return;
            }
        }
    }
    KITLOutputDebugString("\r\n");

    memcpy((LPBYTE)&g_BINRegionInfo, (LPBYTE)pInfo, sizeof(MultiBINInfo));
}


static PVOID GetKernelExtPointer(DWORD dwRegionStart, DWORD dwRegionLength)
{
    DWORD dwCacheAddress = 0;
    ROMHDR *pROMHeader;
    DWORD dwNumModules = 0;
    TOCentry *pTOC;

    if (dwRegionStart == 0 || dwRegionLength == 0)
        return(NULL);

    if (*(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET) != ROM_SIGNATURE)
        return NULL;

    // A pointer to the ROMHDR structure lives just past the ROM_SIGNATURE (which is a longword value).  Note that
    // this pointer is remapped since it might be a flash address (image destined for flash), but is actually cached
    // in RAM.
    //
    dwCacheAddress = *(LPDWORD) OEMMapMemAddr (dwRegionStart, dwRegionStart + ROM_SIGNATURE_OFFSET + sizeof(ULONG));
    pROMHeader     = (ROMHDR *) OEMMapMemAddr (dwRegionStart, dwCacheAddress);

    // Make sure sure are some modules in the table of contents.
    //
    if ((dwNumModules = pROMHeader->nummods) == 0)
        return NULL;

    // Locate the table of contents and search for the kernel executable and the TOC immediately follows the ROMHDR.
    //
    pTOC = (TOCentry *)(pROMHeader + 1);

    while(dwNumModules--) {
        LPBYTE pFileName = OEMMapMemAddr(dwRegionStart, (DWORD)pTOC->lpszFileName);
        if (!strcmp((const char *)pFileName, "nk.exe")) {
            return ((PVOID)(pROMHeader->pExtensions));
        }
        ++pTOC;
    }
    return NULL;
}

ULONG 
HalSetBusDataByOffset(
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    return (PCISetBusDataByOffset (BusNumber, SlotNumber, Buffer, Offset, Length));
}

BOOL INTERRUPTS_ENABLE (BOOL fEnable)
{
    return FALSE;
}

VOID*
OALPAtoVA(UINT32 pa, BOOL cached)
{
    return (VOID*)pa;
}

UINT32
OALVAtoPA(VOID * va)
{
    return (UINT32)va;
}

// For USB kitl it need this to satisfy the debug build.
#ifdef DEBUG
DBGPARAM
dpCurSettings =
{
    TEXT("RndisMini"),
    {
        //
        //  MDD's, DON"T TOUCH
        //

        TEXT("Init"),
        TEXT("Rndis"),
        TEXT("Host Miniport"),
        TEXT("CE Miniport"),
        TEXT("Mem"),
        TEXT("PDD Call"),
        TEXT("<unused>"),
        TEXT("<unused>"),

        //
        //  PDD can use these..
        //

        TEXT("PDD Interrupt"),
        TEXT("PDD EP0"),
        TEXT("PDD EP123"),
        TEXT("PDD FIFO"),       
        TEXT("PDD"),
        TEXT("<unused>"),


        //
        //  Standard, DON'T TOUCH
        //

        TEXT("Warning"),
        TEXT("Error")       
    },

    0xc000
};
#endif
