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
#define NELEMS(a) (sizeof(a)/sizeof((a)[0]))

#define LOGICAL_VOLUME_MAX  0xFFFF
#define LOGICAL_VOLUME_MIN  0

#define ZONE_HWMIXER 0

#define MIC_IN 1

#ifdef DEBUG

// DEBUG-only support for displaying control type codes in readable form

typedef struct 
{
    DWORD   val;
    PWSTR   str;
} MMSYSCODE;

#define MXCTYPE(typ) {typ, TEXT(#typ)}

MMSYSCODE ctype_table[] =
{
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_CUSTOM),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_BASS),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_EQUALIZER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_FADER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_TREBLE),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_VOLUME),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MIXER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MUX),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_SINGLESELECT),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_BOOLEANMETER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_PEAKMETER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_SIGNEDMETER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_UNSIGNEDMETER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_DECIBELS),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_PERCENT),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_SIGNED),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_UNSIGNED),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_PAN),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_QSOUNDPAN),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_SLIDER),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_BOOLEAN),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_BUTTON),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_LOUDNESS),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MONO),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MUTE),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_ONOFF),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_STEREOENH),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MICROTIME),
	MXCTYPE(MIXERCONTROL_CONTROLTYPE_MILLITIME),
};

PWSTR
COMPTYPE(DWORD dwValue)
{ int i;

    for (i = 0; i < NELEMS(ctype_table); i++) {
        if (ctype_table[i].val == dwValue) {
            return ctype_table[i].str;
        }
    }
    return TEXT("<unknown>");

}

#else
#define COMPTYPE(n) TEXT("<>")
#endif

// destinations
enum {
    LINE_OUT = 0x80,
    PCM_IN,
    CD,
    LINE_IN,
    MIC,
    AUX,
    PCM_OUT,
    STEREO_MIX,
    PHONE,
    VIDEO,
    MONO_MIX,
    NOLINE   = 0xff // doesn't HAVE to be FF, but makes it easier to see
};
// mixer line ID are 16-bit values formed by concatenating the source and destination line indices
// 
#define MXLINEID(dst,src)       ((USHORT) ((USHORT)(dst) | (((USHORT) (src)) << 8)))
#define GET_MXLINE_SRC(lineid)  ((lineid) >> 8)
#define GET_MXLINE_DST(lineid)  ((lineid) & 0xff)
#define GET_MXCONTROL_ID(pCtl)   ((pCtl)-g_controls)

const USHORT
g_dst_lines[] = 
    {
    MXLINEID(LINE_OUT,NOLINE),
    MXLINEID(PCM_IN,NOLINE)
};

const USHORT
g_LINE_OUT_sources[] = 
{
    MXLINEID(LINE_OUT,CD),
    MXLINEID(LINE_OUT,LINE_IN),
#ifdef MIC_IN
    MXLINEID(LINE_OUT,MIC),
#endif
    MXLINEID(LINE_OUT,AUX),
    MXLINEID(LINE_OUT,PCM_OUT)
};

const USHORT
g_PCM_IN_sources[] = 
{
    MXLINEID(PCM_IN,CD),
    MXLINEID(PCM_IN,LINE_IN),
#ifdef MIC_IN
    MXLINEID(PCM_IN,MIC),
#endif
    MXLINEID(PCM_IN,AUX),
    MXLINEID(PCM_IN,STEREO_MIX)
};

// MXLINEDESC corresponds to MIXERLINE, but is designed to conserve space

typedef struct tagMXLINEDESC  const * PMXLINEDESC, MXLINEDESC;
typedef struct tagMXCONTROLDESC  const * PMXCONTROLDESC, MXCONTROLDESC;

struct tagMXLINEDESC { 
    DWORD dwComponentType; 
    PCWSTR szShortName; 
    PCWSTR szName; 
    DWORD ucFlags; 
    USHORT const * pSources;
    USHORT usLineID;
    UINT8 ucChannels; 
    UINT8 ucConnections; 
    UINT8 ucControls; 
    DWORD	dwTargetType;
    UINT8   ucDstIndex;
    UINT8   ucSrcIndex;
} ; 



    // Destinations: 
    //      LINE_OUT - line out jack
    //      PCM_IN - input to ADC
    // Sources:
    //  CD - CD_IN connector
    //  AUX - AUX_IN connector
    //  MIC - MIC jack
    //  LINE_IN - LINE_IN jack
    //  PHONE - input of modem
    //  VIDEO - VIDEO_IN connctor
    //  PCM_OUT - output of DAC0+DAC1
    //  MIX_OUT - mix of (MIC, LINE_IN, PCM_OUT, VIDEO, CD, AUX)


// MXCONTROLDESC is driver shorthand for Volume and Mute MIXERCONTROL
struct tagMXCONTROLDESC
{
    PWSTR   szName;
    DWORD   dwType;
    USHORT  usLineID;           // line that owns this control
    USHORT  usMask;             // mask for valid range for this codec register
    UCHAR   ucCodecRegister;    // address of HDA register for this control
};

#define MXCE(dst,src,nme,type,mask,reg) {TEXT(nme),type,MXLINEID(dst,src),mask,reg}

#define MUTE_BIT    0x8000

