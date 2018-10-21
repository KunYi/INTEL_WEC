//
// -- Intel Copyright Notice -- 
//  
// @par 
// Copyright (c) 2002-2009 Intel Corporation All Rights Reserved. 
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



/*Module Name:  

spi.h

Abstract:  

Holds definitions private to the implementation of the machine independent
driver interface.

Notes: 


--*/

#ifndef __SPI_H__
#define __SPI_H__

#include <windows.h>
#include <nkintr.h>
#include "../../INC/types.h"
#include <notify.h>
#include <memory.h>
#include <nkintr.h>
#include <pegdser.h>
#include <devload.h>
#include <ddkreg.h>
#include <ceddk.h>
#include <cebuscfg.h>
#include <linklist.h>
#include <Giisr.h>
#include <Kfuncs.h>
#include "SPIpublic.h"
#include "..\..\DMA\inc\DmaPublic.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("SPI"), {
        TEXT("Errors"),TEXT("Warnings"),TEXT("Init"),TEXT("Deinit"),
            TEXT("IOCtl"),TEXT("IST"),TEXT("Open"),TEXT("Close"),
            TEXT("Function"),TEXT("Thread"), TEXT("Undefined"),TEXT("Undefined"),
            TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
            0xFFFF
};
/* Debug Zones */
#define    ZONE_ERROR           (DEBUGZONE(0))
#define    ZONE_WARN            (DEBUGZONE(1))
#define    ZONE_INIT            (DEBUGZONE(2))
#define    ZONE_DEINIT          (DEBUGZONE(3))
#define    ZONE_IOCTL           (DEBUGZONE(4))
#define    ZONE_IST		        (DEBUGZONE(5))
#define    ZONE_OPEN            (DEBUGZONE(6))
#define    ZONE_CLOSE           (DEBUGZONE(7))
#endif //#ifdef DEBUG

#define SPI_REGISTER_WRITE(_REG_, _VALUE_)\
    WRITE_REGISTER_ULONG((volatile ULONG*)&(_REG_), (_VALUE_))

#define SPI_REGISTER_READ(_REG_) \
    READ_REGISTER_ULONG((volatile ULONG*)&(_REG_))

#define CLR_REGISTER_BITS(r, bits)  WRITE_REGISTER_ULONG(&r, READ_REGISTER_ULONG(&r) & (~bits))
#define SET_REGISTER_BITS(r, bits)  WRITE_REGISTER_ULONG(&r, READ_REGISTER_ULONG(&r) | ( bits))


#define DEFAULT_SRC_CLK_FREQ    100000000                       // DCM: double check against HAS

#define SPI_SPH 0x00000001
#define SPI_SPO 0x00000002

#define SPI_SPHASE(mode)     ((mode&SPI_SPH)?1:0)
#define SPI_SPOLARITY(mode)  ((mode&SPI_SPO)?1:0)

#define MIN_SER                 6
#define MAX_SER                 4095
#define DMA_MIN_LENGTH          4
#define DEFAULT_DMA_LENGTH      16


#define DMA_DEVICE_NAME          TEXT("DMA2:")


typedef struct _SPI_REGISTERS
{
    // Register structure to match the
    // mapping of controller hardware.

	__declspec(align(4)) UINT32   SSCR0;                  // 0x00
    __declspec(align(4)) UINT32   SSCR1;                  // 0x04
    __declspec(align(4)) UINT32   SSSR;                   // 0x08
    __declspec(align(4)) UINT32   SSITR;                  // 0x0C
    __declspec(align(4)) UINT32   SSDR;                   // 0x10
    __declspec(align(4)) UINT32   RSVD1[5];               // 0x14-0x24
    __declspec(align(4)) UINT32   SSTO;                   // 0x28
    __declspec(align(4)) UINT32   SSPSP;                  // 0x2C
    __declspec(align(4)) UINT32   SSTSA;                  // 0x30
    __declspec(align(4)) UINT32   SSRSA;                  // 0x34
    __declspec(align(4)) UINT32   SSTSS;                  // 0x38
    __declspec(align(4)) UINT32   SSACD;                  // 0x3C
    __declspec(align(4)) UINT32   RSVD2;                  // 0x40
    __declspec(align(4)) UINT32   SITF;                   // 0x44
    __declspec(align(4)) UINT32   SIRF;                   // 0x48
  //__declspec(align(4)) UINT32   RSVD3[493];             // 0x4C-0x7FC
    __declspec(align(4)) UINT32   RSVD3[237];             // 0x4C-0x7FC
    __declspec(align(4)) UINT32   PRV_CLOCKS;             // 0x400
    __declspec(align(4)) UINT32   PRV_RESETS;             // 0x404
    __declspec(align(4)) UINT32   PRV_GENERAL;            // 0x408
    __declspec(align(4)) UINT32   PRV_SSP_REG;            // 0x40C
    __declspec(align(4)) UINT32   PRV_HWLTR_VAL;          // 0x410
    __declspec(align(4)) UINT32   PRV_SWLTR_VAL;          // 0x414
    __declspec(align(4)) UINT32   PRV_CS_CTRL;            // 0x418

} SPI_REGISTERS, *PSPI_REGISTERS;

