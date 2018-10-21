
// This file contains copies from the acpica\tools directory that are handy

#include "tools.h"

#include "acpica.h"
#include "accommon.h"
#include "actables.h"

#include <stdio.h>

#include "osprintf.h"

BOOLEAN Gbl_SummaryMode = FALSE;
BOOLEAN Gbl_VerboseMode = FALSE;
BOOLEAN Gbl_BinaryMAcpiOsGetTableByNameode = FALSE;


/******************************************************************************
 *
 * FUNCTION:    ApIsValidHeader
 *
 * PARAMETERS:  Table               - Pointer to table to be validated
 *
 * RETURN:      TRUE if the header appears to be valid. FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI table header
 *
 ******************************************************************************/

BOOLEAN
ApIsValidHeader (
    ACPI_TABLE_HEADER       *Table)
{
    if (!ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        /* Make sure signature is all ASCII and a valid ACPI name */

        if (!AcpiUtValidAcpiName (Table->Signature))
        {
            AcpiOs_printf ("Table signature (0x%8.8X) is invalid\n",
                *(UINT32 *) Table->Signature);
            return (FALSE);
        }

        /* Check for minimum table length */

        if (Table->Length < sizeof (ACPI_TABLE_HEADER))
        {
            AcpiOs_printf ("Table length (0x%8.8X) is invalid\n",
                Table->Length);
            return (FALSE);
        }
    }

    return (TRUE);
}

/******************************************************************************
 *
 * FUNCTION:    ApGetTableLength
 *
 * PARAMETERS:  Table               - Pointer to the table
 *
 * RETURN:      Table length
 *
 * DESCRIPTION: Obtain table length according to table signature
 *
 ******************************************************************************/

UINT32
ApGetTableLength (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_TABLE_RSDP         *Rsdp;


    /* Check if table is valid */

    if (!ApIsValidHeader (Table))
    {
        return (0);
    }

    if (ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table);
        return (Rsdp->Length);
    }
    else
    {
        return (Table->Length);
    }
}

/******************************************************************************
 *
 * FUNCTION:    ApDumpTableBuffer
 *
 * PARAMETERS:  Table               - ACPI table to be dumped
 *              Instance            - ACPI table instance no. to be dumped
 *              Address             - Physical address of the table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump an ACPI table in standard ASCII hex format, with a
 *              header that is compatible with the AcpiXtract utility.
 *
 ******************************************************************************/

static int
ApDumpTableBuffer (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance,
    ACPI_PHYSICAL_ADDRESS   Address)
{
    UINT32                  TableLength;


    TableLength = ApGetTableLength (Table);

    /* Print only the header if requested */

    if (Gbl_SummaryMode)
    {
        AcpiTbPrintTableHeader (Address, Table);
        return (0);
    }

    /*
     * Dump the table with header for use with acpixtract utility
     * Note: simplest to just always emit a 64-bit address. AcpiXtract
     * utility can handle this.
     */
    AcpiOs_printf ("%4.4s @ 0x%8.8X%8.8X\n", Table->Signature,
        ACPI_FORMAT_UINT64 (Address));

    AcpiUtDumpBuffer (ACPI_CAST_PTR (UINT8, Table), TableLength,
        DB_BYTE_DISPLAY, 0);
    AcpiOs_printf ("\n");
    return (0);
}

/******************************************************************************
 *
 * FUNCTION:    ApDumpTableByName
 *
 * PARAMETERS:  Signature           - Requested ACPI table signature
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table via a signature and dump it. Handles
 *              multiple tables with the same signature (SSDTs).
 *
 ******************************************************************************/

int
ApDumpTableByName (
    char                    *Signature)
{
    char                    LocalSignature [ACPI_NAME_SIZE + 1];
    UINT32                  Instance;
    ACPI_TABLE_HEADER       *Table;
    ACPI_PHYSICAL_ADDRESS   Address;
    ACPI_STATUS             Status;

    Address = 0;

    if (strlen (Signature) != ACPI_NAME_SIZE)
    {
        AcpiOs_printf (
            "Invalid table signature [%s]: must be exactly 4 characters\n",
            Signature);
        return (-1);
    }

    /* Table signatures are expected to be uppercase */

    strcpy (LocalSignature, Signature);
    AcpiUtStrupr (LocalSignature);

    /* To be friendly, handle tables whose signatures do not match the name */

    if (ACPI_COMPARE_NAME (LocalSignature, "FADT"))
    {
        strcpy (LocalSignature, ACPI_SIG_FADT);
    }
    else if (ACPI_COMPARE_NAME (LocalSignature, "MADT"))
    {
        strcpy (LocalSignature, ACPI_SIG_MADT);
    }

    /* Dump all instances of this signature (to handle multiple SSDTs) */

    for (Instance = 0; Instance < AP_MAX_ACPI_FILES; Instance++)
    {
        Status = AcpiGetTable (LocalSignature, Instance, &Table);
        if (ACPI_FAILURE (Status))
        {
            /* AE_LIMIT means that no more tables are available */

            if (Status == AE_LIMIT)
            {
                return (0);
            }

            AcpiOs_printf (
                "Could not get ACPI table with signature [%s], %s\n",
                LocalSignature, AcpiFormatException (Status));
            return (-1);
        }

        if (ApDumpTableBuffer (Table, Instance, Address))
        {
            return (-1);
        }
        free (Table);
    }

    /* Something seriously bad happened if the loop terminates here */

    return (-1);
}