#define  HDA_RESET              0x0000      // 
#define  HDA_MASTER_VOL_STEREO  0x0002      // Line Out
#define  HDA_HEADPHONE_VOL      0x0004      // 
#define  HDA_MASTER_VOL_MONO    0x0006      // TAD Output
#define  HDA_MASTER_TONE        0x0008      //
#define  HDA_PCBEEP_VOL         0x000a      // none
#define  HDA_PHONE_VOL          0x000c      // TAD Input (mono)
#define  HDA_MIC_VOL            0x000e      // MIC Input (mono)
#define  HDA_LINEIN_VOL         0x0010      // Line Input (stereo)
#define  HDA_CD_VOL             0x0012      // CD Input (stereo)
#define  HDA_VIDEO_VOL          0x0014      // none
#define  HDA_AUX_VOL            0x0016      // Aux Input (stereo)
#define  HDA_PCMOUT_VOL         0x0018      // Wave Output (stereo)
#define  HDA_RECORD_SELECT      0x001a      //
#define  HDA_RECORD_GAIN        0x001c
#define  HDA_RECORD_GAIN_MIC    0x001e
#define  HDA_GENERAL_PURPOSE    0x0020
#define  HDA_3D_CONTROL         0x0022
#define  HDA_MODEM_RATE         0x0024
#define  HDA_POWER_CONTROL      0x0026

const MXCONTROLDESC
g_controls[] = {


#if 1
    MXCE(LINE_OUT,NOLINE,   "Master Volume",    MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_MASTER_VOL_STEREO),
    MXCE(LINE_OUT,NOLINE,   "Master Mute",      MIXERCONTROL_CONTROLTYPE_MUTE,    1, HDA_MASTER_VOL_STEREO),


	MXCE(PCM_IN,NOLINE,     "Record Select",    MIXERCONTROL_CONTROLTYPE_MUX,          6, HDA_RECORD_SELECT),



    // LINE_OUT source controls
    MXCE(LINE_OUT,CD,       "CD Volume",        MIXERCONTROL_CONTROLTYPE_VOLUME,  0x001f, HDA_CD_VOL),
    MXCE(LINE_OUT,CD,       "CD Mute",          MIXERCONTROL_CONTROLTYPE_MUTE,    0x8000, HDA_CD_VOL),

    MXCE(LINE_OUT,LINE_IN,  "Line In Volume",   MIXERCONTROL_CONTROLTYPE_VOLUME,  0x001f, HDA_LINEIN_VOL),
    MXCE(LINE_OUT,LINE_IN,  "Line In Mute",     MIXERCONTROL_CONTROLTYPE_MUTE,    0x8000, HDA_LINEIN_VOL),

    MXCE(LINE_OUT,MIC,      "Mic Volume",       MIXERCONTROL_CONTROLTYPE_VOLUME,  0x001f, HDA_MIC_VOL),
    MXCE(LINE_OUT,MIC,      "Mic Mute",         MIXERCONTROL_CONTROLTYPE_MUTE,    0x8000, HDA_MIC_VOL),
    MXCE(LINE_OUT,MIC,      "Mic Boost",        MIXERCONTROL_CONTROLTYPE_ONOFF,   0x0004, HDA_MIC_VOL),

    MXCE(LINE_OUT,AUX,      "Aux Volume",       MIXERCONTROL_CONTROLTYPE_VOLUME,  0x001f, HDA_AUX_VOL),
    MXCE(LINE_OUT,AUX,      "Aux Mute",         MIXERCONTROL_CONTROLTYPE_MUTE,    0x8000, HDA_AUX_VOL),

    MXCE(LINE_OUT,PCM_OUT,  "Digital Volume",   MIXERCONTROL_CONTROLTYPE_VOLUME,  0x001f, HDA_PCMOUT_VOL),
    MXCE(LINE_OUT,PCM_OUT,  "Digital Mute",     MIXERCONTROL_CONTROLTYPE_MUTE,    0x8000, HDA_PCMOUT_VOL),




    // PCM_IN source controls

    // HDA spec indicates that none of the inputs to the input MUX have Volume/Mute capabilities,
    // but that the driver should create a "virtual" volume/mute control for each mux input
    // When the application changes the MUX input line, the driver is responsible for
    // adjusting the Record Gain register accordingly.
    MXCE(PCM_IN,CD,         "CD Volume",        MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_RECORD_GAIN),
    MXCE(PCM_IN,LINE_IN,    "Line In Volume",   MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_RECORD_GAIN),
    MXCE(PCM_IN,STEREO_MIX, "Stereo Mix Volume",MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_RECORD_GAIN),
    MXCE(PCM_IN,MIC,        "Mic Volume",       MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_RECORD_GAIN),
    MXCE(PCM_IN,MIC,        "Mic Mute",        MIXERCONTROL_CONTROLTYPE_MUTE,   1, HDA_MIC_VOL),
    MXCE(PCM_IN,AUX,        "Aux Volume",       MIXERCONTROL_CONTROLTYPE_VOLUME,  0x000f, HDA_RECORD_GAIN),

#endif

};
const int ncontrols = NELEMS(g_controls);

// MuxIndexToLine
// this table translates indices from the HDA_RECORD_SELECT (0x1A) register to line IDs
// note that not all entries in the table correspond to lines that are implemented
const USHORT MuxIndexToLine[] =
{
    MXLINEID(PCM_IN,MIC),
    MXLINEID(PCM_IN,CD),
    MXLINEID(PCM_IN,VIDEO),
    MXLINEID(PCM_IN,AUX),
    MXLINEID(PCM_IN,LINE_IN),
    MXLINEID(PCM_IN,STEREO_MIX),
    MXLINEID(PCM_IN,MONO_MIX),
};


// mixerline ID
#define _MXLE(id,dst,src,flags,comptype,nch,ncnx,nctrl,sname,lname,target,sarray)\
    {comptype,\
    TEXT(sname),TEXT(lname),\
    flags, \
    sarray,\
    id,\
    nch,ncnx,nctrl,\
    target,\
    dst,src}

