
#include "preload.h"

#include "alloc_info.h"
#include "global_storage.h"
#include "log.h"
#include "log_name.h"
#include "macros.h"
#include "overrides.h"
#include "stacktrace.h"

#include <cstddef>
#include <cstdlib>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

// todo: Get from environ configs
constexpr size_t TrackDepth = 32;

namespace hooks
{
enum class HookType
{
    Required,
    Optional
};

template <typename Signature, typename Base, HookType Type>
struct hook
{
    Signature original = nullptr;

    void init() noexcept
    {
        auto ret = dlsym(RTLD_NEXT, Base::identifier);
        if (!ret && Type == HookType::Optional) {
            return;
        }
        if (!ret) {
            fprintf(stderr, "Could not find original function %s\n", Base::identifier);
            abort();
        }
        original = reinterpret_cast<Signature>(ret);
    }

    template <typename... Args>
    auto operator()(Args... args) const noexcept -> decltype(original(args...))
    {
        return original(args...);
    }

    explicit operator bool() const noexcept
    {
        return original;
    }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

#define HOOK(name, type)                                                                                               \
    struct name##_t : public hook<decltype(&::name), name##_t, type>                                                   \
    {                                                                                                                  \
        static constexpr const char* identifier = #name;                                                               \
    } name

HOOK(malloc, HookType::Required);
HOOK(free, HookType::Required);
HOOK(calloc, HookType::Required);
HOOK(realloc, HookType::Required);
HOOK(dlopen, HookType::Required);
HOOK(dlclose, HookType::Required);


HOOK(posix_memalign, HookType::Optional);
// todo: think about optional valloc, aligned_alloc, pvalloc
HOOK(valloc, HookType::Optional);
HOOK(aligned_alloc, HookType::Optional);

#pragma GCC diagnostic pop
#undef HOOK

/**
 * Dummy implementation, since the call to dlsym from findReal triggers a call
 * to calloc.
 *
 * This is only called at startup and will eventually be replaced by the
 * "proper" calloc implementation.
 */
struct DummyPool
{
    static const constexpr size_t MAX_SIZE = 8 * 1024;
    char buf[MAX_SIZE] = {};
    size_t offset = 0;

    bool isDummyAllocation(void* ptr) noexcept
    {
        return ptr >= buf && ptr < buf + MAX_SIZE;
    }

    void* alloc(size_t num, size_t size) noexcept
    {
        size_t oldOffset = offset;
        offset += num * size;
        if (offset >= MAX_SIZE) {
            fprintf(stderr,
                    "failed to initialize, dummy calloc buf size exhausted: "
                    "%zu requested, %zu available\n",
                    offset, MAX_SIZE);
            abort();
        }
        return buf + oldOffset;
    }
};

DummyPool& dummyPool()
{
    static DummyPool pool;
    return pool;
}

void* dummy_calloc(size_t num, size_t size) noexcept
{
    return dummyPool().alloc(num, size);
}

void init()
{
    hooks::calloc.original = &dummy_calloc;

    hooks::calloc.init();
    hooks::malloc.init();
    hooks::free.init();
    hooks::realloc.init();

    hooks::posix_memalign.init();
    hooks::valloc.init();
    hooks::aligned_alloc.init();

    {
        const auto str = GetProcessLogName("log");
        LogInit(str.c_str());
        // register deinitialisation of log system
        std::atexit(LogDeinit);
    }
    LogInfo("[" fI32 "]", getpid());
}

}; // namespace hooks

constexpr size_t AdditionalSize = sizeof(AllocInfo);
static_assert(alignof(max_align_t) == sizeof(AllocInfo));

void* hawk_malloc(size_t size)
{
    LogDebug("requested: " fSzt, size);
    if (unlikely(!hooks::malloc)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);

    auto totalSize = size + AdditionalSize;
    void* ptr = hooks::malloc(totalSize);

    AllocInfo* info = reinterpret_cast<AllocInfo*>(ptr);
    info->offset = AdditionalSize;
    info->size = size;
    info->traceHash = 0;
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;

    LogDebug("result: " fPtr, ptr);

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return ptr;
}

void* hawk_valloc(size_t size)
{
    LogDebug("requested: " fSzt, size);

    if (unlikely(!hooks::valloc)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);

    auto totalSize = size + AdditionalSize;
    void* ptr = hooks::valloc(totalSize);
    AllocInfo* info = reinterpret_cast<AllocInfo*>(ptr);
    info->offset = AdditionalSize;
    info->size = size;
    info->traceHash = 0;
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;

    LogDebug("result: " fPtr, ptr);

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return ptr;
}

void* hawk_aligned_alloc(size_t align, size_t size)
{
    LogDebug("requested: " fSzt, size);

    if (unlikely(!hooks::aligned_alloc)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);

    auto alignedSize = (AdditionalSize + align - 1) / align * align;
    void* ptr = hooks::aligned_alloc(align, (size + alignedSize));
    AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) + alignedSize - AdditionalSize);
    info->offset = alignedSize;
    info->size = size;
    info->traceHash = 0;
    ptr = reinterpret_cast<char*>(ptr) + alignedSize;

    LogDebug("result: " fPtr, ptr);

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return ptr;
}

