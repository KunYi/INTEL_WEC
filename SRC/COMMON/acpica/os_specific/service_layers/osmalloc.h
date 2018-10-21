
#ifndef MYMALLOC_H
#define MYMALLOC_H

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef __w64 unsigned int   size_t;
#endif
#define _SIZE_T_DEFINED
#endif

void *os_malloc(size_t size);
void *os_calloc(size_t number, size_t size);
void *os_realloc(void *p, size_t size);
void os_free(void* p);

#endif // MYMALLOC_H