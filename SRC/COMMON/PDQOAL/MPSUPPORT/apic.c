#include <windows.h>
#include <pc_smp.h>
#include <oemwake.h>

#include "timer.h"
#include "pci.h"
#include "acpica_itf.h"   
#include "mpsupport.h"	 //mptable

#define VECTOR_OFFSET       	0x64  // vector offset in vector map
#define RCBA_SIZE				0x3420 
#define OIC_OFFSET				0x31FF // Other Interrupt Control register

extern BOOL  fIntrTime;
extern DWORD dwIntrTimeIsr1;
extern DWORD dwIntrTimeIsr2;
extern DWORD dwIntrTimeNumInts;
extern DWORD dwIntrTimeCountdown;
extern DWORD dwIntrTimeCountdownRef;
extern DWORD dwIntrTimeSPC;

DWORD volatile * glapicBASE = NULL;
DWORD volatile * glapicISR = NULL;
DWORD volatile * glapicEOI = NULL;
DWORD volatile * glapicLDR = NULL;
DWORD volatile * glapicDFR = NULL;
DWORD volatile * glapicSPV = NULL;
DWORD volatile * glapicIRR = NULL;

#define LPCUNKNOWN     0  
#define LPCDIRECT      1  
#define LPCINDIRECT    2 
#define LPCSOC		 3  // FOR SOC PRODUCT like BYT

#define PCI_CHIPSET_MASK	0xFF000000
#define PCI_BUS_MASK 		0x00FF0000
#define PCI_DEVICE_MASK 	0x0000FF00
#define PCI_FUNCTION_MASK 	0x000000FF

#ifdef DEBUG
#define APIC_MSG_ZONE 	TRUE
#else
#define APIC_MSG_ZONE 	FALSE
#endif

typedef struct
{
	WORD  vendor;
	WORD  device;
	const WCHAR* chipset;
	DWORD  Mode;
} PciLpcInfo;

static PciLpcInfo g_Intellpc[] = {
	// Intel product datasheet
	{  0x8086, 0x24c0, L"ICH4",     LPCDIRECT   },

	{  0x8086, 0x24dc, L"ICH5",     LPCDIRECT   },
	{  0x8086, 0x2640, L"ICH6",     LPCDIRECT   },
	{  0x8086, 0x2641, L"ICH6M",    LPCDIRECT   },
	
	{  0x8086, 0x27b0, L"ICH7 DH",  LPCINDIRECT },
	{  0x8086, 0x27b8, L"ICH7",     LPCINDIRECT },
	{  0x8086, 0x27b9, L"ICH7M",    LPCINDIRECT },
	{  0x8086, 0x27bd, L"ICH7M DH", LPCINDIRECT },

	{  0x8086, 0x27bc, L"NM10",     LPCINDIRECT },

	{  0x8086, 0x2810, L"ICH8R",    LPCINDIRECT },
	{  0x8086, 0x2811, L"ICH8M-E",  LPCINDIRECT },
	{  0x8086, 0x2812, L"ICH8DH",   LPCINDIRECT },
	{  0x8086, 0x2814, L"ICH8DO",   LPCINDIRECT },
	{  0x8086, 0x2815, L"ICH8M",    LPCINDIRECT },
	
	{  0x8086, 0x2912, L"ICH9DH",   LPCINDIRECT },
	{  0x8086, 0x2914, L"ICH9DO",   LPCINDIRECT },
	{  0x8086, 0x2916, L"ICH9R",    LPCINDIRECT },
	{  0x8086, 0x2917, L"ICH9M-E",  LPCINDIRECT },
	{  0x8086, 0x2918, L"ICH9",     LPCINDIRECT },
	{  0x8086, 0x2919, L"ICH9M",    LPCINDIRECT },
	
	{  0x8086, 0x3a14, L"ICH10DO",  LPCINDIRECT },
	{  0x8086, 0x3a16, L"ICH10R",   LPCINDIRECT },
	{  0x8086, 0x3a18, L"ICH10",    LPCINDIRECT },
	{  0x8086, 0x3a1a, L"ICH10D",   LPCINDIRECT },

	{ 0x8086, 0x0f1c, L"BYT", LPCSOC },
	{ 0x8086, 0x8c4f, L"HSWLPT", LPCINDIRECT },

    	{  0, 0, NULL, LPCUNKNOWN },
};

DWORD ApicRead(BYTE volatile * apic, BYTE index);
void ApicWrite(BYTE volatile * apic, BYTE index, DWORD value);
BOOL SetApicEnable(BOOL enable);
void SetApicEntry( BYTE volatile * apic, BYTE entry, BYTE trigger, BYTE polarity, BYTE vector);
void APICEnableInterrupt(BYTE index, BOOL bValue);
void pci_IRQ_mapping(void);
static ULONG APIC_ISR(void);
void x86InitAPICs(void);

