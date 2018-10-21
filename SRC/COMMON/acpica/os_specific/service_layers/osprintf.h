
#ifndef _OSPRINTF_H
#define _OSPRINTF_H

int AcpiOs_printf(const char *format, ...);
int AcpiOs_sprintf(char *out, const char *format, ...);
int AcpiOs_vprintf(const char* format, va_list args);
int AcpiOs_vsprintf(char* out, const char* format, va_list args);

int AcpiOs_AnsiToWideChar(unsigned short* dest, const int destSize, const char* src, const int srcSize);

#endif  // _OSPRINTF_H
