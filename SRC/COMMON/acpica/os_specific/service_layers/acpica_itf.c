
#include "acpica_itf.h"
#include "oswincextr.h"

#include "acpica.h"
#include "acoutput.h"
#include "aclocal.h"
#include "accommon.h"
#include "acnamesp.h"
#include "acresrc.h"
#include "actables.h"

#include <windows.h>
#include <oal.h>

#include "osprintf.h"
#include "osmalloc.h"
#include "tools.h"
#include "common.h"


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("acpica_itf")


#ifdef DEBUG
#define ZONE_ACPI TRUE
#else
#define ZONE_ACPI FALSE
#endif
#define ACPI_PREFIX  L"*** ACPI *** "

            
void
AcpiDbDisplayObjectType (
    char                    *Name);
    
int AcpiFindPrtTables(char* ObjectArg);

int x86InitACPICA()
{
	ACPI_STATUS status = AE_OK;

    OALMSG(ZONE_ACPI, (ACPI_PREFIX L"+InitAcpi()"));
    status = InitAcpi();
    OALMSG(ZONE_ACPI, (ACPI_PREFIX L"-InitAcpi() status=%d\r\n", status));

    OALMSG(ZONE_ACPI, (ACPI_PREFIX L" AcpiDbgLevel=%08X  AcpiDbgLayer=%02X\r\n", AcpiDbgLevel, AcpiDbgLayer));

    return status;
}

int x86UninitACPICA()
{
	ACPI_STATUS status = AE_OK;

    OALMSG(ZONE_ACPI, (ACPI_PREFIX L" AcpiDbgLevel=%08X  AcpiDbgLayer=%02X\r\n", AcpiDbgLevel, AcpiDbgLayer));

    OALMSG(ZONE_ACPI, (ACPI_PREFIX L"+UninitAcpi()"));
    status = UninitAcpi();
    OALMSG(ZONE_ACPI, (ACPI_PREFIX L"-UninitAcpi() status=%d\r\n", status));

    return status;
}

ApicGlobalSystemInterrupt g_gsi[MAX_GSI];

/**
 * This function will update the g_gsi[] array with the GlobalSystemInterrupt info found in the ACPI MADT tables
 */
int InitApicGlobalSystemInterruptInfo()
{
	int i = 0;
	for (i = 0; i < MAX_GSI; ++i)
    {
        memset(&g_gsi[i], 0, sizeof(ApicGlobalSystemInterrupt));
        g_gsi[i].irq = 0xFF;
		g_gsi[i].ioApicID = -1;                          // not known
		g_gsi[i].ioApicPAddr = -1;                       // not known
		g_gsi[i].ioApicVAddr = (BYTE volatile *)-1;      // not known
    }

	// the first 16, are now the ISA interrupts and are always identity mapped
	// these interrupts are active high and edge triggered
	for (i = 0; i < 16; ++i)
	{
		g_gsi[i].irq = i;
		g_gsi[i].trigger = EDGE_TRIGGERED;
		g_gsi[i].polarity = ACTIVE_HIGH;
		g_gsi[i].ioApicID = -1;                          // not known
		g_gsi[i].ioApicPAddr = -1;                       // not known
		g_gsi[i].ioApicVAddr = (BYTE volatile *)-1;      // not known
		g_gsi[i].ioApicInput = i;                        // input of IOAPIC
	}

    return AE_OK;
}

/**
 * Walk the MADT Interrupt Override table and update g_gsi[] accordingly
 */