// external function
LPVOID NKCreateStaticMapping(DWORD dwPhysBase, DWORD dwSize);
BOOL   NKDeleteStaticMapping(LPVOID pVirtAddr, DWORD dwSize);

// external variables
extern ApicGlobalSystemInterrupt  g_gsi[MAX_GSI];  
extern ApicIrqRoutingMap  g_irm[MAX_IRM];             
extern DWORD intsrccount;
extern DWORD girmcount;
extern MP_INTSRC IntSrcList[MAX_IA_ENTRY];
extern const BOOL volatile fOalMpEnable;		

// Profiling
static BOOL sgTimeAdvance = TRUE;
extern volatile LARGE_INTEGER   CurTicks;
extern DWORD g_dwOALTimerCount;

extern int (*PProfileInterrupt)(void);

DWORD IntrFreePerfCountSinceTick();

// Warm reset handling
extern DWORD dwRebootAddress;
extern void RebootHandler();

// Search LPC  controller, returns index , bus, device, function of lpc bridge in lpcInfoIntel table
DWORD FindLpc()
{
	DWORD b=0,d=0,f=0;
	for (b = 0; b < 255; ++b) {
	    for (d = 0; d < 32; ++d) {
	        for (f = 0; f < 8; ++f) {
	            	PciConfig_Header pciHeader;
					DWORD c = 0; // chipset index
	            	PCIReadBusData(b, d, f, (void*)(&pciHeader), 0, sizeof(pciHeader));
			

	            	if (pciHeader.vendorID != 0xFFFF) {
	              	OALMSG (APIC_MSG_ZONE, (L"FindLpc %d %d %d 0x%04X 0x%04X", b, d, f, pciHeader.vendorID, pciHeader.deviceID));
	            	}

	            
	           	while (g_Intellpc[c].vendor != 0){
	                	if (pciHeader.vendorID == g_Intellpc[c].vendor &&
	                    		pciHeader.deviceID == g_Intellpc[c].device) {
	           			DWORD loc = (((DWORD)c << 24) | ((DWORD)b << 16) | ((DWORD)d << 8) | ((DWORD)f << 0));
					OALMSG (APIC_MSG_ZONE, (L"FindLpc found %d %d %d %d 0x%04X 0x%04X %s", c, b, d, f, g_Intellpc[c].vendor, g_Intellpc[c].device,  g_Intellpc[c].chipset));
	                    		return loc;
	                	}
	                	c++;
	            	}
	        }
	    }
	}
	OALMSG (APIC_MSG_ZONE, (L"FindLpc not found LPC chipset"));
	return 0xFFFFFFFF;
}

