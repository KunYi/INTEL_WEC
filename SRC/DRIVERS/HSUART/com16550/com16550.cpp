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
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  

Abstract:

    Serial PDD for 16550 Common Code.

Notes: 
--*/
#include <windows.h>
#include <ceddk.h>

#include <ddkreg.h>
#include <serhw.h>
//#include <cserpdd.h>

#include "pdd16550.h"
#include "pddisr.h"
CSerialPDD * CreateSerialObject(LPTSTR lpActivePath, PVOID pMdd,PHWOBJ pHwObj, DWORD DeviceArrayIndex)
{
    CSerialPDD * pSerialPDD = NULL;
    if (3 >= DeviceArrayIndex ) {
        pSerialPDD = new CPdd16550Isr(lpActivePath,pMdd, pHwObj);
        if (pSerialPDD && !pSerialPDD->Init()) {
            delete pSerialPDD;
            pSerialPDD = NULL;
         }
    }
    if (pSerialPDD == NULL) {
        pSerialPDD= new CPdd16550(lpActivePath,pMdd, pHwObj);
        if (pSerialPDD && !pSerialPDD->Init()) {
            delete pSerialPDD;
            pSerialPDD = NULL;
        }
    }
    return pSerialPDD;
}
void DeleteSerialObject(CSerialPDD * pSerialPDD)
{
    if (pSerialPDD)
        delete pSerialPDD;
}