// declare a destination line
#define MXLED(id,dst,flags,comptype,nch,nctrl,sname,lname,target)\
    _MXLE(MXLINEID(id,NOLINE),dst,NOLINE,flags,comptype,nch,NELEMS(g_##id##_sources),nctrl,sname,lname,target,g_##id##_sources)\

// declar a source line
#define MXSLE(id,dst,src,flags,comptype,nch,nctrl,sname,lname,target)\
    _MXLE(id,dst,src,flags,comptype,nch,0,nctrl,sname,lname,target,NULL)\


const MXLINEDESC 
g_mixerline[] = 
{
    // dst line 0 - speaker out
    MXLED(LINE_OUT, 0,
        MIXERLINE_LINEF_ACTIVE, // no flags
        MIXERLINE_COMPONENTTYPE_DST_SPEAKERS,
        2, // stereo
        2, // controls
        "Volume Control","Volume Control",
        MIXERLINE_TARGETTYPE_WAVEOUT
        ),
        
    // dst line 1 - PCM input
    MXLED(PCM_IN, 1,
        MIXERLINE_LINEF_ACTIVE, // flags
        MIXERLINE_COMPONENTTYPE_DST_WAVEIN,
        2, // stereo
        1, // 1 control (MUX) - the HDA record gain register is virtualized across all mux inputs
        "Recording Contr","Recording Control",
        MIXERLINE_TARGETTYPE_WAVEIN
        ),



    // ----------------------
    // LINE_OUT Sources Lines
    // ----------------------

    // src line 0 - CD
    MXSLE(MXLINEID(LINE_OUT,CD), 0, 0,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,
        2,2,
        "CD Audio","CD Audio",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),
    // src line 1 - LINE IN
    MXSLE(MXLINEID(LINE_OUT,LINE_IN), 0, 1,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_LINE,
        2,2,
        "Line In","Line In",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),

    // src line 2 - Microphone
    MXSLE(MXLINEID(LINE_OUT,MIC), 0, 2,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,
        1,3,
        "Mic","Microphone",
        MIXERLINE_TARGETTYPE_WAVEIN
        ),
    // src line 3 - Aux
    MXSLE(MXLINEID(LINE_OUT,AUX), 0, 3,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_ANALOG,
        2,2,
        "Aux","Aux Volume",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),
    // src line 4 - Aux
    MXSLE(MXLINEID(LINE_OUT,PCM_OUT), 0, 4,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,
        2,2,
        "Digital","Digital Volume",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),


    // ----------------------
    // PCM_IN Sources Lines
    // ----------------------

    // src line 0 - CD
    MXSLE(MXLINEID(PCM_IN, CD), 1, 0,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,
        2,1,
        "CD Audio","CD Audio",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),
    // src line 1 - LINE IN
    MXSLE(MXLINEID(PCM_IN, LINE_IN), 1, 1,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_LINE,
        2,1,
        "Line In","Line In",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),
 
    // src line 2 - Microphone
    MXSLE(MXLINEID(PCM_IN, MIC), 1, 2,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,
        2,2,
        "Microphone","Microphone",
        MIXERLINE_TARGETTYPE_WAVEIN
        ),
    // src line 3 - Aux
    MXSLE(MXLINEID(PCM_IN, AUX), 1, 3,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_ANALOG,
        2,1,
        "Aux Volume","Aux Volume",
        MIXERLINE_TARGETTYPE_UNDEFINED
        ),
    // src line 4 - Stereo Mix
    MXSLE(MXLINEID(PCM_IN, STEREO_MIX), 1, 4,
        MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE, // flags
        MIXERLINE_COMPONENTTYPE_SRC_ANALOG,
        2,1,
        "Stereo Mix", "Stereo Mix",
        MIXERLINE_TARGETTYPE_UNDEFINED
        )
};

const int nlines = NELEMS(g_mixerline);


DWORD
RegToVol(DWORD dwSetting, DWORD dwMax)
{
    return (dwMax - (dwSetting & dwMax)) * (LOGICAL_VOLUME_MAX-1) / dwMax;
}

USHORT
VolToReg(DWORD dwVolume, DWORD dwMax)
{
    return (USHORT) (dwMax - dwVolume * dwMax / LOGICAL_VOLUME_MAX);
}

PMXLINEDESC
LookupMxLine(USHORT usLineID)
{
    // scan for mixer line
    for (int i = 0; i < nlines; i++) {
        if (g_mixerline[i].usLineID == usLineID) {
            return &g_mixerline[i];
        }
    }
    return NULL;
}

int
LookupMxControl (USHORT usLineID, DWORD dwControlType)
{
    for (int i = 0; i < ncontrols; i++) {
        PMXCONTROLDESC pSrcControl = &g_controls[i];
        if (    pSrcControl->usLineID == usLineID 
            &&  pSrcControl->dwType == dwControlType) {
            break;
        }
    }
    return i;
}

void
CopyMixerControl(PMIXERCONTROL pDst, PMXCONTROLDESC pSrc, DWORD dwIndex)
{
    // all of our lines have a volume and a mute.
    // in addition, the PCM_IN has a MUX control
    // fill in the volume:
    pDst->cbStruct = sizeof(MIXERCONTROL);
    pDst->dwControlID = dwIndex;

    wcscpy(pDst->szName, pSrc->szName);
    wcscpy(pDst->szShortName, pSrc->szName);

    pDst->dwControlType = pSrc->dwType;
    pDst->cMultipleItems = 0;

    switch(pSrc->dwType) {
    case MIXERCONTROL_CONTROLTYPE_VOLUME:
        pDst->fdwControl = 0;
        pDst->Metrics.cSteps = pSrc->usMask;
        pDst->Bounds.lMaximum = LOGICAL_VOLUME_MAX;
        pDst->Bounds.lMinimum = LOGICAL_VOLUME_MIN;
        break;
    case MIXERCONTROL_CONTROLTYPE_ONOFF:
    case MIXERCONTROL_CONTROLTYPE_MUTE:
        pDst->fdwControl = MIXERCONTROL_CONTROLF_UNIFORM;
        pDst->Metrics.cSteps = 0;
        pDst->Bounds.lMaximum = 1;
        pDst->Bounds.lMinimum = 0;
        break;
    case MIXERCONTROL_CONTROLTYPE_MUX:

        pDst->cMultipleItems = 5;
        pDst->fdwControl = MIXERCONTROL_CONTROLF_UNIFORM | MIXERCONTROL_CONTROLF_MULTIPLE;
        pDst->Metrics.cSteps = 6;
        pDst->Bounds.lMaximum = 6;
        pDst->Bounds.lMinimum = 0;
        break;
    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("Unexpected control type %08x\r\n"), pSrc->dwType));
        ASSERT(0);
    }
}