//
// SSCR0 register bits.
//

#define SSCR0_MOD               0x80000000
#define SSCR0_ACS               0x40000000                          // ESS==0
#define SSCR0_LNW_CHBIT         0x20000000
#define SSCR0_FDRC_MASK         0x7000000                          //(BIT_26| BIT_25| BIT_24)
#define SSCR0_TUR_NOINT         0x800000
#define SSCR0_ROR_NOINT         0x400000
#define SSCR0_NCS               0x200000                          // ESS==0
#define SSCR0_EDSS_SMASK        0x100000                          // ESS==0
#define SSCR0_EDDS_SHIFT        20
#define SSCR0_SCR_SHIFT         8
#define SSCR0_SCR_MASK          0xFFF
#define SSCR0_SCR_SMASK         (SSCR0_SCR_MASK << SSCR0_SCR_SHIFT)
#define SSCR0_SSE_EN             0x80
#define SSCR0_ECS_EN             0x40                          // ESS==0
#define SSCR0_FRF_SPI           (0x00 << 4)                     // ESS==0
#define SSCR0_FRF_SSP           (0x01 << 4)                     // ESS==0
#define SSCR0_FRF_MW            (0x02 << 4)                     // ESS==0
#define SSCR0_FRF_PSP           (0x03 << 4)                     // ESS==0
#define SSCR0_DSS_MASK          0xF                             // ESS==0
#define SSCR0_DSS_SHIFT         0
#define SSCR0_DSS_SMASK         (SSCR0_DSS_MASK << SSCR0_DSS_SHIFT)

#define SSCR1_TTELP             0x80000000
#define SSCR1_TTE               0x40000000
#define SSCR1_EBCEI             0x20000000
#define SSCR1_SCFR              0x10000000
#define SSCR1_ECRA              0x8000000
#define SSCR1_ECRB              0x4000000
#define SSCR1_SCLKDIR           0x2000000
#define SSCR1_SFRMDIR           0x1000000
#define SSCR1_RWOT              0x800000
#define SSCR1_TRAIL             0x400000
#define SSCR1_TSRE              0x200000
#define SSCR1_RSRE              0x100000
#define SSCR1_TINTE             0x80000
#define SSCR1_PINTE             0x40000
#define SSCR1_IFS               0x10000
#define SSCR1_STRF              0x8000
#define SSCR1_EFWR              0x4000
#define SSCR1_MWDS              0x20
#define SSCR1_SPH               0x10                        // ESS==0
#define SSCR1_SPO               0x08                          // ESS==0
#define SSCR1_LBM               0x04
#define SSCR1_TIE               0x02			// Transmit FIFO level interrupt is enabled
#define SSCR1_RIE               0x01			// Receive FIFO level interrupt is enabled

//
// Interrupt Enable Bits on SSCR0
// 
#define SSCR0_TIM                   0x800000                     // Transmit FIFO Under Run Interrupt disable
#define SSCR0_RIM                   0x400000                      // Receive FIFO Over Run Interrupt disable
#define SPI_INTERRUPT_SSCR0_MASK    (SSCR0_TIM | SSCR0_RIM)
//
// Interrupt Enable Bits on SSCR1
// 