BOOL EnableApic(BOOL enable)
{
	BYTE chipset=0,bus=0,device=0,function=0;
	DWORD apicEnableMode=0;
	PciConfig_Lpc pciLpc;

	DWORD loc = FindLpc();
	if (loc == 0xFFFFFFFF){
	    	OALMSG (APIC_MSG_ZONE, (L"EnableApic could not locate lpc bridge"));
	    	return FALSE;
	}

	memset(&pciLpc, 0, sizeof(pciLpc));
	chipset  = (BYTE)((loc & PCI_CHIPSET_MASK) >> 24);
	bus      = (BYTE)((loc & PCI_BUS_MASK) >> 16);
	device   = (BYTE)((loc & PCI_DEVICE_MASK) >>  8);
	function = (BYTE)((loc & PCI_FUNCTION_MASK) >>  0);
	apicEnableMode = g_Intellpc[chipset].Mode;
		
	if(apicEnableMode==LPCDIRECT){
		
		DWORD offset;
		PCIReadBusData(bus, device, function, (void*)(&pciLpc), 0, sizeof(pciLpc));
		if (enable){
			pciLpc.gen_cntl |= 0x00000100;
		} else{
			pciLpc.gen_cntl &= ~0x00000100;
		}
		offset = (DWORD)(&pciLpc.gen_cntl) - (DWORD)(&pciLpc);

		PCIWriteBusData(bus, device, function, (void*)(&pciLpc.gen_cntl), offset, sizeof(DWORD));  // only write gen_cntl register
		PCIReadBusData(bus, device, function, (void*)(&pciLpc), 0, sizeof(pciLpc));

		OALMSG (APIC_MSG_ZONE, (L"EnableApic %s APIC controller (DIRECT) succesfully (%02X = %08X)\r\n", enable ? L"Enabled" : L"Disabled", offset, pciLpc.gen_cntl));

		return TRUE;
	}else if(apicEnableMode==LPCSOC) {
		DWORD pILB_BAR;
		BYTE volatile * pirqa;
		BYTE volatile * pirqb;
		BYTE volatile * pirqc;
		BYTE volatile * pirqd;
		BYTE volatile * pirqe;
		BYTE volatile * pirqf;
		BYTE volatile * pirqg;
		BYTE volatile * pirqh;
		BYTE volatile * membar;
		BYTE volatile * oic;

		PCIReadBusData(0, 31, 0, &pILB_BAR, 0x50, 4);
		membar = (BYTE*)AcpiOsMapMemory(pILB_BAR & 0xFFFFFE00, 0x9f);
		oic = membar + 0x60;  // Other Interrupt Control register

		OALMSG (APIC_MSG_ZONE, (L"EnableApic APIC_SOC: EnableApic ILB_BAR located at physical address %08X, virtual address %08X\n", pILB_BAR & 0xFFFFFE00, (DWORD)membar));

		if (enable){
			*oic |= 0x100;
		}else{
			*oic &= ~0x100;
		}

		//assign pirqa to ioapic
		pirqa = membar + 0x8;
		pirqb = membar + 0x9;
		pirqc = membar + 0xa;
		pirqd = membar + 0xb;
		pirqe = membar + 0xc;
		pirqf = membar + 0xd;
		pirqg = membar + 0xe;
		pirqh = membar + 0xf;

		*pirqa |= 0x80;
		*pirqb |= 0x80;
		*pirqc |= 0x80;
		*pirqd |= 0x80;
		*pirqe |= 0x80;
		*pirqf |= 0x80;
		*pirqg |= 0x80;
		*pirqh |= 0x80;

		OALMSG (APIC_MSG_ZONE, (L"EnableApic APIC_SOC: EnableApic %s APIC controller (SOC) succesfully (%02X)\r\n", enable ? L"Enabled" : L"Disabled", *oic));

		AcpiOsUnmapMemory((void*)membar, 0x9f);
		return TRUE;
	} else if(apicEnableMode==LPCINDIRECT){
		BYTE volatile *poic;
		BYTE volatile *prcba ;
		PCIReadBusData(bus, device, function, (void*)(&pciLpc), 0, sizeof(pciLpc));
		prcba = (BYTE*)AcpiOsMapMemory(pciLpc.rcba & 0xFFFFC000, RCBA_SIZE);  // 16K aligned

		poic = prcba + OIC_OFFSET;  

		if (enable){
			*poic |= 0x01;
		} else{
			*poic &= ~0x01;
		}
		AcpiOsUnmapMemory(prcba,RCBA_SIZE);

		OALMSG (APIC_MSG_ZONE, (L"EnableApic %s APIC controller (INDIRECT) succesfully (%02X)\r\n", enable ? L"Enabled" : L"Disabled", *poic));
		return TRUE;
	} 


    	OALMSG (APIC_MSG_ZONE, (L"EnableApic failed\r\n"));
	return FALSE;
}



