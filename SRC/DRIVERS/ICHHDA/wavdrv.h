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
//  Module Name:  
//  
//      wavdrv.h
//  
//  Abstract:  
//  
//  Functions:
//  
//  Notes:
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

#ifndef __WAVDRV_H__
#define __WAVDRV_H__


//
// Includes common to all the wave driver files.
//
#include <windows.h>
#include <wavedev.h>
#include <devload.h>
#include <mmddk.h>
#include <mmreg.h>
#include "../INC/types.h"
#include "debug.h"
#include "wavhdrq.h"
#include "cregkey.h"
#include "schda.h"
#include "helper.h"


//
// Number of open input/output streams supported by the ICHHDA driver.
//
// These numbers only include application defined lines (streams opened via
// the Wave API).  They establish the range of values applications may use
// as application-defined stream IDs.
//
// Application defined stream IDs must be within the range:
//
//    0   <=   Application_Defined_Input_Stream_Id   <  NUM_WAVE_INPUT_STREAMS
//    0   <=   Application_Defined_Output_Stream_Id  <  NUM_WAVE_OUTPUT_STREAMS
//
// Note that these determine the number of device contexts preallocated by the
// Windows CE Wave API manager when the WaveDev driver is loaded.  These should
// be kept close to the number of streams actually required to limit unnecessary
// memory consumption.

#define NUM_MGM_WAVE_INPUT_STREAMS  9
#define NUM_MGM_WAVE_OUTPUT_STREAMS 9
#define NUM_WAVE_INPUT_STREAMS  NUM_MGM_WAVE_INPUT_STREAMS
#define NUM_WAVE_OUTPUT_STREAMS NUM_MGM_WAVE_OUTPUT_STREAMS

#define DEVICE_NAME     L"Intel HDAUAM"
#define DRIVER_VERSION  0x100

// non-PCM format codes
#define WAVE_FORMAT_A52              0x2000
#define WAVE_FORMAT_DTS              0x0008
#define WAVE_FORMAT_DOLBY_AC3_SPDIF  0x0092
#define WAVE_FORMAT_WMA3             0x0162

#define A52_FRAMESIZE  6144

#define REGVALUE_AC3ALLOWED      TEXT("AC3Allowed")
#define REGVALUE_DTSALLOWED      TEXT("DTSAllowed")
#define REGVALUE_WMA3ALLOWED     TEXT("WMA3Allowed")
#define REGVALUE_MAXPCMOUTLEVELGAIN TEXT("MaxPcmOutLevelGain")
#define REGVALUE_44KHZALLOWED    TEXT("44kHzAllowed")

#define MMFAILED(mmresult) ((mmresult)!=MMSYSERR_NOERROR)

#define VOLUME_MIN               -10000

#define WODM_SET_STREAM_PRIORITY	49
#define WIDM_SET_STREAM_PRIORITY	99
#define WODM_SET_STREAM_SINK		48
#define WIDM_SET_STREAM_SINK		98
#define WIDM_GETVOLUME				96
#define WIDM_SETVOLUME				97
			
typedef class CDriverContext * PDriverContext, * HDRIVER;
typedef class CStreamContext * PStreamContext, * HSTREAM;

// Mixer API structures
typedef struct tagMXCONTROLSTATE
{
    union {
        USHORT  us;
        DWORD   dw;
        BOOL    b;
    } uSetting;
    BOOL    fVirtualized;       // this control is virtualized by a mux
} * PMXCONTROLSTATE, MXCONTROLSTATE;

typedef struct tagMXLINESTATE {
    DWORD dwState;
} * PMXLINESTATE, MXLINESTATE;


/////////////////////////////////////////////////////////////////////////////
// class: CDriverContext
// Driver Context Data
//
// This class abstracts the notion of a device driver instance. Each instance 
// of the driver (WAV1, WAV2) gets one of these.
// A fully general driver that supports multiple physical devices (e.g. two
// separate PCI cards, one appearing as WAV1 and the other as WAV2) 
// should create separate CDriverContext objects for each driver instance.
// 
// There's an important distinction between physical devices that are exposed to
// the Device Manager and separate waveform devices as understood by
// the waveform API. Consider the hypothetical case of a PCI audio adapter
// that has two separate stereo channels (e.g. Speaker A, Speaker B).
// The Windows CE Device Manager will view this a single physical device
// but the waveform API will treat each speaker set a separate waveform
// device.
// 
/////////////////////////////////////////////////////////////////////////////

class CIndexable // helper class for stream table
{
protected:
    ULONG index;
    friend class CDriverContext;
};
 
