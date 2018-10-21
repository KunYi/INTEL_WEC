
// This file contains the interface we expose

#ifndef _ACPI_ACPICA_ITF_H
#define _ACPI_ACPICA_ITF_H

#include <windows.h>
#include <oal.h>

typedef struct
{
	WORD                irq;           // source irq
	BYTE				trigger;
	BYTE                polarity;
	WORD                ioApicID;		
	DWORD               ioApicPAddr;   // physical address
	BYTE volatile *     ioApicVAddr;   // virtual mapped address
	BYTE                ioApicInput;
} ApicGlobalSystemInterrupt;

typedef struct 
{
    BYTE                bridgeBus;         // the primary bus the PCI bridge resides on
    BYTE                bridgeDevice;      // the device Id of the PCI bridge on the primary bus
    BYTE                bridgeFunction;    // the function Id of the PCI bridge on the primary bus

    BYTE                deviceBus;         // the secondary bus the device resides on
    DWORD               deviceAddress;     // the device Id on the secondary bus this route applies to
    DWORD               pin;               // the pin (INTA, INTB, INTC, INTD)
    DWORD               source;            // the GlobalSystemInterrupt irq number to which the pin is connected
} ApicIrqRoutingMap;

#define MAX_GSI             64
#define MAX_IRM             256

#define ACTIVE_LOW          0
#define ACTIVE_HIGH         1

#define LEVEL_TRIGGERED     1
#define EDGE_TRIGGERED      0

int x86InitACPICA(void);
int x86UninitACPICA(void);

int ApicGlobalSystemInterruptInfo(void);
void PrintGSI();

int ApicIrqRoutingMapInfo(void);
void PrintIRM();

void PrintAcpiHierarchy();

#endif // _ACPI_ACPICA_ITF_H