//#########################################################################
// Mixer Driver Entries
//#########################################################################

BOOL
CDriverContext::InitializeMixerState (void)
{ int i;
  USHORT mux_setting = 0; // default to microphone

    // The mixer lines and controls are represented with two separate structures:
    // a 'description' structure, which is constant and global,
    // and a 'state' structure, which is modifiable and device-specific
    // At initialization time, we allocate the line and control state objects
    // and associate them with the corresponding description objects.
    // There is a one-to-one correspondence between entries in the global
    // line and control description tables (g_mixerline and g_controls) and
    // the instance-specific state arrays.
    m_pMxLineState = new MXLINESTATE [nlines];
    if (m_pMxLineState == NULL) {
        return FALSE;
    }
    m_pMxControlState = new MXCONTROLSTATE [ncontrols];
    if (m_pMxControlState == NULL) {
        return FALSE;
    }
    // this function assumes that the audio device has been initialized and that it's 
    // safe to go read the startup settings from the device
    for (i = 0; i < nlines; i++) {
        m_pMxLineState[i].dwState = 0;
    }
    for (i = 0; i < ncontrols; i++) {

        // figure out which kind of control

	USHORT regval = 0;

        if (g_controls[i].ucCodecRegister == HDA_RECORD_GAIN) {
            m_pMxControlState[i].fVirtualized = TRUE;
        }
        else {
            m_pMxControlState[i].fVirtualized = FALSE;
        }

        switch (g_controls[i].dwType) {
        case MIXERCONTROL_CONTROLTYPE_VOLUME:
            { 
                DWORD l = RegToVol(regval & g_controls[i].usMask, g_controls[i].usMask);
                DWORD r = RegToVol((regval >> 8) & g_controls[i].usMask, g_controls[i].usMask);
                m_pMxControlState[i].uSetting.dw = l | (r << 16);
            }
            break;
			
        case MIXERCONTROL_CONTROLTYPE_ONOFF:
        case MIXERCONTROL_CONTROLTYPE_MUTE:
            m_pMxControlState[i].uSetting.b = !!(regval & g_controls[i].usMask);
            break;
			
        case MIXERCONTROL_CONTROLTYPE_MUX:
            mux_setting =  regval & 0xff;
            if (mux_setting >= NELEMS(MuxIndexToLine)) {
                // if hardware reads back garbage, default to Microphone to at least avoid crashing.
                mux_setting = 0;
            }
            m_pMxControlState[i].uSetting.us = mux_setting;
            break;
			
        default:
            // should never get here!
            DEBUGCHK(0);
            break;
			
        }
    }
	
    // special case handling for the record-select muxed inputs
    // figure out which line is selected and un-virtualize it's control
    int index = LookupMxControl(MuxIndexToLine[mux_setting], MIXERCONTROL_CONTROLTYPE_VOLUME);
    ASSERT(index < ncontrols);
    m_pMxControlState[index].fVirtualized = FALSE;

    return TRUE;
}

typedef DWORD (* PFNDRIVERCALL) 
    (
    DWORD  hmx,
    UINT   uMsg,
    DWORD  dwInstance,
    DWORD  dwParam1,
    DWORD  dwParam2
    );


struct _tagMCB
{
    DWORD           hmx;
    PFNDRIVERCALL   pfnCallback;
    PMIXER_CALLBACK pNext;
};

void
CDriverContext::PerformMixerCallbacks(DWORD dwMessage, DWORD dwId)
{

    PMIXER_CALLBACK pCurr;
    for (pCurr = m_pHead; pCurr != NULL; pCurr = pCurr->pNext) {
        if (pCurr->pfnCallback != NULL) {
            DEBUGMSG(ZONE_ENTER, (TEXT("MixerCallback(%d)\r\n"), dwId));
            pCurr->pfnCallback(pCurr->hmx, dwMessage, 0, dwId, 0);
        }
    }
}


MMRESULT  
CDriverContext::OpenMixerHandle(PDWORD phMixer, PMIXEROPENDESC pMOD, DWORD dwFlags)
{
    PMIXER_CALLBACK pNew = new MIXER_CALLBACK;
    if (pNew == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("OpenMixerHandle: out of memory\r\n")));
        return MMSYSERR_NOMEM;
    }

    pNew->hmx = (DWORD) pMOD->hmx;
    if (dwFlags & CALLBACK_FUNCTION) {
        pNew->pfnCallback = (PFNDRIVERCALL) pMOD->dwCallback;
    }
    else {
        pNew->pfnCallback = NULL;
    }
    pNew->pNext = m_pHead;
    m_pHead = pNew;
    *phMixer = (DWORD) pNew;
 
    return MMSYSERR_NOERROR;
}