typedef DWORD MIXHANDLE;
typedef struct _tagMCB MIXER_CALLBACK, * PMIXER_CALLBACK;

class CDriverContext
{
public:
    CDriverContext (HKEY hDevKey);
    ~CDriverContext ();

	DWORD       WaveMessage(PMMDRV_MESSAGE_PARAMS pParams);
	DWORD       MixerMessage(PMMDRV_MESSAGE_PARAMS pParams);
	DWORD		PowerMessage(DWORD dwCode, PVOID pBufIn, DWORD dwLenIn, 
						 PDWORD pdwBufOut, DWORD dwLenOut, PDWORD pdwActualOut);
	BOOL     Initialize (PCTSTR pszActiveKey);

    MMRESULT GetCaps (ULONG msg, PWAVEOUTCAPS pCaps);
    MMRESULT GetExtCaps (PWAVEOUTEXTCAPS pCaps);
    MMRESULT GetVolume (PULONG pulVolume);
    MMRESULT SetVolume (PMMDRV_MESSAGE_PARAMS pParams);

    MMRESULT OpenStream (ULONG msg, HSTREAM * ppStream, LPWAVEOPENDESC pWOD, DWORD dwFlags, UINT uDeviceId);

    MMRESULT GetMixerCaps(PMIXERCAPS pCaps);
    MMRESULT OpenMixerHandle(PDWORD phmix, PMIXEROPENDESC pMOD, DWORD dwFlags); //mixerOpen
    MMRESULT CloseMixerHandle( MIXHANDLE mxh); // mixerClose
    MMRESULT GetMixerLineControls ( PMIXERLINECONTROLS pmxlc, DWORD dwFlags); // mixerGetLineControls
    MMRESULT GetMixerLineInfo( PMIXERLINE pmxl, DWORD dwFlags); // mixerGetLineInfo
    MMRESULT GetMixerControlDetails( PMIXERCONTROLDETAILS pmxcd, DWORD dwFlags); // mixerGetControlDetails
    MMRESULT SetMixerControlDetails( PMIXERCONTROLDETAILS pmxcd, DWORD dwFlags); // mixerSetControlDetails

    MMRESULT GetFormatTypes (PULONG pulFormat);
    MMRESULT SetFormatTypes (PULONG pulFormat);	

    // power management support
    void PowerUp (void);
    void PowerDown (void);

private:
	// Device configuration
    CRegKey * m_pRegKey;

	// data members
    HDA *       m_pDevice;
    ULONG           m_ulBufferSize; // required DMA buffer size, in bytes

    ULONG           m_ulVolume;

	// power management
	CEDEVICE_POWER_STATE	m_CurrentDx;	// our last power state

	// mixer driver support
    PMIXER_CALLBACK m_pHead;
    PMXLINESTATE    m_pMxLineState;
    PMXCONTROLSTATE m_pMxControlState;
    BOOL    InitializeMixerState (void);

    void    PerformMixerCallbacks(DWORD dwMessage, DWORD dwId);

    HKEY   m_hDevKey;
    DWORD  m_dwAC3Allowed;
    DWORD  m_dwDTSAllowed;
    DWORD  m_dwWMA3Allowed;
	DWORD  m_dwAACAllowed;
	DWORD  m_dw44kHzAllowed;
	WORD   m_wFormatsAllowed;


    MYTHIS_DECLARATION;
};

/////////////////////////////////////////////////////////////////////////////
// class: CStreamContext
// Stream Context Data
//
// This class provides the driver-level implementation of waveform handles
// (HWAVEOUT and HWAVEIN). When an application invokes waveOutOpen or
// waveInOpen, the driver respondes by create a CStreamContext object.
// For the typical audio adapter, the driver will support two CStreamContext
// objects, one for input and one for output.
// For devices that support mixing of multiple sources, the driver can
// allow more than one output stream to be created a time.
//
// A Stream Context consists, fundamentally, of two parts, a queue of
// Wave headers and a DMA channel of some sort.
// The Wave Header Queue is essentially device-independent and is 
// implemented in the CWavehdrQueue class.
// The DMA channel support is device-specific and will have to be
// adapted to each device's programming model.
//
/////////////////////////////////////////////////////////////////////////////

class CStreamContext : public CIndexable
{
public:
	UINT	m_uStreamId;
	DWORD 	m_dwStreamPriority;
	
    CStreamContext              (BOOL fPlayback, BOOL fPaused);
    ~CStreamContext             ();

    MMRESULT Initialize         (HDA * pDevice, ULONG ulChannelIndex, LPWAVEOPENDESC pWOD);

