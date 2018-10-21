
// includes missing parts

#include "acpica.h"
#include "acoutput.h"
#include "aclocal.h"
#include "accommon.h"
#include "acevents.h"
//#include "acdebug.h"

#include <windows.h>
#include <oal.h>

#include "osprintf.h"
#include "osmalloc.h"
#include "tools.h"
#include "common.h"

#define ACPI_CREATE_RESOURCE_TABLE
#include "acpredef.h"

#ifdef DEBUG
#define ZONE_ACPI TRUE
#else
#define ZONE_ACPI FALSE
#endif

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("oswincextr")

// Where to find the RSDP root pointer
ACPI_PHYSICAL_ADDRESS AeLocalGetRootPointer (void)
{
    ACPI_SIZE TableAddress = 0;
    ACPI_STATUS status = AcpiFindRootPointer(&TableAddress);
    if (ACPI_SUCCESS(status))
    {
        OALMSG(ZONE_ACPI , (L"Succesfully found RDSP 0x%08X\r\n", TableAddress));
        return TableAddress;
    } else
    {
        OALMSG(ZONE_ACPI , (L"Failed to find RDSP\r\n"));
        return 0;
    }
}

#ifdef ACPI_SINGLE_THREADED

ACPI_THREAD_ID AcpiOsGetThreadId (void)
{
    return (0xFFFF);
}

ACPI_STATUS AcpiOsExecute (ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    return 0;
}

#endif



// unresolved external symbol _free referenced in function _AcpiOsFree
void free(void* data)
{
    OALMSG(ZONE_ACPI , (L"free redirected to os_free for %08X\r\n", data));
	os_free(data);
}

// unresolved external symbol _malloc referenced in function _AcpiOsAllocate
void* malloc(size_t size)
{
    void* data = NULL;
    OALMSG(ZONE_ACPI , (L"malloc redirected to os_malloc\r\n"));
    data = os_malloc(size);
    OALMSG(ZONE_ACPI , (L"malloc redirected to os_malloc for %08X\r\n", data));
    return data;
}

void* realloc (void* ptr, size_t size)
{
    void* data = NULL;
    OALMSG(ZONE_ACPI , (L"realloc redirected to os_realloc\r\n"));
    data = os_realloc(ptr, size);
    OALMSG(ZONE_ACPI , (L"realloc redirected to os_realloc for %08X\r\n", data));
    return data;
}

void* calloc(size_t number, size_t size)
{
    void* data = NULL;
    OALMSG(ZONE_ACPI , (L"calloc redirected to os_calloc\r\n"));
    data = os_calloc(number, size);
    OALMSG(ZONE_ACPI , (L"calloc redirected to os_calloc for %08X\r\n", data));
    return data;
}
    
// unresolved external symbol _getchar referenced in function _AcpiOsGetLine
int getchar()
{
    OALMSG(ZONE_ACPI , (L"getchar not implemented\r\n"));
    return -1;
}

// unresolved external symbol _vfprintf referenced in function _AcpiOsPrintf
int vfprintf(FILE* file, const char* format, va_list args)
{
#define ACPI_OUTBUFLEN 256
    char                    aOut[ACPI_OUTBUFLEN];
    WCHAR                   wOut[ACPI_OUTBUFLEN];

	AcpiOs_vsprintf(aOut, format, args);
    AcpiOs_AnsiToWideChar(wOut, ACPI_OUTBUFLEN, aOut, ACPI_OUTBUFLEN);

    OALMSG(ZONE_ACPI, (wOut));

	return 0;
}

// unresolved external symbol _fprintf referenced in function xxx
int fprintf(FILE* stream, const char* format, ...)
{
    va_list args;
        
    va_start(args, format);
	return vfprintf(stream, format, args);
}

// unresolved external symbol _printf referenced in function xxx
int printf(const char* format, ...)
{
    va_list args;
        
    va_start(args, format);
	return AcpiOs_printf(format, args);
}

// unresolved external symbol _sprintf referenced in function xxx
int vsprintf(char* str, const char* format, va_list args)
{
	return AcpiOs_vsprintf(str, format, args);
}