int GetApicInterruptOverrideInfo()
{
    ACPI_STATUS						status = AE_OK;
    ACPI_TABLE_HEADER*				tableHeader = NULL;
	UINT32							length = 0;
    ACPI_SUBTABLE_HEADER*			subTable = NULL;
    UINT32							offset = sizeof (ACPI_TABLE_MADT);
	ACPI_MADT_INTERRUPT_OVERRIDE*	miote = NULL;    // madt Interrupt Override table entry
	ACPI_FUNCTION_TRACE (GetApicInterruptOverrideInfo);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiGetTable()"));
    status = AcpiGetTable(ACPI_SIG_MADT, 0, &tableHeader);          // returns pointer to header (and complete table). Maps table into memory, if not already
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiGetTable() %d", status));

    if (ACPI_SUCCESS (status))
    {
		// Walk sub tables until we have the ACPI_MADT_TYPE_INTERRUPT_OVERRIDE table
		subTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, tableHeader, offset);
		while (offset < tableHeader->Length)
		{
			switch (subTable->Type)
			{
				case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
					{
						miote = ACPI_CAST_PTR(ACPI_MADT_INTERRUPT_OVERRIDE, subTable);
					    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Bus %d SrcIrq %d GblIrq %d Flags %d", 
							                             miote->Bus,
														 miote->SourceIrq,
														 miote->GlobalIrq,
														 miote->IntiFlags));
						// is there a rewire ?
						// if so disable the previous entry or the source (as it is identity mapped)
						if (miote->GlobalIrq != miote->SourceIrq)
						{
							g_gsi[miote->SourceIrq].irq = 0xFF; 
						}

						// set values acording flags field in Interrupt source override structures 
						// see "Table 5-50 MPS INTI Flags" from the ACPI specs
						g_gsi[miote->GlobalIrq].irq = miote->SourceIrq;

						// polarity
						if ( (miote->IntiFlags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_CONFORMS )
						{
							// bus specs
							if ( miote->Bus == 0 /*ISA*/ )
							{
								// ISA bus  --> edge triggered, active high
								// EISA bus --> edge triggered, active low
								g_gsi[miote->GlobalIrq].polarity = ACTIVE_HIGH;
							}
						}
						else if ( (miote->IntiFlags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_HIGH )
						{
							// active high
							g_gsi[miote->GlobalIrq].polarity = ACTIVE_HIGH;
						}
						else if ( (miote->IntiFlags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_ACTIVE_LOW )
						{
							// active low
							g_gsi[miote->GlobalIrq].polarity = ACTIVE_LOW;
						}
						else if ( (miote->IntiFlags & ACPI_MADT_POLARITY_MASK) == ACPI_MADT_POLARITY_RESERVED )
						{
							// invalid (reserved)
						}

						// trigger mode
						if ( (miote->IntiFlags & ACPI_MADT_TRIGGER_MASK) == ACPI_MADT_TRIGGER_CONFORMS )
						{
							// bus specs
							if ( miote->Bus == 0 /*ISA*/ )
							{
								// ISA bus  --> edge triggered, active high
								// EISA bus --> edge triggered, active low
								g_gsi[miote->GlobalIrq].trigger= EDGE_TRIGGERED;
							}
						}
						else if ( (miote->IntiFlags & ACPI_MADT_TRIGGER_MASK) == ACPI_MADT_TRIGGER_EDGE )
						{
							// edge triggered
							g_gsi[miote->GlobalIrq].trigger = EDGE_TRIGGERED;
						}
						else if ( (miote->IntiFlags & ACPI_MADT_TRIGGER_MASK) == ACPI_MADT_TRIGGER_LEVEL )
						{
							// level triggered
							g_gsi[miote->GlobalIrq].trigger = LEVEL_TRIGGERED;
						}
						else if ( (miote->IntiFlags & ACPI_MADT_TRIGGER_MASK) == ACPI_MADT_TRIGGER_RESERVED )
						{
							// invalid (reserved)
						}

					} break;

				case ACPI_MADT_TYPE_LOCAL_APIC:
				case ACPI_MADT_TYPE_IO_APIC:
				case ACPI_MADT_TYPE_NMI_SOURCE:
				case ACPI_MADT_TYPE_LOCAL_APIC_NMI:
				case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:
				case ACPI_MADT_TYPE_IO_SAPIC:
				case ACPI_MADT_TYPE_LOCAL_SAPIC:
				case ACPI_MADT_TYPE_INTERRUPT_SOURCE:
				case ACPI_MADT_TYPE_LOCAL_X2APIC:
				case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:
				case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
				case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
					{
						// valid table, not interested
					} break;
				default:
					{
					    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Unknown MADT sub table subtable"));
						if (subTable->Length == 0)
						{
						    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Invalid zero length subtable"));
							return AE_NULL_ENTRY;
						}
					}
			}

			// next table
	        offset += subTable->Length;
		    subTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, subTable, subTable->Length);
		}
	}

	return status;
}

/**
 * Walk the MADT IO Apic table and update g_gsi[] accordingly
 * Maps an IO Apic input pin to a GSI line
 */
