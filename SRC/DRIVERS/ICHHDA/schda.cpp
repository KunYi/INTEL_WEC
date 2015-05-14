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
#include <nkintr.h>
#include <ceddk.h>
#include <ddkreg.h>
#include "schda.h"
#include "debug.h"

#define DEVLOAD_DEVICEID_VALNAME TEXT("DeviceID")
#define DEVLOAD_REVISIONID_VALNAME TEXT("RevisionID")

HDA::HDA(void)
{
	InitializeCriticalSection( &m_csPageLock );

	DEBUGMSG(1,(TEXT("SCHUAM Driver built %s @ %s\r\n"),TEXT(__DATE__),TEXT(__TIME__) ));

	m_hISTInterruptEvent = NULL;

	m_hISThread = NULL;
	m_bExitIST = FALSE;

	m_maxStream = 0;
	m_giisrMask = 0;

	m_fIsMapped = FALSE;
	m_fIOSpace = TRUE; // default is to use I/O space

	m_dwMaxPcmOutLevelGain = 0x08; // default is 0dB

}


HDA::~HDA()
{
	// Close the IST
	m_bExitIST = TRUE;
	SetEvent(m_hISTInterruptEvent);
	if( WAIT_OBJECT_0 != WaitForSingleObject(m_hISThread, INFINITE) )//10000
		RETAILMSG(1,(TEXT("~HDA-- Error closing IST \r\n")));

	CloseHandle(m_hISTInterruptEvent);
    CloseHandle(m_hISThread);
	DeleteCriticalSection( &m_csPageLock );

	if(m_fIsMapped) 
	{
		MmUnmapIoSpace((PVOID)m_MixerBase, m_MixerLen);
		MmUnmapIoSpace((PVOID)m_BusMasterBase, m_BusMasterLen);
		m_fIsMapped = FALSE;
	}
    m_MixerBase = m_BusMasterBase = 0;

    if(m_hIsrHandler != NULL) 
    {
        FreeIntChainHandler(m_hIsrHandler);
    }
}

void HDA::GetDMABuffer(ULONG ulChannelIndex, PULONG pulBufferSize, PVOID * pvVirtAddr)
{
    if(ulChannelIndex < m_maxStream) 
    {
        *pulBufferSize = m_dmachannel[ulChannelIndex].ulBufferSize;
        *pvVirtAddr = m_dmachannel[ulChannelIndex].pvBufferVirtAddr;
    }
	DEBUGMSG(ZONE_VERBOSE,(TEXT("GetDMABuffer %x %x\r\n"),*pulBufferSize, *pvVirtAddr));
}

ULONG HDA::GetNumFreeDMAChannels(ULONG ulDirection)
{ 
	ULONG ulCount = 0;
	UINT iIndex;

    for(iIndex = 0; iIndex < m_maxStream; iIndex++) 
    {
        if((m_dmachannel[iIndex].ulDirection & ulDirection) && 
			!m_dmachannel[iIndex].fAllocated &&
			m_dmachannel[iIndex].fAvailable) 
		{
			ulCount ++;
        }
    }
	DEBUGMSG(ZONE_VERBOSE,(TEXT("GetNumFreeDMAChannels direction %d = %x\r\n"),ulDirection,ulCount));

	return ulCount;
}

ULONG HDA::GetNumDMAChannels(void)
{ 
	ULONG ulCount = 0;
	UINT iIndex;

    for(iIndex = 0; iIndex < m_maxStream; iIndex++) 
	{
		if(m_dmachannel[iIndex].fAvailable) 
		{
			ulCount ++;
        }
    }
	DEBUGMSG(ZONE_VERBOSE,(TEXT("GetNumDMAChannels = %d DMA channelsr\n"),ulCount));

	return ulCount;
}

MMRESULT HDA::AllocDMAChannel(ULONG ulDirection, ULONG ulSize, WORD wFormatTag, UINT uDeviceId, PULONG pulChannelIndex)
{ 
	ULONG ulIndex;
    
    if(ulSize > 64 * 1024) 
    {
        // only handle up to 64Kbyte DMA buffers
        return MMSYSERR_NOMEM;
    }

    // make two passes through the DMA channel table:
    // in pass 1, we try to use pre-allocated memory first.
    // only in pass 2 do we allocate new memory
    for(ulIndex = 0; ulIndex < m_maxStream; ulIndex++) 
    {
        if((m_dmachannel[ulIndex].ulDirection & ulDirection) && 
			!m_dmachannel[ulIndex].fAllocated &&
			m_dmachannel[ulIndex].fAvailable) 
		{

			if( uDeviceId !=0 && ulIndex == m_pHda_Core->iNumIss)
					continue;
	
			// we found an open slot. see if we can get enough memory
            if(m_dmachannel[ulIndex].ulAllocatedSize >= ulSize) 
            {
                // currently allocated memory is sufficient. We're done.
                m_dmachannel[ulIndex].fAllocated = TRUE;
                m_dmachannel[ulIndex].ulBufferSize = ulSize; // record what we're actually using
                m_dmachannel[ulIndex].uDeviceId = uDeviceId;
                *pulChannelIndex = ulIndex;
                return MMSYSERR_NOERROR;
            }
        }
    }
    // pass 2: find a free slot and allocate memory
    for(ulIndex = 0; ulIndex < m_maxStream; ulIndex++) 
    {
        if((m_dmachannel[ulIndex].ulDirection & ulDirection) && 
			!m_dmachannel[ulIndex].fAllocated &&
			m_dmachannel[ulIndex].fAvailable) 
		{
			
			// we only use S/PDIF Out DMA engine for non-PCM audio
			if(((wFormatTag != WAVE_FORMAT_PCM) && (ulIndex != (m_maxStream - 1))) ||
				((wFormatTag == WAVE_FORMAT_PCM) && (ulIndex == (m_maxStream - 1))) )
			{
				continue;
			}
		
			// pass 2: try to allocate memory for the request
            // make new allocation first, only if it succeeds do we release existing memory
            ULONG ulPhysAddr;
            PVOID pVirtAddr;
            PHYSICAL_ADDRESS LogicalAddress;
            DMA_ADAPTER_OBJECT AdapterObject;

            AdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
            AdapterObject.InterfaceType = (INTERFACE_TYPE) m_dwInterfaceType;
            AdapterObject.BusNumber = m_dwBusNumber;

            if((pVirtAddr = HalAllocateCommonBuffer(&AdapterObject, ulSize, &LogicalAddress, FALSE)) == NULL)
            {
        		DEBUGMSG(ZONE_ERROR,(TEXT("HDA::AllocDMAChannel - unable to allocate %d bytes of physical memory\r\n"), ulSize));
                // if this fails, there's not much point in trying it over and over
                return MMSYSERR_NOMEM;
            }

            ulPhysAddr = LogicalAddress.LowPart;

            if(m_dmachannel[ulIndex].ulBufferSize > 0) 
            {
                // free the previous allocation
                LogicalAddress.QuadPart = 0;
                HalFreeCommonBuffer(NULL, 0, LogicalAddress, m_dmachannel[ulIndex].pvBufferVirtAddr, FALSE);
            }
            m_dmachannel[ulIndex].ulAllocatedSize = ulSize;
            m_dmachannel[ulIndex].ulBufferSize = ulSize;
		    m_dmachannel[ulIndex].ulBufferPhysAddr = ulPhysAddr;
		    m_dmachannel[ulIndex].pvBufferVirtAddr = pVirtAddr;
            m_dmachannel[ulIndex].fAllocated = TRUE;
			m_dmachannel[ulIndex].uDeviceId = uDeviceId;
            *pulChannelIndex = ulIndex;
            return MMSYSERR_NOERROR;
        }
    }

    DEBUGMSG(ZONE_ERROR, (TEXT("HDA::AllocDMAChannel - no free channels\r\n")));

    return MMSYSERR_ALLOCATED;
}

BOOL HDA::FreeDMAChannel(ULONG ulChannelIndex)
{
    ASSERT(ulChannelIndex < m_maxStream);
    ASSERT(m_dmachannel[ulChannelIndex].fAllocated);


    m_dmachannel[ulChannelIndex].fAllocated = FALSE;
	m_dmachannel[ulChannelIndex].pfIntHandler = NULL;
    m_dmachannel[ulChannelIndex].pvIntContext = NULL;

    // note that we DO NOT relinquish physical memory, since contigous physical memory is
    // harder to come by as the system runs. We'd run the risk of not getting the memory
    // back next time we need it.

    return TRUE;
}

	
//--------------------------------------------------------------------------
//
//  Name: HDA::InitHardware
//
//  Description: intitializes the HDA hardware
//
//  Parameters: none
//
//  Returns: void
//
//  Note: 
//
//--------------------------------------------------------------------------
BOOL HDA::InitHardware(CRegKey * pRegKey)
{

	PHYSICAL_ADDRESS paBDLBuffer;
	PUCHAR pBDLBuffer;
	DMA_ADAPTER_OBJECT AdapterObject;
	UINT iOffset;
	UINT iIndex;

	 // Initialize HDA hardware
	DEBUGMSG(ZONE_VERBOSE,(TEXT("HDA::InitHardware\r\n")));

	//setup the IST
	if( AudioInitialize(pRegKey) != TRUE )
		DEBUGMSG(1,(TEXT("HDA::InitHardware: IST failed \r\n")));	

	if( InitHDA() != TRUE )
	{
		RETAILMSG(1,(TEXT("HDA::InitHardware: InitHDA() failed \r\n")));
		return FALSE;
	}

	// allocate the Buffer Descriptor Lists memory and initialize the BDPL/Us
	AdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
	AdapterObject.InterfaceType = (INTERFACE_TYPE) m_dwInterfaceType;
	AdapterObject.BusNumber = m_dwBusNumber;
	pBDLBuffer = (PUCHAR)HalAllocateCommonBuffer(&AdapterObject, m_maxStream * sizeof(Buf_Desc_List) * HDA_BDL_DEFAULT, &paBDLBuffer, FALSE);

	for(iIndex = 0; iIndex < m_maxStream; iIndex++) 
	{
		if(m_dmachannel[iIndex].fAvailable)
		{
			m_dmachannel[iIndex].gBDLHeadPtr = pBDLBuffer + (iIndex * sizeof(Buf_Desc_List) * HDA_BDL_DEFAULT);
			m_dmachannel[iIndex].gPhysBDLHeadPtr = (((PBYTE)paBDLBuffer.LowPart) + (iIndex * sizeof(Buf_Desc_List) * HDA_BDL_DEFAULT)); 
			iOffset = 0x20 * iIndex;
			HDA_WRITE_REG_4(&m_pHda_Core->mem, iOffset+ REG_SDCBL, m_dmachannel[iIndex].ulAllocatedSize);
			HDA_WRITE_REG_2(&m_pHda_Core->mem, iOffset + REG_SDLVI, HDA_BDL_DEFAULT - 1);
			HDA_WRITE_REG_4(&m_pHda_Core->mem, iOffset+ REG_SDBDPL, (UINT)m_dmachannel[iIndex].gPhysBDLHeadPtr);
			HDA_WRITE_REG_4(&m_pHda_Core->mem, iOffset+ REG_SDBDPU, (UINT)0x0);
		}
	}

	SetRecSource();
	
	DEBUGMSG(ZONE_INFO,(TEXT("HDA::InitHardware: MIXER\r\n")));
	return TRUE;
}

// }}}

//--------------------------------------------------------------------------
//
//  Name: InitDMAChannel
//
//  Description: Intialize all the things about a DMA channel
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel to init
//              ULONG ulBufferPhysAddr : Physical address of the DMA buffer
//                                    in memory
//              USHORT usBufferSize : Number of bytes in a DMA buffer
//
//  Returns: none   
//
//  Note:
//
//--------------------------------------------------------------------------
void HDA::InitDMAChannel( ULONG ulChannelIndex, DMAINTHANDLER pfHandler, PVOID pContext )
{

  DEBUGMSG(ZONE_ENTER,(TEXT("HDA::InitDMAChannel[%d] pfH=%x pC=%x\r\n"),ulChannelIndex, pfHandler, pContext ));

  m_dmachannel[ulChannelIndex].pfIntHandler = pfHandler;
  m_dmachannel[ulChannelIndex].pvIntContext = pContext;

  m_dmachannel[ulChannelIndex].fTiming = FALSE;

  ULONG ulBufferPhysAddr = m_dmachannel[ulChannelIndex].ulBufferPhysAddr;

  // Initialize the BDL start address 
  HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex+ REG_SDBDPL, (UINT)m_dmachannel[ulChannelIndex].gPhysBDLHeadPtr);
  HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex+ REG_SDBDPU, (UINT)0x0);
	
  DEBUGMSG(ZONE_HDA,(TEXT("HDA::InitDMAChannel[%d]: gBDLHeadPtr=%x gPhysBDLHeadPtr=%x\r\n"),ulChannelIndex,
		m_dmachannel[ulChannelIndex].gBDLHeadPtr,m_dmachannel[ulChannelIndex].gPhysBDLHeadPtr)); 

  return ;
}  

void HDA::SetDMAVolume(ULONG ulChannelIndex, USHORT usLeftVol, USHORT usRightVol)
{
  DEBUGMSG(ZONE_HDA,(TEXT("HDA::SetDMAVolume[%d] L=%x R=%x\r\n"),ulChannelIndex, usLeftVol, usRightVol));
}

//--------------------------------------------------------------------------
//
//  Name: SetDMAChannelFormat
//
//  Description: Set the Format of a particular DMA channel.
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel to init
//              ULONG ulChannels   : Number of audio channels 1=mono 2=stereo
//              ULONG ulSampleSize : Sample size - 8bit or 16bit
//              ULONG ulSampleRate : actual number of samples/second.
//
//  Returns: none   
//
//  Note:
//
//--------------------------------------------------------------------------
void HDA::SetDMAChannelFormat( ULONG ulChannelIndex, ULONG ulChannels, ULONG ulSampleSize, ULONG ulSampleRate, WORD  wFormatTag)
{
  DEBUGMSG(ZONE_HDA,(TEXT("HDA::SetDMAChannelFormat - channel %d, ulChannels = %d, ulSampleSize = %d, ulSampleRate = %d\r\n"),
  		ulChannelIndex, ulChannels, ulSampleSize, ulSampleRate));

  // save off the info 
  m_dmachannel[ulChannelIndex].ulChannels   = ulChannels;
  m_dmachannel[ulChannelIndex].ulSampleSize = ulSampleSize;
  m_dmachannel[ulChannelIndex].ulSampleRate = ulSampleRate;
  m_dmachannel[ulChannelIndex].wFormatTag   = wFormatTag;

  // set the new sample rate
  SetFrequency(ulChannelIndex, ulSampleRate, wFormatTag);

  // Store information about number of channels and data size in the global variable, gdatatype[apidir].   
  if(ulSampleSize == 8) 
  {
      // 8 bit samples
      if(ulChannels == 1)
          // Single channel (i.e. mono)
          m_dmachannel[ulChannelIndex].gdatatype = PCM_TYPE_M8;
      else
          // Dual channel (i.e. stereo)
          m_dmachannel[ulChannelIndex].gdatatype = PCM_TYPE_S8;
  } else {	 // 16 bit samples
      if(ulChannels == 1)
          // Single channel (i.e. mono)
          m_dmachannel[ulChannelIndex].gdatatype = PCM_TYPE_M16;
      else
          // Dual channel (i.e. stereo)
          m_dmachannel[ulChannelIndex].gdatatype = PCM_TYPE_S16;
   }

  return ;  
}

