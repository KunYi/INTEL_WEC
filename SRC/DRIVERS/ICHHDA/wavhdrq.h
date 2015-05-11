//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
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
// -----------------------------------------------------------------------------
//
//  Module Name:  
//  
//      wavhdrq.h
//  
//  Abstract:
//
//      Device-independent WAVEHDR queue support for Windows CE wavedev drivers
//  
//  Functions:
//  
//  Notes:
//  
// -----------------------------------------------------------------------------
#ifndef __WAVHDRQ_H__
#define __WAVHDRQ_H__

class CWavehdrQueue
{
public:
    CWavehdrQueue (void);
    ~CWavehdrQueue (void);
    BOOL    Initialize (LPWAVEOPENDESC lpWOD, UINT uMsg);

    BOOL    IsDone  (void); // all queued data has been consumed and sent back to client
    BOOL    IsEmpty (void); // all queued data has been submitted for playback (but may not have actually played yet)
    BOOL    GetNextBlock (ULONG nBytes, PBYTE * ppBuffer, PULONG pnActual);
    VOID    AdvanceCurrentPosition (DWORD dwAdvance);
    BOOL    AddBuffer (PWAVEHDR pwh);
    VOID    BreakLoop(void);
    VOID    Reset (void);
    DWORD   GetBytePosition(void);

private: // methods

    VOID    RemoveCompleteBlocks(void);

private:
    // header queue state information
    PWAVEHDR        m_pHead;            // first header in list
    PWAVEHDR        m_pTail;            // last header in list
public:  
    PWAVEHDR        m_pCurrent;         // header being worked on
private:
    DWORD           m_dwLoopCount;      // how many times should we loop m_pCurrent
    DWORD           m_dwLoopDecrement;  // 1 for normal cases, 0 for dwLoopCount == INFINITE
    PWAVEHDR        m_pLoopStart;       // when playing looping buffers, back up to here at end of loop
    PWAVEHDR        m_pBuildLoopBegin;  // when adding looping buffers, this points to first buffer in loop

public: 
    DWORD           m_dwBytesInCurrent; // how many bytes left to copy in m_pCurrent
private:
    int             m_nBytesBuffered;   // total bytes remaining in buffer
    DWORD           m_dwBytesCompleted; // total bytes captured or played and sent back via callback
    // callback support
    UINT            m_uMsg;             // MM_WIM_DATA for capture, MM_WOM_DONE for playback
    DRVCALLBACK*    m_pfnCallback;
    HANDLE          m_hWave;            // HWAVEIN/HWAVEOUT to pass back on callbacks
    DWORD           m_dwInstance;       // client context information
	DWORD           count;
    DWORD           m_buffercount;
};

#endif // __WAVHDRQ_H__
