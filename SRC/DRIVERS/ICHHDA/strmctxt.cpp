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
//
// -- Intel Copyright Notice --
// 
// @par
// Copyright (c) 2002-2008 Intel Corporation All Rights Reserved.
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
#include "decibels.h"


/////////////////////////////////////////////////////////////////////////////
//
// CStreamContext Methods
//
/////////////////////////////////////////////////////////////////////////////

CStreamContext::CStreamContext(BOOL fPlayback, BOOL fPaused)
    : m_fIsPlayback(fPlayback)
    , m_fPaused(fPaused)
{
    MYTHIS_CONSTRUCTOR;
    
    InitializeCriticalSection(&m_cs);

    m_dwTransferCursor = 0;
    m_dwPlaybackRate = 0x10000; // initial nominal multiplier value of 1.0
    m_fStarted = FALSE;
    m_lRefCount = 1;
    m_dwPlayFlags = FALSE;

    m_dwAC3FrameBytesCopied = 0;
    m_wNextByte = 0xFFFF;
}

COutputStreamContext::COutputStreamContext()
    : CStreamContext(TRUE, FALSE) // output streams are created in the unpaused state
{
}

CInputStreamContext::CInputStreamContext()
    : CStreamContext(FALSE, TRUE) // input streams are created in the paused state
{
}

CStreamContext::~CStreamContext()
{
    MYTHIS_CHECK;

    Stop();

    m_pDevice->FreeDMAChannel(m_ulDmaChannel);

    DeleteCriticalSection(&m_cs);
    MYTHIS_DESTRUCTOR;
}

void CStreamContext::AddRef(void)
{
    InterlockedIncrement(&m_lRefCount);
}

void CStreamContext::Release(void)
{
    if (InterlockedDecrement(&m_lRefCount) == 0) {
        delete this;
    }
}

MMRESULT
CStreamContext::Initialize (HDA * pDevice, ULONG ulChannelIndex, LPWAVEOPENDESC pWOD)
{

	m_pDevice      = pDevice;
	m_ulDmaChannel  = ulChannelIndex;

	m_wfx = *(pWOD->lpFormat);
	m_ucSilence = (m_wfx.wBitsPerSample == 8) ? 0x80 : 0; // fill buffer w/ this to get silence

	// set up the DMA channel
	m_pDevice->InitDMAChannel(ulChannelIndex, InterruptHandler, this);
	m_pDevice->GetDMABuffer(m_ulDmaChannel, &m_dwBufferSize, (PVOID *) &m_pBufferBits);

	m_dwInterruptInterval = m_dwBufferSize/HDA_BDL_DEFAULT; 


	m_dwBufferWrapTime = (m_dwBufferSize * 1000) / m_wfx.nAvgBytesPerSec;
	m_pDevice->SetDMAChannelFormat(m_ulDmaChannel, m_wfx.nChannels, m_wfx.wBitsPerSample, m_wfx.nSamplesPerSec, m_wfx.wFormatTag);

	if (!m_HdrQ.Initialize(pWOD, m_fIsPlayback ? MM_WOM_DONE : MM_WIM_DATA)) {
	    // something went wrong
	    return MMSYSERR_ERROR;
	}

	return MMSYSERR_NOERROR;
}