//--------------------------------------------------------------------------
//
//  Name: SetDMAChannelBuffer
//
//  Description: Set the buffer sizes of a particular DMA channel.
//
//  Parameters: ULONG ulChannelIndex    : Index of the DMA channel to init
//              ULONG ulBufferLength  : Size in bytes of the whole DMA buffer
//              ULONG ulSamplesPerInt : Count of samples before interrupting
//
//  Returns: none   
//
//  Note:
//
//--------------------------------------------------------------------------
void HDA::SetDMAChannelBuffer( ULONG ulChannelIndex, ULONG ulBufferLength, ULONG ulSamplesPerInt )
{	  

  m_dmachannel[ulChannelIndex].ulBufferLength  = ulBufferLength;
  m_dmachannel[ulChannelIndex].ulSamplesPerInt = ulSamplesPerInt;
  m_dmachannel[ulChannelIndex].cbChunk = ulBufferLength/HDA_BDL_DEFAULT;
  DWORD cbRate = (m_dmachannel[ulChannelIndex].ulSampleRate*m_dmachannel[ulChannelIndex].ulChannels*(m_dmachannel[ulChannelIndex].ulSampleSize>>3));
  m_dmachannel[ulChannelIndex].tWrap = ulBufferLength*1000/cbRate;

  DEBUGMSG(ZONE_HDA, (TEXT("HDA::SetDMAChannelBuffer[%d] : ulBufferLength = %d Buffer = %x/%x ulSamplesPerInt = %x\r\n"),
		ulChannelIndex,ulBufferLength,
		m_dmachannel[ulChannelIndex].pvBufferVirtAddr,
		m_dmachannel[ulChannelIndex].ulBufferPhysAddr,
		ulSamplesPerInt));

  // pg. 19 SW EAS
  // step 1. create the BDL structure in memory
	unsigned long chunksize=(ulBufferLength/HDA_BDL_DEFAULT);
	BYTE *pPhysChunk=(BYTE *)m_dmachannel[ulChannelIndex].ulBufferPhysAddr;  //first chunk starts at passed buffer pointer
	DWORD *BDLPtr = ((DWORD*)m_dmachannel[ulChannelIndex].gBDLHeadPtr);   // base of BDL list
	UINT chunk;

	m_dmachannel[ulChannelIndex].gLastValidIndex=0;
	
	for(chunk = 0; chunk < HDA_BDL_DEFAULT; chunk++,pPhysChunk+=chunksize) 
	{
	  UpdateBDL(ulChannelIndex,(WORD *)pPhysChunk,chunksize);
	  m_dmachannel[ulChannelIndex].gLastValidIndex++;
	}
	
	m_dmachannel[ulChannelIndex].gLastValidIndex = HDA_BDL_DEFAULT-1;

	HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDBDPL, (UINT)m_dmachannel[ulChannelIndex].gPhysBDLHeadPtr);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDBDPU, (UINT)0x0);
	
	HDA_WRITE_REG_2(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDLVI, m_dmachannel[ulChannelIndex].gLastValidIndex);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex+ REG_SDCBL, m_dmachannel[ulChannelIndex].ulBufferSize);
	
	DEBUGMSG(ZONE_VERBOSE, (TEXT("HDA::SetDMAChannelBuffer[%d] : BDLBA = %x pPxLVIreg = %x"), ulChannelIndex,
	 m_dmachannel[ulChannelIndex].gPhysBDLHeadPtr,m_dmachannel[ulChannelIndex].gLastValidIndex ));

  return ;  
}

void HDA::SetDMAInterruptPeriod( ULONG ulChannelIndex, ULONG ulSampleCount)
{
        // ICH only stops at the end of a BDL entry so nothing to do here
	DEBUGMSG(ZONE_HDA,(TEXT("HDA::SetDMAInterruptPeriod[%d] %d\r\n"),ulChannelIndex,ulSampleCount));

}


void HDA::SetDMALooping (ULONG ulChannelIndex, BOOL fEnableLooping)
{
	DEBUGMSG(ZONE_HDA,(TEXT("HDA::SetDMALooping[%d] %x\r\n"),ulChannelIndex,fEnableLooping));
	m_dmachannel[ulChannelIndex].m_bLooping	= fEnableLooping;
}


//--------------------------------------------------------------------------
//
//  Name: StartDMAChannel
//
//  Description: Start a particular DMA channel running.
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel to start
//
//  Returns: none   
//
//  Note:
//
//--------------------------------------------------------------------------
void HDA::StartDMAChannel( ULONG ulChannelIndex  )
{
	SetStreamID(&m_pHda_Core->channel[ulChannelIndex]);
	SetupStream(&m_pHda_Core->channel[ulChannelIndex]);
	StartStream(&m_pHda_Core->channel[ulChannelIndex]);

	  // save the state
	m_dmachannel[ulChannelIndex].ulDMAChannelState = DMA_RUNNING;
	m_dmachannel[ulChannelIndex].tLast = GetTickCount();
	m_dmachannel[ulChannelIndex].cbXferedChunk = 0;

    return;
}

//--------------------------------------------------------------------------
//
//  Name: StopDMAChannel
//
//  Description: Stop a particular DMA channel.
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel to stop
//
//  Returns: none   
//
//  Note:
//
//--------------------------------------------------------------------------
void HDA::StopDMAChannel(ULONG ulChannelIndex)
{

	StopStream(&m_pHda_Core->channel[ulChannelIndex]);

	// save the state
	m_dmachannel[ulChannelIndex].ulDMAChannelState = DMA_STOPPED;

	return;
}

//--------------------------------------------------------------------------
//
//  Name: GetDMAPosition
//
//  Description: reads the current position of a DMA channel.
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel
//
//  Returns: ULONG current DMA position in bytes  
//
//  Note: This function returns the DMA position NOT the sample play
//        cursor.  It appears that this will be used for buffer filling.
//
//--------------------------------------------------------------------------
ULONG HDA::GetDMAPosition( ULONG ulChannelIndex )
{
	ULONG ulCurrentPos;
	Channel_desc *pCh;

	pCh= &m_pHda_Core->channel[ulChannelIndex];

	EnterCriticalSection(&m_pHda_Core->cri_sec);
	ulCurrentPos = HDA_READ_REG_4(&m_pHda_Core->mem, pCh->iOffset + REG_SDLPIB);
	LeaveCriticalSection(&m_pHda_Core->cri_sec);

	DEBUGMSG(ZONE_PLAYCURSOR,(TEXT("HDA::GetDMAPosition channel %d = %d\r\n"),ulChannelIndex,ulCurrentPos ));

	return ulCurrentPos;
}

//--------------------------------------------------------------------------
//
//  Name: SetDMAPosition
//
//  Description: sets the current position of a DMA channel.
//
//  Parameters: ULONG ulChannelIndex : Index of the DMA channel
//              ULONG ulPosition: new DMA position in bytes  
//
//
//--------------------------------------------------------------------------
void HDA::SetDMAPosition( ULONG ulChannelIndex, ULONG ulPosition )
{
    DEBUGMSG(ZONE_INFO,(TEXT("HDA::SetDMAPosition channel %d to %x\r\n"),ulChannelIndex, ulPosition));
	
    if(ulPosition) 
    {														
	    DEBUGMSG(ZONE_ERROR,(TEXT("HDA::SetDMAPosition channel %d to %x (not 0!)\r\n"),ulChannelIndex, ulPosition));
	}
	else 
	{

		UCHAR uIOCE;

		 uIOCE = HDA_READ_REG_1(&m_pHda_Core->mem, 0x20 * ulChannelIndex  + REG_SDCTL0);
		 
		if(uIOCE & REG_SDCTL_RUN)
			StopDMAChannel(ulChannelIndex);


		HDA_WRITE_REG_2(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDLVI, HDA_BDL_DEFAULT - 1);
		HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex+ REG_SDCBL, m_dmachannel[ulChannelIndex].ulBufferSize);

		HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDBDPL, (UINT)m_dmachannel[ulChannelIndex].gPhysBDLHeadPtr);
		HDA_WRITE_REG_4(&m_pHda_Core->mem, 0x20 * ulChannelIndex + REG_SDBDPU, (UINT)0x0);
	}
}

BOOL HDA::MapDevice (CRegKey * pRegKey, PCTSTR pszActiveKey)
{
	DDKWINDOWINFO wini;
	DDKISRINFO isri;
	DDKPCIINFO pcii;

	PHYSICAL_ADDRESS MappedAddress;
	PHYSICAL_ADDRESS HDALowBaseAddr;	
	DMA_ADAPTER_OBJECT AdapterObject;
	PHYSICAL_ADDRESS   LogicalAddress;
	
	DWORD  inIOSpace;
	HANDLE hBusAccess;
	DWORD  dwStatus;
	ULONG  i;
	USHORT usGCAP;
	
	DEBUGMSG(ZONE_INIT,(TEXT("HDA::MapDevice\r\n")));

	m_pHda_Core = (Hda_Core *) LocalAlloc (LMEM_ZEROINIT, sizeof (Hda_Core));
	
	if (m_pHda_Core == NULL)
	{
	    DEBUGMSG(ZONE_ERROR, (_T("HDA: m_pHda_Core is NULL \r\n")));
	    return FALSE;
  }
	
	InitializeCriticalSection(&m_pHda_Core->cri_sec);

	// read window information
	wini.cbSize = sizeof(wini);
	dwStatus = DDKReg_GetWindowInfo(pRegKey->Key(), &wini);
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("HDA: DDKReg_GetWindowInfo() failed %d\r\n"), dwStatus));
		return FALSE;
	}
	m_dwBusNumber = wini.dwBusNumber;
	m_dwInterfaceType = wini.dwInterfaceType;

	// read ISR information
	isri.cbSize = sizeof(isri);
	dwStatus = DDKReg_GetIsrInfo(pRegKey->Key(), &isri);
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("HDA: DDKReg_GetIsrInfo() failed %d\r\n"), dwStatus));
		return FALSE;
	}

	// sanity check return values
	if(isri.dwSysintr == SYSINTR_NOP) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("HDA: no sysintr specified in registry\r\n")));
		return FALSE;
	}
	if(isri.szIsrDll[0] != 0) 
	{
		if(isri.szIsrHandler[0] == 0 || isri.dwIrq == IRQ_UNSPECIFIED) 
		{
			DEBUGMSG(ZONE_ERROR, (_T("HDA: invalid installable ISR information in registry\r\n")));
			return FALSE;
		}
	}

	// read PCI id
	pcii.cbSize = sizeof(pcii);
	dwStatus = DDKReg_GetPciInfo(pRegKey->Key(), &pcii);
	m_dwDeviceNumber = pcii.dwDeviceNumber;
	m_dwFunctionNumber = pcii.dwFunctionNumber;
	if(dwStatus != ERROR_SUCCESS) 
	{
		DEBUGMSG(ZONE_ERROR, (_T("HDA: DDKReg_GetPciInfo() failed %d\r\n"), dwStatus));
		return FALSE;
	}

	m_IntrAudio = isri.dwSysintr;

	m_dwDeviceID = pcii.idVals[PCIID_DEVICEID];
	m_dwRevisionID = pcii.idVals[PCIID_REVISIONID];

	m_pHda_Core->iPCI_SubVendor = pcii.idVals[PCIID_DEVICEID];
	m_pHda_Core->iPCI_SubVendor = (UINT)pcii.idVals[PCIID_SUBSYSTEMVENDORID] << 16;
	m_pHda_Core->iPCI_SubVendor |= (UINT)pcii.idVals[PCIID_SUBSYSTEMID]& 0x0000ffff;

	if(wini.dwNumMemWindows != 0) // memory space
	{
		m_hdaLen = wini.memWindows[0].dwLen;
		HDALowBaseAddr.LowPart = wini.memWindows[0].dwBase;
		HDALowBaseAddr.HighPart = 0;

		m_fIOSpace = FALSE;
	}
	else // I/O space
	{
		m_hdaLen = wini.memWindows[0].dwLen;
		HDALowBaseAddr.LowPart = wini.memWindows[0].dwBase;
		HDALowBaseAddr.HighPart = 0;

		m_fIOSpace = TRUE;
	}

	DEBUGMSG(ZONE_INIT, (TEXT("HDA: HDABase [%x/%x] \r\n"),HDALowBaseAddr.LowPart,m_hdaLen));
	
	// creates a handle that can be used for accessing a bus
	hBusAccess = CreateBusAccessHandle(pszActiveKey);

	inIOSpace = m_fIOSpace;
	if(!BusTransBusAddrToVirtual(hBusAccess, (INTERFACE_TYPE)m_dwInterfaceType, m_dwBusNumber,
								  HDALowBaseAddr, m_hdaLen, &inIOSpace, (PPVOID)&MappedAddress))
	{
       	DEBUGMSG(ZONE_ERROR, (TEXT("HDA::BusTranslateBusAddrToVirtual failed\r\n")));
        return FALSE;
    }

	if(inIOSpace)
	{
		m_fIsMapped = FALSE;	
	}

	m_pHda_Core->mem.HDALowBaseAddr = HDALowBaseAddr;
	m_pHda_Core->mem.MappedAddress = MappedAddress;
	m_pHda_Core->mem.iMemTag = MappedAddress.LowPart;
	m_pHda_Core->mem.iMemHandle = 0;
	m_pHda_Core->mem.iMemRid = 0;

	usGCAP = HDA_READ_REG_2(&m_pHda_Core->mem, REG_GCAP);
	m_maxStream = GET_SUPPORTED_ISS(usGCAP);
	m_maxStream += GET_SUPPORTED_OSS(usGCAP);
	m_maxStream += GET_SUPPORTED_BSS(usGCAP);

	m_dmachannel = (DMACHANNEL *)LocalAlloc(LMEM_ZEROINIT, sizeof (DMACHANNEL) * m_maxStream);
	
	if (m_dmachannel == NULL)
	{
      DEBUGMSG(ZONE_ERROR, (_T("HDA: m_dmachannel is NULL \r\n")));
      return FALSE;
  } 

	// initialize DMA channels
	for(i = 0; i < m_maxStream ; i++) 
	{
       	m_dmachannel[i].ulDMAChannelState = DMA_STOPPED;
        	m_dmachannel[i].fAllocated = FALSE;
		m_dmachannel[i].pfIntHandler = NULL;

		m_dmachannel[i].fAvailable = TRUE;
		m_dmachannel[i].ulDirection = ( i < (ULONG) GET_SUPPORTED_ISS(usGCAP)) ? DMADIR_IN : DMADIR_OUT;
		m_dmachannel[i].ulInitialSize = PRE_ALLOC_BUFFER_SIZE_DAC;
		
#ifdef POULSBO_C0_WORKAROUND			// Workaround required for Poulsbo C0 stepping and earier
			m_dmachannel[i].ucIntMask = ( i < (ULONG) GET_SUPPORTED_ISS(usGCAP)) ? 1 << i : 1 << ( i + 2 );
#else
			m_dmachannel[i].ucIntMask =  1<<i;
#endif			
	
		m_giisrMask |= m_dmachannel[i].ucIntMask;
	}
	m_giisrMask |= ( REG_INTSTS_GIS | REG_INTSTS_CIS );

    // Install ISR handler if there is one
    if(isri.szIsrDll[0] != 0) 
    {
        // Install ISR handler
        m_hIsrHandler = LoadIntChainHandler(isri.szIsrDll, isri.szIsrHandler, (BYTE) isri.dwIrq);
        if(m_hIsrHandler == NULL) 
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("HDA: Couldn't install ISR handler\r\n")));
        } else {
            PVOID dwPhysAddr;
            inIOSpace = m_fIOSpace; 
            
           	if(!BusTransBusAddrToStatic(hBusAccess, (INTERFACE_TYPE)m_dwInterfaceType, m_dwBusNumber,
									     HDALowBaseAddr, m_hdaLen, &inIOSpace, &dwPhysAddr)) 
			{
                DEBUGMSG(ZONE_ERROR, (TEXT("HDA: Failed BusTransBusAddrToStatic\r\n")));
                return FALSE;
            }
            
            DEBUGMSG(ZONE_ERROR, (TEXT("HDA: Installed ISR handler, Dll = '%S', Handler = '%S', Irq = %d, PhysAddr = 0x%x\r\n"),
                isri.szIsrDll, isri.szIsrHandler, isri.dwIrq, dwPhysAddr));
            
            // Set up ISR handler
            m_GiisrInfo.SysIntr    = m_IntrAudio;
            m_GiisrInfo.CheckPort  = TRUE;
            m_GiisrInfo.PortIsIO   = (inIOSpace) ? TRUE : FALSE;
            m_GiisrInfo.UseMaskReg = FALSE;
            m_GiisrInfo.PortAddr   = (DWORD)dwPhysAddr + REG_INTSTS;
            m_GiisrInfo.PortSize   = sizeof(DWORD);
            m_GiisrInfo.Mask       = m_giisrMask;
            
            if(!KernelLibIoControl(m_hIsrHandler, IOCTL_GIISR_INFO, &m_GiisrInfo, sizeof(m_GiisrInfo), NULL, 0, NULL)) 
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("HDA: KernelLibIoControl call failed.\r\n")));
                return FALSE;
            }
        }
    }

	//
	// Map IO and Interupt to System spaces 
	//
	m_hISTInterruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(m_hISTInterruptEvent == NULL) 
    {
		DEBUGMSG(ZONE_ERROR, (TEXT("HDA - unable to create IST event\r\n")));
        return FALSE;
    }

	if(!InterruptInitialize(m_IntrAudio, m_hISTInterruptEvent, NULL, 0) ) 
	{
		DEBUGMSG(ZONE_ERROR, (TEXT("HDA - InterruptInitialize(%d,%08x) Failed\r\n"), m_IntrAudio, m_hISTInterruptEvent));
        return FALSE;
	}
	

	// 
	// We'll try and reserve physical memory since it is more likely to succeed
	// on driver load
	// If this fails, we'll try again at run-time

    for(i = 0; i < m_maxStream; i++) 
	{
		if(m_dmachannel[i].fAvailable)
		{
			AdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
			AdapterObject.InterfaceType = (INTERFACE_TYPE) m_dwInterfaceType;
			AdapterObject.BusNumber = m_dwBusNumber;

			if((m_dmachannel[i].pvBufferVirtAddr = HalAllocateCommonBuffer(&AdapterObject, m_dmachannel[i].ulInitialSize, &LogicalAddress, FALSE)) != NULL)
			{
				m_dmachannel[i].ulBufferPhysAddr = LogicalAddress.LowPart;
				m_dmachannel[i].ulAllocatedSize = m_dmachannel[i].ulInitialSize;
			}
			else
			{
				m_dmachannel[i].ulAllocatedSize = 0;
				DEBUGMSG(ZONE_WARNING,(TEXT("HDA: unable to reserve %d bytes for DMA channel %d\r\n"),
                m_dmachannel[i].ulInitialSize, i ));
			}
		}
	}


    	//call our initialization functions
	if(!InitHardware(pRegKey)) 
		return FALSE;

	m_ulPowerState = D0; // we're now in D0 state

	DEBUGMSG(ZONE_INFO, (TEXT("HDA: PCI Init Done\r\n")));
    return TRUE;
}

