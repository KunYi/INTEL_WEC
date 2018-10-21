
#include "osmalloc.h"

// defines
#define _data_type_align2(x)    ((((x) + 1) >> 1) << 1)
#define _data_type_align4(x)    ((((x) + 3) >> 2) << 2)
#define _data_type_align8(x)    ((((x) + 7) >> 3) << 3)

#define _data_type_size2(x)     ((x) >> 1);
#define _data_type_size4(x)     ((x) >> 2);
#define _data_type_size8(x)     ((x) >> 3);

// our choice (chosen such that it aligns properly on 32bit or 64bit)
typedef int                    _data_type;
#define _data_type_align(x)    _data_type_align4(x)
#define _data_type_size(x)     _data_type_size4(x)

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifndef _INTPTR_T_DEFINED
#ifdef  _WIN64
typedef __int64             intptr_t;
#else
#define _W64 __w64
typedef _W64 int            intptr_t;
#endif
#define _INTPTR_T_DEFINED
#endif

#define MEM_BASE 0x220000
#define MAXIMUM_MEMORY   0x100000

// definitions
#ifdef __cplusplus
extern "C"
{
    struct s_block;
    typedef struct s_block*  t_block;
    const size_t MIN_DATA_SIZE = sizeof(_data_type);
}
#else   // __cplusplus

    #define  MIN_DATA_SIZE  sizeof(_data_type)
    #define t_block   struct s_block*

#endif  // __cplusplus

typedef struct s_block
{
    size_t    size;
    t_block   next;
    t_block   prev;
    int       free;
    void*     ptr;
    char      data[MIN_DATA_SIZE];   // this properly aligns
} s_block;

#ifdef __cplusplus
extern "C"
{
    const size_t BLOCK_SIZE = sizeof(s_block) - MIN_DATA_SIZE;  // data starts already at char data[1]
    const intptr_t MAX_MEMORY = /*262144*/ MAXIMUM_MEMORY;
}
#else   // __cplusplus
    #define  BLOCK_SIZE   ((size_t)(sizeof(s_block) - MIN_DATA_SIZE))
    #define  MAX_MEMORY   MAXIMUM_MEMORY
#endif  // __cplusplus

void os_init();
int os_init_memory(_data_type* ptr, const int N, const _data_type value);
void *os_sbrk(intptr_t increment);
int os_brk(void* end_data_segment);
void os_split_block (t_block b, size_t size);
t_block os_find_block (t_block *last, size_t size);
t_block os_extend_heap(t_block last , size_t size);
t_block os_fusion(t_block b);
t_block os_get_block (void *p);
int os_valid_addr(void *p);
void os_copy_block (t_block src , t_block dst);

enum LogItem
{
    BASIC  = 0x01,
    BLOCK  = 0x02,
    MEMORY = 0x04,
	ERROR  = 0x80,
    ALL = ERROR,
};
int g_whatToLog = ALL;
int os_log(const int what, const char* format, ...);

#ifdef NDEBUG
#define LOG(format, ...)   (void)0
#else
#define LOG(format, ...)  os_log(format, __VA_ARGS__);
#endif

void *g_base = NULL;
intptr_t g_memory = 0;
char *g_buffer; // = 0x220000;
int g_buffer_int = 0;

void *os_sbrk(intptr_t increment)
{
    intptr_t newMemory = (intptr_t)g_memory + increment;
    if (newMemory < 0 || newMemory >= MAX_MEMORY)
    {
        LOG(MEMORY | ERROR, "os_sbrk() out of memory\r\n");
        return (void*)-1;
    }
    g_memory = newMemory;
    LOG(MEMORY, "os_sbrk() %d bytes in use\r\n", g_memory);
    return &g_buffer[g_memory];
}

int os_brk(void* end_data_segment)
{
    intptr_t memory = (intptr_t)end_data_segment - (intptr_t)g_buffer;
    if (memory < 0 || memory >= MAX_MEMORY)
    {
        LOG(MEMORY | ERROR, "os_brk() out of memory\r\n");
        return -1;
    } else
    {
        LOG(MEMORY, "os_brk() %d bytes in use\r\n", g_memory);
        g_memory = memory;
        return 0;
    }
}