MMRESULT 
CStreamContext::Close (void)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);

    EnterCriticalSection(&m_cs); // bypass the usual CAutoLock to avoid releasing the CS after deleting it.

    if (!m_HdrQ.IsDone())
    {
        LeaveCriticalSection(&m_cs);
        return WAVERR_STILLPLAYING;
    }

    // Use Release instead of the destructor, since we don't want to
    // delete the object if it's in use by the interrupt handler.
    // Also, Release will invoke the destructor, which deletes the critical section
    // so we exit the CS here, BEFORE we invoke release.
    LeaveCriticalSection(&m_cs);
    Release();
    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::AddBuffer (LPWAVEHDR lpHeader)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
	EnterCriticalSection(&m_cs);

    if (!m_HdrQ.AddBuffer(lpHeader)) {
        DEBUGMSG(1, (TEXT("[TID=0x%x] header already queued\r\n")));
		LeaveCriticalSection(&m_cs);
        return MMSYSERR_INVALPARAM;
    }

    if (!m_fPaused ) {
        if (m_fStarted) {
            // if buffer is up and running, get this latest data
            // into the buffer: don't wait for the interrupt thread

            // We're trying to minimize the latency by transfering data into
            // the DMA buffer on AddBuffer, rather than wait for
            // the next interrupt. This is a worthy goal,
            // but one should worry about about the callback re-entrancy problem:
            // AdvanceCurrentPosition can trigger callbacks,
            // which in some twisted but legal cases could lead
            // to the stream being closed before AdvanceCurrentPosition returns.
            // However, the wave API guarantees that the Close will never
            // happen on this thread, and we're protected by the m_cs lock
            // we we're OK.
           HandleInterrupt();
        }
        else {
            // buffer was stopped: get it going.
            Start();
        }
    }
	LeaveCriticalSection(&m_cs);
    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::Unpause (void)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
	EnterCriticalSection(&m_cs);
	DEBUGMSG(1, (TEXT("U")));

    if (m_fPaused) {
        ASSERT(!m_fStarted);
        if (! m_HdrQ.IsEmpty()) {
            Start();
        }
        m_fPaused = FALSE;
    }
	LeaveCriticalSection(&m_cs);
    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::Pause (void)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
    CAutoLock cal(&m_cs);
    		
    if (!m_fPaused) {
        Stop();
        m_fPaused = TRUE;
    }
    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::Reset (void)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
	EnterCriticalSection(&m_cs);
	DEBUGMSG(1, (TEXT("R")));

    Stop();
    m_HdrQ.Reset();
	
	if (m_fIsPlayback)
	{
		// playback must be unpaused on reset
		Unpause();
	}
	else 
	{
		// input must be paused on reset.
		Pause();
	}
	
	//Transfer variables reset
    m_dwTransferCursor = 0;
    m_dwPlayFlags = FALSE;
    m_dwAC3FrameBytesCopied = 0;
    m_wNextByte = 0xFFFF;

	LeaveCriticalSection(&m_cs);

	return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::GetPosition (LPMMTIME pmmt)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
    
    CAutoLock cal(&m_cs);
    DWORD dwBytePosition = m_HdrQ.GetBytePosition();

    switch (pmmt->wType) {

        case TIME_BYTES:
            pmmt->u.cb = dwBytePosition;
            break;

        case TIME_SAMPLES:
            pmmt->u.sample = dwBytePosition / m_wfx.nBlockAlign;
            break;

        case TIME_MS:
            // caveat emptor: don't call SetPlaybackRate and expect time-based
            // positions to mean much. Maybe return not supported?
            if (m_wfx.nAvgBytesPerSec != 0 && m_dwPlaybackRate == 0x10000) {
                pmmt->u.ms = (dwBytePosition * 1000) / m_wfx.nAvgBytesPerSec;
                break;
            }

        case TIME_MIDI:
        case TIME_TICKS:
        case TIME_SMPTE:
            //
            // We don't support these, so return TIME_BYTES instead.
            //
            pmmt->wType = TIME_BYTES;
            pmmt->u.cb = dwBytePosition;
            break;
    }

    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::BreakLoop (void)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
    CAutoLock cal(&m_cs);
    m_HdrQ.BreakLoop();
    return MMSYSERR_NOERROR;
} 

MMRESULT 
CStreamContext::GetPlaybackRate (PULONG pulRate)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);

    CAutoLock cal(&m_cs);
    *pulRate = m_dwPlaybackRate;
    return MMSYSERR_NOERROR;
}


// waveOutSetPlaybackRate
MMRESULT 
CStreamContext::SetPlaybackRate (ULONG ulRate)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);

    CAutoLock cal(&m_cs);

    __int64 nFrequency = ((__int64) m_wfx.nSamplesPerSec * (__int64) ulRate) >> 16;
    DEBUGMSG(ZONE_INFO, (TEXT("SetPlaybackRate(%08x,%5d) = %5d\r\n"), ulRate, m_wfx.nSamplesPerSec, (DWORD) nFrequency));

    DWORD dwSampleRate = (DWORD) nFrequency;

    if (dwSampleRate > 48000) {
        return MMSYSERR_ERROR;
    }

    m_pDevice->SetFrequency((UCHAR) m_ulDmaChannel, m_wfx.wFormatTag, (USHORT) dwSampleRate);

    m_dwPlaybackRate = ulRate;
    // also update our expected buffer wrap time
    // Sharp changes in playback rate will cause our current wrap-detection logic to fail
    // We need to insert code here that updates the known cursor positions for the benefit of
    // the heartbeat thread.
    m_dwBufferWrapTime = (m_dwBufferSize * 1000) / (DWORD) nFrequency;
    return MMSYSERR_NOERROR;

}