//--------------------------------------------------------------------------
//
//  Name: HDA::AudioInitialize
//
//  Description: maps hardware and sets up the Interrupt Service Thread
//
//  Parameters: none
//
//  Returns: BOOL
//		TRUE if device was mapped properly and IST was set up correpCtly
//		FALSE if either of the above fails
//
//  Note: 
//
//--------------------------------------------------------------------------
BOOL HDA::AudioInitialize(CRegKey * pRegKey)
{
    DEBUGMSG(ZONE_VERBOSE, (TEXT("HDA::AudioInitialize \r\n")));

	// Set up the IST
	DWORD dwThreadId=0;

	m_hISThread = CreateThread(NULL, 0, ISTStartup, this, 0, &dwThreadId);
	if(m_hISThread == NULL)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("DsDriver::Open -- unable to create IST\r\n")));

		return FALSE;
	}

	DWORD dwPriority = 210;
	pRegKey->QueryDword (TEXT("Priority256"), dwPriority);
	CeSetThreadPriority(m_hISThread, dwPriority);

	return TRUE;
}

//--------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------
DWORD WINAPI HDA::ISTStartup(LPVOID lpParameter)
{
	((HDA *)lpParameter)->IST();
	return 0;
}

//--------------------------------------------------------------------------
//
//  Name: HDA::IST
//
//  Description: The Interrupt service thread,
//
//  Parameters: none
//
//  Returns: VOID
//
//  Note: 
//
//--------------------------------------------------------------------------
VOID HDA::IST()
{
	DWORD dwRet=0;
	BOOL bKeepLooping = TRUE;
	
	UINT iIntStatus;
	UCHAR ucRirbStatus;

	// loop here waiting for an Interrupt to occur
	while(bKeepLooping)
	{
	
       	dwRet = WaitForSingleObject(m_hISTInterruptEvent, INFINITE);

		if(m_bExitIST == TRUE)
			break;

		switch(dwRet)
		{
			case WAIT_OBJECT_0:
			{				
				iIntStatus = HDA_READ_REG_4(&m_pHda_Core->mem, REG_INTSTS);
				
				if(!FLAG_MATCH(iIntStatus, REG_INTSTS_GIS)) 
				{
					InterruptDone(m_IntrAudio);
					break;
				}
								
				// Was this a controller interrupt? 
				if(FLAG_MATCH(iIntStatus, REG_INTSTS_CIS)) 
				{
					ucRirbStatus = HDA_READ_REG_1(&m_pHda_Core->mem, REG_RIRBSTS);
					// Get as many responses that we can 
					while(FLAG_MATCH(ucRirbStatus, REG_RIRBSTS_RINTFL)) 
					{
						HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_RIRBSTS, REG_RIRBSTS_RINTFL);
						FlushRirb();
						ucRirbStatus = HDA_READ_REG_1(&m_pHda_Core->mem, REG_RIRBSTS);
					}
					
					HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_INTSTS, REG_INTSTS_CIS);
				} else {
					for(UINT i = 0; i < m_maxStream; i++) 
					{
			               	if((iIntStatus & m_dmachannel[i].ucIntMask) && m_dmachannel[i].fAvailable) 
						{
							if(m_dmachannel[i].pfIntHandler)
								(*m_dmachannel[i].pfIntHandler) (m_dmachannel[i].pvIntContext);
							else 
								DEBUGMSG(1,(TEXT("m_channel[%d].pfIntHandler == NULL!\r\n"),i));
						}
					}
				}

				InterruptDone(m_IntrAudio);
				break;
			}
			
			case WAIT_ABANDONED:
				DEBUGMSG(ZONE_ERROR, (TEXT("HDA IST: WAIT_ABANDONED.. \r\n")));
				break;
			case WAIT_TIMEOUT:
				DEBUGMSG(ZONE_ERROR, (TEXT("HDA IST: WAIT_TIMEOUT.. \r\n")));
				break;
			case WAIT_FAILED:
				DEBUGMSG(ZONE_ERROR, (TEXT("HDA IST: WAIT_FAILED.. \r\n")));
				break;
			default:
				// bKeepLooping = FALSE;
				break;
		}
	}
	DEBUGMSG(ZONE_INFO, (TEXT("HDA IST: Exiting.. \r\n")));

	InterruptDone(m_IntrAudio);
	InterruptDisable (m_IntrAudio);
}

void HDA::UpdateBDL( ULONG ulChannelIndex, WORD UNALIGNED * pBuffer, ULONG BufferLen)
{
	DWORD *BDLPtr;          // Pointer to the current entry into the Buffer Descriptor List
	BDLPtr = ((DWORD*)m_dmachannel[ulChannelIndex].gBDLHeadPtr)  + (m_dmachannel[ulChannelIndex].gLastValidIndex * 4); 
	*BDLPtr = (DWORD) pBuffer & 0xFFFFFFFF;
	*(BDLPtr+2) = (BufferLen);
	*(BDLPtr+3) = 0x1; //enable the IOC

	DEBUGMSG(ZONE_HDA, (TEXT("HDA: Adding to BDL (@ %x) - 0x%08X 0x%08X.\r\n"),BDLPtr, *BDLPtr, *(BDLPtr+2)));

	return;
}

void HDA::SetFrequency(ULONG ulChannelIndex, ULONG ulSampleRate, WORD wFormatTag) 
{
	Channel_desc *pChannel;
	UINT iFormat=0;

	pChannel = &m_pHda_Core->channel[ulChannelIndex];
       switch ( m_dmachannel[ulChannelIndex].ulSampleSize){
		case 16:
                     iFormat |= AUDIO_FORMAT_SMP16;
			break;
		case 20:
                     iFormat |= AUDIO_FORMAT_SMP20;
			break;
		case 24:
                     iFormat |= AUDIO_FORMAT_SMP24;
			break;
		case 32:
                     iFormat |= AUDIO_FORMAT_SMP32;
			break;
		case 8:
		default:
			iFormat =0;

       }

	if(m_dmachannel[ulChannelIndex].ulChannels == 2)
		iFormat |= AUDIO_FORMAT_STEREO;

	pChannel->iFormat = iFormat;
	pChannel->iSampleRate = (UINT)ulSampleRate;	
}

// Inline functions
DWORD HDA::PCIConfig_Read(ULONG BusNumber, ULONG Device, ULONG Function, ULONG Offset)
{
    ULONG RetVal = FALSE;
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;
    
    HalGetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &RetVal, Offset, sizeof(RetVal));

    return RetVal;
}

void HDA::PCIConfig_Write(ULONG BusNumber, ULONG Device, ULONG Function, ULONG Offset, ULONG Value, ULONG Size)
{
    PCI_SLOT_NUMBER SlotNumber;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = Device;
    SlotNumber.u.bits.FunctionNumber = Function;

    HalSetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber.u.AsULONG, &Value, Offset, Size);
}

void HDA::WriteBusMasterUCHAR(PUCHAR reg, UCHAR val)
{
	if(m_fIOSpace)
	{
		WRITE_PORT_UCHAR(reg, val); 
	}
	else
	{
		WRITE_REGISTER_UCHAR(reg, val);
	}
}

void HDA::WriteBusMasterUSHORT(PUSHORT reg, USHORT val)
{
	if(m_fIOSpace)
	{
		WRITE_PORT_USHORT(reg, val); 
	}
	else
	{
		WRITE_REGISTER_USHORT(reg, val);
	}
}

void HDA::WriteBusMasterULONG(PULONG reg, ULONG val)
{
	if(m_fIOSpace)
	{
		WRITE_PORT_ULONG(reg, val); 
	}
	else
	{
		WRITE_REGISTER_ULONG(reg, val);
	}
}

UCHAR HDA::ReadBusMasterUCHAR(PUCHAR reg)
{
	UCHAR val;

	if(m_fIOSpace)
	{
		val = READ_PORT_UCHAR(reg); 
	}
	else
	{
		val = READ_REGISTER_UCHAR(reg);
	}

	return (val);
}

USHORT HDA::ReadBusMasterUSHORT(PUSHORT reg)
{
	USHORT val;

	if(m_fIOSpace)
	{
		val = READ_PORT_USHORT(reg); 
	}
	else
	{
		val = READ_REGISTER_USHORT(reg);
	}

	return (val);
}
	
ULONG HDA::ReadBusMasterULONG(PULONG reg)
{
	ULONG val;

	if(m_fIOSpace)
	{
		val = READ_PORT_ULONG(reg); 
	}
	else
	{
		val = READ_REGISTER_ULONG(reg);
	}

	return (val);
}