int GetIoApicInfo()
{
    ACPI_STATUS						status = AE_OK;
    ACPI_TABLE_HEADER*				tableHeader = NULL;
	UINT32							length = 0;
    ACPI_SUBTABLE_HEADER*			subTable = NULL;
    UINT32							offset = sizeof (ACPI_TABLE_MADT);
	ACPI_MADT_IO_APIC*				mioate = NULL;    // madt IO Apic table entry
	BYTE volatile * volatile        ioapic = NULL;
	DWORD                           ver = 0;
	BYTE                            maxRedirEntries = 0;
	BYTE                            input = 0;
	BYTE                            index = 1;
	ACPI_FUNCTION_TRACE (GetIoApicInfo);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiGetTable()"));
    status = AcpiGetTable(ACPI_SIG_MADT, 0, &tableHeader);          // returns pointer to header (and complete table). Maps table into memory, if not already
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiGetTable() %d", status));

    if (ACPI_SUCCESS (status))
    {
		// Walk sub tables until we have the ACPI_MADT_IO_APIC table
		subTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, tableHeader, offset);
		while (offset < tableHeader->Length)
		{
			switch (subTable->Type)
			{
				case ACPI_MADT_TYPE_IO_APIC:
					{
						mioate = ACPI_CAST_PTR(ACPI_MADT_IO_APIC, subTable);
					    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "GetIoApicInfo() Id %d Address 0x%08X GblIrq %d", 
							                             mioate->Id,
														 mioate->Address,
														 mioate->GlobalIrqBase));
#define APIC_CHIP_SIZE           0x40
#define APIC_IND                 0x00
#define APIC_DAT                 0x10
#define APIC_EOIR                0x40
#define APIC_IND_ID              0x00
#define APIC_IND_VER             0x01
#define APIC_IND_REDIR_TBL_BASE  0x10
						// the IOAPIC is addressable via an indirect addressing scheme using APIC_IND and APIC_DAT registers
						ioapic = (BYTE volatile *)AcpiOsMapMemory(mioate->Address, APIC_CHIP_SIZE);
						if (ioapic == NULL)
						{
						    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Failed to memory map IOAPIC"));
							return AE_NULL_ENTRY;
						}

						*(ioapic + APIC_IND) = APIC_IND_VER;           // BYTE access to APIC_IND_VER
						ver = *(DWORD volatile *)(ioapic + APIC_DAT);  // read DWORD APIC_IND_VER data
						maxRedirEntries = (BYTE)((ver & 0x00FF0000) >> 16);

						for (input = 0; input <= maxRedirEntries; ++input)
						{
							index = input + mioate->GlobalIrqBase;
							g_gsi[index].ioApicID = mioate->Id;
							g_gsi[index].ioApicInput = input;
							g_gsi[index].ioApicPAddr = mioate->Address;
							g_gsi[index].ioApicVAddr;                  // not yet filled in. Up to whoever decides to use the g_gsi[] info

							// for PCI interrupts (PCI > 15, ISA <= 15) we must set it to LEVEL_TRIGGERED & ACTIVE_LOW
							if (index > 15)
							{
								g_gsi[index].irq = index;
								g_gsi[index].polarity = ACTIVE_LOW;
								g_gsi[index].trigger = LEVEL_TRIGGERED;
							}
						}
						AcpiOsUnmapMemory((void*)ioapic, APIC_CHIP_SIZE);  // release IOAPIC memory again

					} break;

				case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
				case ACPI_MADT_TYPE_LOCAL_APIC:
				case ACPI_MADT_TYPE_NMI_SOURCE:
				case ACPI_MADT_TYPE_LOCAL_APIC_NMI:
				case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:
				case ACPI_MADT_TYPE_IO_SAPIC:
				case ACPI_MADT_TYPE_LOCAL_SAPIC:
				case ACPI_MADT_TYPE_INTERRUPT_SOURCE:
				case ACPI_MADT_TYPE_LOCAL_X2APIC:
				case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:
				case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
				case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
					{
						// valid table, not interested
					} break;
				default:
					{
					    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Unknown MADT sub table subtable"));
						if (subTable->Length == 0)
						{
						    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Invalid zero length subtable"));
							return AE_NULL_ENTRY;
						}
					}
			}

			// next table
	        offset += subTable->Length;
		    subTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, subTable, subTable->Length);
		}
	}

	return status;
}

int ApicGlobalSystemInterruptInfo()
{
    ACPI_STATUS status = AE_OK;

	InitApicGlobalSystemInterruptInfo();
	GetApicInterruptOverrideInfo();
	GetIoApicInfo();

	return status;
}