MMRESULT 
CStreamContext::GetVolume (PULONG pulVolume)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);

    CAutoLock cal(&m_cs);
    *pulVolume = m_dwVolume;
    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::SetVolume (PMMDRV_MESSAGE_PARAMS pParams)	//(ULONG ulVolume)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);

    CAutoLock cal(&m_cs);
    m_dwVolume = pParams->dwParam1;;

    // need to convert the waveOut form of volume to dsound form
    // wave has absolute attenuation for each channel in low/high words of m_dwVolume
    // dsound has relative pan in one LONG, overall volume in another.
    // both have logarithmic scales, though, se we can do simple linear scaling.

    LONG nVolumeLeft  = m_dwVolume & 0xFFFF;
    LONG nVolumeRight = (m_dwVolume >> 16) & 0xFFFF;
    
    const int WAVE_VOLUME_MAX = 0xFFFF;
    const int scale_factor = -10000; // SetGain uses a 0-100dB scale. Adjust accordingly.

    // Convert wave volumes to dB:
    
    LONG nGainLeft, nGainRight;
    if (nVolumeLeft > 0) {
        nGainLeft = scale_factor * (WAVE_VOLUME_MAX - nVolumeLeft ) / WAVE_VOLUME_MAX;
    }
    else {
        nGainLeft = VOLUME_MIN;
    }
    if (nVolumeRight > 0) {
        nGainRight= scale_factor * (WAVE_VOLUME_MAX - nVolumeRight) / WAVE_VOLUME_MAX;
    }
    else {
        nGainRight = VOLUME_MIN;
    }

    DEBUGMSG(ZONE_VOLUME, (TEXT("CStreamContext::SetVolume(%08x) -> %5d, %5d\r\n"), m_dwVolume, nGainRight, nGainLeft));

    USHORT usLeftVol, usRightVol;
    // Convert Decibels to Amplification factor and scale to suit SRC register
    //   [0 - 0xFFFF] scaled to [0 - 0x1000]
    // The Ensoniq SRC volume registers represent a 4.12 fixed-point scale factor.
    // In our case, we never amplify, only attenuate, so the largest scale factor
    // we'll ever use is 0x1000, which corresponds to 1.0. 
    // A value of 0x800 corresponds to a scale factor of 0.5.
    usRightVol = (USHORT) (DBToAmpFactor(nGainRight) >> 4);
    usLeftVol =  (USHORT) (DBToAmpFactor(nGainLeft)  >> 4);

    DEBUGMSG(ZONE_VOLUME, (TEXT("CStreamContext::SetVolume(%08x) => %04x,%04x\r\n"), m_dwVolume, usLeftVol,usRightVol));
    m_pDevice->SetDMAVolume (m_ulDmaChannel, usLeftVol, usRightVol);
	if(pParams->uMsg == WODM_SETVOLUME)
		m_pDevice->SetVolume(pParams->uDeviceId, DIR_PLAY, usLeftVol, usRightVol);	
	else 
		m_pDevice->SetVolume(pParams->uDeviceId, DIR_REC, usLeftVol, usRightVol);

	DEBUGMSG(1, (TEXT("CStreamContext::SetVolume()\r\n")));

    return MMSYSERR_NOERROR;
}

MMRESULT 
CStreamContext::PrepareHeader (LPWAVEHDR lpHeader)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
    // nothing to do
	return MMSYSERR_NOTSUPPORTED;
}

MMRESULT 
CStreamContext::UnprepareHeader (LPWAVEHDR lpHeader)
{
    _MYTHIS_CHECK(return MMSYSERR_INVALHANDLE);
    // nothing to do
	return MMSYSERR_NOTSUPPORTED;
}

BOOL
CStreamContext::Swap16(USHORT *pOutBuffer, USHORT *pInBuffer, DWORD dwInBufferSize)
{
    USHORT value;
    DWORD i;

    for (i = 0; i < dwInBufferSize/2; i++)
    {
		value = pInBuffer[i];
		pOutBuffer[i] = ((value >> 8) | (value << 8));
    }  
  
    return (TRUE);
}

