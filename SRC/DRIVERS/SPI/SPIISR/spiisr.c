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
// GIISR.c
//
// Generic Installable Interrupt Service Routine.
//
//
#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>
#include <giisr.h>
#include <spi.h>

// Globals
static GIISR_INFO g_Info[MAX_GIISR_INSTANCES];
static DWORD g_PortValue[MAX_GIISR_INSTANCES];
static BOOL g_InstanceValid[MAX_GIISR_INSTANCES] = {FALSE};
static DWORD g_Instances = 0;

/*
 @doc INTERNAL
 @func    BOOL | DllEntry | Process attach/detach api.
 *
 @rdesc The return is a BOOL, representing success (TRUE) or failure (FALSE).
 */
BOOL __stdcall
DllEntry(
    HINSTANCE hinstDll,         // @parm Instance pointer.
    DWORD dwReason,             // @parm Reason routine is called.
    LPVOID lpReserved           // @parm system parameter.
     )
{
    UNREFERENCED_PARAMETER(hinstDll);
    UNREFERENCED_PARAMETER(lpReserved);

    if (dwReason == DLL_PROCESS_ATTACH) {
    }

    if (dwReason == DLL_PROCESS_DETACH) {
    }

    return TRUE;
}


DWORD CreateInstance(
    void
    )
{
    DWORD InstanceIndex;
    
    if (g_Instances >= MAX_GIISR_INSTANCES) {
        return (DWORD)-1;
    }
    
    g_Instances++;

    // Search for next free instance
    for (InstanceIndex = 0; InstanceIndex < MAX_GIISR_INSTANCES; InstanceIndex++) {
        if (!g_InstanceValid[InstanceIndex]) break;
    }

    // Didn't find a free instance (this shouldn't happen)
    if (InstanceIndex >= MAX_GIISR_INSTANCES) {
        return (DWORD)-1;
    }

    g_InstanceValid[InstanceIndex] = TRUE;
    
    // Initialize info
    g_Info[InstanceIndex].SysIntr = SYSINTR_CHAIN;
    g_Info[InstanceIndex].CheckPort = FALSE;
    g_Info[InstanceIndex].UseMaskReg = FALSE;

    // Initialize data
    g_PortValue[InstanceIndex] = 0;

    return InstanceIndex;
}


void DestroyInstance(
    DWORD InstanceIndex 
    )
{
    if (g_InstanceValid[InstanceIndex]) {
        g_InstanceValid[InstanceIndex] = FALSE;
        g_Instances--;
    }
}


// The compiler generates a call to memcpy() for assignments of large objects.
// Since this library is not linked to the CRT, define our own copy routine.
void
InfoCopy(
    PGIISR_INFO dst,
    const GIISR_INFO *src
    )
{
    size_t count = sizeof(GIISR_INFO);
    PBYTE bdst = (PBYTE)dst;
    const BYTE* bsrc = (const BYTE*)src;
    
    while (count--) {
        *bdst++ = *bsrc++;
    }
}


BOOL 
IOControl(
    DWORD   InstanceIndex,
    DWORD   IoControlCode, 
    LPVOID  pInBuf, 
    DWORD   InBufSize,
    LPVOID  pOutBuf, 
    DWORD   OutBufSize, 
    LPDWORD pBytesReturned
    ) 
{
    if (pBytesReturned) {
        *pBytesReturned = 0;
    }

    switch (IoControlCode) {
    case IOCTL_GIISR_PORTVALUE:
        if ((OutBufSize != g_Info[InstanceIndex].PortSize) || !g_Info[InstanceIndex].CheckPort || !pOutBuf) {
            // Invalid size of output buffer, not checking port, or invalid output buffer pointer
            return FALSE;
        }

        if (pBytesReturned) {
            *pBytesReturned = g_Info[InstanceIndex].PortSize;
        }
        
        switch (g_Info[InstanceIndex].PortSize) {
        case sizeof(BYTE):
            *(BYTE *)pOutBuf = (BYTE)g_PortValue[InstanceIndex];
            break;
            
        case sizeof(WORD):
            *(WORD *)pOutBuf = (WORD)g_PortValue[InstanceIndex];
            break;

        case sizeof(DWORD):
            *(DWORD *)pOutBuf = g_PortValue[InstanceIndex];
            break;

        default:
            if (pBytesReturned) {
                *pBytesReturned = 0;
            }
            
            return FALSE;
        }

        break;
        
    case IOCTL_GIISR_INFO:
        // Copy instance information
        if ((InBufSize != sizeof(GIISR_INFO)) || !pInBuf) {
            // Invalid size of input buffer or input buffer pointer
            return FALSE;
        }

        // The compiler may generate a memcpy call for a structure assignment,
        // and we're not linking with the CRT, so use our own copy routine.
        InfoCopy(&g_Info[InstanceIndex], (PGIISR_INFO)pInBuf);

        break;

    default:
        // Invalid IOCTL
        return FALSE;
    }
    
    return TRUE;
}