MMRESULT  
CDriverContext::CloseMixerHandle( MIXHANDLE mxh)
{
    PMIXER_CALLBACK pCurr, pPrev;
    pPrev = NULL;
    for (pCurr = m_pHead; pCurr != NULL; pCurr = pCurr->pNext) {
        if (pCurr == (PMIXER_CALLBACK) mxh) {
            if (pPrev == NULL) {
                // we're deleting the first item
                m_pHead = pCurr->pNext;
            }
            else {
                pPrev->pNext = pCurr->pNext;
            }
            delete pCurr;
            break;
        }
        pPrev = pCurr;
    }

    return MMSYSERR_NOERROR;
}

/*
- mixerGetLineInfo

MIXER_GETLINEINFOF_COMPONENTTYPE - first line whose type matches dwComponentType member of the MIXERLINE structure.
MIXER_GETLINEINFOF_DESTINATION  - line specified by dwDestination
MIXER_GETLINEINFOF_LINEID - line specified by the dwLineID member
MIXER_GETLINEINFOF_SOURCE - line specified by the dwDestination and dwSource
MIXER_GETLINEINFOF_TARGETTYPE - line that is for the dwType member of the Target structure

*/

MMRESULT  
CDriverContext::GetMixerLineInfo( PMIXERLINE pQuery, DWORD dwFlags)
{ int i;

    // pQuery is validated by API - points to accessible, properly sized MIXERLINE structure

    // result - assume failure
    PMXLINEDESC pFound = NULL;
    MMRESULT mmRet = MIXERR_INVALLINE;
    USHORT usLineID = 0;

    switch (dwFlags & MIXER_GETLINEINFOF_QUERYMASK) {
    case MIXER_GETLINEINFOF_DESTINATION:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo DESTINATION %x\r\n"), pQuery->dwDestination));
        {
            if (pQuery->dwDestination >= NELEMS(g_dst_lines)) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: invalid destination line %d\r\n"), pQuery->dwDestination));
                return MIXERR_INVALLINE;
            }
            usLineID = g_dst_lines[pQuery->dwDestination];
        }
        break;
    case MIXER_GETLINEINFOF_LINEID:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo LINEID %x\r\n"), pQuery->dwLineID));
        usLineID = (USHORT) pQuery->dwLineID;
        break;
    case MIXER_GETLINEINFOF_SOURCE:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo SOURCE %x %x\r\n"), pQuery->dwDestination, pQuery->dwSource));
        {
            // look up the destination line, then index into it's source table
            // to find the indexed source.
            if (pQuery->dwDestination >= NELEMS(g_dst_lines)) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: invalid destination line %d\r\n"), pQuery->dwDestination));
                return MIXERR_INVALLINE;
            }
            PMXLINEDESC pLine = LookupMxLine(g_dst_lines[pQuery->dwDestination]);
            if (pLine == NULL) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: inconsistent internal mixer line table\r\n")));
                return MMSYSERR_ERROR;
            }
            if (pQuery->dwSource >= pLine->ucConnections) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: invalid source line %d\r\n"), pQuery->dwSource));
                return MIXERR_INVALLINE;
            }
            usLineID = pLine->pSources[pQuery->dwSource];
        }
        break;
    case MIXER_GETLINEINFOF_COMPONENTTYPE:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo COMPONENT\r\n")));
        break;

    case MIXER_GETLINEINFOF_TARGETTYPE:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo TARGET\r\n")));
        // valid query, but we're not going to form usLineID
        break;
    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: invalid query %08x\r\n"), dwFlags & MIXER_GETLINEINFOF_QUERYMASK));
        return MMSYSERR_INVALPARAM;
    }

    switch (dwFlags & MIXER_GETLINEINFOF_QUERYMASK) {
    case MIXER_GETLINEINFOF_COMPONENTTYPE:
        // scan for line of proper type
        for (i = 0; i < nlines; i++) {
            if (g_mixerline[i].dwComponentType == pQuery->dwComponentType) {
                pFound = &g_mixerline[i];
                break;
            }
        }
#ifdef DEBUG
        if (pFound == NULL) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: no line of component type %08x\r\n"), pQuery->dwComponentType));
        }
#endif
        break;
    case MIXER_GETLINEINFOF_TARGETTYPE:
        // scan for target type
        for (i = 0; i < nlines; i++) {
            if (g_mixerline[i].dwTargetType == pQuery->Target.dwType) {
                pFound = &g_mixerline[i];
                break;
            }
        }
#ifdef DEBUG
        if (pFound == NULL) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: no line of target type %08x\r\n"), pQuery->Target.dwType));
        }
#endif
        break;

    case MIXER_GETLINEINFOF_DESTINATION:
    case MIXER_GETLINEINFOF_LINEID:
    case MIXER_GETLINEINFOF_SOURCE:
        pFound = LookupMxLine(usLineID);
        if (pFound == NULL) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineInfo: invalid line ID %08x\r\n"), usLineID));
            return MMSYSERR_ERROR;
        }
        break;
    default:
        // should never happen - we filter for this in the first switch()
        break;

    }

    if (pFound != NULL) {
        pQuery->cChannels = pFound->ucChannels;
        pQuery->cConnections = pFound->ucConnections;
        pQuery->cControls = pFound->ucControls;
        pQuery->dwComponentType = pFound->dwComponentType;
        pQuery->dwLineID = pFound->usLineID;
        pQuery->dwDestination = pFound->ucDstIndex;
        pQuery->dwSource = pFound->ucDstIndex;
        pQuery->fdwLine = pFound->ucFlags;
        pQuery->Target.dwDeviceID = 0;
        pQuery->Target.dwType = pFound->dwTargetType;
        pQuery->Target.vDriverVersion = DRIVER_VERSION;
        pQuery->Target.wMid = MM_MICROSOFT;
        pQuery->Target.wPid = MM_MSFT_WSS_MIXER;
        wcscpy(pQuery->szName, pFound->szName);
        wcscpy(pQuery->szShortName, pFound->szShortName);
        wcscpy(pQuery->Target.szPname, DEVICE_NAME);
        mmRet = MMSYSERR_NOERROR;

		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineInfo: \"%s\" %08x\r\n"), pFound->szName, pFound->ucFlags));
    
	}

    return mmRet;
}