#define SSCR1_PINTE                 0x40000                     // Peripheral Trailing Byte Interrupts are enabled
#define SSCR1_TINTE                 0x80000                      // Receiver Time-out interrupts are enabled
//#define SSCR1_RSRE                                            // DMA Receive Service Request Enable
//#define SSCR1_TSRE                BIT_21                      // DMA Transmit Service Request Enable
#define SSCR1_EBCEI                 0x20000000                      // Enable Bit Count Error Interrupt (slave mode)
#define SPI_INTERRUPT_SSCR1_MASK    (SSCR1_RIE | SSCR1_TIE | SSCR1_PINTE | SSCR1_TINTE | SSCR1_EBCEI)
//#define SPI_INTERRUPT_SSCR1_MASK  (SSCR1_RIE | SSCR1_TIE | SSCR1_PINTE | SSCR1_TINTE | SSCR1_RSRE | SSCR1_TSRE | SSCR1_EBCEI)
#define SPI_INTERRUPT_SSSR_OFFSET    0x08
#define SPI_INTERRUPT_SSCR1_OFFSET    0x04
#define CLR_SSSR_RFS_TFS		0xFFFFFF9F		//Clear RFS and TFS

//
// Interrupt Status bits in SSSR
//
#define SSSR_BCE                0x800000       // Bit Count Error                             - SSCR1_EBCEI
#define SSSR_CSS                0x400000
#define SSSR_TUR                0x200000       // Transmit FIFO Under Run                     - SSCR0_TIM
#define SSSR_EOC                0x100000       // DMA has signaled an end of chain condition
#define SSSR_TINT               0x80000       // Receiver Time-out Interrupt                 - SSCR1_TINTE
#define SSSR_PINT               0x40000       // Peripheral Trailing Byte Interrupt          - SSCR0_PINTE
#define SSSR_ROR                0x80         // Receive FIFO Overrun                        - SSCR0_RIM
#define SSSR_RFS                0x40         // Receive FIFO Service Request                - SSCR1_RIE
#define SSSR_TFS                0x20         // Transmit FIFO Service Request               - SSCR1_TIE
#define SSSR_BSY                0x08
#define SSSR_RNE                0x04
#define SSSR_TNF                0x02

#define SPI_INTERRUPT_STATUS_MASK   (SSSR_TFS | SSSR_RFS | SSSR_ROR | SSSR_PINT | SSSR_TINT | SSSR_EOC | SSSR_TUR | SSSR_BCE)
#define SPI_TX_FIFO_NOT_FULL         SSSR_TNF                   // TX FIFO not full
#define SPI_RX_FIFO_NOT_EMPTY        SSSR_RNE                   // RX FIFO not empty

#define SITF_SITFL_SHIFT        16
#define SITF_SITFL_MASK         0xFF
#define SITF_SITFL_SMASK        (SITF_SITFL_MASK << SITF_SITFL_SHIFT)
#define SITF_LWMTF_SHIFT        8
#define SITF_LWMTF_MASK         0xFF
#define SITF_LWMTF_SMASK        (SITF_LWMTF_MASK << SITF_LWMTF_SHIFT)
#define SITF_HWMTF_SHIFT        0
#define SITF_HWMTF_MASK         0xFF
#define SITF_HWMTF_SMASK        (SITF_HWMTF_MASK << SITF_HWMTF_SHIFT)

#define SIRF_SIRFL_SHIFT        8
#define SIRF_SIRFL_MASK         0xFF
#define SIRF_SIRFL_SMASK        (SIRF_SIRFL_MASK << SITF_LWMTF_SHIFT)
#define SIRF_WMRF_SHIFT         0
#define SIRF_WMRF_MASK          0xFF
#define SIRF_WMRF_SMASK         (SIRF_WMRF_MASK << SITF_HWMTF_SHIFT)

/*------------------------------------------------------------------------------
// FIFO and Clock related register bits
------------------------------------------------------------------------------*/

#define LPSS_FIFO_SIZE          256
#define SPI_RX_FIFO_SIZE        256
#define SPI_TX_FIFO_SIZE        256

#define LPSS_MAX_TRANSFER_LENGTH (64 * 1024 - 1)                // max bytes per transfer

#define SET_TRANSMIT_THRESHOLD(pDevice, low, high)     WRITE_REGISTER_ULONG(&pDevice->pRegisters->SITF, ((low & SITF_LWMTF_MASK) << SITF_LWMTF_SHIFT) | (high & SITF_HWMTF_MASK))
#define SET_RECEIVE_THRESHOLD(pDevice, thr)            WRITE_REGISTER_ULONG(&pDevice->pRegisters->SIRF, ((thr & SIRF_WMRF_MASK) << SIRF_WMRF_SHIFT))