DWORD
ISRHandler(
    DWORD InstanceIndex
    )
{
    DWORD Mask = g_Info[InstanceIndex].Mask;
    
    if (!g_Info[InstanceIndex].CheckPort) {
        return g_Info[InstanceIndex].SysIntr;
    }
    
    if (g_Info[InstanceIndex].PortAddr == 0 ) {
        return SYSINTR_CHAIN ;
    }

    g_PortValue[InstanceIndex] = 0;
    
    // Check port
    if (g_Info[InstanceIndex].PortIsIO) {
        // I/O port
        switch (g_Info[InstanceIndex].PortSize) {
        case sizeof(BYTE):
            g_PortValue[InstanceIndex] = READ_PORT_UCHAR((PUCHAR)g_Info[InstanceIndex].PortAddr);

            if (g_Info[InstanceIndex].UseMaskReg) {
                // Read mask from mask register
                Mask = READ_PORT_UCHAR((PUCHAR)g_Info[InstanceIndex].MaskAddr);
            }
            
            break;

        case sizeof(WORD):
            g_PortValue[InstanceIndex] = READ_PORT_USHORT((PUSHORT)g_Info[InstanceIndex].PortAddr);

            if (g_Info[InstanceIndex].UseMaskReg) {
                // Read mask from mask register
                Mask = READ_PORT_USHORT((PUSHORT)g_Info[InstanceIndex].MaskAddr);
            }
            
            break;

        case sizeof(DWORD):
            g_PortValue[InstanceIndex] = READ_PORT_ULONG((PULONG)g_Info[InstanceIndex].PortAddr);

            if (g_Info[InstanceIndex].UseMaskReg) {
                // Read mask from mask register
                Mask = READ_PORT_ULONG((PULONG)g_Info[InstanceIndex].MaskAddr);
            }
            
            break;

        default:
            // No interrupt will be returned
            break;
        }
    } else {
        // Memory-mapped port
        switch (g_Info[InstanceIndex].PortSize) {
        case sizeof(BYTE):
            g_PortValue[InstanceIndex] = READ_REGISTER_UCHAR((BYTE *)g_Info[InstanceIndex].PortAddr);

            if (g_Info[InstanceIndex].UseMaskReg) {
                // Read mask from mask register
                Mask = READ_REGISTER_UCHAR((BYTE *)g_Info[InstanceIndex].MaskAddr);
            }
            
            break;

        case sizeof(WORD):
            g_PortValue[InstanceIndex] = READ_REGISTER_USHORT((WORD *)g_Info[InstanceIndex].PortAddr);

            if (g_Info[InstanceIndex].UseMaskReg) {
                // Read mask from mask register
                Mask = READ_REGISTER_USHORT((WORD *)g_Info[InstanceIndex].MaskAddr);
            }
            
            break;

        case sizeof(DWORD):
			{
				DWORD dStatus_SSSR = 0;
				DWORD dStatus_SSCR1 = 0;

				//Clear the initial value of the RFS and TFS on SSSR
				g_PortValue[InstanceIndex] = (READ_REGISTER_ULONG(((DWORD *)(g_Info[InstanceIndex].PortAddr + SPI_INTERRUPT_SSSR_OFFSET)))) & CLR_SSSR_RFS_TFS;

				dStatus_SSCR1= READ_REGISTER_ULONG(((DWORD *)(g_Info[InstanceIndex].PortAddr + SPI_INTERRUPT_SSCR1_OFFSET)));
				dStatus_SSSR= READ_REGISTER_ULONG(((DWORD *)(g_Info[InstanceIndex].PortAddr + SPI_INTERRUPT_SSSR_OFFSET)));

				if ((dStatus_SSCR1 & SSCR1_TIE) && (dStatus_SSSR & SSSR_TFS ))
				{
					g_PortValue[InstanceIndex] = dStatus_SSSR; 

				}
				else if ((dStatus_SSCR1 & SSCR1_RIE) && (dStatus_SSSR & SSSR_RFS ))
				{
					g_PortValue[InstanceIndex] = dStatus_SSSR ; 
				}			
			}
            break;

        default:
            // No interrupt will be returned
            break;
        }    
    }

    // If interrupt bit set, return corresponding SYSINTR
    return (g_PortValue[InstanceIndex] & Mask) ? g_Info[InstanceIndex].SysIntr : SYSINTR_CHAIN;
}