// unresolved external symbol _sprintf referenced in function xxx
int sprintf(char* str, const char* format, ...)
{
    va_list args;
        
    va_start(args, format);
	return AcpiOs_vsprintf(str, format, args);
}

// Compiler, Yacc und Lex
FILE * fopen ( const char * filename, const char * mode )
{
    OALMSG(ZONE_ACPI , (L"fopen not implemented\r\n"));
	return NULL;
}
FILE * freopen ( const char * filename, const char * mode, FILE * stream )
{
   OALMSG(ZONE_ACPI , (L"freopen not implemented\r\n"));
	return NULL;
}
int fclose ( FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"fclose not implemented\r\n"));
	return 0;
}
size_t fread ( void * ptr, size_t size, size_t count, FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"fread not implemented\r\n"));
	return 0;
}
size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"fwrite not implemented\r\n"));
	return 0;
}
int fseek ( FILE * stream, long int offset, int origin )
{
    OALMSG(ZONE_ACPI , (L"fseek not implemented\r\n"));
	return 0;
}
long int ftell ( FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"ftell not implemented\r\n"));
	return 0;
}
char * fgets ( char * str, int num, FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"fgets not implemented\r\n"));
	return 0;
}
int feof ( FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"feof not implemented\r\n"));
	return 0;
}
int ferror ( FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"ferror not implemented\r\n"));
	return 0;
}
int remove(const char * _Filename)
{
    OALMSG(ZONE_ACPI , (L"remove not implemented\r\n"));
	return 0;
}
int ungetc ( int character, FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"ungetc not implemented\r\n"));
	return 0;
}
int getc ( FILE * stream )
{
    OALMSG(ZONE_ACPI , (L"getc not implemented\r\n"));
	return 0;
}
// To resolve this
//_CRTIMP extern int * __cdecl _errno(void);
//#define errno   (*_errno())
int my_errno;
int *_errno()
{
	return &my_errno;
}
char * strerror ( int errnum )
{
    OALMSG(ZONE_ACPI , (L"strerror not implemented\r\n"));
	return "strerror not implemented";
}
void perror ( const char * str )
{
    OALMSG(ZONE_ACPI , (L"perror not implemented\r\n"));
}
void exit (int status)
{
    OALMSG(ZONE_ACPI , (L"exit not implemented\r\n"));
}
char* asctime (const struct tm * timeptr)
{
    OALMSG(ZONE_ACPI , (L"asctime not implemented\r\n"));
	return "";
}
__time32_t _time32(__time32_t *timer)
{
	__time32_t t;
	memset(&t, 0,  sizeof(t));
    OALMSG(ZONE_ACPI , (L"_time32 not implemented\r\n"));
	return t;
}
char *_ctime32( const __time32_t *timer )
{
    OALMSG(ZONE_ACPI , (L"_ctime32 not implemented\r\n"));
	return NULL;
}
struct tm *_localtime32(const __time32_t *timer)
{
    OALMSG(ZONE_ACPI , (L"_localtime32 not implemented\r\n"));
	return NULL;
}

#if (_WIN32_WCE < 0x800)
void *fileno(FILE *stream)
{
    OALMSG(ZONE_ACPI , (L"fileno not implemented\r\n"));
	return NULL;
}
#else
int fileno(FILE *stream)
{
    OALMSG(ZONE_ACPI , (L"fileno not implemented\r\n"));
	return 0;
}
#endif

int isatty( int fd )
{
    OALMSG(ZONE_ACPI , (L"isatty not implemented\r\n"));
	return 0;
}

LPVOID WINAPI TlsGetValue(DWORD dwTlsIndex)
{
    OALMSG(ZONE_ACPI , (L"TlsGetValue not implemented\r\n"));
	return NULL;
}

// unresolved external symbol _QueryPerformanceCounter referenced in function _AcpiOsGetTimer
BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{
    OALMSG(ZONE_ACPI , (L"QueryPerformanceCounter not implemented\r\n"));
    return 0;
}