void
CStreamContext::DoTransferAC3Frames(PBYTE pDstBuffer, DWORD dwDstBytes)
{
    PBYTE  pSrcBuffer;
    ULONG  ulSrcBytes;
	
    DWORD  dwNumRawAC3BytesNeeded;
    DWORD  dwNumZerosNeeded;
    DWORD  dwNumDMABytesAvailable;
    BYTE   bTempBuffer[0x0a00];
    DWORD  dwTempBufferSize;
    BOOL   checkFlag;
    BYTE   bPreamble[] = {0x72, 0xf8, 0x1f, 0x4e, 0x01, 0x00, 0x00, 0x00}; 
	
	// update the last two bytes of preamble
	bPreamble[6] = ((m_wfx.nBlockAlign * 8) & 0xff);
	bPreamble[7] = (((m_wfx.nBlockAlign * 8) >> 8) & 0xff);

    DEBUGMSG(ZONE_VERBOSE,(TEXT("CStreamContext::DoTransferAC3Frames dwDstBytes=%d\r\n"), dwDstBytes));

    while (dwDstBytes > 0) 
    {
		// check if we have completely filled the DMA buffer with enough raw AC-3 bytes and zeros.
		// if we haven't then continue to fill the DMA buffer with enough zeros to complete an AC-3
		// frame of 6144 bytes. Also, keep in mind that we may need more zeros to complete a frame 
		// but we can not write pass the DMA buffer. 
		if (m_dwAC3FrameBytesCopied >= (sizeof(bPreamble) + m_wfx.nBlockAlign)) 
		{
			// An AC-3 frame contains 6144 bytes (8 bytes of preamble, 'm_wfx.nBlockAlign' bytes of raw AC-3 and
			// the rest filled with zeros 
			dwNumZerosNeeded = (dwDstBytes > (A52_FRAMESIZE - m_dwAC3FrameBytesCopied)) ? 
			              (A52_FRAMESIZE - m_dwAC3FrameBytesCopied) : dwDstBytes;	
			
			if (dwNumZerosNeeded > 0)
			{
				// fill the frame with zeros
				memset(pDstBuffer, 0, dwNumZerosNeeded);
				pDstBuffer += dwNumZerosNeeded;
				dwDstBytes -= dwNumZerosNeeded;
				m_dwTransferCursor += dwNumZerosNeeded;
				m_dwAC3FrameBytesCopied += dwNumZerosNeeded;	
                DEBUGMSG(ZONE_VERBOSE,(TEXT("Number of Zeros copied=%d\r\n"), dwNumZerosNeeded));	
			}
			
			if (m_dwAC3FrameBytesCopied == A52_FRAMESIZE)
			{
				// end of a frame, reset 'm_wAC3FrameBytesCopied'
				m_dwAC3FrameBytesCopied = 0; 
			}
		}
		else
		{
			// the frame starts with 8 preamble bytes 
			if (m_dwAC3FrameBytesCopied == 0)
			{
				if (dwDstBytes < sizeof(bPreamble))
				{
					// we're at the begining of the AC-3 frame and we don't have enough space in the DMA buffer 
					// so don't do anything, wait until we have enough space in the DMA buffer
					break;
				}
				
				// compute how many raw AC-3 bytes needed to fill a frame
				dwNumRawAC3BytesNeeded = m_wfx.nBlockAlign; 
				
				// we may have more data waiting to be transfered but we can't transfer more data than 
				// the DMA buffer can handle so we need to make sure we're not writing pass the DMA buffer
				dwNumDMABytesAvailable = ((dwDstBytes - sizeof(bPreamble)) > dwNumRawAC3BytesNeeded) ?
						dwNumRawAC3BytesNeeded : (dwDstBytes - sizeof(bPreamble)); 
			}
			else
			{
				// compute how many raw AC-3 bytes needed to fill a frame
				dwNumRawAC3BytesNeeded = ((m_wfx.nBlockAlign + sizeof(bPreamble)) - m_dwAC3FrameBytesCopied);
				
				// we may have more data waiting to be transfered but we can't transfer more data than 
				// the DMA buffer can handle so we need to make sure we're not writing pass the DMA buffer
				dwNumDMABytesAvailable = (dwDstBytes > dwNumRawAC3BytesNeeded) ? dwNumRawAC3BytesNeeded : dwDstBytes;
			}
			
			while ( (dwNumDMABytesAvailable > 0) &&
					(m_HdrQ.GetNextBlock (dwNumDMABytesAvailable, &pSrcBuffer, &ulSrcBytes))) 
			{
				DEBUGMSG(ZONE_VERBOSE,(TEXT("GetNextBlock (%d, 0x%08x, %d)\r\n"),dwNumDMABytesAvailable, pSrcBuffer, ulSrcBytes));
                ASSERT(ulSrcBytes <= dwDstBytes);
				
				dwTempBufferSize = 0;
				if (m_dwAC3FrameBytesCopied == 0)
				{
					// need to check for sync word (0x0b77)
					checkFlag = FALSE;
					while ((pSrcBuffer != NULL) && (ulSrcBytes > 0))
					{
						if (m_wNextByte != 0xFFFF)
						{
							// one byte remaining from last block
							if ((m_wNextByte == 0x0b) && (*pSrcBuffer == 0x77))
							{
								checkFlag = TRUE;
								break;
							}
							else
							{
								m_wNextByte = 0xFFFF;
								continue;
							}
						}
						else if ((*pSrcBuffer == 0x0b) && (*(pSrcBuffer+1) == 0x77))
						{
							checkFlag = TRUE;
							break;
						}
						else if (ulSrcBytes == 1)
						{
							m_wNextByte = *pSrcBuffer;
							checkFlag = FALSE;
						}
						pSrcBuffer++;
						ulSrcBytes--;
					}
					
					if (checkFlag == FALSE)
					{
						// we could not find the sync word in 'pSrcBuffer'
						continue;
					}
			
					DEBUGMSG(ZONE_VERBOSE,(TEXT("checkFlag=%d, ulSrcBytes=%d, m_wNextByte=%x\r\n"), checkFlag, ulSrcBytes, m_wNextByte));
					// as I mentioned earlier, the preamble needs the information from the first 6 bytes
					// of an AC-3 frame, therefore we need to wait until we have 6 bytes of raw AC-3 data
					// check if we still have a byte remaining when checking for sync word (0x0b77),
					// if we do than add it in first
					if (m_wNextByte != 0xFFFF)
					{
						bTempBuffer[dwTempBufferSize++] = (BYTE) m_wNextByte;
						dwNumDMABytesAvailable -= 1;	
						// reset 'm_wNextByte'
						m_wNextByte = 0xFFFF;
					}

					CeSafeCopyMemory (&bTempBuffer[dwTempBufferSize], pSrcBuffer, ulSrcBytes);
					dwTempBufferSize += ulSrcBytes;
					
					// AC-3 data is big-endian, intel architecture is using little-endian.
					// We don't need to swap the preamble since we already encoded it using little-endian.
					// Now we are ready to copy preamble bytes into DMA buffer.
					CeSafeCopyMemory (pDstBuffer, bPreamble, sizeof(bPreamble)); 
					pDstBuffer += sizeof(bPreamble);
					dwDstBytes -= sizeof(bPreamble);
					m_dwTransferCursor += sizeof(bPreamble);
					
					// we really need to swap data before putting into DMA buffer. Also, if the number of 
					// bytes is odd, we can not swap the last byte so save the last byte for next time use.
					if ((dwTempBufferSize % 2) != 0)
					{
						m_wNextByte = bTempBuffer[dwTempBufferSize-1];
						dwTempBufferSize -= 1;
					}
					
					// now we can swap it and put it in DMA buffer
					Swap16((WORD *) bTempBuffer, (WORD *)bTempBuffer, dwTempBufferSize);
					CeSafeCopyMemory (pDstBuffer, bTempBuffer, dwTempBufferSize);
					pDstBuffer += dwTempBufferSize;
					dwDstBytes -= dwTempBufferSize;
					m_dwTransferCursor += dwTempBufferSize;
					
					// set the number of AC-3 frame bytes so now where we are if we haven't
					// completely filled the frame
					m_dwAC3FrameBytesCopied += (sizeof(bPreamble) + dwTempBufferSize);
				}
				else
				{
					// we are here because we could not get enough data to fill an AC-3 frame
					// the last time because we either did not have enough data in the QUEUE or we
					// did not have enough space in the DMA buffer. Continue where we're left of...
					// check if we have the last odd byte from last try
					if (m_wNextByte != 0xFFFF)
					{
						bTempBuffer[dwTempBufferSize++] = (BYTE) m_wNextByte;
						// reset 'm_wNextByte'
						m_wNextByte = 0xFFFF;
					}
					
					CeSafeCopyMemory (&bTempBuffer[dwTempBufferSize], pSrcBuffer, ulSrcBytes);
					dwTempBufferSize += ulSrcBytes;	
					
					// if the number of bytes is odd, we can not swap the last byte so save the 
					// last byte for next time use.
					if ((dwTempBufferSize % 2) != 0)
					{
						m_wNextByte = bTempBuffer[dwTempBufferSize-1];
						dwTempBufferSize -= 1;
					}
			
					// now we can swap it and put it in DMA buffer
					Swap16((WORD *) bTempBuffer, (WORD *)bTempBuffer, dwTempBufferSize);
					CeSafeCopyMemory (pDstBuffer, bTempBuffer, dwTempBufferSize);
                    pDstBuffer += dwTempBufferSize;
                    dwDstBytes -= dwTempBufferSize;
                    m_dwTransferCursor += dwTempBufferSize;
					
					// set the number of AC-3 frame bytes so now where we are if we haven't
					// completely filled the frame
					m_dwAC3FrameBytesCopied += dwTempBufferSize;
					DEBUGMSG(ZONE_VERBOSE,(TEXT("m_dwAC3FrameBytesCopied=%d\r\n"), m_dwAC3FrameBytesCopied));
				}
				dwNumDMABytesAvailable -= ulSrcBytes;
			}
			
			if (dwNumDMABytesAvailable > 0)
			{
				// we are here because we don't have enough raw data in the QUEUE to fill
				// an AC-3 frame even though we do have enough space in DMA buffer. Let't exit and wait
				// for the next round to complete a frame.
				break;
			}
		}
    }
}