void PrintGSI()
{
	DWORD i = 0;
	ACPI_FUNCTION_TRACE (PrintGSI);
	
	for ( ; i < MAX_GSI; ++i)
	{
		if (g_gsi[i].irq != 0xFF || i < 16 /*we always have the first 16*/)
		{
			ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "PrintGSI() %02d irq %02X %s %s ID %d PA=%08X VA=%08X Input %02X", 
							  i, 
							  g_gsi[i].irq, 
							  g_gsi[i].trigger == EDGE_TRIGGERED ? " Edge" : "Level",
							  g_gsi[i].polarity == ACTIVE_LOW ? " Low" : "High",
							  g_gsi[i].ioApicID,
							  g_gsi[i].ioApicPAddr,
							  g_gsi[i].ioApicVAddr,
							  g_gsi[i].ioApicInput));
		}
	}
}




#define ACPI_DEBUG_BUFFER_SIZE  0x4000      // 16K buffer for return objects, see also oswincextr.c

ACPI_NAMESPACE_NODE *
AcpiDbConvertToNode (
    char                    *InString);

/*static*/ BOOLEAN
AcpiEvIsPciRootBridge (
    ACPI_NAMESPACE_NODE     *Node);

ApicIrqRoutingMap g_irm[MAX_IRM];
static int s_apicIrqRoutingMapPointer = 0;

/**
 * bus             bus where PCI bridge resides on. routTable deals with devices on bus + 1
 * bridgeDevice    device nr of PCI bridge on bus
 * bridgeFunction  function nr of PCI bridge on bus
 */
void AcpiParseIrqList (UINT16 bus, UINT16 bridgeDevice, UINT16 bridgeFunction, UINT8* routeTable)
{
    ACPI_PCI_ROUTING_TABLE  *prtElement = NULL;
    UINT8                   count = 0;

    ACPI_FUNCTION_TRACE (AcpiParseIrqList);

    prtElement = ACPI_CAST_PTR (ACPI_PCI_ROUTING_TABLE, routeTable);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiParseIrqList PCI Bridge on Primary Bus %d Device %d Function %d", bus, bridgeDevice, bridgeFunction));
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiParseIrqList RouteTable of Secondary Bus %d behind PCI bridge :", bus + 1));  // is this correct
    for (count = 0; prtElement->Length; ++count)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "    [%02X] PCI IRQ Routing Table Package: Address 0x%08X, Pin 0x%02X INT%c, SourceIndex 0x%02X, Source %s", 
                      count, 
                      (DWORD)prtElement->Address,
                      (DWORD)prtElement->Pin,
                      (DWORD)prtElement->Pin + 'A',
                      (DWORD)prtElement->SourceIndex,
                      prtElement->Source));

        if (s_apicIrqRoutingMapPointer < MAX_IRM)
        {
            g_irm[s_apicIrqRoutingMapPointer].bridgeBus      = (BYTE)bus;
            g_irm[s_apicIrqRoutingMapPointer].bridgeDevice   = (BYTE)bridgeDevice;
            g_irm[s_apicIrqRoutingMapPointer].bridgeFunction = (BYTE)bridgeFunction;
            g_irm[s_apicIrqRoutingMapPointer].deviceBus      = (BYTE)bus + 1;                      // is this correct?
            if (prtElement->Source[0] == 0)      // important to check only the first BYTE
            {
                g_irm[s_apicIrqRoutingMapPointer].deviceAddress = (DWORD)prtElement->Address;
                g_irm[s_apicIrqRoutingMapPointer].pin           = (DWORD)prtElement->Pin;
                g_irm[s_apicIrqRoutingMapPointer].source        = (DWORD)prtElement->SourceIndex;
            } else
            {
                g_irm[s_apicIrqRoutingMapPointer].deviceAddress = (DWORD)prtElement->Address;
                g_irm[s_apicIrqRoutingMapPointer].pin           = (DWORD)prtElement->Pin;
                g_irm[s_apicIrqRoutingMapPointer].source        = (DWORD)0;  // Fix this! Should be drived from NamePath in prtElement->Source;
                ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiParseIrqList _PRT source is NamePath. Not yet supported!"));
            }
            s_apicIrqRoutingMapPointer++;
        } else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiParseIrqList failed to add RouteTable info. Buffer full"));
        }

        prtElement = ACPI_ADD_PTR (ACPI_PCI_ROUTING_TABLE, prtElement, prtElement->Length);
    }
}