void x86InitAPICs( void )
{	
	BYTE i=0;
	DWORD loc=0 ;
	//initialize the apic info by mptable or acpi table

#ifndef ACPI_CA
	OALMSG (APIC_MSG_ZONE , (L"------------------ x86InitAPICs by Mptable ----------------------\r\n"));
	x86InitMPTable();
#else
	OALMSG(APIC_MSG_ZONE , (L"------------------ x86InitAPICs by Acpi ----------------------\r\n"));
 	x86InitACPICA();
#endif
    	
#ifndef ACPI_CA
 	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs GlobalSystemInterrupt from Mptable"));
	MpGlobalSystemInterruptInfo();
#else
	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs GlobalSystemInterrupt from Acpi"));
	ApicGlobalSystemInterruptInfo();
#endif

	EnableApic(TRUE);

	// set redirection entry for ioApic through gsi
	for (i = 0; i < MAX_GSI; ++i){
	    if (g_gsi[i].irq != 0xFF){
		    g_gsi[i].ioApicVAddr = (BYTE volatile *)AcpiOsMapMemory( g_gsi[i].ioApicPAddr, APIC_CHIP_SIZE );
		    SetApicEntry(g_gsi[i].ioApicVAddr, g_gsi[i].ioApicInput, g_gsi[i].trigger, g_gsi[i].polarity , VECTOR_OFFSET + i);
		    AcpiOsUnmapMemory(g_gsi[i].ioApicVAddr,APIC_CHIP_SIZE);
	    }
	}
	
#ifndef ACPI_CA
	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs PCI Routing Tables and IRQ mapping by Mptable"));
	MpPciIrqMapping();
#else
	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs PCI Routing Tables and IRQ mapping by Acpi"));
	// Fill in the g_gsi[MAX_IRM] structures
      	ApicIrqRoutingMapInfo();
	// walk the IRM table and setup the PCI IRQ interruptLine (so that OS driver knows to what GSI the PCI IRQ is connected)
	pci_IRQ_mapping();
#endif

	loc = FindLpc();
	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs Legacy equipment setup"));
	if (loc != 0xFFFFFFFF) {
	       BYTE chipset  = (BYTE)((loc & PCI_CHIPSET_MASK) >> 24);




	       WORD deviceID = g_Intellpc[chipset].device;
	       OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs Specify legacy settings for 0x%04X 0x%04X %s", g_Intellpc[chipset].device, g_Intellpc[chipset].device, g_Intellpc[chipset].chipset));

            	switch (deviceID){
                	case 0x24C0: // ICH4
	              {
	                	// legacy IDE for the intel controller
            	        	BYTE progIF_8A = 0x8A;
				BYTE intLine = 0xFF;
		              PCIWriteBusData(0, 31, 1, &progIF_8A, 0x09, 1);
                        	PCIWriteBusData(0, 31, 1, &intLine, 0x3C, 1);
		              OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs IDE legacy mode"));
	              } break;
              	case 0x27B0:
              	case 0x27B8:
              	case 0x27B9:
              	case 0x27BD: // ICH7
	              {
		       	// legacy IDE for the intel controller
            	       	BYTE progIF_8A = 0x8A;
				BYTE intLine = 0xFF;
		              PCIWriteBusData(0, 31, 1, &progIF_8A, 0x09, 1);
            	              PCIWriteBusData(0, 31, 1, &intLine, 0x3C, 1);
		              OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs IDE legacy mode"));
	               } break;
			case 0x27BC: // NM10
			case 0x8c4f:
			case 0x0f1c:
	              {
		              // native IDE
		       	OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs IDE native mode"));
	              } break;
                default:
                    {
		              OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs Unknown platform \r\n"));
                    }
            	}
	}else{
	 	 OALMSG (APIC_MSG_ZONE, (L"x86InitAPICs Unknown chipset found"));
	} 

   	// settings some global values and using hookinterrupt to let the kernel know where to jump
	// See http://www.intel.com/content/dam/doc/specification-update/64-architecture-x2apic-specification.pdf page 2-3
	// All Local xApic Register Address Space are offset to (default) 0xFEE00000 base register

	glapicBASE = (DWORD volatile *)AcpiOsMapMemory(0xFEE00000, 4096);  // memory mapped LAPIC in 1 4096 page
	glapicEOI = glapicBASE + (0x000000B0 >> 2);    // End Of Interrupt Register (w/o)
	glapicLDR = glapicBASE + (0x000000D0 >> 2);    // Logical Destination Register (r/w in xApic mode)
	glapicDFR = glapicBASE + (0x000000E0 >> 2);    // Destination Format Register (r/w in xApic mode)
	glapicSPV = glapicBASE + (0x000000F0 >> 2);    // Spurious Interrupt Vector Register (r/w)
	glapicISR = glapicBASE + (0x00000100 >> 2);    // Interrupt Service Register (r/o)
	glapicIRR = glapicBASE + (0x00000200 >> 2);    // Interrupt Request Register (r/o)

	OALMSG (APIC_MSG_ZONE , (L"x86InitAPICs glapicBASE 0x%08x\r\n", glapicBASE ));
	OALMSG(APIC_MSG_ZONE , (L"g_lapic_EOI  0x%08x\r\n", glapicEOI ));
	OALMSG (APIC_MSG_ZONE , (L"x86InitAPICs glapicLDR  0x%08x\r\n", glapicLDR ));
	OALMSG (APIC_MSG_ZONE , (L"x86InitAPICs glapicDFR  0x%08x\r\n", glapicDFR ));
	OALMSG(APIC_MSG_ZONE , (L"g_lapic_SPV  0x%08x\r\n", glapicSPV ));
	OALMSG(APIC_MSG_ZONE , (L"g_lapic_ISR  0x%08x\r\n", glapicISR ));
	OALMSG(APIC_MSG_ZONE , (L"g_lapic_IRR  0x%08x\r\n", glapicIRR ));

	// set processor 0 as interrupt reciever
	*glapicLDR |= SHIFTLEFT(24);     

	
	AcpiOsUnmapMemory(glapicBASE,4096);

	
	// hook vector interrupts
	for (i = VECTOR_OFFSET; i < VECTOR_OFFSET + MAX_GSI; ++i){
		    if (g_gsi[i - VECTOR_OFFSET].irq != 0xFF){
			    BOOL bReturn = HookInterrupt(i, (FARPROC)APIC_ISR);
			    OALMSG (APIC_MSG_ZONE , (L"x86InitAPICs Hookvector %02d 0x%04x, %s\r\n", i - VECTOR_OFFSET, i, bReturn == 0 ? L"NOK" : L"OK" ));
		    }
        }
    
#ifndef ACPI_CA
	OALMSG (APIC_MSG_ZONE , (L"------------------ - x86InitAPICs- x86UnInitMPTable----------------------\r\n"));
	x86UnInitMPTable();
#else
	OALMSG (APIC_MSG_ZONE , (L"------------------ - x86InitAPICs-x86UninitACPICA ----------------------\r\n"));
	x86UninitACPICA();
#endif

	OALMSG (APIC_MSG_ZONE , (L"------------------ - end x86InitAPICs- ----------------------\r\n"));
}


