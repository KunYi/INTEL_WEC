////////////////////////////////////////////////////////////////////////////////
//
// INTEL CONFIDENTIAL
//
// Copyright 2007 Intel Corporation. All Rights Reserved.
//
// The source code contained or described herein and all documents related to
// the source code ("Material") are owned by Intel Corporation or its suppliers
// or licensors. Title to the Material remains with Intel Corporation or its
// suppliers and licensors. The Material contains trade secrets and proprietary
// and confidential information of Intel or its suppliers and licensors. The
// Material is protected by worldwide copyright and trade secret laws and treaty
// provisions. No part of the Material may be used, copied, reproduced,
// modified, published, uploaded, posted, transmitted, distributed, or disclosed
// in any way without Intel's prior express written permission.
//
// No license under any patent, copyright, trade secret or other intellectual
// property right is granted to or conferred upon you by disclosure or delivery
// of the Materials, either expressly, by implication, inducement, estoppel or
// otherwise. Any license under such intellectual property rights must be
// express and approved by Intel in writing.
//
////////////////////////////////////////////////////////////////////////////////
#ifndef __E1000_KITL_H
#define __E1000_KITL_H

#include <windows.h>
#include <halether.h>
#include <ceddk.h>
#include <wdm.h>

BOOL   E1000InitDMABuffer(UINT32 address, UINT32 size);
BOOL   E1000Init(UINT8 *pAddress, UINT32 offset, UINT16 mac[3]);
UINT16 E1000SendFrame(UINT8 *pBuffer, UINT32 length);
UINT16 E1000GetFrame(UINT8 *pBuffer, UINT16 *pLength);
VOID   E1000EnableInts();
VOID   E1000DisableInts();
VOID   E1000CurrentPacketFilter(UINT32 filter);
BOOL   E1000MulticastList(UINT8 *pAddresses, UINT32 count);

#define OAL_ETHDRV_E1000       { \
    E1000Init, E1000InitDMABuffer, NULL, E1000SendFrame, E1000GetFrame, \
    E1000EnableInts, E1000DisableInts, \
    NULL, NULL, E1000CurrentPacketFilter, E1000MulticastList \
}

#endif //__E1000_KITL_H
