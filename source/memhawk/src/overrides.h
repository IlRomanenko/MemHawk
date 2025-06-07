#pragma once


#include <cstddef>
#include <new>

extern "C" {

// replacement for libc functions
void* hawk_malloc(size_t size);
void hawk_free(void* ptr);
void* hawk_realloc(void* ptr, size_t size);
void* hawk_calloc(size_t n, size_t size);
void* hawk_aligned_alloc(size_t align, size_t size);
void* hawk_valloc(size_t size);
void* hawk_pvalloc(size_t size); // calls abort
int hawk_posix_memalign(void** memptr, size_t alignment, size_t size);

// libdl functions
void* hawk_dlopen(const char* file, int mode);
int hawk_dlclose(void* handle);

// also replace some routines
size_t hawk_malloc_usable_size(void* ptr);

// replacement for C++ allocations
void* HawkInternalNew(size_t size) noexcept(false);
void HawkInternalDelete(void* p) noexcept;
void HawkInternalDeleteSized(void* p, size_t /*size*/) noexcept;

void* HawkInternalNewArray(size_t size) noexcept(false);
void HawkInternalDeleteArray(void* p) noexcept;
void HawkInternalDeleteArraySized(void* p, size_t /*size*/) noexcept;

void* HawkInternalNewNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept;
void* HawkInternalNewArrayNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept;
void HawkInternalDeleteNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept;
void HawkInternalDeleteArrayNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept;

void* HawkInternalNewAligned(size_t size, std::align_val_t alignment) noexcept(false);
void* HawkInternalNewAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept;
void HawkInternalDeleteAligned(void* p, std::align_val_t /*alignment*/) noexcept;
void HawkInternalDeleteAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept;
void HawkInternalDeleteSizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept;

void* HawkInternalNewArrayAligned(size_t size, std::align_val_t alignment) noexcept(false);
void* HawkInternalNewArrayAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept;
void HawkInternalDeleteArrayAligned(void* p, std::align_val_t /*alignment*/) noexcept;
void HawkInternalDeleteArrayAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept;
void HawkInternalDeleteArraySizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept;

} // extern "C"
