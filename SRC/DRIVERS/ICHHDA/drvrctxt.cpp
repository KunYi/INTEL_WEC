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

#define DEVICEID_MAX    MAX_STREAM

/////////////////////////////////////////////////////////////////////////////
//
// CDriverContext Methods
//
/////////////////////////////////////////////////////////////////////////////
CDriverContext::CDriverContext (HKEY hDevKey)
{
    MYTHIS_CONSTRUCTOR;

    m_hDevKey = hDevKey;
    m_pDevice = NULL;
    m_ulVolume = 0xFFFFFFFF; // volume start off at max.
    m_pHead = NULL;         // list of open mixer handles
    m_pMxLineState = NULL;
    m_pMxControlState = NULL;
    m_CurrentDx = D0;

	m_dwAC3Allowed = FALSE;
	m_dwDTSAllowed = FALSE;
	m_dwWMA3Allowed = FALSE;
	
	GetFormatTypes((PULONG)&m_wFormatsAllowed);

	m_dw44kHzAllowed = FALSE;
	
}

CDriverContext::~CDriverContext ()
{
    MYTHIS_CHECK;

    if (m_hDevKey)
    { 
        RegCloseKey(m_hDevKey);
    }

    if (m_pDevice != NULL) {
        delete m_pDevice;
        m_pDevice = NULL;
    }

    if (m_pMxLineState) {
        delete [] m_pMxLineState;
        m_pMxLineState = NULL;
    }
    if (m_pMxControlState) {
        delete [] m_pMxControlState;
        m_pMxControlState = NULL;
    }

    MYTHIS_DESTRUCTOR;
}

BOOL     
CDriverContext::Initialize (PCTSTR pszActiveKey)
{
    MYTHIS_CHECK;

    HANDLE threadHandle;

    // set up convenient access to our registry key
    CRegKey regkey(pszActiveKey);

	threadHandle = GetCurrentThread();
	CeSetThreadPriority(threadHandle, 211);
	//RETAILMSG(1, (TEXT("Audio Driver thread priority level = %d\n"), CeGetThreadPriority(threadHandle)));
	
	DWORD timeval = CeGetThreadQuantum(threadHandle);
	//RETAILMSG(1, (TEXT("CeGetThreadQuantum() = %d\r\n"), timeval));
	CeSetThreadQuantum(threadHandle, 0);
	timeval = CeGetThreadQuantum(threadHandle);
	//RETAILMSG(1, (TEXT("CeGetThreadQuantum() = %d\r\n"), timeval));
	
    m_pDevice = new HDA;
    if (m_pDevice == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("Unable to allocate device object\r\n")));
        return FALSE;
    }

    WavGetRegistryValue(m_hDevKey, REGVALUE_AC3ALLOWED, &m_dwAC3Allowed);
    WavGetRegistryValue(m_hDevKey, REGVALUE_DTSALLOWED, &m_dwDTSAllowed);
    WavGetRegistryValue(m_hDevKey, REGVALUE_WMA3ALLOWED, &m_dwWMA3Allowed);
    WavGetRegistryValue(m_hDevKey, REGVALUE_MAXPCMOUTLEVELGAIN, &m_pDevice->m_dwMaxPcmOutLevelGain);

	// save original format
	GetFormatTypes((PULONG)&m_wFormatsAllowed);

    if (! m_pDevice->MapDevice(&regkey, pszActiveKey)) {
        DEBUGMSG (ZONE_ERROR, (TEXT("Error mapping device\r\n")));
        return FALSE;
    }
	
    m_ulBufferSize = HDA_BUF_SIZE_DEFAULT;

	if (! InitializeMixerState ()) {
	        DEBUGMSG(ZONE_ERROR, (TEXT("CDriverContext::Initialize - failed to initialize mixer interface\r\n")));
	        return FALSE;
    }
 
    return TRUE;
}