unsigned int AcpiPhysicalAddressToVirtual(unsigned int dwPhyAddr, unsigned int* pdwSize);
// external declarations (Note all functions are embraced with 'extern "C"')
//LPVOID NKCreateStaticMapping(DWORD dwPhysBase, DWORD dwSize);
//BOOL   NKDeleteStaticMapping(LPVOID pVirtAddr, DWORD dwSize);

void os_init()
{
    LOG(MEMORY, "os_init()\r\n");
	g_buffer = (char*)NKCreateStaticMapping(MEM_BASE>>8, MAX_MEMORY);
    g_buffer_int = os_init_memory((_data_type*)g_buffer, MAX_MEMORY / sizeof(_data_type), (_data_type)0xDDDDDDDD);
}

int os_init_memory(_data_type* ptr, const int N, const _data_type value)
{
    int i = 0;
    for ( ; i < N; ++i)
    {
        ptr[i] = (_data_type)value;
    }
    LOG(MEMORY, "os_init() init memory at 0x%08X, %d values with 0x%X\r\n", g_buffer, N, value);
    return 1;
}

void *os_malloc(size_t size)
{
    t_block b = NULL;
    t_block last = NULL;
    size_t s = _data_type_align(size);

    LOG(BASIC, "os_malloc() request %d bytes\r\n", size);

    if (g_base)
    { // First find a block
        last = (t_block)g_base;
        b = os_find_block(&last, s);
        if (b)
        { // can we split
            if ((b->size - s) >= (BLOCK_SIZE + MIN_DATA_SIZE))
            {
                os_split_block(b, s);
            }
            b->free = 0;
        } else
        { // No fitting block , extend the heap
            b = os_extend_heap(last, s);
            if (!b)
            {
                LOG(BASIC, "os_malloc() failed\r\n");
                return NULL;
            }
        }
    } else
    { // first time
        os_init();
        b = os_extend_heap(NULL, s);
        if (!b)
        {
            LOG(BASIC, "os_malloc() failed\r\n");
            return NULL;
        }
        g_base = b;
    }
    LOG(BASIC, "os_malloc() return %d bytes at 0x%08X\r\n", size, b->data);
    return b->data;
}

void *os_calloc(size_t number, size_t size)
{
    int newSize = number * size;
    void* newMalloc = os_malloc(newSize);
    if (newMalloc)
    {
        const int n = _data_type_size( _data_type_align(newSize) );
        os_init_memory((_data_type*)newMalloc, n, 0); // zero block
    }
    return newMalloc;
}

void os_free(void *p)
{
    t_block b = NULL;

    LOG(BASIC, "os_free() release bytes at 0x%08X\r\n", p);

    if (os_valid_addr (p))
    {
        b = os_get_block (p);
        b->free = 1;

        if (b->prev && b->prev->free)
        { // fusion with previous if possible
            b = os_fusion(b->prev);
        }
        // then fusion with next
        if (b->next)
        {
            os_fusion(b);
        } else
        { // free the end of the heap
            if (b->prev)
            {
                b->prev ->next = NULL;
            } else
            { // No more block!
                g_base = NULL;
            }
            os_brk(b);
        }
#ifndef NDEBUG
        {
            _data_type* ptr = (_data_type*)b->ptr;
            const int N = _data_type_size( b->size );
            os_init_memory(ptr, N, (_data_type)0xEEEEEEEE);
        }
#endif
        LOG(BASIC, "os_free() freed %d bytes at 0x%08X\r\n", b->size, p);
    } else
    {
        LOG(BASIC, "os_free() freed 0 bytes at 0x%08X. Not found.\r\n", p);
    }
}