BOOL HDA::InitHDA()
{
	UINT i;
	
	if(GetControllerCapabilities() != TRUE)
		return FALSE;

	RestController();

	for(i = 0; i < HDA_MAX_CODEC; i++)
		m_pHda_Core->codecs[i] = NULL;

	m_pHda_Core->channel = (Channel_desc*) LocalAlloc(LMEM_ZEROINIT, (m_pHda_Core->iNumIss + m_pHda_Core->iNumOss + m_pHda_Core->iNumBss) * sizeof (Channel_desc));
	
	if(Alloc_DMA(&m_pHda_Core->Dma_Corb, m_pHda_Core->iCorbSize * sizeof(UINT)) != TRUE)
		return FALSE;
	if(Alloc_DMA(&m_pHda_Core->Dma_Rirb, m_pHda_Core->iRirbSize * sizeof(rirb_fmt)) != TRUE)
		return FALSE;

	InitCorb();
	InitRirb();
	
	EnterCriticalSection(&m_pHda_Core->cri_sec);
	
	StartCorb();
	StartRirb();

	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_WAKEEN, 0x0000);		//If PME is enabled in BIOS, we should disable WAKEEN to prevent unexpected interrupt.

	//Set Interrupt Control Register to enable interrupt
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_INTCTL, REG_INTCTL_CIE | REG_INTCTL_GIE);
	
	//Set to accept Unsolicited response
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_GCTL, HDA_READ_REG_4(&m_pHda_Core->mem, REG_GCTL) |REG_GCTL_UNSOL);

	DiscoverCodecs(0);
	
	if(m_pDevInfo == NULL)
	{
		DEBUGMSG(1,(TEXT("Codec not found!!\n")));	
		return FALSE;
	}
	
	//Parse Phase 
	ParseAFG();	
	ParseAudioCtl();
	VendorSpecificConfig();
	BuildTopology();
	
	//Configure Phase
	ConfigPinWidgetCtl();
	SetDefaultCtlVol();
	SetupPcmChannel(DIR_PLAY);
	SetupPcmChannel(DIR_REC);

	LeaveCriticalSection(&m_pHda_Core->cri_sec);

	return TRUE;
}


BOOL HDA::GetControllerCapabilities()
{
	USHORT usGCAP;
	UCHAR ucCorbSize, ucRirbSize;

	usGCAP = HDA_READ_REG_2(&m_pHda_Core->mem, REG_GCAP);
	m_pHda_Core->iNumIss = GET_SUPPORTED_ISS(usGCAP);
	m_pHda_Core->iNumOss = GET_SUPPORTED_OSS(usGCAP);
	m_pHda_Core->iNumBss = GET_SUPPORTED_BSS(usGCAP);

	ucCorbSize = HDA_READ_REG_1(&m_pHda_Core->mem, REG_CORBSIZE);
	
	switch(ucCorbSize & REG_CORBSIZE_CAP_MASK)
	{
		case REG_CORBSIZE_CAP_256:
					m_pHda_Core->iCorbSize = 256;
					break;
		case REG_CORBSIZE_CAP_16:
					m_pHda_Core->iCorbSize = 16;
					break;		
		case REG_CORBSIZE_CAP_2:
					m_pHda_Core->iCorbSize = 2;
					break;				
		default : 		
					DEBUGMSG(1,(TEXT("hda_get_cap: Invalid corb size (%x)\n"), m_pHda_Core->iCorbSize ));
					return FALSE;
	}
	
	ucRirbSize = HDA_READ_REG_1(&m_pHda_Core->mem, REG_RIRBSIZE);
	switch(ucRirbSize & REG_RIRBSIZE_CAP_MASK)
	{
		case REG_RIRBSIZE_CAP_256:
					m_pHda_Core->iRirbSize = 256;
					break;
		case REG_RIRBSIZE_CAP_16:
					m_pHda_Core->iRirbSize = 16;
					break;
		case REG_RIRBSIZE_CAP_2:
					m_pHda_Core->iRirbSize = 2;
					break;
		default:
					DEBUGMSG(1,(TEXT("hda_get_cap: Invalid rirb size (%x)\n"), m_pHda_Core->iRirbSize ));
					return FALSE;
	}
	
	return TRUE;
}

BOOL HDA::Alloc_DMA(dma_desc *pDma, UINT size)
{
	UINT new_size;
	DMA_ADAPTER_OBJECT AdapterObject;

	//setup the DMA object.
	AdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
	AdapterObject.InterfaceType = (INTERFACE_TYPE) m_dwInterfaceType;
	AdapterObject.BusNumber = m_dwBusNumber;

	new_size = align_size(size, HDA_DMA_ALIGNMENT);

	pDma->iDmaSize = new_size;
	if(( pDma->DmaVirtualAddr = HalAllocateCommonBuffer(&AdapterObject, pDma->iDmaSize, &pDma->DmaPhysicalAddr, FALSE)) != NULL)
	{

	}
	else
	{
		DEBUGMSG(1,(TEXT("HDA: unable to reserve for CORB buffer\r\n")));
		return FALSE;
	}

	HDA_BOOTVERBOSE(
		DEBUGMSG(1,(TEXT("hdac_dma_alloc: size=%x -> new_size=%ju\n"), size, new_size));		
	);

	return TRUE;
}



BOOL HDA::RestController()
{
	UINT iGctl;
	UINT iTimeout, i;

	 // Stop Streams DMA engine
	for(i = 0; i < m_pHda_Core->iNumIss; i++)
		HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_ISDCTL(i), 0x0);
	for(i = 0; i < m_pHda_Core->iNumOss; i++)
		HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_OSDCTL(i, m_pHda_Core->iNumIss), 0x0);
	for(i = 0; i < m_pHda_Core->iNumBss; i++)
		HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_BSDCTL(i, m_pHda_Core->iNumIss, m_pHda_Core->iNumOss), 0x0);

	// Stop Control DMA engines.
	HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_CORBCTL, 0x0);
	HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_RIRBCTL, 0x0);

	// Reset DMA position buffer.
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_DPIBLBASE, 0x0);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_DPIBUBASE, 0x0);

	// Reset the controller. 
	iGctl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_GCTL);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_GCTL, iGctl & ~REG_GCTL_CRST);
	iTimeout = 10000;
	do {
		iGctl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_GCTL);
		if(!(iGctl & REG_GCTL_CRST))
			break;
		Sleep(10);
	} while	(--iTimeout);
	if(iGctl & REG_GCTL_CRST) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("RestController: Unable to reset controller\n")));
		return FALSE;
	}
	Sleep(100);
	iGctl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_GCTL);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_GCTL, iGctl | REG_GCTL_CRST);
	iTimeout = 10000;
	do {
		iGctl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_GCTL);
		if(iGctl & REG_GCTL_CRST)
			break;
		Sleep(10);
	} while(--iTimeout);
	if(!(iGctl & REG_GCTL_CRST)) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("RestController: Device stuck in reset\n")));
		return FALSE;
	}
	//Sleep(1000);
	return TRUE;
}


void HDA::InitCorb()
{
	UCHAR ucCorbSize;
	PHYSICAL_ADDRESS CorbPhysicalAddr;

	// Setup the CORB size. 
	switch(m_pHda_Core->iCorbSize) 
	{
		case 256:
			ucCorbSize = REG_CORBSIZE_256;
			break;
		case 16:
			ucCorbSize = REG_CORBSIZE_16;
			break;
		case 2:
			ucCorbSize = REG_CORBSIZE_2;
			break;
		default:
			DEBUGMSG(ZONE_INFO,(TEXT("InitCorb: Invalid CORB size (%x)\n"), m_pHda_Core->iCorbSize));
	}
	if(ucCorbSize) HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_CORBSIZE, ucCorbSize);

	// Setup the CORB Address in the hdac 
	CorbPhysicalAddr = m_pHda_Core->Dma_Corb.DmaPhysicalAddr;
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_CORBLBASE, (UINT) CorbPhysicalAddr.LowPart );
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_CORBUBASE, (UINT)(CorbPhysicalAddr.HighPart));

	// Set the WP and RP 
	m_pHda_Core->iCorb_wp = 0;
	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_CORBWP, m_pHda_Core->iCorb_wp);
	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_CORBRP, REG_CORBRP_RST);

	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_CORBRP, 0x0);
}

void HDA::InitRirb()
{
	UCHAR ucRirbSize;
	PHYSICAL_ADDRESS RirbPhysicalAddr;

	// Setup the RIRB size. 
	switch(m_pHda_Core->iRirbSize) 
	{
		case 256:
			ucRirbSize = REG_RIRBSIZE_256;
			break;
		case 16:
			ucRirbSize = REG_RIRBSIZE_16;
			break;
		case 2:
			ucRirbSize = REG_RIRBSIZE_2;
			break;
		default:
			DEBUGMSG(ZONE_INFO,(TEXT("InitRirb: Invalid RIRB size (%x)\n"), m_pHda_Core->iRirbSize));
	}
	if(ucRirbSize) HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_RIRBSIZE, ucRirbSize);

	// Setup the RIRB Address in the hdac 
	RirbPhysicalAddr = m_pHda_Core->Dma_Rirb.DmaPhysicalAddr;
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_RIRBLBASE, (UINT)RirbPhysicalAddr.LowPart);
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_RIRBUBASE, (UINT)RirbPhysicalAddr.HighPart);

	// Setup the WP and RP 
	m_pHda_Core->iRirb_rp = 0;
	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_RIRBWP, REG_RIRBWP_RST);

	// Setup the interrupt threshold 
	HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_RINTCNT, m_pHda_Core->iRirbSize / 2);

	// Enable Overrun and response received reporting 
	HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_RIRBCTL, REG_RIRBCTL_RINTCTL);
}

void HDA::StartCorb()
{
	UINT iCorbCtl;

	iCorbCtl = HDA_READ_REG_1(&m_pHda_Core->mem, REG_CORBCTL);
	iCorbCtl |= REG_CORBCTL_CORBRUN;
	HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_CORBCTL, iCorbCtl);
}

void HDA::StartRirb()
{
	UINT iRirbCtl;

	iRirbCtl = HDA_READ_REG_1(&m_pHda_Core->mem, REG_RIRBCTL);
	iRirbCtl |= REG_RIRBCTL_DMAEN;
	HDA_WRITE_REG_1(&m_pHda_Core->mem, REG_RIRBCTL, iRirbCtl);
}

void HDA::DiscoverCodecs(UINT num)
{
	Codec_desc *pCodec;
	UINT i;
	USHORT usStatests;

	if(num >= HDA_MAX_CODEC)
		num = HDA_MAX_CODEC - 1;
	else if(num < 0)
		num = 0;

	usStatests = HDA_READ_REG_2(&m_pHda_Core->mem, REG_STATESTS);
	for(i = num; i < HDA_MAX_CODEC; i++) 
	{
		if(CHECK_SDIWAKE(usStatests, i)) 
		{
			// Codec found
			pCodec = (Codec_desc *) LocalAlloc (LMEM_ZEROINIT, sizeof (Codec_desc));
			if(pCodec == NULL) 
			{
				DEBUGMSG(ZONE_ERROR,(TEXT("Unable to allocate memory for codec\n")));
				continue;
			}
			pCodec->pCmd_List = NULL;
			pCodec->iResponRcved = 0;
			pCodec->iVerbsSent = 0;
			pCodec->cad = i;
			m_pHda_Core->codecs[i] = pCodec;
			if(CheckCodecFG(pCodec) != 0)
				break;
		}
	}
}

UINT HDA::CheckCodecFG(Codec_desc *pCodec)
{
	UINT vendorid, revisionid, subnode;
	UINT StartNode;
	UINT EndNode;
	UINT i;
	type_nid cad = pCodec->cad;

	vendorid = CodecCommand(cad, CMD_12BIT(cad, 0x0, VERB_GET_PARAMETER, CODEC_PARAM_VENDOR_ID));
	revisionid = CodecCommand(cad, CMD_12BIT(cad, 0x0, VERB_GET_PARAMETER, CODEC_PARAM_REVISION_ID));
    subnode = CodecCommand(cad, CMD_12BIT(cad, 0x0, VERB_GET_PARAMETER, CODEC_PARAM_SUB_NODE_COUNT));

    //!!!
    // NM10 platform provides 2 HDA codecs, we should check supported codec list
    // At worst, skip node scanning gracefully
    switch (vendorid) {
        
        case HDA_CODEC_AD1986A:
        case HDA_CODEC_ALC268:
        case HDA_CODEC_ALC885:
        case HDA_CODEC_ALC888:
		case HDA_CODEC_ALC262:
        case HDA_CODEC_ALC280:
	        StartNode = GET_START_NODE(subnode);
	        EndNode = StartNode + GET_TOTAL_NODE_COUNT(subnode);
            break;

        default:
            StartNode = EndNode = 0x00;
    }

	HDA_BOOTVERBOSE(
		DEBUGMSG(ZONE_INFO,(TEXT("HDA_DEBUG: \tStartNode=%d EndNode=%d\n"), StartNode, EndNode));
	);

	
	for(i = StartNode; i < EndNode; i++) 
	{
		m_pDevInfo = NULL;
		GetAudioFuncGroup(pCodec, i);
		if(m_pDevInfo != NULL) 
		{
			// We only accept Audio Function Group.
			m_pDevInfo->sVendorID = (vendorid & CODEC_PARAM_VENDOR_ID_MASK) >> CODEC_PARAM_VENDOR_ID_SHIFT;
			m_pDevInfo->sDeviceID = vendorid & CODEC_PARAM_DEVICE_ID_MASK;
			m_pDevInfo->cRevisionID = (revisionid & CODEC_PARAM_REVISION_ID_MASK) >> CODEC_PARAM_REVISION_ID_SHIFT;
			m_pDevInfo->cSteppingID = revisionid & CODEC_PARAM_STEPPING_ID_MASK;
			DEBUGMSG(ZONE_INFO,(TEXT("HDA_DEBUG: \tFound AFG nid=%d [StartNode=%d EndNode=%d]\n"),m_pDevInfo->nid, StartNode, EndNode));
			return (1);
		}
	}

	HDA_BOOTVERBOSE(
		DEBUGMSG(ZONE_INFO,(TEXT("CheckCodecFG: completed!!\n")));
	);
	return (0);
}