MMRESULT 
CDriverContext::GetCaps (ULONG msg, PWAVEOUTCAPS pCaps)
{
    MYTHIS_CHECK;

    pCaps->wMid = MM_MICROSOFT;
    if (msg == WIDM_GETDEVCAPS) {
        pCaps->wPid = MM_MSFT_WSS_WAVEIN; // generic in
    }
    else {
        pCaps->wPid = MM_MSFT_WSS_WAVEOUT;  // generic out...
    }
    pCaps->vDriverVersion = DRIVER_VERSION;

     // If you change the product name, remember that the szPname field is only 32 characters,
    // so the name must be 31 characters or less to allow for the NULL at the end.
    // If this assert fires, your device name is too long to fit into the caps structure.
    C_ASSERT(sizeof(DEVICE_NAME)<=sizeof(pCaps->szPname));

    StringCbCopy (pCaps->szPname, sizeof(pCaps->szPname), DEVICE_NAME);

	// only support 16-bit sample rate
	pCaps->dwFormats = 0;
	if(m_pDevice->m_CodecID == HDA_CODEC_ALC268)
		pCaps->dwFormats = WAVE_FORMAT_4S16;  
	else 
	pCaps->dwFormats = WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16;  


    pCaps->wChannels = 2;
    if (msg == WODM_GETDEVCAPS) {
        // only fill out dwSupport for output caps
        // this field is not present in the input caps
        pCaps->dwSupport = WAVECAPS_VOLUME;

    }

    return MMSYSERR_NOERROR;
}

MMRESULT 
CDriverContext::GetExtCaps (PWAVEOUTEXTCAPS pCaps)
{	
    pCaps->dwMaxHwStreams = m_pDevice->m_maxStream;            // max number of hw-mixed streams
    pCaps->dwFreeHwStreams = 1;            // available HW streams
    pCaps->dwSwMixerSampleRate = 0;        // preferred sample rate for software mixer (0 indicates no preference)
    pCaps->dwSwMixerBufferSize = 0;        // preferred buffer size for software mixer (0 indicates no preference)
    pCaps->dwSwMixerBufferCount = 0;       // preferred number of buffers for software mixer (0 indicates no preference)
    pCaps->dwMinSampleRate = 44100;            // minimum sample rate for a hw-mixed stream
    pCaps->dwMaxSampleRate = 96000;            // maximum sample rate for a hw-mixed stream

    return MMSYSERR_NOERROR;
}


