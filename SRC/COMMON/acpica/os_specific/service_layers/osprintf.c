

// Small printf source code
// without html formatting: source code for printf.c with stdarg.h
// http://www.menie.org/georges/embedded/#printf

/*
	Copyright 2001, 2002 Georges Menie (www.menie.org)
	stdarg version contributed by Christian Ettinger

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
	putchar is the only external dependency for this file,
	if you have a working putchar, leave it commented out.
	If not, uncomment the define below and
	replace outbyte(c) by your own function call.

#define putchar(c) outbyte(c)
*/

#include <stdarg.h>

static int putchar(int c)
{
    return -1;
}

static void printchar(char **str, int c)
{
	extern int putchar(int c);
	
	if (str) {
		**str = c;
		++(*str);
	}
	else (void)putchar(c);
}

#define PAD_RIGHT  1
#define PAD_ZERO   2

static int prints(char **out, const char *string, int width, int precision, int pad)
{
	register int pc = 0, padchar = ' ';
    const char* from = string;
    const char* to = 0;
    register int len = 0;
	register const char *ptr = string;
	for ( ; *ptr; ++ptr) ++len;
    to = ptr;

    if (pad & PAD_ZERO) padchar = '0';

	if (width > 0 && precision > 0) {
		if (len >= width) {
            to = from + precision;
            width -= precision;
        } else width -= len;
	} else if (width > 0) {
        width -= len;
    } else if (precision > 0) {
        to = from + precision;
    }

	if (!(pad & PAD_RIGHT)) {
		for ( ; width > 0; --width) {
			printchar (out, padchar);
			++pc;
		}
	}
	for ( ptr = from ; ptr != to; ++ptr) {
		printchar (out, *ptr);
		++pc;
	}
	for ( ; width > 0; --width) {
		printchar (out, padchar);
		++pc;
	}

	return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 64

// i         = integer number
// b         = base
// sg        = sign  1 => use sign
// width     = width
// precision = precision
// pad       = pad
// let_base  = letter base
static int printi(char **out, int i, int b, int sg, int width, int precision, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	register char *s;
	register int t, neg = 0, pc = 0;
	register unsigned int u = i;
    register int len = 0;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, precision, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u) {
		t = u % b;
		if( t >= 10 )
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
        ++len;
	}

    for (t = precision - len; t > 0; --t) *--s = '0';  // pad with '0' for precision

	if (neg) {
		if( width && (pad & PAD_ZERO) ) {
			printchar (out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
	}

    if (len > precision) precision = len;  // precision always minimum full number length

	return pc + prints (out, s, width, precision, pad);
}

static int print(char **out, const char *format, va_list args )
{
	register int width, precision, pad;
	register int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = precision = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			while (*format == ' ') {
				++format;
			}
            // width
			if( *format == '*' ) {  // * via argument
                ++format;
                width = va_arg( args, int );
            } else
            {
			    for ( ; *format >= '0' && *format <= '9'; ++format) {
				    width *= 10;
				    width += *format - '0';
			    }
            }
            // .precision
			if( *format == '.' ) {
                ++format;
			    if( *format == '*' ) {  // * via argument
                    ++format;
                    precision = va_arg( args, int );
                } else
                {
			        for ( ; *format >= '0' && *format <= '9'; ++format) {
				        precision *= 10;
				        precision += *format - '0';
			        }
                }
            }
			if( *format == 's' ) {
				register char *s = (char *)va_arg( args, int );
				pc += prints (out, s?s:"(null)", width, precision, pad);
				continue;
			}
			if( *format == 'l' ) {
				++format;
				if ( *format != 'd' && *format != 'x' && *format != 'X' && *format != 'u') {
					continue;
				} else {
					// continue with %ld %lx %lX %lu parsing
				}
			}
			if( *format == 'd' ) {
				pc += printi (out, va_arg( args, int ), 10, 1, width, precision, pad, 'a');
				continue;
			}
			if( *format == 'x' ) {
				pc += printi (out, va_arg( args, int ), 16, 0, width, precision, pad, 'a');
				continue;
			}
			if( *format == 'X' ) {
				pc += printi (out, va_arg( args, int ), 16, 0, width, precision, pad, 'A');
				continue;
			}
			if( *format == 'p' ) {
				pc += printi (out, va_arg( args, int ), 16, 0, 8, 8, PAD_ZERO, 'A');
				continue;
			}
			if( *format == 'u' ) {
				pc += printi (out, va_arg( args, int ), 10, 0, width, precision, pad, 'a');
				continue;
			}
			if( *format == 'c' ) {
				/* char are converted to int then pushed on the stack */
				scr[0] = (char)va_arg( args, int );
				scr[1] = '\0';
				pc += prints (out, scr, width, precision, pad);
				continue;
			}
		}
		else {
		out:
			printchar (out, *format);
			++pc;
		}
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

int AcpiOs_printf(const char *format, ...)
{
    va_list args;
        
    va_start( args, format );
    return print( 0, format, args );
}

int AcpiOs_sprintf(char *out, const char *format, ...)
{
    va_list args;
        
    va_start( args, format );
    return print( &out, format, args );
}

int AcpiOs_vprintf(const char* format, va_list args)
{
    return print( 0, format, args );
}

int AcpiOs_vsprintf(char* out, const char* format, va_list args)
{
    return print( &out, format, args );
}

int AcpiOs_AnsiToWideChar(unsigned short* dest, const int destSize, const char* src, const int srcSize)
{
    int i = 0;
    const char* pSrc = src;
    for (i = 0; (i < destSize) && (i < srcSize) && (*pSrc != '\0'); ++i)
    {
        dest[i] = (*(pSrc++));
    }
    if (i >= destSize) i = destSize - 1;
    dest[i] = 0;

    return i;
}