ACPI_STATUS AcpiFindPrt (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = NULL;
    ACPI_NAMESPACE_NODE     *AdrNode = NULL;
    ACPI_NAMESPACE_NODE     *PrtNode = NULL;
    ACPI_NAMESPACE_NODE     *PciRootNode = NULL;
	UINT64                  PciValue = 0;
	ACPI_PCI_ID             PciId;
    char                    *ParentPath = NULL;
    ACPI_BUFFER             ReturnBuffer;
    ACPI_STATUS             Status = AE_OK;
	ACPI_PNP_DEVICE_ID      *Hid = NULL;
    ACPI_PNP_DEVICE_ID_LIST *Cid = NULL;
    UINT32                  i = 0;
    UINT16                  Bus = NestingLevel - 3;   // Bus we are on, -1 when RootHostBridge
    ACPI_OPERAND_OBJECT     *RegionObj = (ACPI_OPERAND_OBJECT  *) ObjHandle;

	ACPI_FUNCTION_TRACE (AcpiFindPrt);

    Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
	ParentPath = AcpiNsGetExternalPathname (Node);
    if (!ParentPath)
    {
    	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "No memory"));
        return (AE_NO_MEMORY);
    }

	Status = AcpiUtExecute_HID (Node, &Hid);
    if (ACPI_SUCCESS (Status))
    {
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "'%s' : %s", ParentPath, Hid->String));
		ACPI_FREE (Hid);
    }

	//Status = AcpiUtExecute_CID (Node, &Cid);
    //if (ACPI_SUCCESS (Status))
    //{
	//	/* Check all _CIDs in the returned list */
	//	for (i = 0; i < Cid->Count; i++)
	//	{
	//		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "    %d %s", i, Cid->Ids[i].String));
	//	}
	//  ACPI_FREE (Cid);
    //}

    Status = AE_OK; // reset

    (void) AcpiGetHandle (Node, METHOD_NAME__ADR, ACPI_CAST_PTR (ACPI_HANDLE, &AdrNode));
    (void) AcpiGetHandle (Node, METHOD_NAME__PRT, ACPI_CAST_PTR (ACPI_HANDLE, &PrtNode));

    if (PrtNode == NULL)
    {
    	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "\nNo _PRT for device: '%s' [%d]", ParentPath, NestingLevel));
    } else
    {
    	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "\nEvaluating _PRT for device: '%s' [%d]", ParentPath, NestingLevel));

		memset(&PciId, 0, sizeof(PciId));
		Status = AcpiUtEvaluateNumericObject (METHOD_NAME__ADR, Node, &PciValue);
		if (ACPI_SUCCESS (Status))
		{
			PciId.Device   = ACPI_HIWORD (ACPI_LODWORD (PciValue));
			PciId.Function = ACPI_LOWORD (ACPI_LODWORD (PciValue));
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "METHOD_NAME__ADR Dev = 0x%04X Fun = 0x%04X", PciId.Device, PciId.Function));
		}

		Status = AcpiUtEvaluateNumericObject (METHOD_NAME__SEG,	Node, &PciValue);
		if (ACPI_SUCCESS (Status))
		{
			PciId.Segment = ACPI_LOWORD (PciValue);
		}

        // TODO: what if there are 2 PCI root bridges
		Status = AcpiUtEvaluateNumericObject (METHOD_NAME__BBN,	Node, &PciValue);
		if (ACPI_SUCCESS (Status))
		{
			PciId.Bus = ACPI_LOWORD (PciValue);
		}

        // Complete/update the PCI ID for this device
        // - Find RootBridge
        PciRootNode = Node;
        while (PciRootNode != AcpiGbl_RootNode)
        {
            if (AcpiEvIsPciRootBridge (PciRootNode))  // Get the _HID/_CID in order to detect a RootBridge
            {
                break;
            }
            PciRootNode = PciRootNode->Parent;
        }

        // - For this RootBridge, on what Bus does it reside?
        //   PciId.Bus will be updated by AcpiHwDerivePciId()
        Status = AcpiHwDerivePciId (&PciId, PciRootNode, Node);  // This will call AcpiOsReadPciConfiguration() to access the PCI bus
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiHwDerivePciId failed for %d %d %d with error %d", PciId.Bus, PciId.Device, PciId.Function, Status));
        }

		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "level %d\n", NestingLevel));
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "segment %d\n", PciId.Segment));
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "bus %d\n", PciId.Bus));            // is this correct?
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "device %d\n", PciId.Device));
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "function %d\n", PciId.Function));

        // Evaluate IRQ Routing table
        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

		Status = AcpiEvaluateObject (PrtNode, NULL, NULL, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Could not evaluate _PRT: %s\n", AcpiFormatException (Status)));
            goto Cleanup;
        }

        SetApicMode(1 /* APIC */);  // important to tell ACPI we want to query the APIC irq tables (and not PIC)

        ReturnBuffer.Pointer = AcpiGbl_DbBuffer;
        ReturnBuffer.Length  = ACPI_DEBUG_BUFFER_SIZE;

		Status = AcpiGetIrqRoutingTable (Node, &ReturnBuffer);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "GetIrqRoutingTable failed: %s\n", AcpiFormatException (Status)));
            goto Cleanup;
        }

        // Bus BEFORE PCI bridge, Bus + 1 AFTER PCI bridge
        // PciId.Device of PCI bridge on Bus
        // PciId.Function of PCI bridge on Bus
        AcpiParseIrqList(Bus, PciId.Device, PciId.Function, ReturnBuffer.Pointer);
    }