MMRESULT 
CDriverContext::OpenStream (ULONG msg, HSTREAM * ppStream, LPWAVEOPENDESC pWOD, DWORD dwFlags, UINT uDeviceId)
{
    MYTHIS_CHECK;
    MMRESULT mr;
	BOOL isNonPcmAudio = FALSE;

    DEBUGMSG(1,(TEXT("CDriverContext::OpenStream( %S ) - format (%x %d:%d %d %d %d flag=%d)\r\n"),
		(msg == WODM_OPEN) ? "OUT" : "IN",
		pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec, pWOD->lpFormat->nBlockAlign, pWOD->lpFormat->nAvgBytesPerSec,dwFlags));

	switch (pWOD->lpFormat->wFormatTag)
    {
		case WAVE_FORMAT_A52:
		{
			if ( (!m_dwAC3Allowed) ||
				 (pWOD->lpFormat->nSamplesPerSec != 48000) )
            {
 				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
				pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
				return WAVERR_BADFORMAT;
            }

            isNonPcmAudio = TRUE;
			break;
        }
		case WAVE_FORMAT_DTS:
		{
			if ( (!m_dwDTSAllowed) || (pWOD->lpFormat->nSamplesPerSec != 48000) )
            {
 				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
				pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
				return WAVERR_BADFORMAT;
            }

            isNonPcmAudio = TRUE;
			break;
		}
		case WAVE_FORMAT_DOLBY_AC3_SPDIF:
		{        
			if ( (!m_dwAC3Allowed) || (pWOD->lpFormat->nChannels != 2) || 
				 (pWOD->lpFormat->wBitsPerSample != 16) ||
				 (pWOD->lpFormat->nSamplesPerSec != 48000) )
			{

				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
		  		pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
				return WAVERR_BADFORMAT;
   			}
			isNonPcmAudio = TRUE;
			break;
		}
		case WAVE_FORMAT_WMA3:
		{
			if ( (!m_dwWMA3Allowed) || (pWOD->lpFormat->nSamplesPerSec != 48000) )
            {
 				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
				pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
				return WAVERR_BADFORMAT;
            }

            isNonPcmAudio = TRUE;
			break;
		}
		case WAVE_FORMAT_PCM:
		{
			if (pWOD->lpFormat->nChannels != 2) {
				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream -  (%d) Channels not supported\r\n"), pWOD->lpFormat->nChannels));

			}

			if ( (pWOD->lpFormat->nBlockAlign != (pWOD->lpFormat->wBitsPerSample * pWOD->lpFormat->nChannels) / 8) || 
				 (pWOD->lpFormat->nAvgBytesPerSec != pWOD->lpFormat->nBlockAlign * pWOD->lpFormat->nSamplesPerSec) ) 
			{
				DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream - ill-formed waveformat\r\n")));
				return MMSYSERR_INVALPARAM;
			}
			break;
		}
		default:
		{
			DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
		  		pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
			return WAVERR_BADFORMAT;
		}
	}

	if (m_pDevice->TestCap(msg, pWOD) != TRUE) {
        DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream(WODM) - format (%x %d:%d %d) not supported\r\n"),
            pWOD->lpFormat->wFormatTag,pWOD->lpFormat->nChannels,pWOD->lpFormat->wBitsPerSample,pWOD->lpFormat->nSamplesPerSec));
		return WAVERR_BADFORMAT;
	}

    if (dwFlags & WAVE_FORMAT_QUERY) {
        // if this is just a query, we're done.
        return MMSYSERR_NOERROR;
    }
  
	// attempt to allocate a DMA channel
    ULONG ulChannelIndex;
    mr = m_pDevice->AllocDMAChannel((msg==WODM_OPEN) ? DMADIR_OUT : DMADIR_IN, m_ulBufferSize, 
									 pWOD->lpFormat->wFormatTag, uDeviceId, &ulChannelIndex);
    if (MMFAILED(mr)) {
        DEBUGMSG(ZONE_ERROR,(TEXT("CDriverContext::OpenStream - allocate DMA channel failed\r\n")));
        return mr;
    }

	if (ulChannelIndex >= (ULONG) m_pDevice->m_pHda_Core->iNumIss){
		m_pDevice->InitChannel(DIR_PLAY,ulChannelIndex);
	}
	else
		{
		m_pDevice->InitChannel(DIR_REC,ulChannelIndex);
	}
	DEBUGMSG(1,(TEXT("INFO: CDriverContext::OpenStream() uDeviceId = %d.\r\n"), uDeviceId));
	
    // if that worked, allocate a stream context
    HSTREAM pStream;
    if (msg == WODM_OPEN) {
        pStream = new COutputStreamContext;
    }
    else {
        ASSERT(msg == WIDM_OPEN);
        pStream = new CInputStreamContext;
    }
	
	if (pStream == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CDriverContext::OpenStream - CStreamContext::ctor failed\r\n")));
        m_pDevice->FreeDMAChannel(ulChannelIndex);
        return MMSYSERR_NOMEM;
    }
    
    pStream->m_uStreamId = uDeviceId;

    mr = pStream->Initialize(m_pDevice, ulChannelIndex, pWOD);

    if (mr == MMSYSERR_ERROR) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CDriverContext::OpenStream - CStreamContext::Initialize failed\r\n")));
        delete pStream;
        return mr;
    }

    *ppStream = pStream;

    // we're done! it all worked
    return MMSYSERR_NOERROR;
}


MMRESULT 
CDriverContext::GetVolume (PULONG pulVolume)
{
    MYTHIS_CHECK;

    *pulVolume = m_ulVolume;

    return MMSYSERR_NOERROR;
}

MMRESULT 
CDriverContext::SetVolume (PMMDRV_MESSAGE_PARAMS pParams)		//(ULONG ulVolume)
{
	MYTHIS_CHECK;

	// Device-level SetVolume semantics: 
	// Set the "global" volume that affects all other streams uniformly
	m_ulVolume = pParams->dwParam1; // keep track of what the nominal volume is

	USHORT usLeft = (USHORT)  ((m_ulVolume & 0x0000FFFF));
	USHORT usRight = (USHORT) ((m_ulVolume & 0xFFFF0000) >> 16);

	usLeft = usLeft * 100 / 0xFFFF;
	usRight = usRight * 100 / 0xFFFF;

	DEBUGMSG(ZONE_VOLUME, (TEXT("CDriverContext::SetVolume(%08x) => L:%04x R:%04x\r\n"), m_ulVolume, usLeft, usRight));
	DEBUGMSG(1,(TEXT("CDriverContext::SetVolume(%08x) => L:%04x R:%04x\r\n"), m_ulVolume, usLeft, usRight));

	if(pParams->uMsg == WODM_SETVOLUME)
		m_pDevice->SetVolume(pParams->uDeviceId, DIR_PLAY, usLeft, usRight);	
	else 
		m_pDevice->SetVolume(pParams->uDeviceId, DIR_REC, usLeft, usRight);

	return MMSYSERR_NOERROR;
}

