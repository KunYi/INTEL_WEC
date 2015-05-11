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

//
// HDA class definition
//
#ifndef HDA_H
#define HDA_H

#define	PRE_ALLOC_BUFFER_SIZE_DAC	0x10000		//64kb	needed by dmusic	
#define	PRE_ALLOC_BUFFER_SIZE_ADC	0x8000		//32kb	

#include <wtypes.h>
#include <ceddk.h>
#include <giisr.h>
#include <pm.h>
#include "cregkey.h"
#include "hdactrl.h"

//
// Do nothing emulation of WDM InteruptSync interface
// 
typedef LONG NTSTATUS;

typedef NTSTATUS
(*PINTERRUPTSYNCROUTINE)
(
    IN struct IInterruptSync_WinCeStub* InterruptSync,
    IN PVOID  DynamicContext
);

struct IInterruptSync_WinCeStub
{
    STDMETHOD(CallSynchronizedRoutine)(PINTERRUPTSYNCROUTINE Routine, PVOID DynamicContext)
	{
		return Routine(NULL,DynamicContext);
	}
};

typedef IInterruptSync_WinCeStub *PINTERRUPTSYNC;

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

typedef enum {
     PCM_TYPE_M8,
     PCM_TYPE_M16,
     PCM_TYPE_S8,
     PCM_TYPE_S16
} PCM_TYPE, *PPCM_TYPE;

/* DMA Channel State defines */
#define DMA_STOPPED         0
#define DMA_RUNNING         1
#define DMA_PAUSED          2

#define DMADIR_IN       1           // DMA Input Channel
#define DMADIR_OUT      2           // DMA Output Channel

#define BDL_SIZE	8192		// Buffer Descriptor List size
#define BDL_NUM		0x20	// Buffer Descriptor List entry cound

#define NUM_NABMCR_SAVE	 0x34		// save index 0 - index 0x30 of the Native Audio Bus Master Control Registers

typedef VOID (*DMAINTHANDLER) (PVOID);

typedef struct NABMCtrlRegsTag
{
	ULONG bdbarAddr;	  // buffer descriptor address (32bit)
	ULONG bdbarOffset;    // buffer descriptor offset
	ULONG civOffset;
	ULONG lviOffset;
	ULONG srOffset;
	ULONG picbOffset;
	ULONG pivOffset;
	ULONG crOffset;
	UCHAR ctrlReg;
} NABMCtrlRegs;

typedef struct PMContextTag
{
	ULONG         numChannels;
	NABMCtrlRegs  regInfo[7];
	ULONG         globSta;
	ULONG		  pciState[64/sizeof(ULONG)];
} PMContext;


typedef struct DMAChannelTag
{
    BOOL  fAllocated;           // is the channel available for allocation?
	BOOL  fAvailable;           // is this channel available?
    ULONG ulDirection;          // combination of DMADIR_IN and DMADIR_OUT
    ULONG ucIntMask;            // which bit in interrupt register belongs to this channel?
    ULONG ulDMAChannelState;
	WORD  wFormatTag;           // audio format tag
	 
    // system things
    PVOID pvBufferVirtAddr;
    ULONG ulBufferPhysAddr;      // physical RAM address of the buffer
    ULONG ulBufferSize;          // length of contiguous physical RAM
    ULONG ulAllocatedSize;      // allocated size - might be larger than current buffer size
    ULONG ulInitialSize;         // default pre-allocation sizes

	// used for providing audio clock
	BOOL  fTiming;				 // need to provide timing information
	ULONG cbChunk;				 // chuck size in byte
	ULONG tWrap;				 // time, in ms, for buffer to wrap around
	ULONG tLast;				 // last system clock time when interrupt handler is called
	LONGLONG cbXferedChunk;		 // bytes transfered, in chunks 							

    // internal things
    ULONG ulChannels;            // number of channels (1 or 2)
    ULONG ulSampleSize;          // sample size - 8bit or 16 bit
    ULONG ulSampleRate;          // sample rate in samples/second
    ULONG ulBufferLength;        // size of buffer currently in use
    ULONG ulSamplesPerInt;       // number of samples to play before interrupting
    DMAINTHANDLER pfIntHandler;  // callback function to invoke on DMA interrupts
    PVOID pvIntContext;          // int handler callback context information
    UINT uDeviceId;

	// Intel Buffer Descriptor List things
	PBYTE gBDLHeadPtr;  		 // Pointer to the head of the Buffer Descriptor List
	PBYTE gPhysBDLHeadPtr; 	     // Pointers to the physical heads of the BDLs
	BYTE  gLastValidIndex;		 // PDDs copy of the LVI register
	BOOL  m_bLooping;			 // Looping status

	PCM_TYPE gdatatype;			 // Data type to be played/recorded back (8/16 bit Stero/Mono)
} DMACHANNEL;