// mixerGetLineControls
MMRESULT  
CDriverContext::GetMixerLineControls ( PMIXERLINECONTROLS pQuery, DWORD dwFlags)
{ int i;

    PMIXERCONTROL pDstControl = pQuery->pamxctrl;
    USHORT usLineID = (USHORT) pQuery->dwLineID;
    DWORD dwCount = pQuery->cControls;

    switch (dwFlags & MIXER_GETLINECONTROLSF_QUERYMASK) {
    case MIXER_GETLINECONTROLSF_ALL:
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineControls: ALL %x\r\n"), usLineID));
        // retrieve all controls for the line pQuery->dwLineID
        {
            PMXLINEDESC pFound = LookupMxLine(usLineID);
            if (pFound == NULL) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: invalid line ID %04x\r\n"), usLineID));
                return MIXERR_INVALLINE;
            }
            if (pFound->ucControls != dwCount) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: incorrect number of controls. Expect %d, found %d.\r\n"),dwCount,pFound->ucControls));
                return MMSYSERR_INVALPARAM;
            }
            for (i = 0; i < ncontrols && dwCount > 0; i++) {
                PMXCONTROLDESC pSrcControl = &g_controls[i];
                if (pSrcControl->usLineID == usLineID) {
                    CopyMixerControl(pDstControl, pSrcControl, i);
                    pDstControl++;
                    dwCount--;
                }
            }
        }
        break;

    case MIXER_GETLINECONTROLSF_ONEBYID: 
		DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineControls: ONEBYID %x\r\n"), pQuery->dwControlID));
        // retrieve the control specified by pQuery->dwControlID
        if (pQuery->dwControlID >= ncontrols) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: invalid control ID %d (max %d)\r\n"), pQuery->dwControlID, ncontrols));
            return MIXERR_INVALCONTROL;
        }
        if (dwCount < 1) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: control count must be nonzero\r\n")));
            return MMSYSERR_INVALPARAM;
        }
        CopyMixerControl(pDstControl, &g_controls[pQuery->dwControlID], pQuery->dwControlID);
        pQuery->dwLineID = g_controls[pQuery->dwControlID].usLineID;
        break;

    case MIXER_GETLINECONTROLSF_ONEBYTYPE: 
        // retrieve the control specified by pQuery->dwLineID and pQuery->dwControlType
        {
            PMXLINEDESC pFound = LookupMxLine(usLineID);
            if (pFound == NULL) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: invalid line ID %04x\r\n"), usLineID));
                return MIXERR_INVALLINE;
            }
			DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineControls: ONEBYTYPE %x \"%s\"\r\n"), usLineID, pFound->szName));
            if (dwCount < 1) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: control count must be non zero\r\n")));
                return MMSYSERR_INVALPARAM;
            }
            int index = LookupMxControl(usLineID, pQuery->dwControlType);
            if (index >= ncontrols) {
                // not to be alarmed: SndVol32 queries for LOTS of control types we don't have
                DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerLineControls: line %04x (%s) has no control of type %s (%08x)\r\n"), usLineID, pFound->szName, COMPTYPE(pQuery->dwControlType), pQuery->dwControlType));
                return MMSYSERR_INVALPARAM;
            }
            
            CopyMixerControl(pDstControl, &g_controls[index], index);
            return MMSYSERR_NOERROR;
            // if we fall out of the search loop, we return failure
        }
        break;
    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerLineControls: invalid query %08x\r\n"), dwFlags & MIXER_GETLINECONTROLSF_QUERYMASK));
        break;

    }
    return MMSYSERR_NOERROR;
}