// The main ISR routine
static ULONG APIC_ISR()
{
	int i=0,j=0;
	ULONG ulRet = SYSINTR_NOP;

	BYTE active_vector = 0;
	BYTE active_IRQ = 0xFF;
	DWORD volatile * _lapic_ISR = glapicISR;

	if (fIntrTime)  {
#ifdef EXTERNAL_VERIFY
	    WRITE_PORT_UCHAR((UCHAR*)0x80, 0xE1);
#endif
	    dwIntrTimeIsr1 = IntrFreePerfCountSinceTick();
	    dwIntrTimeNumInts++;
	}

	// find the active bits the In Service Register ISR (there are 8 registers of each 32 bit)
	// when a bit# is set in this register, vector# is active (we only support MAX_GSI vectors)
	for (i = 0; i < 8; ++i){
 		if (*_lapic_ISR != 0){
			for ( j = 0; j < 32; ++j){
				if ( (*_lapic_ISR & SHIFTLEFT(j)) >= 1 )	{
					active_vector = i * 32 + j;
					active_IRQ = active_vector - VECTOR_OFFSET;
				}
			}
		}
		_lapic_ISR = (DWORD volatile *)(((BYTE volatile *)(_lapic_ISR)) + 0x10); // next
	}

	if (active_IRQ == INTR_TIMER0) {   
		if (PProfileInterrupt) {
		   	ulRet= PProfileInterrupt();
		}

		if (!PProfileInterrupt || ulRet == SYSINTR_RESCHED) {
			if (sgTimeAdvance) {
			    CurMSec += g_dwBSPMsPerIntr;
			    CurTicks.QuadPart += g_dwOALTimerCount;
			}

			if (fIntrTime) {
				// We're doing interrupt timing. Every nth tick is a SYSINTR_TIMING.
				dwIntrTimeCountdown--;

				if (dwIntrTimeCountdown == 0)  {
				    dwIntrTimeCountdown = dwIntrTimeCountdownRef;
				    dwIntrTimeNumInts = 0;

				    dwIntrTimeIsr2 = IntrFreePerfCountSinceTick();
				    ulRet = SYSINTR_TIMING;
				} 
				else {
				    if ((int) (CurMSec - dwReschedTime) >= 0){
				        ulRet = SYSINTR_RESCHED;
				    	}
				}
			} 
			else {
			    if ((int) (CurMSec - dwReschedTime) >= 0){
			        ulRet = SYSINTR_RESCHED;
			    	}
			}
		}
		// Check if a reboot was requested.
		if (dwRebootAddress)  {
		    RebootHandler();
		} 
	}
	else if (active_IRQ == INTR_RTC){ //RTC clock has an interrupt
		// Check to see if this was an alarm interrupt
		UCHAR cStatusC = CMOS_Read( RTC_STATUS_C);
		OALMSG(APIC_MSG_ZONE, (L"APIC_ISR RTC Reg C %x looking for %x\r\n", cStatusC, RTC_SRC_IRQ|RTC_SRC_AS));
		if ((cStatusC & (RTC_SRC_IRQ|RTC_SRC_AS)) == (RTC_SRC_IRQ|RTC_SRC_AS))
		    ulRet = SYSINTR_RTC_ALARM;
	}
	else if (active_IRQ <= INTR_MAXIMUM) {  
		// Mask off interrupt source
		APICEnableInterrupt(active_IRQ, FALSE);

		// We have a physical interrupt ID, but want to return a SYSINTR_ID        
		// Call interrupt chain to see if any installed ISRs handle this interrupt
		ulRet = NKCallIntChain(active_IRQ);

		// IRQ not claimed by installed ISR; translate into SYSINTR
		if (ulRet == SYSINTR_CHAIN) {	
			// ulret = sysintr
		    	ulRet = OALIntrTranslateIrq (active_IRQ);
		    
			// if not enabeld --> ulret = sysintr_undefined
			if (!OALIsInterruptEnabled(ulRet)) { 
		       	 // This default IST is not intialized.
		        	ulRet = (ULONG)SYSINTR_UNDEFINED;
		    }
		}

		if (ulRet == SYSINTR_NOP ) {
		    	// If SYSINTR_NOP, IRQ claimed by installed ISR, but no further action required
			APICEnableInterrupt(active_IRQ, TRUE);
		}
		else if (ulRet == SYSINTR_UNDEFINED || !NKIsSysIntrValid(ulRet))  {
		    	// If SYSINTR_UNDEFINED, ignore
		    	// If SysIntr was never initialized, ignore
			OALMSG(APIC_MSG_ZONE, (L"APIC_ISR Spurious interrupt on IRQ %d\r\n", active_IRQ));
		    	ulRet = OALMarkUnknownIRQ(active_IRQ); 
		}

    	}
	OEMIndicateIntSource(ulRet);

	 __asm { 
       	cli         
    	}
	
	*glapicEOI = active_vector;	// EOI message

	return ulRet;
}

