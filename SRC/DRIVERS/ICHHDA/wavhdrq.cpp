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
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
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
#include "wavdrv.h"


//
// dwBytesRecorded is being used to indicate how far output has progressed
//
#define IS_BUFFER_FULL(pwh)      ((pwh)->dwBytesRecorded >= (pwh)->dwBufferLength)
#define IS_BUFFER_DONE(pwh)      ((pwh)->dwFlags & WHDR_DONE)
#define IS_BUFFER_PREPARED(pwh)  ((pwh)->dwFlags & WHDR_PREPARED)
#define IS_BUFFER_QUEUED(pwh)    ((pwh)->dwFlags & WHDR_INQUEUE)
#define IS_BUFFER_BEGINLOOP(pwh) ((pwh)->dwFlags & WHDR_BEGINLOOP)
#define IS_BUFFER_ENDLOOP(pwh)   ((pwh)->dwFlags & WHDR_ENDLOOP)
#define IS_BUFFER_INLOOP(pwh)    ((pwh)->reserved != 0)

#define MARK_BUFFER_FULL(pwh)       ((pwh)->dwBytesRecorded = (pwh)->dwBufferLength)
#define MARK_BUFFER_EMPTY(pwh)      ((pwh)->dwBytesRecorded = 0)
#define MARK_BUFFER_NOT_INLOOP(pwh) ((pwh)->reserved = 0)
#define MARK_BUFFER_INLOOP(pwh,ls)  ((pwh)->reserved = (DWORD) (ls))
#define MARK_BUFFER_DONE(pwh)       ((pwh)->dwFlags |=  WHDR_DONE)
#define MARK_BUFFER_NOT_DONE(pwh)   ((pwh)->dwFlags &= ~WHDR_DONE)
#define MARK_BUFFER_PREPARED(pwh)   ((pwh)->dwFlags |=  WHDR_PREPARED)
#define MARK_BUFFER_UNPREPARED(pwh) ((pwh)->dwFlags &= ~WHDR_PREPARED)
#define MARK_BUFFER_QUEUED(pwh)     ((pwh)->dwFlags |=  WHDR_INQUEUE)
#define MARK_BUFFER_DEQUEUED(pwh)   ((pwh)->dwFlags &= ~WHDR_INQUEUE)
#define GET_BUFFER_LOOPSTART(pwh)   (PWAVEHDR ((pwh)->reserved))

CWavehdrQueue::CWavehdrQueue (void)
{
    DEBUGMSG(ZONE_INFO, (TEXT("CWavehdrQueue::ctor\r\n")));
    m_pCurrent = NULL;
    m_pLoopStart = NULL;
    m_pHead = NULL;
    m_pTail = NULL;
    m_pfnCallback = NULL;
    m_hWave = NULL;
    m_dwBytesCompleted = 0;
    m_dwBytesInCurrent = 0;
    m_nBytesBuffered = 0;
    m_dwLoopCount = 0;
    m_dwLoopDecrement = 0;
    m_pBuildLoopBegin = NULL;
    m_pLoopStart = NULL;
	m_buffercount=0;
	count = 0;//cren
}


CWavehdrQueue::~CWavehdrQueue ()
{
}


BOOL    
CWavehdrQueue::Initialize (LPWAVEOPENDESC lpWOD, UINT uMsg)
{
    m_pfnCallback = (DRVCALLBACK*) lpWOD->dwCallback;
    m_hWave = lpWOD->hWave;
    m_dwInstance = lpWOD->dwInstance;
    m_uMsg = uMsg;
    return TRUE;
}


VOID    
CWavehdrQueue::Reset (void)
{ PWAVEHDR pwh;

    for (pwh = m_pHead; pwh != NULL; pwh = pwh->lpNext) {
        MARK_BUFFER_NOT_INLOOP(pwh);
		#if 1 // ldmai
		MARK_BUFFER_FULL(pwh);
		#endif
        MARK_BUFFER_DONE(pwh);
    }
	m_pCurrent = NULL;

    RemoveCompleteBlocks();

    m_dwBytesCompleted = 0;
    m_dwBytesInCurrent = 0;
    m_nBytesBuffered = 0;
    m_dwLoopCount = 0;
    m_pBuildLoopBegin = NULL;
    m_pLoopStart = NULL;
}