Cleanup:
	ACPI_FREE (ParentPath);
	return AE_OK;   // return AE_OK to continue the Walk
}

/**
 * This function will update the g_irm[] array with the PCI Routing Table info found in the ACPI _PRT tables
 * Starting from ObjectArg (e.g. _SB_ )
 */
int AcpiFindPrtTables(char* ObjectArg)
{
	ACPI_NAMESPACE_NODE     *Node = NULL;
	int i = 0;

    for (i = 0; i < MAX_IRM; ++i)
    {
        memset(&g_irm[i], 0, sizeof(ApicIrqRoutingMap));
    }
    s_apicIrqRoutingMapPointer = 0;

	Node = AcpiDbConvertToNode (ObjectArg);

	AcpiWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, AcpiFindPrt, NULL, NULL, NULL);

	return AE_OK;
}

int ApicIrqRoutingMapInfo()
{
    return AcpiFindPrtTables("\\_SB_");
}

void PrintIRM()
{
	DWORD i = 0;
	ACPI_FUNCTION_TRACE (PrintIRM);
	
	for ( ; i < MAX_IRM; ++i)
	{
		if (g_irm[i].deviceAddress != 0x000000000)
		{
			ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "PrintIRM() %02d bridge %d %d %d device bus=%02d addr=%08X pin=%02X src=%02X", 
							  i, 
                              g_irm[i].bridgeBus,
                              g_irm[i].bridgeDevice,
                              g_irm[i].bridgeFunction,
                              g_irm[i].deviceBus,
                              g_irm[i].deviceAddress,
                              g_irm[i].pin,
                              g_irm[i].source));

		}
	}
}


static int s_apicNodes = 0;

ACPI_STATUS AcpiWalkHierarchy (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = NULL;
    ACPI_NAMESPACE_NODE     *PrtNode = NULL;
    char                    *ParentPath = NULL;
    ACPI_STATUS             Status = AE_OK;
	ACPI_PNP_DEVICE_ID      *Hid = NULL;

	ACPI_FUNCTION_TRACE (AcpiWalkHierarchy);

    Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
	ParentPath = AcpiNsGetExternalPathname (Node);
    if (!ParentPath)
    {
    	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "No memory"));
        return (AE_NO_MEMORY);
    }

    (void) AcpiGetHandle (Node, METHOD_NAME__PRT, ACPI_CAST_PTR (ACPI_HANDLE, &PrtNode));

    if (PrtNode == NULL)
    s_apicNodes++;
	Status = AcpiUtExecute_HID (Node, &Hid);
    if (ACPI_SUCCESS (Status))
    {
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Hierarchy Node %3d [%d] :'%s', %s  %s", s_apicNodes, NestingLevel, ParentPath, Hid->String, PrtNode ? METHOD_NAME__PRT : ""));
		ACPI_FREE (Hid);
    } else
    {
		ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Hierarchy Node %3d [%d] :'%s'  %s", s_apicNodes, NestingLevel, ParentPath, PrtNode ? METHOD_NAME__PRT : ""));
    }

	ACPI_FREE (ParentPath);
	return AE_OK;   // return AE_OK to continue the Walk
}

int PrintAcpiHierarchyFrom(char* ObjectArg)
{
	ACPI_NAMESPACE_NODE     *Node = NULL;

	Node = AcpiDbConvertToNode (ObjectArg);

	AcpiWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, AcpiWalkHierarchy, NULL, NULL, NULL);

	return AE_OK;
}

void PrintAcpiHierarchy()
{
    PrintAcpiHierarchyFrom("\\_SB_");
}