// mixerGetControlDetails
MMRESULT  
CDriverContext::GetMixerControlDetails( PMIXERCONTROLDETAILS pQuery, DWORD dwFlags)
{
    // API guarantees that pQuery points to accessible, aligned, properly sized MIXERCONTROLDETAILS structure
    DEBUGMSG(ZONE_HWMIXER, (TEXT("GetMixerControlDetails(%d)\r\n"), pQuery->dwControlID));

    if (pQuery->dwControlID >= ncontrols) {
        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: invalid control %d (max %d)\r\n"), pQuery->dwControlID, ncontrols));
        return MIXERR_INVALCONTROL;
    }

    PMXCONTROLDESC pSrcControlDesc = &g_controls[pQuery->dwControlID];
    PMXCONTROLSTATE pSrcControlState = &m_pMxControlState[pQuery->dwControlID];
    PMXLINEDESC pLine = LookupMxLine(pSrcControlDesc->usLineID);
    if (pLine == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: inconsistent internal mixer line table\r\n")));
        return MMSYSERR_ERROR;
    }
 
    switch (dwFlags & MIXER_GETCONTROLDETAILSF_QUERYMASK) {
    case MIXER_GETCONTROLDETAILSF_LISTTEXT:
        // only support this for the Record Select mux
        if (pSrcControlDesc->usLineID != MXLINEID(PCM_IN,NOLINE)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: invalid line ID %04x\r\n"), pSrcControlDesc->usLineID));
            return MMSYSERR_INVALPARAM;
        }
        {
            if (pQuery->cMultipleItems != pLine->ucConnections) {
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: incorrect cMultipleItems. Expected %d, found\r\n"),
                    pLine->ucConnections, pQuery->cMultipleItems));
                return MMSYSERR_INVALPARAM;
            }
            MIXERCONTROLDETAILS_LISTTEXT * pValue = (MIXERCONTROLDETAILS_LISTTEXT * ) pQuery->paDetails;
            for (int i = 0; i < pLine->ucConnections; i++) {
                PMXLINEDESC pl = LookupMxLine(pLine->pSources[i]);
                if (pl == NULL) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails - inconsistent internal mixer line table\r\n")));
                    return MMSYSERR_ERROR;
                }
                pValue[i].dwParam1 = pl->usLineID;
                pValue[i].dwParam2 = pl->dwComponentType;
                wcscpy(pValue[i].szName, pl->szShortName);
            }
        }
        break;
    case MIXER_GETCONTROLDETAILSF_VALUE:
        switch(pSrcControlDesc->dwType) {
            case MIXERCONTROL_CONTROLTYPE_VOLUME:
                {
                    MIXERCONTROLDETAILS_UNSIGNED * pValue = (MIXERCONTROLDETAILS_UNSIGNED * ) pQuery->paDetails;
					ULONG ulVolR, ulVolL;
					ulVolR = pSrcControlState->uSetting.dw & 0xffff;
                    if (pLine->ucChannels == 2) {
                        ulVolL = (pSrcControlState->uSetting.dw >> 16) & 0xffff;
                    }
					else {
						ulVolL = ulVolR;
					}

					if (pQuery->cChannels == 1) {
						pValue[0].dwValue = (ulVolR + ulVolL)/2;
					}
					else {
	                    pValue[0].dwValue = ulVolL;
	                    pValue[1].dwValue = ulVolR;
					}
                }
                break;
            case MIXERCONTROL_CONTROLTYPE_ONOFF:
            case MIXERCONTROL_CONTROLTYPE_MUTE:
                {
                    MIXERCONTROLDETAILS_BOOLEAN * pValue = (MIXERCONTROLDETAILS_BOOLEAN *) pQuery->paDetails;
                    pValue[0].fValue = pSrcControlState->uSetting.b;
                }
                break;
            case MIXERCONTROL_CONTROLTYPE_MUX:
                {
                    MIXERCONTROLDETAILS_BOOLEAN * pValue = (MIXERCONTROLDETAILS_BOOLEAN *) pQuery->paDetails;
                    memset(pValue, 0, sizeof(pValue[0]) * pSrcControlDesc->usMask); // set all entries to FALSE
                    ASSERT((pSrcControlState->uSetting.dw & 0xF) < NELEMS(MuxIndexToLine));
                    USHORT usSelectedLine = MuxIndexToLine[pSrcControlState->uSetting.us];
                    PMXLINEDESC pl = LookupMxLine(usSelectedLine);
                    if (pl == NULL) {
                        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: inconsistent internal mixer line table\r\n")));
                        return MMSYSERR_ERROR;
                    }
                    pValue[pl->ucSrcIndex].fValue = TRUE;
                }
                break;
            default:
                DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: unexpected control type %08x\r\n"), pSrcControlDesc->dwType));
                ASSERT(0);
                break;
        }
        break;
    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("GetMixerControlDetails: invalid query %08x\r\n"), dwFlags & MIXER_GETCONTROLDETAILSF_QUERYMASK));
        break;
    }
    return MMSYSERR_NOERROR;
}