int hawk_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    LogDebug("requested: " fSzt, size);

    if (unlikely(!hooks::posix_memalign)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);

    auto alignedSize = (AdditionalSize + alignment - 1) / alignment * alignment;
    int res = hooks::posix_memalign(memptr, alignment, size + alignedSize);
    LogDebug("result: " fPtr, *memptr);
    if (res != 0) {
        return res;
    }

    AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(*memptr) + alignedSize - AdditionalSize);
    info->offset = alignedSize;
    info->size = size;
    info->traceHash = 0;
    *memptr = reinterpret_cast<char*>(*memptr) + alignedSize;

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return res;
}

void* hawk_calloc(size_t nm, size_t size)
{
    LogDebug("requested: " fSzt " " fSzt, nm, size);

    if (unlikely(!hooks::calloc)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);

    size_t totalSize = nm * size + AdditionalSize;
    void* ptr = hooks::calloc(1ul, totalSize);
    if (unlikely(ptr == nullptr)) {
        return ptr;
    }

    AllocInfo* info = reinterpret_cast<AllocInfo*>(ptr);
    info->offset = AdditionalSize;
    info->size = nm * size;
    info->traceHash = 0;
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    LogDebug("result: " fPtr, ptr);

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return ptr;
}

void* hawk_realloc(void* ptr, size_t size)
{
    LogDebug("requested: " fPtr " " fSzt, ptr, size);

    if (unlikely(!hooks::realloc)) {
        hooks::init();
    }
    auto trace = Stacktrace::Unwind(TrackDepth);


    if (ptr) {
        AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
        ptr = reinterpret_cast<char*>(ptr) - info->offset;

        if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
            auto memhawk = storage->GetMemHawk();
            memhawk->TrackDealloc(*info, trace);
        }
    }
    void* realloced = hooks::realloc(ptr, size + AdditionalSize);
    if (unlikely(realloced == nullptr)) {
        return ptr;
    }

    AllocInfo* info = reinterpret_cast<AllocInfo*>(realloced);
    info->offset = AdditionalSize;
    info->size = size;
    info->traceHash = 0;
    realloced = reinterpret_cast<char*>(realloced) + AdditionalSize;
    LogDebug("result: " fPtr, realloced);

    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        // save trace hash only after initialisation of GlobalStorage
        info->traceHash = trace.Hash();
        auto memhawk = storage->GetMemHawk();
        memhawk->TrackAlloc(*info, std::move(trace));
    }
    return realloced;
}

void* hawk_pvalloc(size_t /*size*/)
{
    LogError("pvalloc is deprecated and wasn't implemented");
    abort();
}

void hawk_free(void* ptr)
{
    if (!ptr || unlikely(hooks::dummyPool().isDummyAllocation(ptr))) {
        // don't track nullptr
        return;
    }
    LogDebug("requested: " fPtr, ptr);

    if (unlikely(!hooks::free)) {
        hooks::init();
    }

    AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    ptr = reinterpret_cast<char*>(ptr) - info->offset;
    if (auto storage = GlobalStorage::GetGlobalStorage(); storage) {
        auto memhawk = storage->GetMemHawk();
        auto trace = Stacktrace::Unwind(TrackDepth);
        memhawk->TrackDealloc(*info, trace);
    }
    hooks::free(ptr);
}

size_t hawk_malloc_usable_size(void* ptr)
{
    AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    return info->size;
}

void* HawkInternalNew(size_t size) noexcept(false)
{
    return hawk_malloc(size);
}

void HawkInternalDelete(void* p) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteSized(void* p, size_t /*size*/) noexcept
{
    return hawk_free(p);
}

void* HawkInternalNewArray(size_t size) noexcept(false)
{
    return hawk_malloc(size);
}

void HawkInternalDeleteArray(void* p) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteArraySized(void* p, size_t /*size*/) noexcept
{
    return hawk_free(p);
}

void* HawkInternalNewNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept
{
    return hawk_malloc(size);
}

void* HawkInternalNewArrayNothrow(size_t size, const std::nothrow_t& /*nt*/) noexcept
{
    return hawk_malloc(size);
}

void HawkInternalDeleteNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteArrayNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept
{
    return hawk_free(p);
}

void* HawkInternalNewAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    return hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void* HawkInternalNewAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteSizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    return hawk_free(p);
}

void* HawkInternalNewArrayAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    return hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void* HawkInternalNewArrayAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteArrayAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteArrayAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    return hawk_free(p);
}

void HawkInternalDeleteArraySizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    return hawk_free(p);
}