DWORD ApicRead(BYTE volatile * ioapic, BYTE index)
{
	*(ioapic + APIC_IND) = index;
	return *(DWORD volatile *)(ioapic + APIC_DAT);
}

void ApicWrite(BYTE volatile * ioapic, BYTE index, DWORD value)
{
	*(ioapic + APIC_IND) = index;
	*(DWORD volatile *)(ioapic + APIC_DAT) = value;
}


void SetApicEntry(BYTE volatile * ioapic, BYTE entry, BYTE trigger, BYTE polarity, BYTE vector)
{
	// assume default settings
	DWORD high_redirection = 0x01000000;   
	DWORD low_redirection  = 0x00010000; 
	                                      
 	const BYTE index = entry << 1;       

	if (trigger == LEVEL_TRIGGERED)      
		low_redirection |= SHIFTLEFT(15);
	else if (trigger == EDGE_TRIGGERED) 
		low_redirection &= ~SHIFTLEFT(15);

	if (polarity == ACTIVE_LOW) {           
		low_redirection |= SHIFTLEFT(13);
		}
	else if (polarity == ACTIVE_HIGH) {
		low_redirection &= ~SHIFTLEFT(13);
		}

	low_redirection &= ~SHIFTLEFT(16);        
	low_redirection &= ~SHIFTLEFT(14);       
	low_redirection |= SHIFTLEFT(11);		  
	low_redirection |= (DWORD)vector;			   

	ApicWrite(ioapic, APIC_IND_REDIR_TBL_BASE + index + 1, high_redirection);
	ApicWrite(ioapic, APIC_IND_REDIR_TBL_BASE + index, low_redirection);

	low_redirection  = ApicRead(ioapic, APIC_IND_REDIR_TBL_BASE + index);
	high_redirection = ApicRead(ioapic, APIC_IND_REDIR_TBL_BASE + index + 1);
	OALMSG (APIC_MSG_ZONE , (L"SetApicEntry redirection entry %d: 0x%08X_%08X\r\n", index >> 1, high_redirection, low_redirection));
}

#ifndef ACPI_CA
void MpPciIrqMapping(){

	DWORD b=0,d=0,f=0;
	DWORD i = 0;

	for (i = 0; i < MAX_IRM; ++i){
	    memset(&g_irm[i], 0, sizeof(ApicIrqRoutingMap));
	}

	OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping intsrccount:%d",intsrccount));

	for (i = 0; i<intsrccount; i++){

		MP_INTSRC *intsrc = &IntSrcList[i];

		if(intsrc->dstirq<16){  // filter the none pci device
			OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping skip the isa device intsrc->dstirq:%d \n",intsrc->dstirq));
			continue;
			}

		if (girmcount < MAX_IRM){

			g_irm[girmcount].deviceBus      = (BYTE)intsrc->srcbus;          
			g_irm[girmcount].deviceAddress = (intsrc->srcbusirq&MP_DEVICE_MASK)>>2;
			g_irm[girmcount].pin           = (DWORD)intsrc->srcbusirq&MP_PIN_MASK;
			g_irm[girmcount].source        = (DWORD)intsrc->dstirq;

			OALMSG(APIC_MSG_ZONE, (TEXT("intsrc: girmcount %d, bus %02x, deviceAddress %02x,pin %x, source %02x\n"),
			girmcount,g_irm[girmcount].deviceBus,g_irm[girmcount].deviceAddress,g_irm[girmcount].pin,g_irm[girmcount].source));

			girmcount++;
		} else{
		    	OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping failed to add RouteTable info. Buffer full"));
		}
	}

	for ( b = 0; b < 255; ++b){ 
		for ( d = 0; d < 32; ++d) {
			for ( f = 0; f < 8; ++f) {
			    PciConfig_Header pciHeader;
			    ULONG r = PCIReadBusData(b, d, f, (void*)(&pciHeader), 0, sizeof(pciHeader));
			    BYTE p = pciHeader.interruptPin;  // INT_ABCD 1-based

			    if (pciHeader.vendorID != 0xFFFF)   {
					BOOL found = FALSE;
					WORD i;
					OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping %d %d %d 0x%04X 0x%04X Pin=0x%02X (INT%c) Line=0x%02X", 
					                    b, d, f, pciHeader.vendorID, pciHeader.deviceID, p, (p < 1) ? '-' : (p > 4) ? 'x' : p - 1 + 'A', pciHeader.interruptLine));

					p -= 1;  // correct 1-based -> 0-based for INTx


					for (i = 0; i < girmcount; ++i)  {
					    BYTE bus      = (BYTE)  g_irm[i].deviceBus;
					    BYTE device   = (BYTE)(g_irm[i].deviceAddress );
					    BYTE pin      = (BYTE)  g_irm[i].pin; 


					    if (b == bus && d == device && p == pin) {
					        BYTE intLine = (BYTE)(g_irm[i].source);
					        DWORD offset = (DWORD)(&pciHeader.interruptLine) - (DWORD)(&pciHeader);
						    ULONG w = PCIWriteBusData(b, d, f, &intLine, offset, sizeof(BYTE));
					        OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping   --> routing found at %d: %d %d (0x%08X) Pin=0x%02X (INT%c) Line=0x%02X", 
					                            i, bus, device, g_irm[i].deviceAddress, g_irm[i].pin, g_irm[i].pin + 'A', intLine));
					        found = TRUE;
					        break;
					  }
			        }
			        if (!found) {
					        OALMSG (APIC_MSG_ZONE, (L"MpPciIrqMapping   --> no routing found"));
			        }
			    }
			}
		}
	}
	return;
}