// unresolved external symbol _QueryPerformanceFrequency referenced in function _AcpiOsInitialize
BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
    OALMSG(ZONE_ACPI , (L"QueryPerformanceFrequency not implemented\r\n"));
    return 0;
}

// unresolved external symbol __imp__GetTickCount referenced in function _AcpiOsGetTimer
DWORD GetTickCount(void)
{
    OALMSG(ZONE_ACPI , (L"GetTickCount not implemented\r\n"));
    return 0;
}

// unresolved external symbol __imp__Sleep referenced in function _AcpiOsSleep
void Sleep(DWORD dwMilliseconds)
{
    OALMSG(ZONE_ACPI , (L"Sleep not implemented\r\n"));
}

// unresolved external symbol ___iob_func referenced in function _AcpiOsInitialize -> stdout
FILE* __iob_func(void)
{
    OALMSG(ZONE_ACPI , (L"__iob_func not implemented\r\n"));
    return NULL;
}

// unresolved external symbol __imp__IsBadReadPtr referenced in function _AcpiOsReadable
BOOL IsBadReadPtr(const void *lp, UINT_PTR ucb)
{
    OALMSG(ZONE_ACPI , (L"IsBadReadPtr not implemented\r\n"));
    return FALSE;
}

// unresolved external symbol __imp__IsBadWritePtr referenced in function _AcpiOsWritable
BOOL IsBadWritePtr(void* lp, UINT_PTR ucb)
{
    OALMSG(ZONE_ACPI , (L"IsBadWritePtr not implemented\r\n"));
    return FALSE;
}


// *********************************************************************************************************

// forward declaration from acutils.h
void
AcpiUtTrace (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId);

// forward declaration from acdebug.h
void
AcpiDbSetOutputDestination (
    UINT32                  Where);

extern UINT32    AcpiDbgLevel;
extern UINT32    AcpiDbgLayer;

extern BOOLEAN   AcpiGbl_DbOutputToFile;
extern UINT32    AcpiGbl_DbDebugLevel;
extern UINT32    AcpiGbl_DbConsoleDebugLevel;
extern UINT8     AcpiGbl_DbOutputFlags;

void TestLogger(void)
{
	BOOL b = 0;
	ACPI_FUNCTION_TRACE (TestLoggerStart);

	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "ACPI testlogger: %s\n", "hello"));

	AcpiDebugPrint(ACPI_LV_EXEC, __LINE__, "FunctionName", "ComponentName", ACPI_TOOLS, "DebugPrint testlogger: %s\n", "hello");

	AcpiOsPrintf ("AcpiOsPrintf %d", __LINE__);
	AcpiOsPrintf ("%9s-%04ld ", "ModuleName", __LINE__);

	b = ACPI_IS_DEBUG_ENABLED(ACPI_DB_EXEC, ACPI_TOOLS);

	OALMSG(ZONE_ACPI , (L"OALMSG TestLogger %d\n", b));
}

ACPI_STATUS InitAcpiLogger(void)
{
    // These are actually used in ACPI_DEBUG_PRINT macro -> AcpiDebugPrint -> ACPI_IS_DEBUG_ENABLED
	//AcpiDbgLevel = ACPI_LV_EXEC | ACPI_LV_TABLES | ACPI_LV_VALUES | ACPI_LV_OBJECTS | ACPI_LV_RESOURCES | ACPI_LV_NAMES | ACPI_LV_INIT_NAMES | ACPI_LV_INFO | ACPI_LV_DISPATCH;  //ACPI_LV_ALL | ACPI_LV_NAMES | ACPI_LV_INFO | ACPI_LV_OPREGION | ACPI_LV_BFIELD
	//AcpiDbgLayer = ACPI_EXECUTER | ACPI_CA_DEBUGGER | ACPI_RESOURCES | ACPI_TABLES | ACPI_NAMESPACE | ACPI_TOOLS | ACPI_OS_SERVICES;  // ACPI_ALL_COMPONENTS;

#ifndef DEBUG
	AcpiDbgLevel = ACPI_LV_INFO;
	AcpiDbgLayer = ACPI_TABLES;
#else
	AcpiDbgLevel = ACPI_LV_EXEC | ACPI_LV_TABLES | ACPI_LV_VALUES | ACPI_LV_OBJECTS | ACPI_LV_RESOURCES;  //ACPI_LV_ALL | ACPI_LV_NAMES | ACPI_LV_INFO | ACPI_LV_OPREGION | ACPI_LV_BFIELD
	AcpiDbgLayer = ACPI_CA_DEBUGGER | ACPI_RESOURCES | ACPI_TABLES | ACPI_NAMESPACE | ACPI_TOOLS | ACPI_OS_SERVICES;  // ACPI_ALL_COMPONENTS;
#endif

	//AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
	AcpiGbl_DbOutputToFile = 0;
	AcpiGbl_DbDebugLevel = AcpiDbgLevel;
	AcpiGbl_DbConsoleDebugLevel = AcpiDbgLevel;
	AcpiGbl_DbOutputFlags = 0xFF;

    TestLogger();

    return AE_OK;
}