UINT HDA::CodecCommand(type_nid cad, UINT verb)
{
	cmd_list Cmd_List;
	UINT iResponse;
	Codec_desc *pCodec;
	UINT iCorbRp;
	UINT *pCorb;
	UINT iRetry;

	iRetry = 1;
	iResponse = HDA_INVALID;
	EnterCriticalSection(&m_pHda_Core->cri_sec);	

	Cmd_List.iNumCmds = 1;
	Cmd_List.pVerbs = &verb;
	Cmd_List.pRespons = &iResponse;

	pCodec = m_pHda_Core->codecs[cad];
	pCodec->pCmd_List = &Cmd_List;
	pCodec->iResponRcved = 0;
	pCodec->iVerbsSent = 0;
	pCorb = (UINT *)m_pHda_Core->Dma_Corb.DmaVirtualAddr;

	do {
		if(pCodec->iVerbsSent != Cmd_List.iNumCmds) 
		{
			// Queue the verbs
			iCorbRp = HDA_READ_REG_2(&m_pHda_Core->mem, REG_CORBRP);

			while(pCodec->iVerbsSent != Cmd_List.iNumCmds &&
			    ((m_pHda_Core->iCorb_wp + 1) % m_pHda_Core->iCorbSize) != iCorbRp) 
			{
				m_pHda_Core->iCorb_wp++;
				m_pHda_Core->iCorb_wp %= m_pHda_Core->iCorbSize;
				pCorb[m_pHda_Core->iCorb_wp] = Cmd_List.pVerbs[pCodec->iVerbsSent++];
			}

			//Send the verbs to the codecs 
			HDA_WRITE_REG_2(&m_pHda_Core->mem, REG_CORBWP, m_pHda_Core->iCorb_wp);
		}
		Sleep(10);
	} while((pCodec->iVerbsSent != Cmd_List.iNumCmds ||
	    pCodec->iResponRcved != Cmd_List.iNumCmds) && --iRetry);

	if(iRetry == 0)
		DEBUGMSG(ZONE_INFO,(TEXT("%S: TIMEOUT numcmd=%d, sent=%d, received=%d\n"),
			"hdac_command_send_internal", Cmd_List.iNumCmds, pCodec->iVerbsSent, pCodec->iResponRcved));		

	pCodec->pCmd_List = NULL;
	pCodec->iResponRcved = 0;
	pCodec->iVerbsSent = 0;
	
	// Flush the unsolicited queue
	if(m_pHda_Core->iUnsolicQ_st == UNSOL_Q_READY) 
	{
		m_pHda_Core->iUnsolicQ_st = UNSOL_Q_BUSY;
		while(m_pHda_Core->iUnsolicQ_rp != m_pHda_Core->iUnsolicQ_wp) 
		{
			cad = m_pHda_Core->iUnsolicQ[m_pHda_Core->iUnsolicQ_rp] >> 16;
			m_pHda_Core->iUnsolicQ_rp %= MAX_UNSOLIC_Q;
		}
		m_pHda_Core->iUnsolicQ_st = UNSOL_Q_READY;
	}

	LeaveCriticalSection(&m_pHda_Core->cri_sec);
	
	return (iResponse);
}



UINT HDA::FlushRirb()
{
	type_nid cad;
	UINT iResponse;
	UCHAR cRirbWp;
	Codec_desc *pCodec;
	cmd_list *pCmd_List;
	rirb_fmt *pRirbBaseAddr, *pRirb;
	UINT iRet;

	pRirbBaseAddr = (rirb_fmt *)m_pHda_Core->Dma_Rirb.DmaVirtualAddr;
	cRirbWp = HDA_READ_REG_1(&m_pHda_Core->mem, REG_RIRBWP);

	iRet = 0;

	while(m_pHda_Core->iRirb_rp != cRirbWp) 
	{
		m_pHda_Core->iRirb_rp++;
		m_pHda_Core->iRirb_rp %= m_pHda_Core->iRirbSize;
		pRirb = &pRirbBaseAddr[m_pHda_Core->iRirb_rp];
		cad = pRirb->iResp_Ex & RIRB_RESPONSE_EX_CAD_MASK;
		if(cad < 0 || cad >= HDA_MAX_CODEC ||
		    m_pHda_Core->codecs[cad] == NULL)
			continue;
		iResponse = pRirb->iResponse;
		pCodec = m_pHda_Core->codecs[cad];
		pCmd_List = pCodec->pCmd_List;
		if(pRirb->iResp_Ex & RIRB_RESPONSE_EX_UNSOLICITED) 
		{
			m_pHda_Core->iUnsolicQ[m_pHda_Core->iUnsolicQ_wp++] = (cad << 16) |
			    ((iResponse >> 26) & 0xffff);
			m_pHda_Core->iUnsolicQ_wp %= MAX_UNSOLIC_Q;
		} else if(pCmd_List != NULL && pCmd_List->iNumCmds > 0 &&
		    pCodec->iResponRcved < pCmd_List->iNumCmds) {
		    
			pCmd_List->pRespons[pCodec->iResponRcved++] = iResponse;
		}
		iRet++;
	}

	return (iRet);
}
void HDA::InterruptRoutine(ULONG ulChannelIndex)
{

	UINT iIntStatus;
	Channel_desc *pChannel;

	pChannel = &m_pHda_Core->channel[ulChannelIndex];	
	iIntStatus = HDA_READ_REG_4(&m_pHda_Core->mem, REG_INTSTS);

	if(iIntStatus & REG_INTSTS_SIS_MASK) 
	{
		if(!(pChannel->iStatus & HDA_CHN_RUNNING))
			return ;
		
		HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDSTS,
		    REG_SDSTS_DESE | REG_SDSTS_FIFOE | REG_SDSTS_BCIS );		
	}
}


void HDA::GetAudioFuncGroup(Codec_desc *pCodec, type_nid nid)
{
	UINT iFuncGroupType;
	type_nid cad = pCodec->cad;

	iFuncGroupType = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_FCT_GRP_TYPE));
	iFuncGroupType &= CODEC_PARAM_FCT_GRP_TYPE_NODE_TYPE_MASK;

	// We only support Audio Function Group now
	if(iFuncGroupType != CODEC_PARAM_FUNC_GROUP_TYPE_AUDIO)
	{
		DEBUGMSG(ZONE_INFO,(TEXT("GetAudioFuncGroup: This is not Audio Function Group!!!!!!!!\n")));
		return ;
	}

	m_pDevInfo = (DEVINFO *)LocalAlloc(LMEM_ZEROINIT, sizeof(DEVINFO));
	
	if(m_pDevInfo == NULL) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("GetAudioFuncGroup: Unable to allocate memory for m_pDevInfo!!!!!!!!\n")));
		return ;
	}

	m_pDevInfo->nid = nid;
	m_pDevInfo->cFuncGroupType = iFuncGroupType;
	m_pDevInfo->pCodec = pCodec;

	return ;
}

void HDA::ParseAFG()
{
	WIDGET *pWidget;
	UINT iValue;
	UINT i;
	type_nid cad, nid;

	cad = m_pDevInfo->pCodec->cad;
	nid = m_pDevInfo->nid;

	CodecCommand(cad, CMD_12BIT(cad, nid, VERB_SET_POWER_STATE, CODEC_CMD_POWER_STATE_D0));

	Sleep(100);

	iValue = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_SUB_NODE_COUNT));

	m_pDevInfo->iNodeCnt = GET_TOTAL_NODE_COUNT(iValue);
	m_pDevInfo->StartNode = GET_START_NODE(iValue);
	m_pDevInfo->EndNode = m_pDevInfo->StartNode + m_pDevInfo->iNodeCnt;

	DEBUGMSG(1,(TEXT("       Vendor: 0x%08x\n"),m_pDevInfo->sVendorID));
	DEBUGMSG(1,(TEXT("       Device: 0x%08x\n"), m_pDevInfo->sDeviceID));
	DEBUGMSG(1,(TEXT("PCI Subvendor: 0x%08x\n"), m_pHda_Core->iPCI_SubVendor));
	DEBUGMSG(1,(TEXT("        Nodes: start=%d EndNode=%d total=%d\n"), m_pDevInfo->StartNode, m_pDevInfo->EndNode, m_pDevInfo->iNodeCnt));
	DEBUGMSG(1,(TEXT("      Streams: ISS=%d OSS=%d BSS=%d\n"), m_pHda_Core->iNumIss, m_pHda_Core->iNumOss, m_pHda_Core->iNumBss));

	m_pDevInfo->AudioFun.iSupportedFormats = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_SUPP_STREAM_FORMATS));
	m_pDevInfo->AudioFun.iSuppPcmSizeRate = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_SUPP_PCM_SIZE_RATE));
	m_pDevInfo->AudioFun.iOutAmpCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_OUTPUT_AMP_CAP));
	m_pDevInfo->AudioFun.iInAmpCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_INPUT_AMP_CAP));

	if(m_pDevInfo->iNodeCnt > 0)
		m_pDevInfo->pWidget = (WIDGET *)LocalAlloc(LMEM_ZEROINIT, sizeof(*(m_pDevInfo->pWidget)) * m_pDevInfo->iNodeCnt);
	else
		m_pDevInfo->pWidget = NULL;

	if(m_pDevInfo->pWidget == NULL) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("unable to allocate widgets!\n")));
		m_pDevInfo->EndNode = m_pDevInfo->StartNode;
		m_pDevInfo->iNodeCnt = 0;
		return;
	}

	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget)
		{
			pWidget->nid = i;
			pWidget->iWgtEnable = 1;
			pWidget->iCurrConnIdx = -1;
			pWidget->iPathType = 0;
			ParseWidget(pWidget);
		}
	}
}

WIDGET * HDA::GetWidget(type_nid nid)
{
	if(m_pDevInfo->pWidget == NULL || nid < m_pDevInfo->StartNode || nid >= m_pDevInfo->EndNode)
		return (NULL);
	return (&m_pDevInfo->pWidget[nid - m_pDevInfo->StartNode]);
}

void HDA::ParseWidget(WIDGET *pWidget)
{
	UINT iWidgetCap, iConvCap;
	type_nid cad = m_pDevInfo->pCodec->cad;
	type_nid nid = pWidget->nid;

	iWidgetCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_WIDGET_CAP));
	pWidget->Param.iWidgetCap = iWidgetCap;
	pWidget->iWgtType = GET_WIDGET_TYPE(iWidgetCap);

	if(iWidgetCap & CODEC_PARAM_WIDGET_CAP_POWER_CTRL_MASK) 	//Support power control
	{
		CodecCommand(cad, CMD_12BIT(cad, nid, VERB_SET_POWER_STATE, CODEC_CMD_POWER_STATE_D0));
		Sleep(500);
	}

	ParseWidgetConn(pWidget);

	if(FLAG_MATCH(iWidgetCap, CODEC_PARAM_WIDGET_CAP_OUT_AMP_MASK)) 
	{
		if(FLAG_MATCH(iWidgetCap, CODEC_PARAM_WIDGET_CAP_AMP_OVR_MASK))
			pWidget->Param.iOutAmpCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_OUTPUT_AMP_CAP));
		else
			pWidget->Param.iOutAmpCap = m_pDevInfo->AudioFun.iOutAmpCap;
	} else
		pWidget->Param.iOutAmpCap = 0;

	if(FLAG_MATCH(iWidgetCap, CODEC_PARAM_WIDGET_CAP_IN_AMP_MASK)) 
	{
		if(FLAG_MATCH(iWidgetCap, CODEC_PARAM_WIDGET_CAP_AMP_OVR_MASK))
			pWidget->Param.iInAmpCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_INPUT_AMP_CAP));
		else
			pWidget->Param.iInAmpCap = m_pDevInfo->AudioFun.iInAmpCap;
	} else
		pWidget->Param.iInAmpCap = 0;

	if(pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_OUTPUT ||
	    pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_INPUT) 
	{
		if(FLAG_MATCH(iWidgetCap, CODEC_PARAM_WIDGET_CAP_FORMAT_OVR_MASK)) 
		{
			iConvCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_SUPP_STREAM_FORMATS));
			pWidget->Param.iSupportedFormats = (iConvCap != 0) ? iConvCap : m_pDevInfo->AudioFun.iSupportedFormats;
			iConvCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_SUPP_PCM_SIZE_RATE));
			pWidget->Param.iSuppPcmSizeRate = (iConvCap != 0) ? iConvCap : m_pDevInfo->AudioFun.iSuppPcmSizeRate;
		} else {
			pWidget->Param.iSupportedFormats = m_pDevInfo->AudioFun.iSupportedFormats;
			pWidget->Param.iSuppPcmSizeRate = m_pDevInfo->AudioFun.iSuppPcmSizeRate;
		}
	} else {
		pWidget->Param.iSupportedFormats = 0;
		pWidget->Param.iSuppPcmSizeRate = 0;
	}

	if(pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_PIN_COMPLEX)
		ParsePinWidget(pWidget);
	
}


void HDA::ParseWidgetConn(WIDGET *pWidget)
{
	UINT iValue;
	UINT i, j;
	UINT iMaxConEntrys, iConListLen, iFormLen;
	type_nid cad = m_pDevInfo->pCodec->cad;
	type_nid nid = pWidget->nid;
	type_nid ChildNid, AddChildNid, PreChildNid;

	pWidget->iNumConns = 0;

	iValue = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_CONN_LIST_LENGTH));
	iConListLen = iValue & CODEC_PARAM_CONN_LIST_LENGTH_LIST_LENGTH_MASK;

	if(iConListLen < 1)
		return;

	iFormLen = FLAG_MATCH(iValue, CODEC_PARAM_CONN_LIST_LENGTH_LONG_FORM_MASK) ? 2 : 4;
	iMaxConEntrys = (sizeof(pWidget->ConListEntry) / sizeof(pWidget->ConListEntry[0])) - 1;
	PreChildNid = 0;

	for(i = 0; i < iConListLen; i += iFormLen) 
	{
		iValue = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_CONN_LIST_ENTRY, i));
		for(j = 0; j < iFormLen; j++) 
		{
			ChildNid = (iValue >> (j * (32 / iFormLen))) & ((iFormLen == 2)? 0xFFFF : 0xFF);
			if(ChildNid == 0) 
			{
				if(pWidget->iNumConns < iConListLen)
					DEBUGMSG(ZONE_INFO,(TEXT("ParseWidgetConn: connection list entry nid = 0\n")));					
				else
					return ;
			}
			if(ChildNid < m_pDevInfo->StartNode || ChildNid >= m_pDevInfo->EndNode) 
				HDA_BOOTVERBOSE(DEBUGMSG(ZONE_INFO,(TEXT("ParseWidgetConn: connection list entry nid is less than StartNode of greater than EndNode\n"))););

			if(((iValue >> (j * (32 / iFormLen))) & (1 << ((32 / iFormLen) - 1))) == 0)
				AddChildNid = ChildNid;
			else if(PreChildNid == 0 || PreChildNid >= ChildNid) {
				DEBUGMSG(ZONE_INFO,(TEXT("%ParseWidgetConn: WARNING: Invalid child range nid=%d index=%d j=%d iFormLen=%d PreChildNid=%d ChildNid=%d iValue=0x%08x\n"), nid, i, j, iFormLen, PreChildNid,ChildNid, iValue));				
				AddChildNid = ChildNid;
			} else
				AddChildNid = PreChildNid + 1;
			while(AddChildNid <= ChildNid) 
			{
				if(pWidget->iNumConns > iMaxConEntrys) 
				{
					DEBUGMSG(ZONE_INFO,(TEXT("%S: nid=0x%x: Adding %d: Max connection reached! iMaxConEntrys=%d\n"),"ParseWidgetConn", nid, AddChildNid, iMaxConEntrys + 1));		
					return ;
				}
				pWidget->ConListEntry[pWidget->iNumConns++] = AddChildNid++;
			}
			PreChildNid = ChildNid;
		}
	}
	return;
}