class HDA
{
public:
	HDA ();
    ~HDA ();

    // Power Management
    ULONG SetPowerState( ULONG ulState );

    // DMA control
    MMRESULT AllocDMAChannel (ULONG ulDirection, 
                              ULONG ulSize, 
							  WORD  wFormatTag, UINT uDeviceId,
                              PULONG pulChannelIndex);
    BOOL FreeDMAChannel (ULONG ulChannelIndex);
	ULONG GetNumFreeDMAChannels(ULONG ulDirection);
	ULONG GetNumDMAChannels(void);
    void GetDMABuffer(      ULONG ulChannelIndex, 
                              PULONG pulBufferSize,
                              PVOID* ppvVirtAddr);
    
    void InitDMAChannel( ULONG ulChannelIndex, 
                         DMAINTHANDLER pfHandler,
                         PVOID pvContext);

    void SetDMAChannelFormat( ULONG ulChannelIndex,
                              ULONG ulChannels,
                              ULONG ulSampleSize,
                              ULONG ulSampleRate,
							  WORD  wFormatTag);

    void SetDMAChannelBuffer( ULONG ulChannelIndex,
                              ULONG ulBufferLength,
                              ULONG ulSamplesPerInt );
    void StartDMAChannel( ULONG ulChannelIndex );
    void StopDMAChannel( ULONG ulChannelIndex );
    void SetDMAInterruptPeriod( ULONG ulChannelIndex, ULONG ulSamplesPerInt);
    void SetDMALooping (ULONG ulChannelIndex, BOOL fIsLooping);
    void SetDMAVolume (ULONG ulChannelIndex, USHORT usVolLeft, USHORT usVolRight);
    ULONG GetDMAPosition( ULONG ulChannelIndex);
    void SetDMAPosition( ULONG ulChannelIndex, ULONG ulPosition );

    // Codec control
	void SetFrequency(ULONG ulChannelIndex, ULONG ulSampleRate, WORD wFormatTag);

	// Bus Master control
	void   WriteBusMasterUCHAR(PUCHAR reg, UCHAR val);
	void   WriteBusMasterUSHORT(PUSHORT reg, USHORT val);
	void   WriteBusMasterULONG(PULONG reg, ULONG val);
	UCHAR  ReadBusMasterUCHAR(PUCHAR reg);
	USHORT ReadBusMasterUSHORT(PUSHORT reg);
	ULONG  ReadBusMasterULONG(PULONG reg);

	// PCI routines
	BOOL AudioInitialize(CRegKey * pKey);
    BOOL MapDevice (CRegKey * pKey, PCTSTR pszActiveKey);

