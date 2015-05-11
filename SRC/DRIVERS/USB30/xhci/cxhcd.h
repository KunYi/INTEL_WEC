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
// File: cxhcd.h - CXhcd implements the HCDI interface. It mostly just passes 
//                 requests on to other objects, which do the real work. 
//----------------------------------------------------------------------------------

#ifndef XHCD_H
#define XHCD_H

#include "hal.h"
#include "datastructures.h"
#include "chw.h"
#include "cdevice.hpp"
#include "cpipe.h"

class CXhcd;
// this class gets passed into the CXhcd Initialize routine
class CPhysMem;
// this class is our access point to all USB devices
class CRootHub;

class CXhcd : public CHW
{
public:

    CXhcd(IN LPVOID pvXhcdPddObject,
            IN CPhysMem * pCPhysMem,
            IN LPCWSTR szDriverRegistryKey,
            IN REGISTER portBase,
            IN DWORD dwSysIntr);

    ~CXhcd();

    // These functions are called by the HCDI interface
    virtual BOOL InitializeDevice();
    virtual VOID DeInitializeDevice(VOID);
};

#endif /* XHCD_H */