    void AddRef (void);
    void Release (void);

    // waveapi methods    

    MMRESULT PrepareHeader      (LPWAVEHDR lpHeader);
    MMRESULT UnprepareHeader    (LPWAVEHDR lpHeader);

    MMRESULT AddBuffer          (LPWAVEHDR lpHeader); // aka waveOutWrite()

    MMRESULT Unpause            (void); // aka waveOutRestart()
    MMRESULT Pause              (void); // aka waveOutPause()

    MMRESULT GetPosition        (LPMMTIME lpMMTime);

    MMRESULT Close              (void);
    MMRESULT Reset              (void);

    MMRESULT BreakLoop          (void); 

    MMRESULT GetPlaybackRate    (PULONG pulRate);
    MMRESULT SetPlaybackRate    (ULONG ulRate);

    MMRESULT GetVolume          (PULONG pulVolume);
    MMRESULT SetVolume          (PMMDRV_MESSAGE_PARAMS pParams);

protected:
	// internal methods

	static  VOID    InterruptHandler (PVOID pThis);
	virtual VOID    HandleInterrupt (void) = 0; // called by the interrupt handler every so often to keep things moving
	virtual VOID    DoTransfer (PBYTE pBuffer, DWORD dwCount) = 0;
	VOID    DoSplitTransfer(DWORD dwSize);
	VOID    DoTransferAC3Frames (PBYTE pBuffer, DWORD dwCount);
	VOID    Start (void);
	VOID    Stop (void);
	BOOL    Swap16(USHORT *pOutBuffer, USHORT *pInBuffer, DWORD dwInBufferSize);

	// buffer stuff
	HDA*        m_pDevice;          // Pointer to the HDA hw-driver class
	ULONG           m_ulDmaChannel;     // The dma channel the buffer is associated with

	DWORD           m_dwInterruptInterval; // how many bytes between interrupt positions?
	PBYTE           m_pBufferBits;      // DMA buffer address
	DWORD           m_dwBufferSize;     // size of DMA buffer
	DWORD           m_dwBufferWrapTime; // how long it takes the DMA buffer to wrap
	DWORD           m_dwLastDMACursor;  // last known position of the DMA cursor cursor
	DWORD           m_dwPreTime;        // earliest possible time at which m_dwLastPlayCursor was accurate
	DWORD           m_dwTransferCursor; // the offset in the DMA buffer where we'll start the next transfer
	                                    // (will lag behind the silence pad if the header queue us running low)
	LONG            m_nBytesToTransfer; // tracks how much data to read/write from the DMA buffer.
	UCHAR           m_ucSilence;        // 0 for 16-bit streams, 0x80 for 8-bit streams

	DWORD           m_dwVolume;
	DWORD           m_dwPlaybackRate;   // 16:16 fixed point multiplier for m_wfx.nSampleRate
	// stream stuff
	CWavehdrQueue   m_HdrQ;             // device-independent WAVEHDR Queue
	BOOL            m_fStarted;         // buffer is playing
	BOOL            m_fPaused;          // Is stream paused?
	WAVEFORMATEX    m_wfx;
	CRITICAL_SECTION m_cs;
	BOOL            m_fIsPlayback;      // FALSE for capture, TRUE for playback
	LONG            m_lRefCount;
	BOOL            m_dwPlayFlags;
	DWORD           m_dwAC3FrameBytesCopied;
	WORD            m_wNextByte;
	MYTHIS_DECLARATION;
};

class COutputStreamContext : public CStreamContext
{
public:
	COutputStreamContext (void);
	virtual void   DoTransfer (PBYTE pBuffer, DWORD dwCount);
	virtual void   HandleInterrupt (void); // called by the driver every so often to keep things moving
	void GetAdvance(DWORD &dwAdvance, DWORD &dwRoomInBuffer);
};

class CInputStreamContext : public CStreamContext
{
public:
    CInputStreamContext (void);
    virtual void   DoTransfer (PBYTE pBuffer, DWORD dwCount);
    virtual void   HandleInterrupt (void); // called by the driver every so often to keep things moving
    void           GetAdvance(DWORD & dwAdvance);
};


// CAutoLock helper class
class CAutoLock
{
public:
    CAutoLock (CRITICAL_SECTION * cs)
    {
        pcs = cs;
        EnterCriticalSection(pcs);
    }
    ~CAutoLock ()
    {
        LeaveCriticalSection(pcs);
    }
private:
    CRITICAL_SECTION * pcs;
};

#endif // __WAVDRV_H__
