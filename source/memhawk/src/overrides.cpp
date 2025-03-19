#include "overrides.h"

#include "hawk_malloc.h"
#include "overrides-libc.h" // IWYU pragma: export

extern "C" {

void* hawk_malloc(size_t size)
{
    return memhawk::hawk_malloc(size);
}

void* hawk_valloc(size_t size)
{
    return memhawk::hawk_valloc(size);
}

void* hawk_aligned_alloc(size_t align, size_t size)
{
    return memhawk::hawk_aligned_alloc(align, size);
}

int hawk_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    return memhawk::hawk_posix_memalign(memptr, alignment, size);
}

void* hawk_calloc(size_t nm, size_t size)
{
    return memhawk::hawk_calloc(nm, size);
}

void* hawk_realloc(void* ptr, size_t size)
{
    return memhawk::hawk_realloc(ptr, size);
}

void* hawk_pvalloc(size_t size)
{
    return memhawk::hawk_pvalloc(size);
}

void hawk_free(void* ptr)
{
    return memhawk::hawk_free(ptr);
}

size_t hawk_malloc_usable_size(void* ptr)
{
    return memhawk::hawk_malloc_usable_size(ptr);
}

void* HawkInternalNew(size_t size) noexcept(false)
{
    return memhawk::hawk_malloc(size);
}

void HawkInternalDelete(void* p) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteSized(void* p, size_t /*size*/) noexcept
{
    return memhawk::hawk_free(p);
}

void* HawkInternalNewArray(size_t size) noexcept(false)
{
    return memhawk::hawk_malloc(size);
}

void HawkInternalDeleteArray(void* p) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteArraySized(void* p, size_t /*size*/) noexcept
{
    return memhawk::hawk_free(p);
}

void* HawkInternalNewNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept
{
    return memhawk::hawk_malloc(size);
}

void* HawkInternalNewArrayNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept
{
    return memhawk::hawk_malloc(size);
}

void HawkInternalDeleteNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteArrayNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept
{
    return memhawk::hawk_free(p);
}

void* HawkInternalNewAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void* HawkInternalNewAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteSizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    return memhawk::hawk_free(p);
}

void* HawkInternalNewArrayAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void* HawkInternalNewArrayAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteArrayAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteArrayAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_free(p);
}

void HawkInternalDeleteArraySizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    return memhawk::hawk_free(p);
}

} // extern "C"