char* AcpiGbl_DbBuffer = NULL;
#define ACPI_DEBUG_BUFFER_SIZE  0x4000      // 16K buffer for return objects

#define ACPI_MAX_INIT_TABLES 16
static ACPI_TABLE_DESC TableArray[ACPI_MAX_INIT_TABLES];  // TableArray for AcpiInitializeTables that works internally with AcpiGbl_RootTableList

/**
 * InitAcpi.
 */
ACPI_STATUS InitAcpi(void)
{
	ACPI_STATUS status = AE_OK;
	ACPI_FUNCTION_TRACE (InitAcpi);

	AcpiGbl_DbOutputToFile = 0;
	AcpiGbl_DbDebugLevel = 0;
	AcpiGbl_DbConsoleDebugLevel = 0;
	AcpiGbl_DbOutputFlags = 0;
    AcpiGbl_DbBuffer = NULL;

    // Might reset debug level
    InitAcpiLogger();

    AcpiGbl_DbBuffer = AcpiOsAllocate (ACPI_DEBUG_BUFFER_SIZE);
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiGbl_DbBuffer (%d bytes) allocation %s\r\n", ACPI_DEBUG_BUFFER_SIZE, AcpiGbl_DbBuffer != NULL ? "succeeded" : "failed" ));

    //ACPI_DEBUG_PRINT (ACPI_DB_EXEC, (ACPI_PREFIX L"+AcpiOsGetRootPointer()\r\n"));
    //ACPI_PHYSICAL_ADDRESS address = AcpiOsGetRootPointer();
    //ACPI_DEBUG_PRINT (ACPI_DB_EXEC, (ACPI_PREFIX L"-AcpiOsGetRootPointer() address=0x%08X\r\n", address));

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiInitializeSubsystem()\r\n"));
	status = AcpiInitializeSubsystem ();
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiInitializeSubsystem() status=%d\r\n", status));

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "+AcpiInitializeTables()\r\n"));
	//status = AcpiInitializeTables (TableArray, ACPI_MAX_INIT_TABLES, FALSE);
	status = AcpiInitializeTables(NULL, ACPI_MAX_INIT_TABLES, FALSE);
	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "-AcpiInitializeTables() status=%d\r\n", status));


    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiLoadTables()\r\n"));
	status = AcpiLoadTables ();
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiLoadTables() status=%d\r\n", status));

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiEnableSubsystem()\r\n"));
    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiEnableSubsystem() status=%d\r\n", status));

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "+AcpiInitializeObjects()\r\n"));
    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "-AcpiInitializeObjects() status=%d\r\n", status));

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "InitAcpi done\r\n"));

    return AE_OK;
}

/**
 * UninitAcpi. Hereafter no access is allowed
 */
ACPI_STATUS UninitAcpi(void)
{
	ACPI_FUNCTION_TRACE (UninitAcpi);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "UninitAcpi done\r\n"));

    AcpiTerminate();  // Unload the namespace and free all resources

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiTerminate\r\n"));

    AcpiOsFree(AcpiGbl_DbBuffer);
    AcpiGbl_DbBuffer = NULL;
	AcpiGbl_DbOutputFlags = 0;
	AcpiGbl_DbConsoleDebugLevel = 0;
	AcpiGbl_DbDebugLevel = 0;
	AcpiGbl_DbOutputToFile = 0;

    return AE_OK;
}