// transfers queued data into the directsound buffer
// advances the write cursor
// If we don't have enough data queued up, we pad with silence, but only advance the
// transfer cursor as far as the real data goes.
VOID
COutputStreamContext::DoTransfer (PBYTE pDstBuffer, DWORD dwDstBytes)
{ 
	PBYTE pSrcBuffer;
	ULONG ulSrcBytes;

    switch (m_wfx.wFormatTag)
    {
        case WAVE_FORMAT_A52:
        { 
            DoTransferAC3Frames(pDstBuffer, dwDstBytes);
            break;
        }
		default:  // PCM or AC3 in WAVE format
        {
            while (dwDstBytes > 0 && m_HdrQ.GetNextBlock (dwDstBytes, &pSrcBuffer, &ulSrcBytes)) 
            {
				DEBUGMSG(ZONE_VERBOSE,(TEXT("GetNextBlock (%d, 0x%08x, %d)\r\n"),dwDstBytes, pSrcBuffer, ulSrcBytes));
				ASSERT(ulSrcBytes <= dwDstBytes);
				CeSafeCopyMemory  (pDstBuffer, pSrcBuffer, ulSrcBytes);
				//memset (pDstBuffer, 0, ulSrcBytes);
				pDstBuffer += ulSrcBytes;
				dwDstBytes -= ulSrcBytes;
				m_dwTransferCursor += ulSrcBytes;
			}
			break;
        }
    }

    // check for fill cursor at end of buffer and wrap
    ASSERT(m_dwTransferCursor <= m_dwBufferSize);
    if (m_dwTransferCursor == m_dwBufferSize) {
        m_dwTransferCursor = 0;
    }
}

