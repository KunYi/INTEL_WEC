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
//------------------------------------------------------------------------------
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
//
//  File:  id.c
//
//  This file implements helper PCI bus functions.
//
#include <windows.h>
#include <ceddk.h>
#include <oal.h>
#include <nkintr.h>

ULONG PCIReadBusData(IN ULONG BusNumber, IN ULONG DeviceNumber, IN ULONG FunctionNumber, OUT PVOID Buffer, IN ULONG Offset, IN ULONG Length);
ULONG PCIWriteBusData(IN ULONG BusNumber, IN ULONG DeviceNumber, IN ULONG FunctionNumber, IN PVOID Buffer, IN ULONG Offset, IN ULONG Length);
void printPCIConfig(PCI_COMMON_CONFIG* config);

//------------------------------------------------------------------------------
//
//  Function:  OALPCIGetId
//
//  
//
UINT32 OALPCIGetId(
                   UINT32 busId, 
                   OAL_PCI_LOCATION pciLoc
                   )
{
    UNREFERENCED_PARAMETER(busId);
    UNREFERENCED_PARAMETER(pciLoc);
    // stub function - x86 doesn't use this
    return 0;
}

//------------------------------------------------------------------------------
//
//  Function:  OALPCICfgRead
//
//  This function reads PCI configuration space at location specified by pciLoc.
//  This routine is hardware dependend and in most cases it will be implemented
//  in SoC library or it will platform specific.
//
UINT32 OALPCICfgRead(
                     UINT32 busId, 
                     OAL_PCI_LOCATION pciLoc, 
                     UINT32 offset, 
                     UINT32 size,
                     __out_bcount(size) void *pData
                    )
{
    OAL_DDK_PARAMS params;

    memset(pData, 0xFF, size);
    
    params.function = IOCTL_OAL_READBUSDATA;
    params.rc = 0;
    params.busData.devLoc.IfcType = PCIBus;
    params.busData.devLoc.BusNumber = busId;
    params.busData.devLoc.LogicalLoc = pciLoc.logicalLoc;
    params.busData.offset = offset;
    params.busData.length = size;
    params.busData.pBuffer = pData;

    if (!OEMIoControl(IOCTL_HAL_DDK_CALL, &params, sizeof(params), NULL, 0, NULL) || !params.rc) 
        return FALSE;

    return params.rc;
}

//------------------------------------------------------------------------------
//
//  Function:  OALPCICfgWrite
//
//  This function write PCI configuration space at location specified by pciLoc.
//  This routine is hardware dependend and in most cases it will be implemented
//  in SoC library or it will be platform specific.
//
UINT32 OALPCICfgWrite(
                      UINT32 busId, 
                      OAL_PCI_LOCATION pciLoc, 
                      UINT32 offset, 
                      UINT32 size,
                      __in_bcount(size) void *pData
                      )
{
    OAL_DDK_PARAMS params;

    params.function = IOCTL_OAL_WRITEBUSDATA;
    params.rc = 0;
    params.busData.devLoc.IfcType = PCIBus;
    params.busData.devLoc.BusNumber = busId;
    params.busData.devLoc.LogicalLoc = pciLoc.logicalLoc;
    params.busData.offset = offset;
    params.busData.length = size;
    params.busData.pBuffer = pData;

    if (!OEMIoControl(IOCTL_HAL_DDK_CALL, &params, sizeof(params), NULL, 0, NULL) || !params.rc) 
        return FALSE;

    return params.rc;
}