// mixerSetControlDetails
MMRESULT  
CDriverContext::SetMixerControlDetails( PMIXERCONTROLDETAILS pDetail, DWORD dwFlags)
{


    // API guarantees that pDetail points to accessible, aligned, properly siezd MIXERCONTROLDETAILS structure
    DEBUGMSG(ZONE_ENTER, (TEXT("SetMixerControlDetails(%d)\r\n"), pDetail->dwControlID));
#if 1
    if (pDetail->dwControlID >= ncontrols) {
        DEBUGMSG(ZONE_ERROR, (TEXT("SetMixerControlDetails: invalid control %d (max %d)\r\n"), pDetail->dwControlID, ncontrols));
        return MIXERR_INVALCONTROL;
    }

    PMXCONTROLDESC pSrcControlDesc = &g_controls[pDetail->dwControlID];
    PMXCONTROLSTATE pSrcControlState = &m_pMxControlState[pDetail->dwControlID];
    PMXLINEDESC pLine = LookupMxLine(pSrcControlDesc->usLineID);
    if (pLine == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("SetMixerControlDetails: inconsistent internal mixer line table\r\n")));
        return MMSYSERR_ERROR;
    }
 
    switch(pSrcControlDesc->dwType) {
    case MIXERCONTROL_CONTROLTYPE_VOLUME:
        {
            MIXERCONTROLDETAILS_UNSIGNED * pValue = (MIXERCONTROLDETAILS_UNSIGNED * ) pDetail->paDetails;
			DWORD dwSettingL, dwSettingR;
			dwSettingL = pValue[0].dwValue;
			// setting might be mono or stereo. For mono, apply same volume to both channels
			if (pDetail->cChannels == 2) {
                dwSettingR = pValue[1].dwValue;
			}
			else {
				dwSettingR = dwSettingL; 
			}

            USHORT settingL, settingR;

            if (pLine->ucChannels == 1) {
				pSrcControlState->uSetting.dw = (dwSettingL + dwSettingR) / 2;
				settingR = settingL = VolToReg(pSrcControlState->uSetting.dw & 0xffff, pSrcControlDesc->usMask);
			}
			else {
                pSrcControlState->uSetting.dw = (dwSettingL << 16) | dwSettingR;
                settingR = VolToReg(dwSettingR & 0xffff, pSrcControlDesc->usMask);
				settingL = VolToReg(dwSettingL & 0xffff, pSrcControlDesc->usMask);
			}
            settingR = (pSrcControlDesc->usMask - settingR) * 100 / 0xf;
            settingL = (pSrcControlDesc->usMask - settingL) * 100 / 0xf;

            m_pDevice->SetVolume(0, pLine->ucDstIndex, settingL, settingR);

            if (! (pSrcControlState->fVirtualized)) {
				DEBUGMSG(ZONE_VOLUME, (TEXT("CDriverContext::SetMixerControlDetailsX: %08x %08x %04x %04x\r\n"), dwSettingL, dwSettingR, pSrcControlDesc->ucCodecRegister,  (settingL << 8) | settingR));
            }
        }
        break;
    case MIXERCONTROL_CONTROLTYPE_ONOFF:
    case MIXERCONTROL_CONTROLTYPE_MUTE:
        {
            MIXERCONTROLDETAILS_BOOLEAN * pValue = (MIXERCONTROLDETAILS_BOOLEAN *) pDetail->paDetails;
            pSrcControlState->uSetting.b = pValue[0].fValue;
            USHORT setting = (pValue[0].fValue) ?  pSrcControlDesc->usMask : 0;
            m_pDevice->SetMute(0, pLine->ucDstIndex, setting);
            if (! (pSrcControlState->fVirtualized)) {
            }
        }
        break;
    case MIXERCONTROL_CONTROLTYPE_MUX:
        {
            if (pDetail->cMultipleItems != pLine->ucConnections) {
                DEBUGMSG(ZONE_ERROR, (TEXT("SetMixerControlDetails: incorrect cMultipleItems. Expect %d, found %d\r\n"),
                    pLine->ucConnections, pDetail->cMultipleItems));
                return MMSYSERR_INVALPARAM;
            }
            MIXERCONTROLDETAILS_BOOLEAN * pValue = (MIXERCONTROLDETAILS_BOOLEAN *) pDetail->paDetails;
            for (USHORT i = 0; i < pLine->ucConnections; i++) {
                // find the entry that's set.
                if (pValue[i].fValue) {
                    USHORT usSelectedLine = pLine->pSources[i];
                    // Do a reverse look up for the selected line in the MuxIndexToLine table
                    // This gives us the desired new setting for the mux register.
                    for (USHORT j = 0; j < NELEMS(MuxIndexToLine); j++) {
                        if (usSelectedLine == MuxIndexToLine[j]) {
                            int index;

                            // set mux to j
                            // But first, the "Virtual Volume" trick:
                            // All of the PCM_IN source line volume controls have, in fact, no underlying codec register.
                            // Instead, when the Record Select MUX changes values, we copy the selected line's
                            // volume setting to the Record Gain register.

                            // first, re-virtualize the current source line
                            index = LookupMxControl(MuxIndexToLine[pSrcControlState->uSetting.us], MIXERCONTROL_CONTROLTYPE_VOLUME);
                            ASSERT(index < ncontrols);
                            ASSERT( ! m_pMxControlState[index].fVirtualized);
                            m_pMxControlState[index].fVirtualized = TRUE;

                            // now, switch the mux to the new source
                            // note that the mux has separate Left/Right settings, but for simplicity,
                            // we expose the mux as uniform
                            pSrcControlState->uSetting.us = j;
							DEBUGMSG(ZONE_VOLUME, (TEXT("CDriverContext::SetMixerControlDetailsA: %04x %04x)\r\n"), pSrcControlDesc->ucCodecRegister, j | (j << 8)));

                            // finally, un-virtualize the new source and copy the volume/mute settings
                            // into the HDA_RECORD_GAIN register.
                            index = LookupMxControl(MuxIndexToLine[j], MIXERCONTROL_CONTROLTYPE_VOLUME);
                            ASSERT(index < ncontrols);
                            ASSERT (m_pMxControlState[index].fVirtualized);
                            m_pMxControlState[index].fVirtualized = FALSE;

							// Recording level is actually a true gain, so reverse-bias this.
                            USHORT r = (USHORT) ((m_pMxControlState[index].uSetting.dw >>  8 ) & 0xf);
                            USHORT l = (USHORT) ((m_pMxControlState[index].uSetting.dw >> 24 ) & 0xf);
				DEBUGMSG(ZONE_VOLUME, (TEXT("CDriverContext::SetMixerControlDetailsB: %04x %04x)\r\n"), g_controls[index].ucCodecRegister, l | (r << 8)));

                            break;
                        } // if selected
                    } // for j
                    break; // found the first TRUE entry - break out.
                } // if fValue
            } // for i
        }
        break;
    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("SetMixerControlDetails: unexpected control type %08x\r\n"), pSrcControlDesc->dwType));
        ASSERT(0);
        break;
    }

    PerformMixerCallbacks (MM_MIXM_CONTROL_CHANGE, GET_MXCONTROL_ID(pSrcControlDesc));
#endif
    return MMSYSERR_NOERROR;
}

// mixerGetDevCaps
MMRESULT  
CDriverContext::GetMixerCaps(PMIXERCAPS pCaps)
{
    pCaps->wMid = MM_MICROSOFT;
    pCaps->wPid = MM_MSFT_WSS_MIXER;
    wcscpy(pCaps->szPname, DEVICE_NAME);
    pCaps->vDriverVersion = DRIVER_VERSION;
    pCaps->cDestinations = NELEMS(g_dst_lines);
    pCaps->fdwSupport = 0;

    return MMSYSERR_NOERROR;
}