// the symmetric opposite of COutputStreamContext::DoTransfer
// Copies data FROM the buffer TO the Header Queue
void CInputStreamContext::DoTransfer (PBYTE pSrcBuffer, DWORD dwSrctBytes)
{ PBYTE pDstBuffer;
  ULONG ulDstBytes;

    while (dwSrctBytes > 0 && m_HdrQ.GetNextBlock (dwSrctBytes, &pDstBuffer, &ulDstBytes)) {
        CeSafeCopyMemory (pDstBuffer, pSrcBuffer, ulDstBytes);
        pSrcBuffer += ulDstBytes;
        dwSrctBytes -= ulDstBytes;
        m_dwTransferCursor += ulDstBytes;
    }
    // when there is more capture data than fits in the Header Queue, we drop it immediately

    // check for fill cursor at end of buffer and wrap
    ASSERT(m_dwTransferCursor <= m_dwBufferSize);
    if (m_dwTransferCursor == m_dwBufferSize) {
        m_dwTransferCursor = 0;
    }

}

// Transfer data to/from the DMA buffer and the WAVEHDR buffer.
// Start at the current m_dwTransferCursor and transfer dwTransferSize bytes.
// Handle the wrap-around case using the buffer lock mechanism
// Uses the virtual DoTransfer function to handle both input and output transfers

VOID
CStreamContext::DoSplitTransfer(DWORD dwTransferSize)
{ DWORD dwCount;
  DWORD dwBytesLeft;
  PBYTE pBuffer;

    ASSERT(dwTransferSize <= m_dwBufferSize);

    // we want to transfer dwTransferSize, but we need to figure out the address within the DMA buffer
    dwBytesLeft = m_dwBufferSize - m_dwTransferCursor;
    pBuffer = m_pBufferBits + m_dwTransferCursor;
    dwCount = dwTransferSize;
    if (dwCount > dwBytesLeft) {
        dwCount -= dwBytesLeft;
        // transfer to the end of the DMA buffer, then
        // wrap around to the start of the buffer
        DoTransfer(pBuffer, dwBytesLeft);
        pBuffer = m_pBufferBits;
    }

    DoTransfer(pBuffer, dwCount);
}

VOID 
CStreamContext::Stop(void)
{
    if (m_fStarted) {
        m_pDevice->StopDMAChannel( m_ulDmaChannel );
        m_fStarted = FALSE;
    }
}