	// Initialization
    BOOL InitHardware(CRegKey * pRegKey);
 	void InitStream();
	BOOL InitHDA();
	void InterruptRoutine(ULONG ulChannelIndex);
	BOOL RestController();
	BOOL GetControllerCapabilities();
	BOOL Alloc_DMA(dma_desc *, UINT);
	void InitCorb();
	void InitRirb();
	void StartCorb();
	void StartRirb();
	void DiscoverCodecs(UINT);
	UINT CheckCodecFG(Codec_desc *);
	void GetAudioFuncGroup(Codec_desc *, type_nid);
	UINT CodecCommand(type_nid cad, UINT verb);
	UINT FlushRirb();
	void ParseAFG();
	WIDGET * GetWidget(type_nid nid);
	void ParseWidget(WIDGET *pWidget);
	void ParseWidgetConn(WIDGET *pWidget);
	void ParsePinWidget(WIDGET *pWidget);
	void ParseAudioCtl();
	Audio_Ctl *GetAudioCtl(UINT iIndex);
	void BuildTopology();
	UINT EnumDacPath();
	UINT FindDacPath(type_nid nid, UINT depth);
	UINT FindAdcPath(type_nid nid, UINT depth);
	UINT BuildRecSelector(type_nid nid, UINT depth);
	void ConfigPinWidgetCtl();
	void SetDefaultCtlVol();
	void WriteGainToWidget(type_nid cad, type_nid nid, UINT index, UINT iMute, UINT left, UINT right, UINT dir);
	void SetAmpGain(Audio_Ctl *pCtl, UINT mute, UINT left, UINT right);	
	UINT SetupPcmChannel(UINT dir);
	UINT SetMute(unsigned dev, UINT iDirection, USHORT iMute);
	UINT SetVolume(unsigned dev, UINT iDirection, USHORT left, USHORT right);
	void SetRecSource();
	void InitChannel(UINT dir,ULONG ulChannelIndex);
	void StopStream(Channel_desc *ch);
	void StartStream(Channel_desc *ch);
	void ResetStream(Channel_desc *ch);
	void SetStreamID(Channel_desc *ch);
	void SetupStream(Channel_desc *ch);
	BOOLEAN TestCap(ULONG msg, LPWAVEOPENDESC pWOD);
	void VendorSpecificConfig();
	void SetOutputStreamSink(UINT uDeviceId, DWORD SinkNumber);
	
public:
	DWORD 		m_dwDeviceID;             // the PCI Device ID
	// Maximum PCM Out levelGain, range from 0-31 (0=+12dB, 08=0dB, 1F=-34dB) 
   	DWORD 		m_dwMaxPcmOutLevelGain;
	UINT 		m_CodecID;
	type_nid	m_PowerControlNID;
	type_nid	m_HeadPhoneNID;
#ifndef USE_ALC262_PORTA
	type_nid	m_FrontHPOutNID;
#endif
	type_nid	m_LineOutNID;
	type_nid	m_OutputVolNID[MAX_STREAM];
	type_nid	m_InputVolNID[MAX_STREAM];
	type_nid	m_InputSelNID[MAX_STREAM];
	type_nid	m_InputPinNID[MAX_STREAM];
	Hda_Core 	*m_pHda_Core;
	DEVINFO 	*m_pDevInfo;
	
private:

    // Member Variable
    ULONG	m_hdaLen;
//----------------------------------------
	ULONG	m_MixerBase;
	ULONG	m_MixerLen;
	ULONG	m_BusMasterBase;
	ULONG	m_BusMasterLen;
	BOOL    m_fIOSpace;
	DWORD   m_giisrMask;
	GIISR_INFO m_GiisrInfo;

	DWORD m_dwDeviceNumber;			// the PCI Function Number
	DWORD m_dwFunctionNumber;		// the PCI Function Number
    DWORD m_dwRevisionID;        // the PCI chip revision
    HANDLE m_hIsrHandler;       // installable ISR handler
    DWORD m_dwBusNumber;
    DWORD m_dwInterfaceType;

    // power management
    ULONG  m_ulPowerState; // power state
	PMContext m_pmContext;
    USHORT m_usCRegsPMContext[128];
    ULONG  m_ulNABMCRContext[NUM_NABMCR_SAVE/4]; //Native Audio Bus Master Control Registers

public:
DMACHANNEL * m_dmachannel; // info about dma channel states
UCHAR m_maxStream;

private:
	// PCI related variables
	DWORD	m_IntrAudio;	// Interrupt ID
	BOOL	m_fIsMapped;	// must call MmUnmapIoSpace when destroyed

    // Power Management
	ULONG FindPMCapability(ULONG cap);
	void  SavePMContext(void);
	void  SavePCIState(void);
	void  RestorePCIState(void);
	void  SuspendDMAChannels(void);
	BOOL  SetPMPowerState(ULONG ulPowerState);
	void  DisablePCIBusMaster(void);
	void  PowerUpACLink(void);
	BOOL  EnablePMEWake(ULONG state, BOOL enable);
    void Codec_WaitForPowerState( USHORT usState );
    void Codec_SaveRegisterState( USHORT *pusRegisters );
    void Codec_RestoreRegisterState( USHORT *pusRegisters );

	// IST
	static DWORD WINAPI ISTStartup(LPVOID lpParameter);
	void IST();
	HANDLE m_hISTInterruptEvent;
	BOOL   m_bExitIST;
	HANDLE m_hISThread;

	CRITICAL_SECTION m_csPageLock;

	void UpdateBDL( ULONG ulChannelIndex, WORD UNALIGNED * pBuffer, ULONG BufferLen );

	// PCI Config Space routines
	DWORD PCIConfig_Read(ULONG BusNumber, ULONG Device, ULONG Function,	ULONG Offset );
	void PCIConfig_Write(ULONG BusNumber, ULONG Device,	ULONG Function,	ULONG Offset, ULONG Value, ULONG Size );

};

#endif HDA_H