// forward declaration from acnamesp.h
void
AcpiNsDumpObjects (
    ACPI_OBJECT_TYPE        Type,
    UINT8                   DisplayType,
    UINT32                  MaxDepth,
    ACPI_OWNER_ID           OwnerId,
    ACPI_HANDLE             StartHandle);

#define ACPI_DISPLAY_SUMMARY        (UINT8) 0
#define ACPI_DISPLAY_OBJECTS        (UINT8) 1
#define ACPI_DISPLAY_MASK           (UINT8) 1
#define ACPI_DISPLAY_SHORT          (UINT8) 2

extern ACPI_NAMESPACE_NODE        *AcpiGbl_RootNode;

/**
 * Dumps all low level bytes of all tables
 */
ACPI_STATUS DumpObjects(void)
{
	ACPI_STATUS status = AE_OK;
	ACPI_FUNCTION_TRACE (DumpObjects);

	AcpiNsDumpObjects(ACPI_TYPE_ANY, ACPI_DISPLAY_SUMMARY /*| ACPI_DISPLAY_SHORT*/, ACPI_UINT32_MAX, ACPI_OWNER_ID_MAX, AcpiGbl_RootNode);

	return status;
}


/**
 * Switch to APIC mode for querying PIC information
 * mode : 0 = PIC 
 *        1 = APIC
 *        2 = SAPIC
 */
ACPI_STATUS SetApicMode(UINT64 mode)
{
   ACPI_OBJECT arg1;
   ACPI_OBJECT_LIST args;
   ACPI_STATUS ret;

   // 0 = PIC
   // 1 = APIC
   // 2 = SAPIC
   arg1.Type = ACPI_TYPE_INTEGER;
   arg1.Integer.Value = mode;
   args.Count = 1;
   args.Pointer = &arg1;

   if (ACPI_FAILURE(ret = AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &args, NULL)))
   {
      return ret;
   }

   return AE_OK;
}

ACPI_NAMESPACE_NODE *
AcpiDbConvertToNode (
    char                    *InString);

void
AcpiRsDumpIrqList (
    UINT8                   *RouteTable);

void
AcpiDbDisplayResources (
    char                    *ObjectArg);
    
/**
 * Dumps the APIC IRQ table for a given bus (path).
 * In order to get the APIC IRQ table info, we need to switch to APIC mode first
 */
ACPI_STATUS DumpIRQRoutingTable(UINT8 mode, char* bus)
{
	ACPI_STATUS status = AE_OK;
    ACPI_NAMESPACE_NODE* node = 0;
    ACPI_SIZE length = 0;
    ACPI_BUFFER returnBuffer;
	ACPI_FUNCTION_TRACE (DumpIRQRoutingTable);

    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "DumpIRQRoutingTable started\r\n"));

        SetApicMode(mode);

        node = AcpiDbConvertToNode(bus);
        if (node == 0)
        {
    	    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiDbConvertToNode failed\r\n"));
            return AE_NULL_OBJECT;
        }

        returnBuffer.Length = 0;
        returnBuffer.Pointer = 0;
        status = AcpiGetIrqRoutingTable (node, &returnBuffer);
        if (status != AE_BUFFER_OVERFLOW)
        {
    	    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiGetIrqRoutingTable 1st failed: %d\r\n", status));
            return status;
        }
        
        // returnBuffer.Length now contains requested size
        length = ((returnBuffer.Length + 255) >> 8) << 8; // round to nearest 255
        returnBuffer.Pointer = AcpiOsAllocate( length );
        if ( returnBuffer.Pointer == 0 )
        {
    	    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiOsAllocate failed: out-of-memory\r\n"));
            return AE_NO_MEMORY;
        }
        status = AcpiGetIrqRoutingTable (node, &returnBuffer);

        if (ACPI_FAILURE (status))
        {
    	    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiGetIrqRoutingTable 2nd failed: %d\r\n", status));
            return status;
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "AcpiRsDumpIrqList\r\n"));

        AcpiRsDumpIrqList (ACPI_CAST_PTR (UINT8, returnBuffer.Pointer));

        AcpiOsFree( returnBuffer.Pointer );

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "DumpIRQRoutingTable finished\r\n"));
    }

    return AE_OK;
}