#define GET_TRANSMIT_FIFO_LEVEL(pDevice)               ((READ_REGISTER_ULONG(&pDevice->pRegisters->SITF) >> SITF_SITFL_SHIFT) & SITF_SITFL_MASK)
#define GET_RECEIVE_FIFO_LEVEL(pDevice)                ((READ_REGISTER_ULONG(&pDevice->pRegisters->SIRF) >> SIRF_SIRFL_SHIFT) & SIRF_SIRFL_MASK)

#define SPI_TXRX_DMA_DISABLE(pDevice)                  CLR_REGISTER_BITS(pDevice->pRegisters->SSCR1, (ULONG)(SSCR1_TSRE | SSCR1_RSRE | SSCR1_TRAIL));
#define SPI_TX_DMA_DISABLE(pDevice)                    CLR_REGISTER_BITS(pDevice->pRegisters->SSCR1, (ULONG)(SSCR1_TSRE));
#define SPI_RX_DMA_DISABLE(pDevice)                    CLR_REGISTER_BITS(pDevice->pRegisters->SSCR1, (ULONG)(SSCR1_RSRE));
#define SPI_TXRX_DMA_ENABLE(pDevice)                   SET_REGISTER_BITS(pDevice->pRegisters->SSCR1, (SSCR1_TSRE | SSCR1_RSRE | SSCR1_TRAIL));
#define SPI_TX_DMA_ENABLE(pDevice)                     SET_REGISTER_BITS(pDevice->pRegisters->SSCR1, (SSCR1_TSRE | SSCR1_TRAIL));
#define SPI_RX_DMA_ENABLE(pDevice)                     SET_REGISTER_BITS(pDevice->pRegisters->SSCR1, (SSCR1_RSRE | SSCR1_TRAIL));
//
// Private space bits
//
#define PRV_RESETS_NOT_FUNC_RST     0x00000001
#define PRV_RESETS_NOT_APB_RST      0x00000002

#define PRV_CLOCKS_CLK_EN           0x00000001
#define PRV_CLOCKS_UPDATE           0x80000000
#define PRV_CLOCKS_M_VAL_SHIFT      1
#define PRV_CLOCKS_M_VAL_MASK       0x7FFF
#define PRV_CLOCKS_M_VAL_DEFAULT    1                           // 9154
#define PRV_CLOCKS_N_VAL_SHIFT      16
#define PRV_CLOCKS_N_VAL_MASK       0x7FFF
#define PRV_CLOCKS_N_VAL_DEFAULT    1                           // 9154

#define PRV_GENERAL_ISOLATION       0x00000001
#define PRV_GENERAL_LTR_EN          0x00000002
#define PRV_GENERAL_LTR_MODE        0x00000004


#define PRV_CS_CTRL_MODE_SW         1
#define PRV_CS_CTRL_STATE_HI_BIT    2
#define PRV_CS_CTRL_STATE_SHIFT     1

#define DEFAULT_DMA_BUFFER_SIZE     4092
#define DEFAULT_TOTAL_TIMEOUT_IN_MS    ((ULONG)1000*60)