MMRESULT 
CDriverContext::GetFormatTypes(PULONG pulFormat)
{
    MYTHIS_CHECK;

    *pulFormat = (WORD)( (m_dwWMA3Allowed << 2) | 
						 (m_dwDTSAllowed  << 1) | 
						 (m_dwAC3Allowed) );

    return MMSYSERR_NOERROR;
}


MMRESULT 
CDriverContext::SetFormatTypes(PULONG pulFormat)
{
    WORD wFormat;
	
    MYTHIS_CHECK;

    wFormat = (WORD) *pulFormat;
  
    m_dwAC3Allowed  =  (wFormat & 0x0001);
    m_dwDTSAllowed  = ((wFormat & 0x0002) >> 1);
    m_dwWMA3Allowed = ((wFormat & 0x0004) >> 2);

	// save original format
	GetFormatTypes((PULONG)&m_wFormatsAllowed);

    return MMSYSERR_NOERROR;
}

//---------------------------------------------------------------------------
// Power Management Support
//---------------------------------------------------------------------------
void 
CDriverContext::PowerUp (void)
{

}

void 
CDriverContext::PowerDown (void)
{

}


// -----------------------------------------------------------------------------
// The Mixer Driver message handler
// -----------------------------------------------------------------------------
DWORD 
CDriverContext::MixerMessage(PMMDRV_MESSAGE_PARAMS pParams)
{ DWORD dwRet;
  DWORD dwParam1, dwParam2;
  UINT uDeviceId;
  HSTREAM pStream;

    pStream = (HSTREAM) pParams->dwUser;
    dwParam1 = pParams->dwParam1;
    dwParam2 = pParams->dwParam2;
    uDeviceId = pParams->uDeviceId;

    if (uDeviceId != 0) {
        return MMSYSERR_BADDEVICEID;
    }


    switch (pParams->uMsg) {
        // mixer API messages:
// only 
// only purpose of mixer handles at driver level is to provide a target for callbacks.
// as such, only really need one handle per device. if client application(s) open more than one
// mixer handle, the mixer API should MUX them, so driver only have has to manage a single instance.
    case MXDM_GETNUMDEVS:
        dwRet = 1;
        break;

    case MXDM_GETDEVCAPS:
        dwRet = GetMixerCaps((PMIXERCAPS) dwParam1);
        break;

    case MXDM_OPEN:
        dwRet = OpenMixerHandle((PDWORD) pParams->dwUser, (PMIXEROPENDESC) dwParam1, dwParam2);
        break;

    case MXDM_CLOSE:
        dwRet = CloseMixerHandle(dwParam2);
        break;

    case MXDM_GETLINECONTROLS:
        dwRet = GetMixerLineControls((PMIXERLINECONTROLS) dwParam1, dwParam2);
        break;

    case MXDM_GETLINEINFO:
        dwRet = GetMixerLineInfo((PMIXERLINE) dwParam1, dwParam2);
        break;

    case MXDM_GETCONTROLDETAILS:
        dwRet = GetMixerControlDetails((PMIXERCONTROLDETAILS) dwParam1, dwParam2);
        break;

    case MXDM_SETCONTROLDETAILS:
        dwRet = SetMixerControlDetails((PMIXERCONTROLDETAILS) dwParam1, dwParam2);
        break;

    default:
        dwRet = MMSYSERR_NOTSUPPORTED;
        break;
    }



    return dwRet;
}