void HDA::ParsePinWidget(WIDGET *pWidget)
{
	UINT iPinCap;
	type_nid cad = m_pDevInfo->pCodec->cad;
	type_nid nid = pWidget->nid;

	pWidget->Pin.iPinConfig = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_CONFIGURATION_DEFAULT, 0x0)); 
	iPinCap = CodecCommand(cad, CMD_12BIT(cad, nid, VERB_GET_PARAMETER, CODEC_PARAM_PIN_CAP));
	pWidget->Pin.iPinCap = iPinCap;

	pWidget->Pin.iPinCtrl = 0;
	
	if(FLAG_MATCH(iPinCap, CODEC_PARAM_PIN_CAP_HEADPHONE_CAP_MASK))
		pWidget->Pin.iPinCtrl |= CODEC_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE;
	if(FLAG_MATCH(iPinCap, CODEC_PARAM_PIN_CAP_OUTPUT_CAP_MASK))
		pWidget->Pin.iPinCtrl |= CODEC_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
	if(FLAG_MATCH(iPinCap,CODEC_PARAM_PIN_CAP_INPUT_CAP_MASK))
		pWidget->Pin.iPinCtrl |= CODEC_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE;
}


void HDA::ParseAudioCtl()
{

	Audio_Ctl *pCtls;
	WIDGET *pWidget, *pChildWidget;
	UINT i, j, iCount, iMaxCtlCnt, iOutAmpCap, iInAmpCap;
	UINT iStep0Db, iNumSteps;

	iMaxCtlCnt = 0;
	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		if(pWidget->Param.iOutAmpCap != 0)
			iMaxCtlCnt++;
		if(pWidget->Param.iInAmpCap != 0) 
		{
			switch(pWidget->iWgtType) 
			{
				case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
				case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER:
					for(j = 0; j < pWidget->iNumConns; j++) 
					{
						pChildWidget = GetWidget(pWidget->ConListEntry[j]);
						if(pChildWidget == NULL || pChildWidget->iWgtEnable == 0)
							continue;
						iMaxCtlCnt++;
					}
					break;
				default:
					iMaxCtlCnt++;
					break;
			}
		}
	}
	
	m_pDevInfo->AudioFun.iCtlCnt = iMaxCtlCnt;

	if(iMaxCtlCnt < 1)
		return;
	
	pCtls = (Audio_Ctl *)LocalAlloc(LMEM_ZEROINIT, sizeof(*pCtls) * iMaxCtlCnt);

	if(pCtls == NULL) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("unable to allocate pCtls!\n")));
		m_pDevInfo->AudioFun.iCtlCnt = 0;
		return;
	}

	iCount = 0;
	for(i = m_pDevInfo->StartNode; iCount < iMaxCtlCnt && i < m_pDevInfo->EndNode; i++) 
	{
		if(iCount >= iMaxCtlCnt) 
		{
			DEBUGMSG(ZONE_INFO,(TEXT("Ctl overflow!\n")));
			break;
		}
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		iOutAmpCap = pWidget->Param.iOutAmpCap;
		iInAmpCap = pWidget->Param.iInAmpCap;
		if(iOutAmpCap != 0) 
		{
			iStep0Db = GET_AMP_0DB_STEP(iOutAmpCap);
			iNumSteps = GET_AMP_NUM_STEPS(iOutAmpCap);
			pCtls[iCount].iCtlEnable = 1;
			pCtls[iCount].pWidget = pWidget;
			pCtls[iCount].iStep0Db = iStep0Db;
			pCtls[iCount].iLeftVol = iStep0Db;
			pCtls[iCount].iRightVol = iStep0Db;
			pCtls[iCount].iNumSteps = iNumSteps;
			pCtls[iCount].iMuted = GET_AMP_MUTE(iOutAmpCap);
			pCtls[iCount++].iDirection = HDA_CTL_OUT;
		}

		if(iInAmpCap != 0) 
		{
			iStep0Db = GET_AMP_0DB_STEP(iInAmpCap);
			iNumSteps = GET_AMP_NUM_STEPS(iInAmpCap);
			switch(pWidget->iWgtType) 
			{
				case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
				case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER:
					for(j = 0; j < pWidget->iNumConns; j++) 
					{
						if(iCount >= iMaxCtlCnt) 
						{
							DEBUGMSG(1,(TEXT("Ctl overflow 2!\n")));
							break;
						}
						pChildWidget = GetWidget(pWidget->ConListEntry[j]);
						if(pChildWidget == NULL || pChildWidget->iWgtEnable == 0)
							continue;
						pCtls[iCount].iCtlEnable = 1;
						pCtls[iCount].pWidget = pWidget;
						pCtls[iCount].iIndex = j;
						pCtls[iCount].iStep0Db = iStep0Db;
						pCtls[iCount].iLeftVol = iStep0Db;
						pCtls[iCount].iRightVol = iStep0Db;
						pCtls[iCount].iNumSteps = iNumSteps;
						pCtls[iCount].iMuted = GET_AMP_MUTE(iInAmpCap);
						pCtls[iCount++].iDirection = HDA_CTL_IN;
					}
					break;
				default:
					if(iCount >= iMaxCtlCnt) 
					{
						DEBUGMSG(1,(TEXT("Ctl overflow 3!\n")));
						break;
					}
					pCtls[iCount].iCtlEnable = 1;
					pCtls[iCount].pWidget = pWidget;
					pCtls[iCount].iStep0Db = iStep0Db;
					pCtls[iCount].iLeftVol = iStep0Db;
					pCtls[iCount].iRightVol = iStep0Db;
					pCtls[iCount].iNumSteps = iNumSteps;
					pCtls[iCount].iMuted = GET_AMP_MUTE(iInAmpCap);
					pCtls[iCount++].iDirection = HDA_CTL_IN;
					break;
			}
		}
	}

	m_pDevInfo->AudioFun.pCtl = pCtls;
}

Audio_Ctl * HDA::GetAudioCtl(UINT iIndex)
{
	if( m_pDevInfo->AudioFun.pCtl == NULL ||
	    m_pDevInfo->AudioFun.iCtlCnt < 1 ||
	    iIndex < 0 || iIndex >= m_pDevInfo->AudioFun.iCtlCnt)
		return (NULL);
	return (&m_pDevInfo->AudioFun.pCtl[iIndex]);
}

void HDA::BuildTopology()
{
	WIDGET *pWidget;
	UINT i;

	// Enumerate DAC path 
	EnumDacPath();

	// Enumerate ADC path 
	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		if(pWidget->iWgtType != CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_INPUT)
			continue;
		FindAdcPath(pWidget->nid, 0);
	}

	// Build rec selector
	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		if(!(pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_INPUT &&
		    pWidget->iPathType & HDA_ADC_PATH))
			continue;

		BuildRecSelector(pWidget->nid, 0);
	}	
}

UINT HDA::EnumDacPath()
{
	WIDGET *pWidget, *pChildWidget;
	UINT i, j, iDevType, iDacCount = 0;

	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		if(pWidget->iWgtType != CODEC_PARAM_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if(!FLAG_MATCH(pWidget->Pin.iPinCap, CODEC_PARAM_PIN_CAP_OUTPUT_CAP_MASK))
			continue;
		iDevType = pWidget->Pin.iPinConfig & HDA_DEFAULT_CONFIG_DEVICE_MASK;
		if(!(iDevType == HDA_DEFAULT_CONFIG_DEVICE_HP_OUT ||
		    iDevType == HDA_DEFAULT_CONFIG_DEVICE_SPEAKER ||
		    iDevType == HDA_DEFAULT_CONFIG_DEVICE_LINE_OUT))
			continue;
		for(j = 0; j < pWidget->iNumConns; j++) 
		{
			pChildWidget = GetWidget(pWidget->ConListEntry[j]);
			if(pChildWidget == NULL || pChildWidget->iWgtEnable == 0)
				continue;
			if(!(pChildWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER ||
			    pChildWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_SELECTOR))
				continue;
			if(FindDacPath(pChildWidget->nid, 0) != 0) 
			{
				if(pWidget->iCurrConnIdx == -1)
					pWidget->iCurrConnIdx = j;
					
				pWidget->iPathType |= HDA_DAC_PATH;
				iDacCount++;
			}
		}
	}

	return (iDacCount);
}



UINT HDA::FindDacPath(type_nid nid, UINT depth)
{
	WIDGET *pWidget;
	UINT i, iRet = 0;

	if(depth > HDA_PARSE_MAXDEPTH)
		return (0);
	pWidget = GetWidget(nid);
	if(pWidget == NULL || pWidget->iWgtEnable == 0)
		return (0);
	switch(pWidget->iWgtType) 
	{
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_OUTPUT:
			pWidget->iPathType |= HDA_DAC_PATH;
			iRet = 1;
			break;
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER:
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
			for(i = 0; i < pWidget->iNumConns; i++) 
			{
				if(FindDacPath(pWidget->ConListEntry[i], depth + 1) != 0) 
				{
					if(pWidget->iCurrConnIdx == -1)
						pWidget->iCurrConnIdx = i;
					iRet = 1;
					pWidget->iPathType |= HDA_DAC_PATH;
				}
			}
			break;
		default:
			break;
	}
	return (iRet);
}


UINT HDA::FindAdcPath(type_nid nid, UINT depth)
{
	WIDGET *pWidget;
	UINT i, iDevType, iRet = 0;

	if(depth > HDA_PARSE_MAXDEPTH)
		return (0);
	pWidget = GetWidget(nid);
	if(pWidget == NULL || pWidget->iWgtEnable == 0)
		return (0);
		
	switch(pWidget->iWgtType) 
	{
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_INPUT:
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
		case CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER:
			for(i = 0; i < pWidget->iNumConns; i++) 
			{
				if(FindAdcPath(pWidget->ConListEntry[i],
				    depth + 1) != 0) 
				{
					if(pWidget->iCurrConnIdx == -1)
						pWidget->iCurrConnIdx = i;
						
					pWidget->iPathType |= HDA_ADC_PATH;
					iRet = 1;
				}
			}
			break;
		case CODEC_PARAM_WIDGET_CAP_TYPE_PIN_COMPLEX:
			iDevType = pWidget->Pin.iPinConfig & HDA_DEFAULT_CONFIG_DEVICE_MASK;
			if(FLAG_MATCH(pWidget->Pin.iPinCap, CODEC_PARAM_PIN_CAP_INPUT_CAP_MASK) &&
			    (iDevType == HDA_DEFAULT_CONFIG_DEVICE_CD ||
			    iDevType == HDA_DEFAULT_CONFIG_DEVICE_LINE_IN ||
			    iDevType == HDA_DEFAULT_CONFIG_DEVICE_MIC_IN)) 
			{
				pWidget->iPathType |= HDA_ADC_PATH;
				iRet = 1;
			}
			break;
		default:
			break;
	}
	return (iRet);
}

UINT HDA::BuildRecSelector(type_nid nid, UINT depth)
{
	WIDGET *pWidget;
	UINT i;

	if(depth > HDA_PARSE_MAXDEPTH)
		return (0);

	pWidget = GetWidget(nid);
	if(pWidget == NULL || pWidget->iWgtEnable == 0)
		return (0);
	if((pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_PIN_COMPLEX)||(pWidget->iPathType & HDA_ADC_RECSEL))
		return (0);

	if(pWidget->iNumConns > 1)
			pWidget->iPathType |= HDA_ADC_RECSEL;

	for(i = 0; i < pWidget->iNumConns; i++) 
	{
		BuildRecSelector(pWidget->ConListEntry[i], depth+1);
	}

	return (1);
}


void HDA::ConfigPinWidgetCtl()
{
	WIDGET *pWidget;
	type_nid cad;
	UINT i;
	UINT iPinCap;

	cad = m_pDevInfo->pCodec->cad;

	for(i = 0; i < m_pDevInfo->iNodeCnt; i++) 
	{
		pWidget = &m_pDevInfo->pWidget[i];
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
			
		if(pWidget->iCurrConnIdx == -1)
			pWidget->iCurrConnIdx = 0;		//Use index 0 for Stream 1

		if(pWidget->iNumConns > 0)
			CodecCommand(m_pDevInfo->pCodec->cad, CMD_12BIT(m_pDevInfo->pCodec->cad, pWidget->nid, VERB_SET_CONN_SELECT_CONTROL, pWidget->iCurrConnIdx));

		if(pWidget->iWgtType == CODEC_PARAM_WIDGET_CAP_TYPE_PIN_COMPLEX) 
		{
			iPinCap = pWidget->Pin.iPinCap;

			if((pWidget->iPathType & (HDA_DAC_PATH | HDA_ADC_PATH)) == (HDA_DAC_PATH | HDA_ADC_PATH))
				DEBUGMSG (1, (TEXT("WARNING: node %d participate both for DAC/ADC!\n"), pWidget->nid));
			if(pWidget->iPathType & HDA_DAC_PATH) 
			{
				pWidget->Pin.iPinCtrl &= ~CODEC_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE;
				if((pWidget->Pin.iPinConfig & HDA_DEFAULT_CONFIG_DEVICE_MASK) != HDA_DEFAULT_CONFIG_DEVICE_HP_OUT)
					pWidget->Pin.iPinCtrl &= ~CODEC_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE;
					    
			} else if(pWidget->iPathType & HDA_ADC_PATH) {
				pWidget->Pin.iPinCtrl &= ~(CODEC_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE | CODEC_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE);				    
				pWidget->Pin.iPinCtrl |= CODEC_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_100;
			} else {
				pWidget->Pin.iPinCtrl &= ~(
				    CODEC_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
				    CODEC_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
				    CODEC_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE |
				    CODEC_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE_MASK);
			}
			
			CodecCommand(cad, CMD_12BIT(cad, pWidget->nid, VERB_SET_PIN_WIDGET_CTRL, pWidget->Pin.iPinCtrl));
		}
		Sleep(10);
	}	
}

void HDA::SetDefaultCtlVol()
{
	Audio_Ctl *pCtl;
	UINT i;
	
	for(i=0;(pCtl = GetAudioCtl(i)) != NULL; i++) 
	{
		if(pCtl->iCtlEnable == 0 || pCtl->pWidget == NULL) 
			continue;

		SetAmpGain(pCtl, HDA_AMP_MUTE_DEFAULT, pCtl->iLeftVol, pCtl->iRightVol);
	}
}