/**
 * Dumps all resources to logging
 */
ACPI_STATUS DumpResources()
{
	ACPI_STATUS status = AE_OK;
	ACPI_FUNCTION_TRACE (DumpResources);

    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "DumpResources started\r\n"));

        AcpiDbDisplayResources(NULL);

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "DumpResources finished\r\n"));
    }

    return AE_OK;
}


void
AcpiDmDumpMadt (
    ACPI_TABLE_HEADER       *Table);

ACPI_STATUS
AcpiTbFindTable (
    char                    *Signature,
    char                    *OemId,
    char                    *OemTableId,
    UINT32                  *TableIndex);

ACPI_STATUS
AcpiGetTableHeader (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       *OutTableHeader);

#if (_WIN32_WCE < 0x800)

HINSTANCE  hInstCoreDll = NULL;

__time32_t time(__time32_t *timer)
{
	__time32_t t;
	memset(&t, 0,  sizeof(t));
    OALMSG(ZONE_ACPI , (L"_time32 not implemented\r\n"));
	return t;
}
char *ctime( const __time32_t *timer )
{
    OALMSG(ZONE_ACPI , (L"_ctime32 not implemented\r\n"));
	return NULL;
}
struct tm *localtime(const __time32_t *timer)
{
    OALMSG(ZONE_ACPI , (L"_localtime32 not implemented\r\n"));
	return NULL;
}

BOOL	CloseHandle(
    HANDLE hObject
    )
{
	OALMSG(ZONE_ACPI , (L"CloseHandle not implemented\r\n"));
	return 1;
}

VOID WINAPI xxx_SetLastError(DWORD dwError)
{
	OALMSG(ZONE_ACPI , (L"xxx_SetLastError not implemented\r\n"));
	return ;
}

LONG xxx_RegCloseKey (HKEY hKey)
{
	OALMSG(ZONE_ACPI , (L"xxx_RegCloseKey not implemented\r\n"));
	return 0;
}

LONG xxx_RegQueryValueExW (HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	OALMSG(ZONE_ACPI , (L"xxx_RegQueryValueExW not implemented\r\n"));
	return 0;
}

LONG xxx_RegOpenKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	OALMSG(ZONE_ACPI , (L"xxx_RegOpenKeyExW not implemented\r\n"));
	return 0;	
}

DWORD xxx_SetFilePointer (HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	OALMSG(ZONE_ACPI , (L"xxx_SetFilePointer not implemented\r\n"));
	return 0;	
}

BOOL xxx_SetStdioPathW(DWORD id, LPCWSTR pwszPath)
{
	OALMSG(ZONE_ACPI , (L"xxx_SetStdioPathW not implemented\r\n"));
	return 0;	
}

HANDLE xxx_CreateFileW (LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	OALMSG(ZONE_ACPI , (L"xxx_CreateFileW not implemented\r\n"));
	return 0;
}

BOOL xxx_GetStdioPathW(DWORD id, PWSTR pwszBuf, LPDWORD lpdwLen)
{
	OALMSG(ZONE_ACPI , (L"xxx_GetStdioPathW not implemented\r\n"));
	return 0;
}

BOOL xxx_FlushFileBuffers (HANDLE hFile)
{
	OALMSG(ZONE_ACPI , (L"xxx_FlushFileBuffers not implemented\r\n"));
	return 0;
}

BOOL xxx_ReadFile (HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	OALMSG(ZONE_ACPI , (L"xxx_ReadFile not implemented\r\n"));
	return 0;
}

BOOL xxx_WriteFile (HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,   LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	OALMSG(ZONE_ACPI , (L"xxx_WriteFile not implemented\r\n"));
	return 0;
}