// -----------------------------------------------------------------------------
// The Wave Driver message handler
// -----------------------------------------------------------------------------
DWORD 
CDriverContext::WaveMessage(PMMDRV_MESSAGE_PARAMS pParams)
{   
	DWORD dwRet = MMSYSERR_ERROR;
    DWORD dwParam1, dwParam2;
    UINT uDeviceId;
    HSTREAM pStream;

    __try {

        pStream = (HSTREAM) pParams->dwUser;
        dwParam1 = pParams->dwParam1;
        dwParam2 = pParams->dwParam2;
        uDeviceId = pParams->uDeviceId;

        switch (pParams->uMsg) {
            //
            // device enumeration
            //
            // This driver supports exactly one input and one output device
            case WIDM_GETNUMDEVS:
            case WODM_GETNUMDEVS:
                dwRet = DEVICEID_MAX;
                break;
            //
            // device caps
            //
            case WIDM_GETDEVCAPS:
            case WODM_GETDEVCAPS:
                if (pStream != NULL || uDeviceId < DEVICEID_MAX) {
                    dwRet = GetCaps(pParams->uMsg, (PWAVEOUTCAPS) dwParam1);
                }
                else {
                    dwRet = MMSYSERR_BADDEVICEID;
                }
                break;

            case WODM_GETEXTDEVCAPS:
                if (uDeviceId < DEVICEID_MAX) {
                    dwRet = GetExtCaps((PWAVEOUTEXTCAPS) dwParam1);
                }
                else {
                    dwRet = MMSYSERR_BADDEVICEID;
                }
                break;

            //
            // stream open
            //
            case WIDM_OPEN:
            case WODM_OPEN:
                if (uDeviceId  < DEVICEID_MAX) {
                   dwRet = OpenStream(pParams->uMsg, (HSTREAM*) pParams->dwUser, (LPWAVEOPENDESC) dwParam1, dwParam2, uDeviceId);
                }
                else {
                    dwRet = MMSYSERR_BADDEVICEID;
                }
                break;
            //
            // Input/Output Shared messages
            //
            case WIDM_ADDBUFFER:
            case WODM_WRITE:
                dwRet = pStream->AddBuffer((PWAVEHDR) dwParam1);
                break;

            case WIDM_CLOSE:
            case WODM_CLOSE:
                dwRet = pStream->Close();
                break;

            case WIDM_STOP:
            case WODM_PAUSE:
                dwRet = pStream->Pause();
                break;

            case WIDM_START:
            case WODM_RESTART:
                dwRet = pStream->Unpause();
                break;

            case WIDM_UNPREPARE:
            case WODM_UNPREPARE:
                dwRet = pStream->UnprepareHeader((PWAVEHDR) dwParam1);
                break;

            case WIDM_PREPARE:
            case WODM_PREPARE:
                dwRet = pStream->PrepareHeader((PWAVEHDR) dwParam1);
                break;

            case WIDM_RESET:
            case WODM_RESET:
                dwRet = pStream->Reset();
                break;

            case WIDM_GETPOS:
            case WODM_GETPOS:
                dwRet = pStream->GetPosition((PMMTIME) dwParam1);
                break;

            // Output-only messages
            // set/get volume is ugly, because we can get either a valid stream handle
            // OR a device iD.
            case WODM_GETVOLUME:
                if (pStream != NULL) 
                {
                    dwRet = pStream->GetVolume((PDWORD) dwParam1);
                }
                else {
                    if (uDeviceId > DEVICEID_MAX) {
                        dwRet = MMSYSERR_BADDEVICEID;
                    }
                    else {
                        dwRet = GetVolume( (PULONG) dwParam1);
                    }
                }
                break;
			case WIDM_SETVOLUME:
            case WODM_SETVOLUME:
                if (pStream != NULL) 
                {
                    dwRet = pStream->SetVolume(pParams);
                }
                else {
                    if (uDeviceId > DEVICEID_MAX) {
                        dwRet = MMSYSERR_BADDEVICEID;
                    }
                    else {
                        dwRet = SetVolume(pParams);
                    }
                }
                break;

            case WODM_BREAKLOOP:
                dwRet = pStream->BreakLoop();
                break;

            case WODM_GETPLAYBACKRATE:
                dwRet = pStream->GetPlaybackRate((PDWORD) dwParam1);
                break;

            case WODM_SETPLAYBACKRATE:
                dwRet = pStream->SetPlaybackRate(dwParam1);
                break;

			case WODM_SET_STREAM_PRIORITY:
			case WIDM_SET_STREAM_PRIORITY:
            	 pStream->m_dwStreamPriority = dwParam1;
            	 break;
            case WODM_SET_STREAM_SINK:
            	 m_pDevice->SetOutputStreamSink(pParams->uDeviceId, pParams->dwParam1);
            	 break;
			case WIDM_SET_STREAM_SINK:
				 break;
            case WODM_GETPITCH:
            case WODM_SETPITCH:
            default:
                //
                // Add support for custom WAV messages here
                //
                dwRet = MMSYSERR_NOTSUPPORTED;
        }


    } __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {

        RETAILMSG(1, (TEXT("EXCEPTION IN WAV_IOControl!!!!\r\n")));
    }   

    return dwRet;
}