void HDA::WriteGainToWidget(type_nid cad, type_nid nid, UINT iIndex, UINT iMute, UINT iLeftVol, UINT iRightVol, UINT iDirecion)
{
	USHORT usVol = 0;

	iLeftVol &= 0x7F;
	iRightVol &= 0x7F;
	
	if(iLeftVol != iRightVol) 
	{
		usVol = (1 << (15 - iDirecion)) | (1 << 13) | (iIndex << 8) | (iMute << 7) | iLeftVol;
		CodecCommand(cad, CMD_4BIT(cad, nid, VERB_SET_AMP_GAIN, usVol));
		usVol = (1 << (15 - iDirecion)) | (1 << 12) | (iIndex << 8) | (iMute << 7) | iRightVol;
	} else
		usVol = (1 << (15 - iDirecion)) | (3 << 12) | (iIndex << 8) | (iMute << 7) | iRightVol;


	CodecCommand(cad, CMD_4BIT(cad, nid, VERB_SET_AMP_GAIN, usVol));
}

void HDA::SetAmpGain(Audio_Ctl *pCtl, UINT mute, UINT iLeftVol, UINT iRightVol)
{
	type_nid nid, cad;
	UINT iMute;
	UINT iCodecID;

	if(pCtl == NULL || pCtl->pWidget == NULL || m_pDevInfo == NULL || m_pDevInfo->pCodec == NULL)
		return;

	cad = m_pDevInfo->pCodec->cad;
	nid = pCtl->pWidget->nid;

	if(mute == HDA_AMP_MUTE_DEFAULT) 
		iMute = pCtl->iMuted;
	else 
		iMute = mute;

	if(pCtl->iDirection & HDA_CTL_OUT)
		WriteGainToWidget(cad, nid, pCtl->iIndex, iMute, iLeftVol, iRightVol, 0);
		
	if(pCtl->iDirection & HDA_CTL_IN)
	{
		iCodecID = GetCodecID(m_pDevInfo);
		if( iCodecID == HDA_CODEC_AD1986A)
			iLeftVol = iRightVol = pCtl->iNumSteps;
		else if(iCodecID == HDA_CODEC_ALC268)
		{
			// Input widget (NID=0x18, 0x19, 0x1A) are 2 steps (20/40dB)
			// We maximize the volume for these widgets and let the audio input
			// control be done on audio selector (NID=0x23 & 0x24)
			iLeftVol = iRightVol = pCtl->iNumSteps;
		}
		else if( iCodecID == HDA_CODEC_ALC885)
			iLeftVol = iRightVol = pCtl->iNumSteps * 2 / 3;
		else if( iCodecID == HDA_CODEC_ALC888  && nid == m_InputVolNID[0])		//For Atom E600 
			iLeftVol = iRightVol = pCtl->iStep0Db + 1;
		else if(iCodecID == HDA_CODEC_ALC888 && nid == 0x18 )		//For Atom E600
			iLeftVol = iRightVol = pCtl->iStep0Db + 0x7F;
		else 
			iLeftVol = iRightVol = pCtl->iStep0Db;

		if ((nid == m_InputVolNID[0]) || (nid == m_InputVolNID[1])
			//|| (nid == m_OutputVolNID[0] )|| (nid == m_OutputVolNID[1])
			)
			iMute = 0;

		WriteGainToWidget(cad, nid, pCtl->iIndex, iMute, iLeftVol, iRightVol, 1);
	}

	pCtl->iLeftVol = iLeftVol;
	pCtl->iRightVol = iRightVol;
}

UINT HDA::SetupPcmChannel(UINT iDirection)
{
	Channel_desc *pChannel;
	WIDGET *pWidget;
	UINT iValue, iFormat, iPcmSizeRate, iPathType;
	UINT i, iWidgetType, iChCount, iMaxCh;

	if(iDirection == DIR_PLAY) 
	{
		iWidgetType = CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_OUTPUT;
		pChannel = &m_pHda_Core->channel[m_pHda_Core->iNumIss];
		iPathType = HDA_DAC_PATH;
	} else {
		iWidgetType = CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_INPUT;
		pChannel = &m_pHda_Core->channel[0];
		iPathType = HDA_ADC_PATH;
	}

	iChCount = 0;
	iFormat = m_pDevInfo->AudioFun.iSupportedFormats;
	iPcmSizeRate = m_pDevInfo->AudioFun.iSuppPcmSizeRate;
	iMaxCh = (sizeof(pChannel->ConvNid) / sizeof(pChannel->ConvNid[0])) - 1;

	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode && iChCount < iMaxCh; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0 || pWidget->iWgtType != iWidgetType || !(pWidget->iPathType & iPathType))
			continue;
		iValue = pWidget->Param.iWidgetCap;
		if(!FLAG_MATCH(iValue, CODEC_PARAM_WIDGET_CAP_STEREO_MASK))
			continue;
		iValue = pWidget->Param.iSupportedFormats;
		if(!FLAG_MATCH(iValue, CODEC_PARAM_SUPP_STREAM_FORMATS_PCM_MASK))
			continue;
		if(iChCount == 0) 
		{
			iFormat = pWidget->Param.iSupportedFormats;
			iPcmSizeRate = pWidget->Param.iSuppPcmSizeRate;
		} else {
			iFormat &= pWidget->Param.iSupportedFormats;
			iPcmSizeRate &= pWidget->Param.iSuppPcmSizeRate;
		}
		pChannel->ConvNid[iChCount++] = i;
		//RETAILMSG(1,(TEXT("ConvNid[%d] = 0x%x.\r\n"), (iChCount-1), i));
	}

	pChannel->iSupportedFormats = iFormat;
	pChannel->iSuppPcmSizeRate = iPcmSizeRate;

	if(iDirection == DIR_PLAY) 
	{
		for(i = 1 ; i < m_pHda_Core->iNumOss; i++)
			memcpy (&m_pHda_Core->channel[m_pHda_Core->iNumIss + i], pChannel, sizeof (Channel_desc));

	} else {
		for(i = 1 ; i < m_pHda_Core->iNumIss; i++)
			memcpy (&m_pHda_Core->channel[i], pChannel, sizeof (Channel_desc));
	}

	return (iChCount);
}

UINT HDA::SetMute(unsigned dev, UINT iDirection, USHORT iMute)
{
	#define INPUT_AMP     1
	#define OUTPUT_AMP    0

	WIDGET *pWidget;
	type_nid nid;
	EnterCriticalSection(&m_pHda_Core->cri_sec);

	if(iDirection == DIR_PLAY && dev < MAX_STREAM)
	{
		pWidget = GetWidget(m_OutputVolNID[dev]);
		nid = m_OutputVolNID[dev];
	} else if(iDirection == DIR_REC && dev < MAX_STREAM) {
		pWidget = GetWidget(m_InputVolNID[dev]);
		nid = m_InputVolNID[dev];
	} else {
		LeaveCriticalSection(&m_pHda_Core->cri_sec);
		return 0;
	}

	if(nid == NULL)
	{
		LeaveCriticalSection(&m_pHda_Core->cri_sec);
                return 0;
	}
	if(pWidget)
	{
        if (GET_AMP_MUTE(pWidget->Param.iOutAmpCap))
	{
            WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, 0, 0, OUTPUT_AMP);
	}
	else if (GET_AMP_MUTE(pWidget->Param.iInAmpCap) )
	{
		UINT iLeftVol, iRightVol;
		iLeftVol = iRightVol = CodecCommand(m_pDevInfo->pCodec->cad, CMD_4BIT(m_pDevInfo->pCodec->cad, nid, VERB_GET_AMP_GAIN_MUTE, 0));
        WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, iLeftVol, iRightVol, INPUT_AMP);
	}
	}
	LeaveCriticalSection(&m_pHda_Core->cri_sec);

	return 0;
}
UINT HDA::SetVolume(unsigned dev, UINT iDirection, USHORT usLeftVol, USHORT usRightVol)
{

	#define INPUT_AMP     1
	#define OUTPUT_AMP    0
	
	UINT iMute;
	UINT iStepLeft, iStepRight;
	WIDGET *pWidget;
	type_nid nid;

	EnterCriticalSection(&m_pHda_Core->cri_sec);	
	
	if(usLeftVol == 0 && usRightVol == 0)
		iMute = 1;
	else 
		iMute = 0;
			
	if(iDirection == DIR_PLAY && dev < MAX_STREAM)			
	{
		pWidget = GetWidget(m_OutputVolNID[dev]);		
		nid = m_OutputVolNID[dev];
	} else if(iDirection == DIR_REC && dev < MAX_STREAM) {
		pWidget = GetWidget(m_InputVolNID[dev]);		
		nid = m_InputVolNID[dev];
	} else {
		LeaveCriticalSection(&m_pHda_Core->cri_sec);
		return 0;		
	}
	
	if(nid == NULL)
	{
		LeaveCriticalSection(&m_pHda_Core->cri_sec);
	 	return 0;
	}
	
	if(pWidget)
	{
        if (GET_AMP_MUTE(pWidget->Param.iOutAmpCap))
		{
            WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, 0, 0, OUTPUT_AMP);
		}
		else if (GET_AMP_MUTE(pWidget->Param.iInAmpCap) )
		{
            WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, 0, 0, INPUT_AMP);
		}
		if( GET_AMP_NUM_STEPS( pWidget->Param.iOutAmpCap ) )
		{
            iStepLeft = usLeftVol * GET_AMP_NUM_STEPS( pWidget->Param.iOutAmpCap ) / 100;
            iStepRight = usRightVol * GET_AMP_NUM_STEPS( pWidget->Param.iOutAmpCap ) / 100;
            WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, iStepLeft, iStepRight, OUTPUT_AMP);
		}
		else if( GET_AMP_NUM_STEPS( pWidget->Param.iInAmpCap ) )
		{
            iStepLeft = usLeftVol * GET_AMP_NUM_STEPS( pWidget->Param.iInAmpCap ) / 100;
            iStepRight = usRightVol * GET_AMP_NUM_STEPS( pWidget->Param.iInAmpCap ) / 100;
            WriteGainToWidget(m_pDevInfo->pCodec->cad, nid, 0, iMute, iStepLeft, iStepRight, INPUT_AMP);
		}
		else
		{
            LeaveCriticalSection(&m_pHda_Core->cri_sec);
            return 0;
		}
	}

	LeaveCriticalSection(&m_pHda_Core->cri_sec);
	
	return ( usLeftVol | (usRightVol << 8) );
}

void HDA::SetRecSource()
{
	WIDGET *pWidget, *pChildWidget;
	UINT i, j;

	EnterCriticalSection(&m_pHda_Core->cri_sec);

	for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
	{
		pWidget = GetWidget(i);
		if(pWidget == NULL || pWidget->iWgtEnable == 0)
			continue;
		if(!(pWidget->iPathType & HDA_ADC_RECSEL))
			continue;

		for(j = 0; j < pWidget->iNumConns; j++) 
		{
			pChildWidget = GetWidget(pWidget->ConListEntry[j]);

			if(pChildWidget == NULL || pChildWidget->iWgtEnable == 0)
				continue;
			if(pChildWidget->iWgtType != CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_MIXER)
			{
				if ( !((m_InputSelNID[0] ==  pWidget->nid) && (pWidget->ConListEntry[j] == m_InputPinNID[0])) && 
					 !((m_InputSelNID[1] ==  pWidget->nid) && (pWidget->ConListEntry[j] == m_InputPinNID[1])) )
				{
					continue;
				}
			}
			DEBUGMSG(1,(TEXT("INFO: SetRecSource() NID = 0x%x <--> NID=0x%x index:%d\r\n"), pWidget->nid, pWidget->ConListEntry[j], j));
			CodecCommand(m_pDevInfo->pCodec->cad, CMD_12BIT(m_pDevInfo->pCodec->cad, pWidget->nid, VERB_SET_CONN_SELECT_CONTROL, j));
			WriteGainToWidget(m_pDevInfo->pCodec->cad, pWidget->nid, j, 0, GET_AMP_0DB_STEP(pWidget->Param.iInAmpCap), GET_AMP_0DB_STEP(pWidget->Param.iInAmpCap), 1);
			pWidget->iCurrConnIdx = j;
			j += pWidget->iNumConns;

		}
	}
	
	LeaveCriticalSection(&m_pHda_Core->cri_sec);

	return ;
}


void HDA:: InitChannel(UINT iDirection, ULONG ulChannelIndex)
{
	Channel_desc *pCh;
	WIDGET *pWidget;
	UCHAR con_index = 0;
	
	EnterCriticalSection(&m_pHda_Core->cri_sec);

	if(m_CodecID == HDA_CODEC_AD1986A || m_CodecID == HDA_CODEC_ALC885 ||
		m_CodecID == HDA_CODEC_ALC888 || m_CodecID == HDA_CODEC_ALC262 )		//for Atom E600
	{
		if(iDirection == DIR_PLAY)
		{		
			if( m_dmachannel[m_pHda_Core->iNumIss+1].fAllocated ) {
				con_index =  1;
#ifndef USE_ALC262_PORTA
				pWidget = GetWidget(m_FrontHPOutNID);
#endif
			} else {
				pWidget = GetWidget(m_HeadPhoneNID);
			}
			if(pWidget)
			{
				CodecCommand(m_pDevInfo->pCodec->cad, CMD_12BIT(m_pDevInfo->pCodec->cad, pWidget->nid, VERB_SET_CONN_SELECT_CONTROL, con_index));
				WriteGainToWidget(m_pDevInfo->pCodec->cad, pWidget->nid, 0, 0, 0, 0, 0);
				WriteGainToWidget(m_pDevInfo->pCodec->cad, m_OutputVolNID[con_index], 0, 0, 0, 0, 1);
				pWidget->iCurrConnIdx = con_index;
				DEBUGMSG(1,(TEXT("INFO: InitChannel() NID = 0x%x, index = %d\r\n"), pWidget->nid, con_index));
			}
		}
	}
	
	pCh = &m_pHda_Core->channel[ulChannelIndex];
	pCh->iOffset = ulChannelIndex << 5;
		
	pCh->iStreamID = ++ulChannelIndex;		//stream 0 is reserved as unused, therefore increase it

	pCh->iDirection = iDirection;
	
	LeaveCriticalSection(&m_pHda_Core->cri_sec);
	
	DEBUGMSG(1,(TEXT("INFO: InitChannel() pCh->iStreamID = %d.\r\n"), pCh->iStreamID));

	return ;
}

void HDA::StopStream(Channel_desc *pChannel)
{
	UINT iSDCtrl;
	UINT iIntCtrl;
	
	iSDCtrl = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0);
	iSDCtrl &= ~(REG_SDCTL_IOCE | REG_SDCTL_FEIE | REG_SDCTL_DEIE |REG_SDCTL_RUN);
	HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0, iSDCtrl);

	pChannel->iStatus &= ~HDA_CHN_RUNNING;

	iIntCtrl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_INTCTL);
		