BOOL    
CWavehdrQueue::AddBuffer (PWAVEHDR phdr)
{
    if (IS_BUFFER_QUEUED(phdr)) {
        return FALSE;
    }

    // Check the looping status first, so we can handle poorly formed loops
    if (IS_BUFFER_BEGINLOOP(phdr))  {
        // defensive programming: if client sends consecutive BEGINS w/o intervening end:
        // a) behavior is undefined, so we get to do anything we want
        // b) except crash
        // c) so we'll close off the current loop on behalf of the neuron-deprived application
        if (m_pBuildLoopBegin != NULL) {
            DEBUGMSG(ZONE_WARNING, (TEXT("Inserting missing WHDR_ENDLOOP\r\n")));
            ASSERT(m_pTail != NULL);
            m_pTail->dwFlags |= WHDR_ENDLOOP;
        }
        // subsequent buffers will get marked looping back to this buffer
        m_pBuildLoopBegin = phdr;
    }
    // We mark all all buffers as either looping or not looping
    MARK_BUFFER_INLOOP(phdr, m_pBuildLoopBegin);
    if (IS_BUFFER_ENDLOOP(phdr))  {
        // subsequent buffers will get marked as not looping
        m_pBuildLoopBegin = NULL;
    }

    // now put the thing in the queue

    if (m_pHead == NULL) {
        ASSERT(m_pTail == NULL);
        ASSERT(m_pCurrent == NULL);
        m_pHead = phdr;
        m_pTail = phdr;
        m_pCurrent = phdr;
        m_dwBytesInCurrent = m_pCurrent->dwBufferLength; // always reset this when m_pCurrent is set
    } else {
        //  add the buffer to the tail
        m_pTail->lpNext = phdr;
        m_pTail = phdr;
        // if we had previously exhausted all available buffers,
        // make this new one immediately available
        if (m_pCurrent == NULL) {
            m_pCurrent = phdr;
            m_dwBytesInCurrent = m_pCurrent->dwBufferLength; // always reset this when m_pCurrent is set
        }
    }

    phdr->lpNext = NULL;
    MARK_BUFFER_NOT_DONE(phdr);
    MARK_BUFFER_EMPTY(phdr);
    MARK_BUFFER_QUEUED(phdr);
	m_buffercount++;
	count++;//cren

	DEBUGMSG(ZONE_VERBOSE,(TEXT("AddBuffer (0x%08x, %d, %d , %d )\r\n"),m_pCurrent->lpData, m_pCurrent->dwBufferLength,m_dwBytesInCurrent,count ));
	return TRUE;
}

BOOL    
CWavehdrQueue::GetNextBlock (ULONG nBytes, PBYTE * ppBuffer, PULONG pnActual)
{
	if (m_pCurrent == NULL) {
		return FALSE;
	}

    if (m_pLoopStart == NULL && IS_BUFFER_BEGINLOOP(m_pCurrent)) {
        // keep track of where the loop begins so we can get back to it.
        m_pLoopStart = m_pCurrent;
        m_dwLoopCount = m_pCurrent->dwLoops;
        m_dwLoopDecrement = (m_dwLoopCount == INFINITE) ? 0 : 1;
    }

    if (nBytes > m_dwBytesInCurrent) {
        nBytes = m_dwBytesInCurrent;
    }
    *ppBuffer = (PBYTE) (m_pCurrent->lpData + m_pCurrent->dwBufferLength - m_dwBytesInCurrent);
    *pnActual = nBytes;
//DEBUGMSG(ZONE_VERBOSE,(TEXT("GetNextBlock (%d, 0x%08x, %d, %d )\r\n"),nBytes, m_pCurrent->lpData, m_pCurrent->dwBufferLength,m_dwBytesInCurrent));

    m_nBytesBuffered += nBytes;
    m_dwBytesInCurrent -= nBytes;

	if (m_dwBytesInCurrent == 0) {
		// we've consumed all the space in the current buffer - advance to next header, accounting for looping
		if (m_pLoopStart != NULL) {
			// we're looping
			ASSERT(IS_BUFFER_INLOOP(m_pCurrent));
			// if loop is poorly formed (a begin but no end, just null) treat as if it were endloop
			if (IS_BUFFER_ENDLOOP(m_pCurrent) || m_pCurrent->lpNext == NULL) {
				m_dwLoopCount -= m_dwLoopDecrement;
				if (m_dwLoopCount > 0) {
					// skip back to start of loop and keep going
					m_pCurrent = m_pLoopStart;
					m_dwBytesInCurrent = m_pCurrent->dwBufferLength; // always reset this when m_pCurrent is set
					return TRUE; //bypass the rest of the processing
				}
				else {
					// we're done looping the current loop!
					m_pLoopStart = NULL;
				}
			}
		}
		// just advance to the next buffer
		m_pCurrent = m_pCurrent->lpNext;
		if (m_pCurrent != NULL) {
			m_dwBytesInCurrent = m_pCurrent->dwBufferLength; // always reset this when m_pCurrent is set
		}
	}

    return TRUE;
}