void* os_realloc(void *p, size_t size)
{
    size_t s = 0;
    t_block b = NULL;
    t_block newBlock = NULL;
    void *newp = NULL;

    LOG(BASIC, "os_realloc() 0x%08X %d bytes\r\n", p, size);

    if (!p)
    {
        return os_malloc(size);
    }
    if (os_valid_addr(p))
    {
        s = _data_type_align(size);
        b = os_get_block(p);
        if (b->size >= s)
        {
            if (b->size - s >= (BLOCK_SIZE + MIN_DATA_SIZE))
            {
                os_split_block(b, s);
            }
        } else
        {
            /* Try fusion with next if possible */
            if (b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s)
            {
                os_fusion(b);
                if (b->size - s >= (BLOCK_SIZE + MIN_DATA_SIZE))
                {
                    os_split_block(b, s);
                }
            }
            else
            {
                // good old realloc with a new block
                newp = os_malloc(s);
                if (!newp)
                { // we’re doomed!
                    LOG(BASIC, "os_realloc() failed");
                    return NULL;
                }
                // I assume this works
                newBlock = os_get_block(newp);
                // Copy data
                os_copy_block(b, newBlock);
                // free the old one
                os_free(p);

                LOG(BASIC, "os_realloc() returned 0x%08X %d bytes\r\n", b->ptr, b->size);
                return newp;
            }
        }
        LOG(BASIC, "os_realloc() returned 0x%08X %d bytes\r\n", p, b->size);
        return p;
    }
    LOG(BASIC, "os_realloc() failed");
    return NULL;
}

void os_split_block(t_block b, size_t s)
{
    t_block newBlock = (t_block)(b->data + s);

    LOG(BLOCK, "os_split_block() 0x%08X %d bytes => 0x%08X\r\n", b->ptr, b->size, newBlock->ptr);

    newBlock->size = b->size - s - BLOCK_SIZE;
    newBlock->next = b->next;
    newBlock->prev = b;
    newBlock->free = 1;
    newBlock->ptr = newBlock->data;
    b->size = s;
    b->next = newBlock;
    if (newBlock ->next)
    {
        newBlock ->next->prev = newBlock;
    }
}

t_block os_find_block(t_block *last, size_t size)
{
    t_block b = (t_block)g_base;

    LOG(BLOCK, "os_find_block()\r\n");

    while (b != NULL && !(b->free != 0 && b->size >= size))
    {
        *last = b;
        b = b->next;
    }
    return b;
}

void os_copy_block(t_block src , t_block dst)
{
    _data_type* sdata = (_data_type*)src->ptr, *ddata = (_data_type*)dst->ptr;
    int i = 0;

    LOG(BLOCK, "os_copy_block()\r\n");

    for ( ; i * sizeof(_data_type) < src->size && i * sizeof(_data_type) < dst->size; ++i)
    {
        ddata[i] = sdata[i];
    }
}

t_block os_fusion(t_block b)
{
    LOG(BLOCK, "os_fusion()\r\n");

    if (b->next && b->next ->free)
    {
        b->size += (BLOCK_SIZE + b->next->size);
        b->next = b->next->next;
        if (b->next)
        {
            b->next->prev = b;
        }
    }
    return b;
}

t_block os_extend_heap(t_block last, size_t s)
{
    void* sb = 0;
    t_block b = (t_block)os_sbrk(0);

    LOG(MEMORY, "os_extend_heap() request %d bytes\r\n", s);

    sb = os_sbrk(BLOCK_SIZE + s);
    if (sb == (void*)-1)
    {
        LOG(MEMORY, "os_extend_heap() failed\r\n");
        return NULL;
    }
    b->size = s;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    if (last)
    {
        last ->next = b;
    }
    b->free = 0;
    LOG(MEMORY, "os_extend_heap() succeeded %d bytes\r\n");
    return b;
}

t_block os_get_block (void *p)
{
    intptr_t tmp = (intptr_t)p;
    tmp -= BLOCK_SIZE;
    return (t_block)tmp;
}

int os_valid_addr(void *p)
{
    if (g_base)
    {
        if (p > g_base && p < os_sbrk(0))
        {
            t_block b = os_get_block(p);
            return p == b->ptr;
        }
    }
    return 0;
}

// Your own log mechanism
#include <stdio.h>
#include <stdarg.h>

int os_log(const int what, const char* format, ...)
{
    va_list args;
        
    va_start(args, format);
    if (g_whatToLog & what)
    {
#ifdef UNDER_CE
        return vfprintf(0, format, args);
#else
        return vprintf(format, args);
#endif
    } else
    {
        return 0;
    }
}