/*SPI Device Driver's Context Structure*/
typedef struct SPI_DEVICE_CONTEXT
{
	DWORD                   dwDeviceId;  
	DWORD                   dwMemBaseAddr;    //Memory base address
	DWORD                   dwMemLen;         //Size of memory base address
	DWORD                   dwIrq;       /* SPI irq */
	DWORD                   dwBusNumber;      
	DWORD                   dwInterfaceType;   
	HANDLE					hSpiHandler;
    DWORD                   dwSysIntr;   /* system interrupt */
	DWORD					InterruptMask; /* Mask for Interrupt*/
    HANDLE                  hInterruptEvent;  /* event mapped to system interrupt */
    HANDLE                  hInterruptThread;   /* system thread */
	HANDLE					hIsrHandler;
	HANDLE                  hWriteReadEvent; /*event to track completion of read/write operation  */
	PSPI_REGISTERS          pRegisters;
	PHYSICAL_ADDRESS        phyAddress;
    PUCHAR				    pBuffer;
	HKEY					hKey;

	DWORD                   BufLength;
	DWORD                   BytesPerEntry;
	DWORD                   ConnectionSpeed;
	DWORD                   Direction;
	DWORD					Mode;
    DWORD                   ISTLoopTrack;			//Track the looping in SPI_IST
	DWORD					Status;
    // This needs to have everything that is needed to configure CTRL0 / CTRL1
 
    DWORD                  SerialClockPolarity; // Byte 17
    DWORD                  SerialClockPhase;    // Byte 16
    DWORD                  DataFrameSize;       // Byte 15 
    DWORD                  ClockDivider;        // Baud Rate - Bytes 11 12 13 14
	DWORD				   ChipSelect;
	
    DWORD				   ClockPhase;
	DWORD                  SrcClockFreq;
	DWORD				   ChipSelectPolarity;
	DWORD                  ClockPolarity;

	 // Debug feature
    ULONG                  LoopbackMode;
	
	BOOL                   bIoComplete;
	DWORD				   TxTransferredLength;
	DWORD				   RxTransferredlength;
	DWORD                  RequestTransferLength;

	//DMA Settings
	ULONG                   ForceDma;
	BOOL                    UseDma;
	DMA_ADAPTER_OBJECT      AdapterObject;
	HANDLE					hDmaHandler;
	PHYSICAL_ADDRESS        MemPhysicalAddress;  //Physical address of DMA Common Buffer
	PUCHAR					VirtualAddress;
	HANDLE				    hDmaCompletionEvent;
	DWORD                   TotalDMATransRequested;
	DWORD					DMABytesReturned;



} SPI_DEVICE_CTXT, *PDEVICE_CONTEXT;



/*SPI driver init function*/
DWORD SPI_Init( LPCTSTR pContext, LPCVOID lpvBusContext);

/*SPI driver open*/
DWORD SPI_Open(DWORD pDevice, DWORD AccessCode, DWORD ShareMode);

/*SPI driver Interrupt handler*/
static DWORD SPI_IST(PDEVICE_CONTEXT pDevice);

/*SPI driver IOControl function*/
BOOL SPI_IOControl(DWORD hOpenContext,DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,PBYTE pBufOut, 
                   DWORD dwLenOut,PDWORD pdwActualOut);

/*SPI close function*/
BOOL SPI_Close(DWORD hOpenContext);

/*SPI driver deinit function*/
BOOL SPI_Deinit(DWORD hDeviceContext);

//******************************************************************************
// Function for Read/Write Operation
//******************************************************************************
BOOL SpiTransferPreSettings(PDEVICE_CONTEXT pDevice);

BOOL PrepareForTransfer(PDEVICE_CONTEXT pDevice,PSPI_TRANS_BUFFER pTransmissionBuf);

BOOL TransferDataWrite(PDEVICE_CONTEXT pDevice);

BOOL TransferDataRead(PDEVICE_CONTEXT pDevice);

BOOL PrepareForRead(PDEVICE_CONTEXT pDevice);

BOOL ControllerCompleteTransfer(PDEVICE_CONTEXT pDevice);

//******************************************************************************
// Function to enable and disable SPI controller/interrupt
//******************************************************************************

BOOL CheckControllerError(PDEVICE_CONTEXT pDevice);

BOOL InitSpiController(PDEVICE_CONTEXT pDevice);

BOOL DeinitSpiController(PDEVICE_CONTEXT pDevice);

VOID spiDisableInterrupts(PDEVICE_CONTEXT pDevice);

VOID spiEnableInterrupts(PDEVICE_CONTEXT pDevice);

VOID AssertChipSelect(PDEVICE_CONTEXT pDevice);

VOID DeassertChipSelect(PDEVICE_CONTEXT pDevice);

//******************************************************************************
// Function for SPI via DMA Operation
//******************************************************************************

VOID DmaInitSettings(PDEVICE_CONTEXT pDevice);

VOID DmaPreExecute(PDEVICE_CONTEXT pDevice);

BOOL InitDMA(PDEVICE_CONTEXT pDevice);

HANDLE GetDmaAdapterHandle(PDEVICE_CONTEXT pDevice);

BOOL ProgramExecuteDmaTransaction(PDEVICE_CONTEXT pDevice);

BOOL StartExecuteDmaTransaction(PDEVICE_CONTEXT pDevice,DMA_REQUEST_CONTEXT DmaRequest,DWORD offset);

VOID DeinitDMA(PDEVICE_CONTEXT pDevice);




#ifdef __cplusplus
}
#endif 
#endif // __SPI_H__