#ifdef POULSBO_C0_WORKAROUND			// Workaround required for Poulsbo C0 stepping and earier		
		if((pChannel->iOffset >> 5) < m_pHda_Core->iNumIss)
			iIntCtrl &= ~(1 << (pChannel->iOffset >> 5));
		else
			iIntCtrl &= ~(1 << ((pChannel->iOffset >> 5) + 2));
#else	
		iIntCtrl &= ~(1 << (pChannel->iOffset >> 5));		
#endif		

		
	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_INTCTL, iIntCtrl);	
}

void HDA::StartStream(Channel_desc *pChannel)
{
	UINT iIntCtrl;
	UINT iSDCtrl;

	iIntCtrl = HDA_READ_REG_4(&m_pHda_Core->mem, REG_INTCTL);

#ifdef POULSBO_C0_WORKAROUND			// Workaround required for Poulsbo C0 stepping and earier			
	if((pChannel->iOffset >> 5) < m_pHda_Core->iNumIss)
		iIntCtrl |= 1 << (pChannel->iOffset >> 5);
	else
		iIntCtrl |= 1 << ((pChannel->iOffset >> 5) + 2);
#else	
	iIntCtrl |= 1 << (pChannel->iOffset >> 5);	
#endif

	HDA_WRITE_REG_4(&m_pHda_Core->mem, REG_INTCTL, iIntCtrl);

	iSDCtrl = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0);
	iSDCtrl |= REG_SDCTL_IOCE | REG_SDCTL_FEIE | REG_SDCTL_DEIE | REG_SDCTL_RUN;
	
	HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0, iSDCtrl);

	pChannel->iStatus |= HDA_CHN_RUNNING;
}

void HDA::ResetStream(Channel_desc *pChannel)
{
	UINT iTimeout;
	UINT iStreamCtl;

	iTimeout = 1000;
	iStreamCtl = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0);
	iStreamCtl |= REG_SDCTL_SRST;
	HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0, iStreamCtl);
	do {
		iStreamCtl = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0);
		if(iStreamCtl & REG_SDCTL_SRST)
			break;
		Sleep(10);
	} while(--iTimeout);
	if(!(iStreamCtl & REG_SDCTL_SRST)) 
	{
		DEBUGMSG(ZONE_INFO,(TEXT("timeout in reset\n")));
	}
	iStreamCtl &= ~REG_SDCTL_SRST;
	HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0, iStreamCtl);
	iTimeout = 1000;
	do {
		iStreamCtl = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL0);
		if(!(iStreamCtl & REG_SDCTL_SRST))
			break;
		Sleep(10);
	} while(--iTimeout);
	if(iStreamCtl & REG_SDCTL_SRST)
		DEBUGMSG(ZONE_INFO,(TEXT("tcan't reset!\n")));
}

void HDA::SetStreamID(Channel_desc *pChannel)
{
	UINT uiValue;

	uiValue = HDA_READ_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL2);
	uiValue &= ~REG_SDCTL2_STRM_MASK;
	uiValue |= pChannel->iStreamID << REG_SDCTL2_STRM_SHIFT;
	HDA_WRITE_REG_1(&m_pHda_Core->mem, pChannel->iOffset + REG_SDCTL2, uiValue);
}

void HDA::SetupStream(Channel_desc *pChannel)
{
	type_nid cad = m_pDevInfo->pCodec->cad;
	USHORT sFormat;
	USHORT sBaseSampleRate, sSampleRateMult, sSampleRateDiv;

	sFormat = 0;
	if(pChannel->iFormat & AUDIO_FORMAT_SMP16)
		sFormat |= AUDIO_FORMAT_SMP16;
	else if(pChannel->iFormat & AUDIO_FORMAT_SMP32)
		sFormat |= AUDIO_FORMAT_SMP32;
	else if(pChannel->iFormat & AUDIO_FORMAT_SMP20)
        sFormat |= AUDIO_FORMAT_SMP20;
    else if(pChannel->iFormat & AUDIO_FORMAT_SMP24)
        sFormat |= AUDIO_FORMAT_SMP24;
	else
        sFormat = 0; /* Support 8 Bits mode */

	if(pChannel->iFormat & AUDIO_FORMAT_STEREO) 
		sFormat |= AUDIO_FORMAT_STEREO;
		
	//Set the Sample rate
	switch(pChannel->iSampleRate)
	{
		case 22050: 
					sBaseSampleRate = BASE_SAMPLE_RATE_44KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_1;
					sSampleRateDiv = SAMPLE_RATE_DIV_2;
					break;
		case 32000: 
					sBaseSampleRate = BASE_SAMPLE_RATE_48KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_2;
					sSampleRateDiv = SAMPLE_RATE_DIV_3;
					break;
		case 44100: 
					sBaseSampleRate = BASE_SAMPLE_RATE_44KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_1;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
					
		case 48000: 
					sBaseSampleRate = BASE_SAMPLE_RATE_48KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_1;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
		case 88200: 
					sBaseSampleRate = BASE_SAMPLE_RATE_44KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_2;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
		case 96000: 
					sBaseSampleRate = BASE_SAMPLE_RATE_48KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_2;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
		case 17640: 
					sBaseSampleRate = BASE_SAMPLE_RATE_44KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_4;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
		case 19200: 
					sBaseSampleRate = BASE_SAMPLE_RATE_48KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_4;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
		default:
					sBaseSampleRate = BASE_SAMPLE_RATE_44KHZ;
					sSampleRateMult = SAMPLE_RATE_MULT_1;
					sSampleRateDiv = SAMPLE_RATE_DIV_1;
					break;
	}
	sFormat |= sBaseSampleRate << 14;	
	sFormat |= sSampleRateMult << 11;	
	sFormat |= sSampleRateDiv << 8;		


	HDA_WRITE_REG_2(&m_pHda_Core->mem, pChannel->iOffset + REG_SDFMT, sFormat);

		    		
	if(pChannel->iDirection == DIR_REC) 
	{
		CodecCommand(cad, CMD_4BIT(cad, pChannel->ConvNid[pChannel->iStreamID -1], VERB_SET_CONV_FMT, sFormat));	
		CodecCommand(cad, CMD_12BIT(cad, pChannel->ConvNid[pChannel->iStreamID -1], VERB_SET_CONV_STREAM_CHAN, pChannel->iStreamID << 4));
	} else {
		CodecCommand(cad, CMD_4BIT(cad, pChannel->ConvNid[pChannel->iStreamID -1 - m_pHda_Core->iNumIss], VERB_SET_CONV_FMT, sFormat));			
		CodecCommand(cad, CMD_12BIT(cad, pChannel->ConvNid[pChannel->iStreamID -1 - m_pHda_Core->iNumIss], VERB_SET_CONV_STREAM_CHAN, pChannel->iStreamID << 4));
		DEBUGMSG(1,(TEXT("SetupStream() DAC nid = 0x%x\r\n"), pChannel->ConvNid[pChannel->iStreamID -1 - m_pHda_Core->iNumIss]));
	}
}

BOOLEAN HDA::TestCap(ULONG msg, LPWAVEOPENDESC pWOD)
{
	Channel_desc *pChannel;
	BOOLEAN bFound = FALSE;
	
	pChannel = (msg == WODM_OPEN) ? &m_pHda_Core->channel[m_pHda_Core->iNumIss] :&m_pHda_Core->channel[0];

	switch(pWOD->lpFormat->nSamplesPerSec)
	{
		case 8000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_8KHZ_MASK))
					bFound = TRUE;
				break;
		case 11025:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_11KHZ_MASK))
					bFound = TRUE;
				break;
		case 16000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_16KHZ_MASK))
					bFound = TRUE;
				break;
		case 22050:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_22KHZ_MASK))
					bFound = TRUE;
				break;
		case 32000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_32KHZ_MASK))
					bFound = TRUE;
				break;
		case 44100:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_44KHZ_MASK))
					bFound = TRUE;
				break;
		case 48000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_48KHZ_MASK)) 
					bFound = TRUE;
				break;
		case 88200:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_88KHZ_MASK))
					bFound = TRUE;
				break;
		case 96000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_96KHZ_MASK))
					bFound = TRUE;
				break;
		case 176400:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_176KHZ_MASK))
					bFound = TRUE;
				break;
		case 192000:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_192KHZ_MASK))
						bFound = TRUE;
				break;
		default:
				break;
	}

	if(bFound != TRUE) 
	{
		DEBUGMSG(1,(TEXT("TestCap():Unsupported pcmrates: %d\n"),pWOD->lpFormat->nSamplesPerSec));
		return FALSE;
	}
	
	
	bFound = FALSE;
	switch(pWOD->lpFormat->wBitsPerSample)
	{
		case 8:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_8BIT_MASK))
					bFound = TRUE;
				break;
		case 16:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_16BIT_MASK))
					bFound = TRUE;
				break;
		case 20:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_20BIT_MASK))
					bFound = TRUE;
				break;
		case 24:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_24BIT_MASK))
					bFound = TRUE;
				break;
		case 32:
				if(FLAG_MATCH(pChannel->iSuppPcmSizeRate, CODEC_PARAM_SUPP_PCM_SIZE_RATE_32BIT_MASK))
					bFound = TRUE;
				break;
		default: 
				break;

	}

	if(bFound != TRUE) 
	{
		DEBUGMSG(1,(TEXT("TestCap():Unsupported sampling_size : %d\n"),pWOD->lpFormat->wBitsPerSample));
		return FALSE;
	}

	return TRUE;
}

void HDA::VendorSpecificConfig()
{
	WIDGET *pWidget;
	Audio_Ctl *pCtl;
	UINT i;

	m_CodecID = GetCodecID(m_pDevInfo);

	switch(m_CodecID) 
	{

	case HDA_CODEC_AD1986A:
		for(i = m_pDevInfo->StartNode; i < m_pDevInfo->EndNode; i++) 
		{
			pWidget = GetWidget(i);
			if(pWidget == NULL || pWidget->iWgtEnable == 0)
				continue;
			if(pWidget->iWgtType != CODEC_PARAM_WIDGET_CAP_TYPE_AUDIO_OUTPUT)
				continue;
			if(pWidget->nid != 3 && pWidget->nid != 4)
				pWidget->iWgtEnable = 0;
		}


		for(i=0;(pCtl = GetAudioCtl(i)) != NULL; i++) 
		{
			if(pCtl->iCtlEnable == 0 || pCtl->pWidget == NULL)
				continue;
			if(pCtl->pWidget->nid == 9 &&  pCtl->iIndex == 0)
				pCtl->iMuted = HDA_AMP_MUTE_ALL;
		}

		m_PowerControlNID = 0x26;	
		m_HeadPhoneNID = 0x0A; 	//0xA is Headphone Selector
		m_LineOutNID = 0x0B;	//0xB is Line out selector		
		m_OutputVolNID[0] = 0x03;
		m_OutputVolNID[1] = 0x04;
		m_InputVolNID[0] = 0x13;	//The NumSteps of 0x06 is 0, therefore use 0x13
		m_InputVolNID[1] = NULL;
		break;

	case HDA_CODEC_ALC885:
		m_PowerControlNID = NULL;
		m_HeadPhoneNID = 0x15;
		m_LineOutNID = NULL;
		m_OutputVolNID[0] = 0x02;
		m_OutputVolNID[1] = 0x03;
		m_InputVolNID[0] = 0x07;
		m_InputVolNID[1] = 0x08;
		break;

	case HDA_CODEC_ALC268:
		m_PowerControlNID = NULL;
		m_HeadPhoneNID = NULL;
		m_LineOutNID = NULL;
		m_OutputVolNID[0] = 0x02;
		m_OutputVolNID[1] = NULL;
		m_InputVolNID[0] = 0x07;
		m_InputVolNID[1] = NULL;
		m_InputSelNID[0] = 0x24;
		m_InputSelNID[1] = NULL;
		m_InputPinNID[0] = 0x19;
		m_InputPinNID[1] = NULL;
		break;
	case HDA_CODEC_ALC888:
		m_PowerControlNID = NULL;
		m_HeadPhoneNID = 0x14;
		m_LineOutNID = 0x15;
		m_OutputVolNID[0] = 0x0C;
		m_OutputVolNID[1] = 0x0D;
		m_InputVolNID[0] = 0x08;		//The NumSteps of 0x08, 0x0B and 0x18 are 0, therefore it only can be mute/unmute
		m_InputVolNID[1] = NULL;
		break;
	case HDA_CODEC_ALC262:
	    m_PowerControlNID = NULL;
#ifdef USE_ALC262_PORTA
		m_HeadPhoneNID = 0x15;
		m_InputPinNID[0] = 0x19;
#else
		m_HeadPhoneNID = 0x14;
		m_FrontHPOutNID = 0x1B;
		m_InputPinNID[0] = 0x18;
#endif
		m_LineOutNID = 0x1A;
		m_OutputVolNID[0] = 0x0C;
		m_OutputVolNID[1] = 0x0D;
		m_InputVolNID[0] = 0x07;
		m_InputVolNID[1] = NULL;
		m_InputSelNID[0] = 0x0B;
		m_InputSelNID[1] = NULL;
		m_InputPinNID[1] = NULL;

		break;
	default:
		m_PowerControlNID = NULL;
		m_HeadPhoneNID = NULL;
		m_LineOutNID = NULL;
		m_OutputVolNID[0] = NULL;
		m_OutputVolNID[1] = NULL;
		m_InputVolNID[0] = NULL;
		m_InputVolNID[1] = NULL;		
		break;
	}
}

void HDA::SetOutputStreamSink(UINT uDeviceId, DWORD SinkNumber)
{
	WIDGET *pWidget = NULL;
	
	switch(SinkNumber)
	{
		case SINK_LINE_OUT:
				if(m_LineOutNID == NULL)	break;
				pWidget = GetWidget(m_LineOutNID);
				break;
		case SINK_HEAD_PHONE:
				if(m_HeadPhoneNID == NULL)	break;				
				pWidget = GetWidget(m_HeadPhoneNID);
				break;
		default:
				break;
	}
	
	if (pWidget)
  {  
	  CodecCommand(m_pDevInfo->pCodec->cad, CMD_12BIT(m_pDevInfo->pCodec->cad, pWidget->nid, VERB_SET_CONN_SELECT_CONTROL, uDeviceId));
	  pWidget->iCurrConnIdx = uDeviceId;
	}  
}

ULONG HDA::SetPowerState(ULONG ulState)
{
	if(m_PowerControlNID == NULL)
		return 0;
	
	CodecCommand(m_pDevInfo->pCodec->cad, CMD_12BIT(m_pDevInfo->pCodec->cad, m_PowerControlNID, VERB_SET_POWER_STATE, ulState));
	m_ulPowerState = ulState;
	DEBUGMSG(1,(TEXT("SetCodecPowerState() nid = 0x%x., value = 0x%x\r\n"),m_PowerControlNID, ulState));
	return 1;
}