// AdvanceCurrentPosition
// Called by the buffer engine to notify the header queue that the current 
// position has advanced by the specified number of bytes.
// The dwBytesRecorded field of queued headers is advanced and all
// the fully consumed buffers are sent back to the client
// via the callback.
//
// One tricky point: looped headers need special treatment.
// A loop is only considered DONE when it has completely
// played out.

void
CWavehdrQueue::AdvanceCurrentPosition (DWORD dwAdvance)
{
    PWAVEHDR pwh;

    // don't allow us to mark data as played when it hasn't been copied to the buffer yet.
    // under cpu load and/or low-latency conditions, this is a possibility:
    //
    if (m_nBytesBuffered <= 0) {
        // we're all caught up - nothing to do.
        return;
    }

    // limit the amount we advance so we don't mark stuff as played if we haven't copied it out
    if (dwAdvance > (DWORD) m_nBytesBuffered) {
        dwAdvance = m_nBytesBuffered;
    }

    pwh = m_pHead;
    while (pwh != NULL && dwAdvance > 0) {
        // We'll be utterly paranoid about apps tampering with the header underneath us.
        // If the stream winds up playing oddly, tough luck.
        // Just want to make sure we don't get into some undefined state & crash.
        DWORD dwBufferLength = pwh->dwBufferLength;
        DWORD dwBytesRecorded = pwh->dwBytesRecorded;
        DWORD dwBytesToConsume;

        if (dwBytesRecorded < dwBufferLength) {
            dwBytesToConsume = dwBufferLength - dwBytesRecorded;
        }
        else {
            // just in case the application has tampered with the header...
            dwBytesToConsume = 0;
        }
        // what comes first: fill the block or consume all the data?
        if (dwBytesToConsume > dwAdvance) {
            // we'll only partially consume this buffer & then quit
            dwBytesToConsume = dwAdvance;
            pwh->dwBytesRecorded += dwBytesToConsume;
        }
        else {
            // the current buffer will be completely consumed
            // but now check to see if this buffer is looped.
            if (IS_BUFFER_INLOOP(pwh)) {
                PWAVEHDR pStart = GET_BUFFER_LOOPSTART(pwh);
                if (pStart->dwLoops == 1) {
                    MARK_BUFFER_FULL(pwh);
                    MARK_BUFFER_DONE(pwh);
                }
                if (IS_BUFFER_ENDLOOP(pwh)) {
                    // at loop's end. decide if we need to loop back or not.
                    if (pStart->dwLoops != INFINITE) {
                        pStart->dwLoops --;
                    }
                    if (pStart->dwLoops == 0) {
                        // exit the loop
                        pwh = pwh->lpNext;
                    }
                    else {
                        // go back to the top of the loop
                        pwh = pStart;
						pwh->dwBytesRecorded = 0;

                    }
                }
                else {
                    // mid-loop buffer: just advance to next buffer in loop
                    pwh = pwh->lpNext;
                }
            }
            else {
                // not a looping buffer
                ASSERT(pwh != m_pCurrent);
                MARK_BUFFER_FULL(pwh);
                MARK_BUFFER_DONE(pwh);
                pwh = pwh->lpNext;
            }
        }

        m_dwBytesCompleted += dwBytesToConsume;
        dwAdvance -= dwBytesToConsume;
        m_nBytesBuffered -= dwBytesToConsume;
        if (m_nBytesBuffered < 0) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNDERFLOW %8d %8d\r\n"), m_dwBytesCompleted, m_nBytesBuffered));
        }
    }
    RemoveCompleteBlocks();
}

