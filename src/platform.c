#include <assert.h>
#include <stdlib.h>
#include "platform.h"

void *my_aligned_alloc(size_t align, size_t size)
{
    // align must be a power of two, at least the pointer size, to avoid misaligned memory access
    assert(align >= sizeof(uintptr_t) && !(align & (align - 1)) && size);

    // allocate extra head room to store and pointer and padding bytes to align
    const uintptr_t base = (uintptr_t)malloc(size + sizeof(uintptr_t) + align - 1);

    // start adress of the aligned memory chunk (ie. return value)
    const uintptr_t start = (base + sizeof(uintptr_t) + align - 1) & ~(align - 1);

    // Remember the original base pointer returned by malloc(), so that we can free() it.
    *((uintptr_t *)start - 1) = base;

    return (void *)start;
}

void my_aligned_free(void *p)
// WARNING: Only use if p was created by my_aligned_alloc()
{
    if (p)
        // Original base pointer is stored sizeof(uintptr_t) bytes behind p
        free((void *)*((uintptr_t *)p - 1));
}
