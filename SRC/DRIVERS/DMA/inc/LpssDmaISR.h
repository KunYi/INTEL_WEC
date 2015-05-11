#ifndef _LPSSDMAISR_H_
#define _LPSSDMAISR_H_

#include <pkfuncs.h>
#include "BYTAllRegisters.h"
#define USER_IOCTL(X) (IOCTL_KLIB_USER + (X))
  
#define IOCTL_LPSSDMAISR_INFO USER_IOCTL(0)
#define IOCTL_LPSSDMAISR_UNLOAD USER_IOCTL(1)

typedef struct _LPSSDMAISR_INFO {
    PUCHAR              DMAC_BASE;
    DWORD               SysIntr;
    DWORD               InterruptStatusTfr;
    DWORD               InterruptStatusTfrPerChannel[DMAH_NUM_CHANNELS];
    DWORD               InterruptStatusBlock;
    DWORD               InterruptStatusSrcTran;
    DWORD               InterruptStatusDstTran;
    DWORD               InterruptStatusErr;
} LPSSDMAISR_INFO, *PLPSSDMAISR_INFO;
    
#endif // _GIISR_H_