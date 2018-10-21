
// This file contains copies from the acpica\components directory that are handy

#include "common.h"

//#define DEFINE_ACPI_GLOBALS  // this will define many global variables in acglobal.h -> doesn't work as some declarations are "executed" twice (in utilities.lib and here)

#include "acpica.h"
#include "accommon.h"

#include "acglobal.h"

BOOLEAN   AcpiGbl_DbOutputToFile;
UINT32    AcpiGbl_DbDebugLevel;
UINT32    AcpiGbl_DbConsoleDebugLevel;
UINT8     AcpiGbl_DbOutputFlags;