//--------------------------------------------------------------------------
//
//  Name: PowerMessage
//
//  Description: handles power event messages from system.
//
//  Parameters: DWORD				 dwCode:	operation code - QUERY, SET, or GET
//              CEDEVICE_POWER_STATE cpState:	the current state we should set
//					ourselves to, or the place to fill in our capabilities
//
//
//--------------------------------------------------------------------------
DWORD 
CDriverContext::PowerMessage(
                        DWORD  dwCode, 
                        PVOID pBufIn,
                        DWORD  dwLenIn, 
                        PDWORD  pdwBufOut, 
                        DWORD  dwLenOut,
                        PDWORD pdwActualOut
                      )
{
    BOOL retVal = TRUE;

    switch ( dwCode ) 
	{
		case IOCTL_POWER_CAPABILITIES:
		{
			if (!pdwBufOut || dwLenOut < sizeof(POWER_CAPABILITIES) || !pdwActualOut) 
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				retVal = FALSE;
			} 
			else
			{
				PPOWER_CAPABILITIES ppc = (PPOWER_CAPABILITIES)pdwBufOut;

				memset(ppc, 0, sizeof(*ppc));
				

				ppc->DeviceDx   = 0x19;   // supports D0, D3 & D4
				ppc->WakeFromDx = 0x18;   // supports D3 & D4

				*pdwActualOut = sizeof(POWER_CAPABILITIES);
				DEBUGMSG(1, (TEXT("HDA - IOCTL_POWER_CAPABILITIES: DeviceDx=0x%02x, WakeFromDx=0x%02x\r\n"), ppc->DeviceDx, ppc->WakeFromDx));
			}
			break;
		}
		case IOCTL_POWER_GET:
		{
			if (!pdwBufOut || dwLenOut < sizeof(CEDEVICE_POWER_STATE) || !pdwActualOut) 
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				retVal = FALSE;
			} 
			else 
			{
				*(PCEDEVICE_POWER_STATE)pdwBufOut = m_CurrentDx;
	
				*pdwActualOut = sizeof(CEDEVICE_POWER_STATE);
			}
			break;
		}
	    case IOCTL_POWER_SET:
		{
			DEBUGMSG(1,(TEXT("IOCTL_POWER_SET \r\n")));
			if (!pdwBufOut || dwLenOut < sizeof(CEDEVICE_POWER_STATE) || !pdwActualOut)
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				retVal = FALSE;
			} 
			else
			{
				CEDEVICE_POWER_STATE newDx = *(PCEDEVICE_POWER_STATE) pdwBufOut;

				switch (newDx)
				{
					case D0:       
						m_pDevice->SetPowerState(D0);
						retVal = TRUE;
						break;
					case D1:
					case D2:
					case D3:
					case D4:
						m_pDevice->SetPowerState(D3);
						newDx = D3;
						retVal = TRUE;
						break;

					default:
						SetLastError(ERROR_INVALID_PARAMETER);
						retVal = FALSE;
						break;

				}

				// did we set the device power?
				if(retVal == TRUE)
				{
					*(PCEDEVICE_POWER_STATE)pdwBufOut = newDx;
				
					*pdwActualOut = sizeof(newDx);

					m_CurrentDx = newDx;
				}
			}
			break;
		}
		default:
			retVal = FALSE;
			break;
	}
    return retVal;
}


































