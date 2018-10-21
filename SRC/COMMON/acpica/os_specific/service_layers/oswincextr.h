
#ifndef _OSWINCEXTR_H
#define _OSWINCEXTR_H

#include "acpica.h"

ACPI_STATUS InitAcpi(void);
ACPI_STATUS UninitAcpi(void);

ACPI_STATUS DumpIRQRoutingTable(UINT8 mode, char* bus);
ACPI_STATUS DumpResources();
ACPI_STATUS SetApicMode(UINT64 mode);

#endif  // _OSWINCEXTR_H