VOID 
CStreamContext::Start(void)
{
    ASSERT(!m_fStarted);
    ASSERT(!m_HdrQ.IsEmpty()); // not true for capture streams

	if (!m_fStarted) {
    // initialize the current DMA position and window

		m_dwPreTime = GetTickCount();
		m_dwTransferCursor = 0;
		m_dwLastDMACursor = 0;
		m_dwAC3FrameBytesCopied = 0;	
		m_wNextByte = 0xFFFF;

		if (!m_dwPlayFlags)
		{
			m_pDevice->SetDMALooping(m_ulDmaChannel, TRUE);
			m_dwPlayFlags = TRUE;
		}

		if (m_fIsPlayback) {
			// output streams start off with a full-buffer transfer
			// input streams must wait for data to accumulate in DMA bufffer before first transfer
			DoSplitTransfer(m_dwBufferSize);
		}
  
		m_fStarted = TRUE;
		m_pDevice->SetDMAPosition(  m_ulDmaChannel, 0);
		m_pDevice->SetDMAChannelBuffer( m_ulDmaChannel, m_dwBufferSize, (USHORT) m_dwInterruptInterval/m_wfx.nBlockAlign);
		m_pDevice->StartDMAChannel(  m_ulDmaChannel );
	}
	DEBUGMSG(0, (TEXT("[TID=0x%x]  - CSC::Start. (0x%x)\r\n"), GetCurrentThreadId(),this));
}

VOID
COutputStreamContext::GetAdvance(DWORD & dwAdvance, DWORD & dwRoomInBuffer)
{
    // Reading the DMA position in a way that completely excludes the possibility
    // of race conditions is an exercise in surreal temporal mechanics.
    // Strictly speaking, one can never know for sure where the DMA position is, because
    // an arbitrary amount of time may have elapsed since we actually looked at the hardware.
    // By the same reasoning, it's impossible to know precisely what the current time is.
    // However, one can get a grip on reality by reading the time before and after
    // reading the DMA position, thereby ensuring that we at least have window
    // of time during which we know the DMA position was valid.

    DWORD dwPreTime = GetTickCount();
    DWORD dwDMACursor = m_pDevice->GetDMAPosition(m_ulDmaChannel);

    ASSERT(dwDMACursor <= m_dwBufferSize);

    DWORD dwPostTime = GetTickCount();

    // can't advance past the transfer cursor
    DWORD dwMaxAdvance;
    if (m_dwLastDMACursor < m_dwTransferCursor) {
        dwMaxAdvance = m_dwTransferCursor - m_dwLastDMACursor;
    }
    else {
        dwMaxAdvance = m_dwBufferSize + m_dwTransferCursor - m_dwLastDMACursor;
    }


    // determine how much the DMA position has advanced
    // first, compute the worst-case elapsed time since we last read the DMA position
    DWORD dwElapsed = dwPostTime - m_dwPreTime;
    DWORD dwOldLast = m_dwLastDMACursor;

    // assume we're safe & that there hasn't been enough time to wrap.
    if (dwDMACursor >= m_dwLastDMACursor) {
        dwAdvance = dwDMACursor - m_dwLastDMACursor;
    }
    else {
        dwAdvance =  m_dwBufferSize + dwDMACursor- m_dwLastDMACursor;
    }
    if ((dwElapsed >= m_dwBufferWrapTime-1) || (dwAdvance > dwMaxAdvance)) {
        // we know for sure we've wrapped...
        // or that the dma cursor advanced past the transfer cursor.
        // either way, limit the advance to the end of cued data, mark the entire buffer as available
//        DEBUGMSG(ZONE_WARNING, (TEXT("GetAdvance: underflow %3d %3d\r\n"), dwElapsed, m_dwBufferWrapTime));
        dwAdvance = dwMaxAdvance;           // we consumed all their was to consume
        dwRoomInBuffer = m_dwBufferSize;    // we can next consume the entire buffer
        m_dwTransferCursor = dwDMACursor;  // but start at where the DMA cursor is *now*
        memset(m_pBufferBits, m_ucSilence, m_dwBufferSize);
    }
    else {
        // but how much room is there in the buffer?
        DWORD dwBytesBeforeUnderflow = dwMaxAdvance - dwAdvance;
        // if we're close to underflowing (i.e. no fresh data from stream source)
        // we may want to silence-pad beyond the transfer cursor to avoid playing garbage
        if (m_fIsPlayback && dwBytesBeforeUnderflow <= m_dwInterruptInterval) {
            // clear enough of the buffer with silence to get us through the next interrupt
            // worry about wrap-around, though
            if (m_dwTransferCursor + m_dwInterruptInterval <= m_dwBufferSize) {
                // safely do it on one piece
                memset(m_pBufferBits + m_dwTransferCursor, m_ucSilence, m_dwInterruptInterval);
            }
            else {
                // must wrap around the end of the buffer
                memset(m_pBufferBits + m_dwTransferCursor, m_ucSilence, m_dwBufferSize - m_dwTransferCursor);
                memset(m_pBufferBits                     , m_ucSilence, m_dwTransferCursor + m_dwInterruptInterval - m_dwBufferSize);
            }
        }

        // finally, compute the amount of room left in the buffer for fresh data
        dwRoomInBuffer = m_dwBufferSize - dwBytesBeforeUnderflow;
    }


    m_dwPreTime = dwPreTime;
    m_dwLastDMACursor = dwDMACursor;

    if (dwAdvance > m_dwBufferSize) {
        DEBUGMSG(1, (TEXT("Ensoniq: Over-advance! %08x %08x %08x\r\n"), dwAdvance, dwDMACursor, dwOldLast));
    }

}