VOID    
CWavehdrQueue::RemoveCompleteBlocks(void)
{
    PWAVEHDR pwh;
    while (m_pHead != NULL && IS_BUFFER_DONE(m_pHead))  {
        ASSERT(m_pHead != m_pCurrent);  // By definition, the current block is not complete.
        pwh = m_pHead;
        m_pHead = pwh->lpNext;
        if (m_pHead == NULL) {
            // keep m_pTail consistent.
            m_pTail = NULL;
        }
        pwh->lpNext = NULL;
        MARK_BUFFER_DEQUEUED(pwh);
        //
        // Make the callback function call to the Wave API Manager
        //

		// call back into the waveform API manager, which takes care of marshalling the callback
		// to the application.
		// Note that the application callback could conceivably trigger events
		// that would lead to the stream being closed before the callback returns
		// In that case, the driver must close gracefully and defer
		// the deletion of the underlying objects until after the callback
		// has returned.
		// This is accomplished in the stream context interrupt handler with
		// reference counting.
        m_pfnCallback(m_hWave, m_uMsg, m_dwInstance, (DWORD) pwh, 0);
		count--;//cren
	        DEBUGMSG(ZONE_CALLBACK, (TEXT("HDRQ: Sending callback hwav=%08x inst=%08x pwh=%08x, %d \r\n"), m_hWave, m_dwInstance, pwh, count));
    }   
}

// seek and destroy the first unscheduled loop
// i.e. start the search at m_pCurrent, not m_pHead.
// (if a loop is between m_pHead and m_pCurrent, then
// it's already been moved into the buffer and there's
// nothing we can do.
VOID    
CWavehdrQueue::BreakLoop(void)
{ PWAVEHDR pwh;


    // When we break a loop that's actively playing (i.e. IS_BUFFER_LOOP(m_pCurrent))
    // we need to make sure go back and update dwBytesRecorded field properly

    // well, first of all, decide if we're currently playing a loop
    if (m_pLoopStart != NULL) {
        pwh = m_pLoopStart;
        m_pLoopStart = NULL;
    }
    else {
        // no loop is currently playing, so let's wander down the
        // current list and break the first loop we find
        for (pwh = m_pCurrent; pwh != NULL; pwh = pwh->lpNext) {
            if (IS_BUFFER_INLOOP(pwh)) {
                break;
            }
        }
    }
    // charge down the loop we found (if we found one) & rip it to shreds
    while (pwh != NULL && IS_BUFFER_INLOOP(pwh)) {
        MARK_BUFFER_NOT_INLOOP(pwh);
        pwh = pwh->lpNext;
    }

}

BOOL    
CWavehdrQueue::IsEmpty (void)
{
    return m_pCurrent == NULL && m_nBytesBuffered <= 0;
}

BOOL    
CWavehdrQueue::IsDone (void)
{
    return m_pHead == NULL;
}

DWORD   
CWavehdrQueue::GetBytePosition(void)
{
    DWORD dwBytePositionTotal;    


#if 0
    // This clause should provide a sample-accurate position
    // instead, it causes the current position to occasionally move backwards.
    
    if (m_pCurrent != NULL) {
        dwBytePositionTotal = m_dwBytesCompleted + m_pHead->dwBytesRecorded;
    }
    else 
#endif
    {
        dwBytePositionTotal = m_dwBytesCompleted;
    }

    return dwBytePositionTotal;
}