VOID WINAPI xxx_OutputDebugStringW(LPCWSTR lpOutputString)
{
	OALMSG(ZONE_ACPI , (L"xxx_OutputDebugStringW not implemented\r\n"));
	return 0;
}


FARPROC WINAPI GetProcAddressA(__in HMODULE hModule, __in LPCSTR lpProcName   )
{
	OALMSG(ZONE_ACPI , (L"xxx_GetStdioPathW not implemented\r\n"));
	return ;
}

void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
	return ;
}

//------------------------------------------------------------------------------
//
//  Return a pointer to a char* for use by strtok.
//
char**
__crt_get_storage_strtok(
    )
{
    static char* dataPtr;
    return &dataPtr;
}


BOOL
WINAPI
GetStringTypeExW(
    LCID     Locale,
    DWORD    dwInfoType,
    __in_ecount(cchSrc) LPCWSTR  lpSrcStr,
    int      cchSrc,
    __out LPWORD  lpCharType) 
{ 
    UNREFERENCED_PARAMETER(Locale);
    UNREFERENCED_PARAMETER(dwInfoType);
    UNREFERENCED_PARAMETER(cchSrc);

    if (lpSrcStr[0] == L' ')
        *lpCharType = _SPACE;

    else if (lpSrcStr[0] >= L'0' && lpSrcStr[0] <= L'9')
        *lpCharType = _DIGIT;

    else if (lpSrcStr[0] == L'-' || lpSrcStr[0] == L',')
        *lpCharType = _PUNCT;

    else if (lpSrcStr[0] >= L'A' && lpSrcStr[0] <= L'F')
        *lpCharType = _UPPER | _HEX | _ALPHA;

    else if (lpSrcStr[0] >= L'a' && lpSrcStr[0] <= L'f')
        *lpCharType = _LOWER | _HEX | _ALPHA;

    else if (lpSrcStr[0] == 0)
        *lpCharType = _CONTROL;

    // This is missing most of the characters not expected in the Range. see _wctype[]
    else 
    {
 //       ASSERT(FALSE);
        return FALSE;
    }

    return TRUE; 
} 

///////////////////////////////////////////////////////////////////////////////
// This function maps a character string to a wide-character (Unicode) string. 
//
// used by isdigit/_isctype
//
int
WINAPI
MultiByteToWideChar(
    UINT     CodePage,
    DWORD    dwFlags,
    __in LPCSTR   lpMultiByteStr,
    int      cbMultiByte,
    __out_ecount(cchWideChar) LPWSTR  lpWideCharStr,
    int      cchWideChar) 
{
    UNREFERENCED_PARAMETER(CodePage);
    UNREFERENCED_PARAMETER(dwFlags);

    // a) we only support single char conversions
    // b) we don't support DB, so assert it is 7bit clean
    if (cbMultiByte == cchWideChar == 1 &&
       (lpMultiByteStr[0] & 0x80) == 0)
    {
        lpWideCharStr[0] = lpMultiByteStr[0];
        return 1; 
    }

 //   ASSERT(FALSE);
    return 0;
} 

///////////////////////////////////////////////////////////////////////////////
// Determines if a specified character is potentially a lead byte. 
// A lead byte is the first byte of a two-byte character in a double-byte character set (DBCS). 
//
// used by _isctype
//
BOOL
WINAPI
IsDBCSLeadByte(BYTE TestChar) 
{
    UNREFERENCED_PARAMETER(TestChar);

    // we don't support DB, so assert it is 7bit clean
//    ASSERT((TestChar & 0x80) == 0);

    return FALSE; 
} 

///////////////////////////////////////////////////////////////////////////////
// Converts a character string or a single character to lower/upper case. 
// If the operand is a character string, the function converts the characters in place. 
// 
// unused, unimplemented stubs which the linker wants
//
LPWSTR WINAPI CharLowerW(__inout LPWSTR lpsz) 
{
//	ASSERT(FALSE);
	return lpsz;
} 

LPWSTR WINAPI CharUpperW(__inout LPWSTR lpsz) 
{
//	ASSERT(FALSE); 
	return lpsz; 
} 

#endif//wec7