VOID
CInputStreamContext::GetAdvance(DWORD & dwAdvance)
{
    DWORD dwPreTime = GetTickCount();
	
    DWORD dwDMACursor = m_pDevice->GetDMAPosition(m_ulDmaChannel);

    ASSERT(dwDMACursor <= m_dwBufferSize);

    DWORD dwPostTime = GetTickCount();

    // determine how much the DMA position has advanced
    // first, compute the worst-case elapsed time since we last read the DMA position
    DWORD dwElapsed = dwPostTime - m_dwPreTime;  
    DWORD dwOldLast = m_dwLastDMACursor;

    if (dwElapsed >= m_dwBufferWrapTime-1) {
        // we know for sure we've wrapped...
        // or that the dma cursor advanced past the transfer cursor.
        // either way, limit the advance to the end of cued data, mark the entire buffer as available
        DEBUGMSG(ZONE_WARNING, (TEXT("GetAdvance: underflow %3d %3d\r\n"), dwElapsed, m_dwBufferWrapTime));
        dwAdvance = m_dwBufferSize;    // we can next consume the entire buffer
        m_dwTransferCursor = dwDMACursor;  // but start at where the DMA cursor is *now*
    }
    else {
        // we're safe & that there hasn't been enough time to wrap.
        if (dwDMACursor >= m_dwLastDMACursor) {
            dwAdvance = dwDMACursor - m_dwLastDMACursor;
        }
        else {
            dwAdvance =  m_dwBufferSize + dwDMACursor- m_dwLastDMACursor;
        }
    }

	if (dwAdvance > m_dwInterruptInterval)
		dwAdvance = m_dwInterruptInterval;
	dwAdvance -= 8; // Remove the unstable noise.

    m_dwPreTime = dwPreTime;
	if ((m_dwLastDMACursor + dwAdvance)>=m_dwBufferSize)
		m_dwLastDMACursor = m_dwLastDMACursor + dwAdvance - m_dwBufferSize;
	else
		m_dwLastDMACursor = m_dwLastDMACursor + dwAdvance;

    if (dwAdvance > m_dwBufferSize) {
		DEBUGMSG(1, (TEXT("HDA: Over-advance! %08x %08x %08x\r\n"), dwAdvance, dwDMACursor, dwOldLast));
    }

}

VOID 
CStreamContext::InterruptHandler(PVOID pArg)
{ 
    CStreamContext * pthis = (CStreamContext *) pArg;
    // addref/release around interrupt handler to avoid having the 
    // object pulled out from under the interrupt thread
    pthis->AddRef();
    pthis->HandleInterrupt();
    pthis->Release();
}

void
COutputStreamContext::HandleInterrupt(void)
{

	EnterCriticalSection(&m_cs);

	DWORD dwAdvance, dwRoomInBuffer;

	m_pDevice->InterruptRoutine(m_ulDmaChannel);

	GetAdvance(dwAdvance, dwRoomInBuffer);

	// notify the Header Queue of the advance so it can (possibly) make callbacks.
	m_HdrQ.AdvanceCurrentPosition(dwAdvance);

	if (m_HdrQ.IsDone()) {
	    // finally, if the header queue is played out and we've played past the last known fill cursor position,
	    // we can stop the buffer and mark this stream as not running
	    Stop();
	}
	else {
	    DoSplitTransfer(dwRoomInBuffer); // transfer fresh data from the header queue into the DMA buffer
	}

	LeaveCriticalSection(&m_cs);

}

void
CInputStreamContext::HandleInterrupt(void)
{
    CAutoLock cal(&m_cs);

    DWORD dwAdvance;

	m_pDevice->InterruptRoutine(m_ulDmaChannel);
	
    GetAdvance(dwAdvance);

    DoSplitTransfer(dwAdvance); // transfer freshly captured data into WAVEHDR buffers

    // notify the Header Queue of the advance so it can (possibly) make callbacks.
    m_HdrQ.AdvanceCurrentPosition(dwAdvance);

    if (m_HdrQ.IsDone()) {
        // finally, if the header queue is played out and we've played past the last known fill cursor position,
        // we can stop the buffer and mark this stream as not running
        Stop();
    }
}