#endif //acpica

void pci_IRQ_mapping(void)
{
    // We first need to adjust the ACPI deviceBus (secondary bus number). We can not derive it directly from the ACPI tables,
    // but when we scan the PCI bus and search for PCI bridges, match the PCI bridge with the bus, device, function number in the ACPI table,
    // we can see in the pciBridgeHeader.secondaryBusNumber the secondary bus number the BIOS has assigned before.
    // Note: you cannot assume a certain order in which the bus number has been assigned. Instead just read what the BIOS has decided.
	BYTE b,d,f;
	WORD i;
	for ( b = 0; b < 255; ++b) {// bus
		for ( d = 0; d < 32; ++d) {// device
			for ( f = 0; f < 8; ++f) {// function
				PciConfig_BridgeHeader pciBridgeHeader;
				PCIReadBusData(b, d, f, (void*)(&pciBridgeHeader), 0, sizeof(pciBridgeHeader));
				if (pciBridgeHeader.baseClass == 0x06){  // Bridge
					if (pciBridgeHeader.subClass == 0x04 ||   // PCI-to-PCI bridge or PCI-to-PCI bridge (subtractive decode)
					    pciBridgeHeader.subClass == 0x09   )  {// PCI-to-PCI bridge (Semi-Transparent, primary) or PCI-to-PCI bridge (Semi-Transparent, secondary)
					    for ( i = 0; i < MAX_IRM; ++i){
					        BYTE bridgeBus      = (BYTE)g_irm[i].bridgeBus;
					        BYTE bridgeDevice   = (BYTE)g_irm[i].bridgeDevice;
					        BYTE bridgeFunction = (BYTE)g_irm[i].bridgeFunction;
					        if (g_irm[i].deviceAddress != 0x00000000 && 
					            b == bridgeBus && d == bridgeDevice && f == bridgeFunction){
					            g_irm[i].deviceBus = pciBridgeHeader.secondaryBusNumber;
						        OALMSG (APIC_MSG_ZONE, (L"pci_IRQ_mapping set secBusNr %d for %d %d %d bridge (0x%02X - 0x%02X) for entry %d", 
					                                pciBridgeHeader.secondaryBusNumber, b, d, f, pciBridgeHeader.baseClass, pciBridgeHeader.subClass, i));
					        }
					    }
					}
				}
			}
		}
	}

	for ( b = 0; b < 255; ++b) {// bus    
		for ( d = 0; d < 32; ++d) {// device
			for ( f = 0; f < 8; ++f) {// function
				PciConfig_Header pciHeader;
				BYTE p = 0;  // INT_ABCD 1-based
				PCIReadBusData(b, d, f, (void*)(&pciHeader), 0, sizeof(pciHeader));
				p = pciHeader.interruptPin;  // INT_ABCD 1-based

				if (pciHeader.vendorID != 0xFFFF) {
					BOOL found = FALSE;
					OALMSG (APIC_MSG_ZONE, (L"pci_IRQ_mapping %d %d %d 0x%04X 0x%04X Pin=0x%02X (INT%c) Line=0x%02X", 
					                    b, d, f, pciHeader.vendorID, pciHeader.deviceID, p, (p < 1) ? '-' : (p > 4) ? 'x' : p - 1 + 'A', pciHeader.interruptLine));

					// Note:
					// - pciHeader.interruptPin : INTA = 1, INTB = 2, INTC = 3, INTD = 4
					// - ACPI g_irm[i].source   : INTA = 0, INTB = 1, INTC = 2, INTD = 3
					p -= 1;  // correct 1-based -> 0-based


					for ( i = 0; i < MAX_IRM; ++i){
						BYTE bus      = (BYTE)  g_irm[i].deviceBus;
						BYTE device   = (BYTE)((g_irm[i].deviceAddress & 0xFFFF0000) >> 16);
						BYTE pin      = (BYTE)  g_irm[i].pin;  // INT_ABCD 0-based
						if (g_irm[i].deviceAddress != 0x00000000 && 
							b == bus && d == device && p == pin){
							BYTE intLine = (BYTE)(g_irm[i].source);
							DWORD offset = (DWORD)(&pciHeader.interruptLine) - (DWORD)(&pciHeader);
							PCIWriteBusData(b, d, f, &intLine, offset, sizeof(BYTE));
							OALMSG (APIC_MSG_ZONE, (L"pci_IRQ_mapping   --> routing found at %d: %d %d (0x%08X) Pin=0x%02X (INT%c) Line=0x%02X", 
							                    i, bus, device, g_irm[i].deviceAddress, g_irm[i].pin, g_irm[i].pin + 'A', intLine));
							found = TRUE;
							break;
						}
					}
					if (!found) {
					        OALMSG (APIC_MSG_ZONE, (L"pci_IRQ_mapping   --> no routing found"));
					}
				}
			}
		}
	}
}

