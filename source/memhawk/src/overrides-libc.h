#pragma once


#include <cstddef>
#include <new>

#define ALIAS(fn) __attribute__((alias(#fn), visibility("default"), used))

void* operator new(size_t size) noexcept(false) ALIAS(HawkInternalNew);
void operator delete(void* p) noexcept ALIAS(HawkInternalDelete);
void operator delete(void* p, size_t size) noexcept ALIAS(HawkInternalDeleteSized);

void* operator new[](size_t size) noexcept(false) ALIAS(HawkInternalNewArray);
void operator delete[](void* p) noexcept ALIAS(HawkInternalDeleteArray);
void operator delete[](void* p, size_t size) noexcept ALIAS(HawkInternalDeleteArraySized);

void* operator new(size_t size, const std::nothrow_t& nt) noexcept ALIAS(HawkInternalNewNothrow);
void* operator new[](size_t size, const std::nothrow_t& nt) noexcept ALIAS(HawkInternalNewArrayNothrow);
void operator delete(void* p, const std::nothrow_t& nt) noexcept ALIAS(HawkInternalDeleteNothrow);
void operator delete[](void* p, const std::nothrow_t& nt) noexcept ALIAS(HawkInternalDeleteArrayNothrow);

void* operator new(size_t size, std::align_val_t alignment) noexcept(false) ALIAS(HawkInternalNewAligned);
void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
    ALIAS(HawkInternalNewAlignedNothrow);
void operator delete(void* p, std::align_val_t alignment) noexcept ALIAS(HawkInternalDeleteAligned);
void operator delete(void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept
    ALIAS(HawkInternalDeleteAlignedNothrow);
void operator delete(void* p, size_t size, std::align_val_t alignment) noexcept ALIAS(HawkInternalDeleteSizedAligned);

void* operator new[](size_t size, std::align_val_t alignment) noexcept(false) ALIAS(HawkInternalNewArrayAligned);
void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
    ALIAS(HawkInternalNewArrayAlignedNothrow);
void operator delete[](void* p, std::align_val_t alignment) noexcept ALIAS(HawkInternalDeleteArrayAligned);
void operator delete[](void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept
    ALIAS(HawkInternalDeleteArrayAlignedNothrow);
void operator delete[](void* p, size_t size, std::align_val_t alignment) noexcept
    ALIAS(HawkInternalDeleteArraySizedAligned);


extern "C" {
// libc counterpart
void* __libc_malloc(size_t size) ALIAS(hawk_malloc);
void __libc_free(void* ptr) ALIAS(hawk_free);
void* __libc_realloc(void* ptr, size_t size) ALIAS(hawk_realloc);
void* __libc_calloc(size_t n, size_t size) ALIAS(hawk_calloc);
void* __libc_memalign(size_t align, size_t s) ALIAS(hawk_aligned_alloc);
void* __libc_valloc(size_t size) ALIAS(hawk_valloc);
int __posix_memalign(void** r, size_t a, size_t s) ALIAS(hawk_posix_memalign);

// ordinary libc functions
void* malloc(size_t size) __THROW ALIAS(hawk_malloc);
void free(void* ptr) __THROW ALIAS(hawk_free);
void* realloc(void* ptr, size_t size) __THROW ALIAS(hawk_realloc);
void* calloc(size_t n, size_t size) __THROW ALIAS(hawk_calloc);
void cfree(void* ptr) __THROW ALIAS(hawk_free);
void* memalign(size_t align, size_t s) __THROW ALIAS(hawk_aligned_alloc);
void* aligned_alloc(size_t align, size_t s) __THROW ALIAS(hawk_aligned_alloc);
void* valloc(size_t size) __THROW ALIAS(hawk_valloc);
void* pvalloc(size_t size) __THROW ALIAS(hawk_pvalloc);
int posix_memalign(void** r, size_t a, size_t s) __THROW ALIAS(hawk_posix_memalign);

size_t malloc_usable_size(void* ptr) ALIAS(hawk_malloc_usable_size);

// libdl functions
void* dlopen(const char* file, int mode) __THROWNL ALIAS(hawk_dlopen);
int dlclose(void* handle)__THROWNL __nonnull ((1)) ALIAS(hawk_dlclose);


} // extern "C"
#undef ALIAS
