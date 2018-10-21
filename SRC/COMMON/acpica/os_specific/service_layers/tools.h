
// This file contains copies from the acpica\tools directory that are handy

#ifndef _ACPI_TOOLS_H
#define _ACPI_TOOLS_H

#define AP_MAX_ACPI_FILES           (UINT32)256 /* Prevent infinite loops */

int ApDumpTableByName (char* Signature);
int DumpMADTTable();

#endif // _ACPI_TOOLS_H