void APICEnableInterrupt(BYTE index, BOOL bValue)
{
	BYTE volatile * ioapic=NULL;
	DWORD high_redirection=0;
	DWORD low_redirection=0;

	if (fOalMpEnable)	{
		NKAcquireOalSpinLock();
	}
	else	{
		__asm {
                 pushfd   ; Save interrupt state
                 cli
		      }
	}

	ioapic = g_gsi[ index ].ioApicVAddr;   // the APIC address that belongs to the IRQ
	if ((DWORD)ioapic == 0x00000000 || (DWORD)ioapic == 0xFFFFFFFF)	{
		OALMSG(APIC_MSG_ZONE , (L"APICEnableInterrupt entry %d APIC address not known\r\n", index));
	}

	index <<= 1;  // index *= 2;
	high_redirection = ApicRead(ioapic, APIC_IND_REDIR_TBL_BASE + index + 1);
	low_redirection  = ApicRead(ioapic, APIC_IND_REDIR_TBL_BASE + index);

	if (bValue == TRUE){
		low_redirection &= ~SHIFTLEFT(16);
		}
	else{
		low_redirection |=  SHIFTLEFT(16);
		}

	ApicWrite(ioapic, APIC_IND_REDIR_TBL_BASE + index + 1, high_redirection);
	ApicWrite(ioapic, APIC_IND_REDIR_TBL_BASE + index, low_redirection);

	if (fOalMpEnable)	{
		NKReleaseOalSpinLock();
	}
	else	{
		__asm {
				popfd	 ; Restore interrupt state
				sti
			  }
	}
}

// These functions are used by userspace drivers
#ifdef APIC

BOOL OALIntrEnableIrqs(
                       UINT32 count, 
                       __out_ecount(count) const UINT32 *pIrqs
                       ){
	UINT32 i;
    for ( i = 0; i < count; ++i) {
		APICEnableInterrupt(g_gsi[pIrqs[i]].ioApicInput, TRUE);
    }      
    return TRUE;
}

void OALIntrDisableIrqs(
                        UINT32 count, 
                        __out_ecount(count) const UINT32 *pIrqs
                        ){
	UINT32 i;
    for ( i = 0; i < count; ++i)  {
		APICEnableInterrupt(g_gsi[pIrqs[i]].ioApicInput, FALSE);
    }      
}


void OALIntrDoneIrqs(
                     UINT32 count, 
                     __out_ecount(count) const UINT32 *pIrqs
                     ){
	UINT32 i;
    for (i = 0; i < count; ++i) {
		APICEnableInterrupt(g_gsi[pIrqs[i]].ioApicInput, TRUE);
    }      
}


BOOL OALIntrRequestIrqs(
		PDEVICE_LOCATION pDevLoc,
		__inout UINT32 *pCount,
		__out UINT32 *pIrq
		){

	DWORD i=0,b=0,d=0,f=0;
	b = (BYTE)((pDevLoc->LogicalLoc >> 16) & 0xFF);
	d = (BYTE)((pDevLoc->LogicalLoc >>  8) & 0xFF);
	f = (BYTE)((pDevLoc->LogicalLoc >>  0) & 0xFF);

	if (pIrq == 0 || pCount == 0) {
		return FALSE;
		}

	*pIrq = 0xFF;
	*pCount = 1;

	for ( i = 0; i < MAX_IRM; ++i){
		
		BYTE bus    = (BYTE)g_irm[i].deviceBus;

#ifndef ACPI_CA
		BYTE device = (BYTE)(g_irm[i].deviceAddress );
#else
		BYTE device = (BYTE)((g_irm[i].deviceAddress & 0xFFFF0000) >> 16);
#endif
		if (g_irm[i].deviceAddress != 0x00000000 &&
			b == bus && d == device){
			BYTE intLine = (BYTE)(g_irm[i].source);
			*pIrq = intLine;
			return TRUE;
		}
	}

	return FALSE;
}

#endif  // APIC

