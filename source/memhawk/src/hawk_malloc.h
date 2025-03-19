#pragma once

#include <cstddef>

namespace memhawk
{

// replacement for libc functions
void* hawk_malloc(size_t size);
void hawk_free(void* ptr);
void* hawk_realloc(void* ptr, size_t size);
void* hawk_calloc(size_t n, size_t size);
void* hawk_aligned_alloc(size_t align, size_t size);
void* hawk_valloc(size_t size);
void* hawk_pvalloc(size_t size); // calls abort
int hawk_posix_memalign(void** memptr, size_t alignment, size_t size);

// also replace some routines
size_t hawk_malloc_usable_size(void* ptr);

}
