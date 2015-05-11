#ifndef DMAPUBLIC_H
#define DMAPUBLIC_H

//******************************************************************************
// IOCTL code definition
//******************************************************************************

#define DMA1_DEVICE_NAME          TEXT("DMA1:")
#define DMA2_DEVICE_NAME          TEXT("DMA2:")

#define LPSSDMA_DEVICE_TYPE 0x8003

#define IOCTL_LPSSDMA_REQUEST_DMA_SPI_RX \
    ((ULONG)CTL_CODE( LPSSDMA_DEVICE_TYPE, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_LPSSDMA_REQUEST_DMA_SPI_TX\
    ((ULONG)CTL_CODE( LPSSDMA_DEVICE_TYPE, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_LPSSDMA_REQUEST_DMA_UART_RX\
    ((ULONG)CTL_CODE( LPSSDMA_DEVICE_TYPE, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS))

#define IOCTL_LPSSDMA_REQUEST_DMA_UART_TX\
    ((ULONG)CTL_CODE( LPSSDMA_DEVICE_TYPE, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS))


typedef enum _LPSSDMA_CHANNELS
{
    LPSSDMA_CHANNEL0 = 0,
    LPSSDMA_CHANNEL1,
    LPSSDMA_CHANNEL2,
    LPSSDMA_CHANNEL3,
    LPSSDMA_CHANNEL4,
    LPSSDMA_CHANNEL5,
    LPSSDMA_CHANNEL6,
    LPSSDMA_CHANNEL7,
    LPSSDMA_CHANNEL_INVALID,
}LPSSDMA_CHANNELS;

typedef enum _DMA_TRANSACTION_TYPE{
    TRANSACTION_TYPE_SINGLE_RX = 0,
    TRANSACTION_TYPE_SINGLE_TX,
    TRANSACTION_TYPE_SINGLE_TXRX
}DMA_TRANSACTION_TYPE;

typedef struct _DMA_REQUEST_CONTEXT
{
    LPSSDMA_CHANNELS        ChannelTx;
    LPSSDMA_CHANNELS        ChannelRx;
    ULONG                   RequestLineTx;
    ULONG                   RequestLineRx;
    PHYSICAL_ADDRESS        SysMemPhysAddress;
    PUCHAR                  VirtualAdd;
    DWORD                   *pBytesReturned;
    ULONG                   PeripheralFIFOPhysicalAddress;
    DWORD                   TransactionSizeInByte;
    ULONG                   DummyDataWidthInByte; 
    ULONG                   DummyDataForI2CSPI;
    DMA_TRANSACTION_TYPE    TransactionType;
    HANDLE                  hDmaCompleteEvent;
    DWORD                   IntervalTimeOutInMs;
    DWORD                   TotalTimeOutInMs;
}DMA_REQUEST_CONTEXT,*PDMA_REQUEST_CTXT;

#define DEFAULT_TOTALTIMEOUT_IN_MS  (1000*60) /* 60 seconds*/
/*
Explain to IntervalTimeOutInMs and TotalTimeOutInMs
1. When IntervalTimeOutInMs is zero
That means don't use Interval TimeOut, TotalTimeOutInMs should not be zero or MAXDWORD.
if dma receive TotalTimeOutInMs as zero or MAXDWORD, it will set a default value to Total TimeOut
2. When IntervalTimeOutInMs is MAXWORD
That means dma should return immediately after it gets one bytes, so TransactionSizeInByte should be 1.
if dam receive TotalTimeOutInMs is MAXDWORD, it will wait until get one byte.
if dma receive TotalTimeOutInMs is zero,it will set a default value to Total TimeOut
3. When IntervalTimeOutInMs is not zero and MAXWORD.
TotalTimeOutInMs can be MAXDWORD, that means no total timeout, the dma will finished when Interval TimeOut or all bytes transfered
TotalTimeOutInMs can't be zero, a default value will be set to it.
*/



#endif