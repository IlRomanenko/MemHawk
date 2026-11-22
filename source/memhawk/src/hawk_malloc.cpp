
#include "hawk_malloc.h"

#include "impl/alloc_info.h"
#include "impl/checked_math.h"
#include "impl/config.h"
#include "impl/i_stacktrace_tracker.h"
#include "impl/logging.h"
#include "impl/macros.h"
#include "impl/memhawk.h"
#include "impl/stacktrace.h"
#include "impl/writers/factory.h"
#include "overrides-libc.h" // IWYU pragma: export
#include "overrides.h"

#include <absl/base/attributes.h>
#include <absl/base/internal/direct_mmap.h>
#include <re2/re2.h>
#include <sys/mman.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <pthread.h>
#include <unistd.h>

namespace memhawk
{

namespace hooks
{

template <typename Signature, typename Base>
struct hook
{
    Signature original = nullptr;

    void init() noexcept
    {
        auto ret = dlsym(RTLD_NEXT, Base::identifier);
        if (!ret)
        {
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
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

#define DEFINE_HOOK(name)                                                                                              \
    struct name##_t : public hook<decltype(&::name), name##_t>                                                         \
    {                                                                                                                  \
        static constexpr const char* identifier = #name;                                                               \
    } name

// Allocation hooks
DEFINE_HOOK(malloc);
DEFINE_HOOK(free);
DEFINE_HOOK(calloc);
DEFINE_HOOK(realloc);
DEFINE_HOOK(posix_memalign);
DEFINE_HOOK(aligned_alloc);
DEFINE_HOOK(valloc);
// Proc maps hooks
DEFINE_HOOK(dlopen);
DEFINE_HOOK(dlclose);

#pragma GCC diagnostic pop
#undef DEFINE_HOOK

#define RETURN_ENOMEM_ON_FAILURE(x)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (unlikely(!(x)))                                                                                            \
        {                                                                                                              \
            errno = ENOMEM;                                                                                            \
            return nullptr;                                                                                            \
        }                                                                                                              \
    } while (0)

static ABSL_CONST_INIT std::atomic<bool> gl_initialised = false;
static ABSL_CONST_INIT std::atomic<bool> gl_dlInitialised = false;
static ABSL_CONST_INIT std::atomic<bool> gl_memhawkReady = false;
static ABSL_CONST_INIT MemHawk* gl_memhawk = nullptr;
static ABSL_CONST_INIT UnwindConfig gl_unwind = {};

ABSL_ATTRIBUTE_ALWAYS_INLINE MemHawk* GetMemHawk()
{
    if (likely(gl_memhawkReady.load(std::memory_order_acquire)))
    {
        return gl_memhawk;
    }
    return nullptr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE bool CheckInitialized()
{
    return gl_initialised.load(std::memory_order_acquire);
}

void InitDlHooks()
{
    if (gl_dlInitialised.load(std::memory_order_acquire))
    {
        return;
    }
    hooks::dlopen.init();
    hooks::dlclose.init();
    gl_dlInitialised.store(true, std::memory_order_release);
}

void InitHooks()
{
    InitDlHooks();
    hooks::calloc.init();
    hooks::malloc.init();
    hooks::free.init();
    hooks::realloc.init();

    hooks::posix_memalign.init();
    hooks::valloc.init();
    hooks::aligned_alloc.init();
}

bool CheckProgname(std::string_view prognameRegex)
{
    if (prognameRegex.empty())
    {
        return true;
    }
    const re2::RE2 pattern(prognameRegex);
    if (!pattern.ok())
    {
        LogError("Incorrect progname regex: " fStr, pattern.error().c_str());
        return false;
    }
    return re2::RE2::FullMatch(program_invocation_name, pattern);
}

static void PreFork()
{
    gl_memhawk->PreFork();
}

static void ParentPostFork()
{
    gl_memhawk->ParentPostFork();
}

static void ChildPostFork()
{
    gl_memhawk->ChildPostFork();
}

__attribute__((__constructor__)) void init_memhawk()
{
    InitHooks();

    const auto cfg = ParseConfig();
    if (CheckProgname(*cfg.PrognameRegex))
    {
        LogInit(*cfg.Logging);
        gl_unwind = *cfg.Unwind;

        LogInfo("[" fI32 "]", getpid());
        gl_memhawk = new MemHawk(*cfg.MemHawk, std::make_unique<writers::WritersFactory>());
        gl_memhawkReady = true;

        // after this, allocation hooks can postpone traces into memhawk
        gl_initialised = true;
        gl_memhawk->PostponedConstruct();

        // set up atfork handlers
        pthread_atfork(PreFork, ParentPostFork, ChildPostFork);
    }
    else
    {
        // allow hooks usage
        gl_initialised = true;
        gl_loggingLevel = LogLevel::Off;
    }
}

__attribute__((__destructor__)) void deinit_memhawk()
{
    if (gl_memhawk)
    {
        gl_memhawkReady = false;
        gl_memhawk->Stop();
        // intentionally leak MemHawk
        // it's safe because the process will be terminated by OS after the main thread exited
        // it's necessary because there can exist detached threads that continue to operate even after
        // main thread started termination process
    }
    LogDeinit();
}

}; // namespace hooks

constexpr size_t AdditionalSize = sizeof(AllocInfo);
// Ensure that we won't ruin malloc invariant
static_assert(alignof(max_align_t) == sizeof(AllocInfo));

ABSL_ATTRIBUTE_ALWAYS_INLINE void TrackAllocation(void* userPtr, size_t userSize, uint32_t offset,
                                                  RecursiveStacktrace& stacktrace)
{
    auto* infoLocation = reinterpret_cast<char*>(userPtr) - AdditionalSize;
    AllocInfo* info = new (infoLocation) AllocInfo{userSize, offset};
    LogTrace("result: " fPtr, userPtr);

    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void TrackReallocation(void* userPtr, size_t userSize, uint32_t offset,
                                                    const AllocInfo& prev, RecursiveStacktrace& stacktrace)
{
    auto* infoLocation = reinterpret_cast<char*>(userPtr) - AdditionalSize;
    AllocInfo* info = new (infoLocation) AllocInfo{userSize, offset};
    LogTrace("result: " fPtr, userPtr);

    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackRealloc(prev, *info, trace, stacktrace.IsExternal());
    }
}

// Anonymous mappings provide zero-filled storage for bootstrap calloc.
void* mmap_malloc(size_t size)
{
    const auto totalSize = CheckedAdd(size, AdditionalSize);
    RETURN_ENOMEM_ON_FAILURE(totalSize);

    void* ptr =
        absl::base_internal::DirectMmap(nullptr, *totalSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (unlikely(ptr == MAP_FAILED))
    {
        return nullptr;
    }

    auto* info = new (ptr) AllocInfo{size, AdditionalSize};
    info->traceId = InvalidTraceId;
    return static_cast<char*>(ptr) + AdditionalSize;
}

void* mmap_alloc_aligned(size_t size, size_t alignment)
{
    if (unlikely(!std::has_single_bit(alignment)))
    {
        errno = EINVAL;
        return nullptr;
    }

    constexpr auto MaxOffset = std::numeric_limits<uint32_t>::max();
    alignment = std::max(alignment, alignof(max_align_t));
    if (unlikely(alignment > MaxOffset))
    {
        errno = ENOMEM;
        return nullptr;
    }

    const auto mappingSize = CheckedAdd(CheckedAdd(size, AdditionalSize), alignment - 1);
    RETURN_ENOMEM_ON_FAILURE(mappingSize);

    void* basePtr =
        absl::base_internal::DirectMmap(nullptr, *mappingSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (unlikely(basePtr == MAP_FAILED))
    {
        return nullptr;
    }

    auto* payloadStart = static_cast<char*>(basePtr) + AdditionalSize;
    // find suitable alignment for user data
    const auto remainder = reinterpret_cast<uintptr_t>(payloadStart) & (alignment - 1);
    const auto padding = remainder == 0 ? 0 : alignment - remainder;
    const auto offset = AdditionalSize + padding;
    if (unlikely(offset > MaxOffset))
    {
        absl::base_internal::DirectMunmap(basePtr, *mappingSize);
        errno = ENOMEM;
        return nullptr;
    }

    auto* userPtr = static_cast<char*>(basePtr) + offset;
    auto* info = new (userPtr - AdditionalSize) AllocInfo{*mappingSize - offset, static_cast<uint32_t>(offset)};
    info->traceId = InvalidTraceId;
    return userPtr;
}

void mmap_free(void* base, const AllocInfo& info)
{
    const auto mappingSize = info.offset + info.size;
    absl::base_internal::DirectMunmap(base, mappingSize);
}

void* mmap_realloc(void* ptr, size_t size)
{
    if (!ptr)
    {
        return mmap_malloc(size);
    }
    const AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    auto origPtr = reinterpret_cast<char*>(ptr) - info->offset;

    auto realloced = mmap_malloc(size);
    if (unlikely(!realloced))
    {
        return nullptr;
    }
    memcpy(realloced, ptr, std::min(info->size, size));
    mmap_free(origPtr, *info);

    return realloced;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_malloc(size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_malloc(size);
    }
    LogTrace("requested: " fSzt, size);

    auto totalSize = CheckedAdd(size, AdditionalSize);
    RETURN_ENOMEM_ON_FAILURE(totalSize);

    void* raw = hooks::malloc(*totalSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + AdditionalSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, size, AdditionalSize, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_aligned_alloc(size_t alignment, size_t size)
{
    if (unlikely(!std::has_single_bit(alignment)))
    {
        errno = EINVAL;
        return nullptr;
    }
    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_alloc_aligned(size, alignment);
    }
    LogTrace("requested: " fSzt, size);

    const auto alignedSize = CheckedAlignUp(AdditionalSize, alignment);
    if (unlikely(!alignedSize || *alignedSize > std::numeric_limits<uint32_t>::max()))
    {
        errno = ENOMEM;
        return nullptr;
    }
    const auto totalSize = CheckedAdd(alignedSize, size);
    RETURN_ENOMEM_ON_FAILURE(totalSize);

    void* raw = hooks::aligned_alloc(alignment, *totalSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + *alignedSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, size, static_cast<uint32_t>(*alignedSize), stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE int hawk_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    // alignment requirements for posix_memalign
    if (unlikely(!std::has_single_bit(alignment) || alignment % sizeof(void*) != 0))
    {
        return EINVAL;
    }

    if (unlikely(!hooks::CheckInitialized()))
    {
        auto result = mmap_alloc_aligned(size, alignment);
        if (!result)
        {
            return ENOMEM;
        }
        *memptr = result;
        return 0;
    }
    LogTrace("requested: " fSzt, size);

    const auto alignedSize = CheckedAlignUp(AdditionalSize, alignment);
    if (unlikely(!alignedSize || *alignedSize > std::numeric_limits<uint32_t>::max()))
    {
        return ENOMEM;
    }
    const auto totalSize = CheckedAdd(*alignedSize, size);
    if (unlikely(!totalSize))
    {
        return ENOMEM;
    }
    const auto res = hooks::posix_memalign(memptr, alignment, *totalSize);
    if (unlikely(res != 0))
    {
        return res;
    }
    *memptr = reinterpret_cast<char*>(*memptr) + *alignedSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(*memptr, size, static_cast<uint32_t>(*alignedSize), stacktrace);
    return res;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_calloc(size_t nm, size_t size)
{
    const auto userSize = CheckedMult(size, nm);
    RETURN_ENOMEM_ON_FAILURE(userSize);

    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_malloc(*userSize);
    }
    LogTrace("requested: " fSzt " " fSzt, nm, size);

    const auto totalSize = CheckedAdd(userSize, AdditionalSize);
    RETURN_ENOMEM_ON_FAILURE(totalSize);

    void* raw = hooks::calloc(1UL, *totalSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + AdditionalSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, *userSize, AdditionalSize, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_realloc(void* ptr, size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_realloc(ptr, size);
    }
    LogTrace("requested: " fPtr " " fSzt, ptr, size);

    if (!ptr)
    {
        // use malloc in case of nullptr
        return hawk_malloc(size);
    }

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);

    const AllocInfo prev = *reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    auto origPtr = reinterpret_cast<char*>(ptr) - prev.offset;

    // check if pointer was allocated during statics initialisation
    if (unlikely(prev.traceId == InvalidTraceId))
    {
        // account realloc by malloc, launder pointer
        void* clearPtr = hawk_malloc(size);
        if (!clearPtr)
        {
            return nullptr;
        }
        // copy user data
        memcpy(clearPtr, ptr, std::min(prev.size, size));
        mmap_free(origPtr, prev);
        return clearPtr;
    }

    const auto newSize = CheckedAdd(size, prev.offset);
    RETURN_ENOMEM_ON_FAILURE(newSize);

    void* realloced = hooks::realloc(origPtr, *newSize);
    if (unlikely(!realloced))
    {
        // not necessary to track deallocation, because failed realloc will not deallocate the memory
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(realloced) + prev.offset;
    TrackReallocation(userPtr, size, prev.offset, prev, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_valloc(size_t size)
{
    LogTrace("requested: " fSzt, size);

    const auto pageSize = getpagesize();
    if (pageSize <= 0)
    {
        // highly unlikely
        LogError("incorrect page size from getpagesize(): " fI32, pageSize);
        abort();
    }
    return hawk_aligned_alloc(static_cast<size_t>(pageSize), size);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_pvalloc(size_t size)
{
    LogTrace("requested: " fSzt, size);

    const auto pageSize = getpagesize();
    if (pageSize <= 0)
    {
        // highly unlikely
        LogError("incorrect page size from getpagesize(): " fI32, pageSize);
        abort();
    }
    const auto szPageSize = static_cast<size_t>(pageSize);
    // rounds the size of the allocation up to the next multiple of the system page size
    const auto totalSize = CheckedAlignUp(size, szPageSize);
    RETURN_ENOMEM_ON_FAILURE(totalSize);

    return hawk_aligned_alloc(szPageSize, *totalSize);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void hawk_free(void* ptr)
{
    // skip nullptr
    if (!ptr)
    {
        return;
    }
    const AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    ptr = reinterpret_cast<char*>(ptr) - info->offset;

    // check if pointer was allocated during statics initialisation
    if (unlikely(info->traceId == InvalidTraceId))
    {
        mmap_free(ptr, *info);
        return;
    }
    // highly unlikely situation, got pointer
    // that wasn't allocated from memhawk during statics initialisation
    // and memhawk wasn't constructed yet
    if (unlikely(!hooks::CheckInitialized()))
    {
        // will be printed into stderr
        LogError("Got pointer from static-initialisation that wasn't allocated via memhawk: " fPtr, ptr);
        abort();
    }

    LogTrace("requested: " fPtr, ptr);

    const RecursiveStacktrace stacktrace(MinUnwindDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        memhawk->TrackDealloc(*info, stacktrace.IsExternal());
    }

    hooks::free(ptr);
}

size_t hawk_malloc_usable_size(void* ptr)
{
    if (!ptr)
    {
        return 0;
    }
    const AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    return info->size;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_dlopen(const char* file, int mode)
{
    if (unlikely(!hooks::gl_dlInitialised.load(std::memory_order_acquire)))
    {
        hooks::InitDlHooks();
    }
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        memhawk->InvalidateModulesCache();
    }
    return hooks::dlopen(file, mode);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE int hawk_dlclose(void* handle)
{
    if (unlikely(!hooks::gl_dlInitialised.load(std::memory_order_acquire)))
    {
        hooks::InitDlHooks();
    }
    const int res = hooks::dlclose(handle);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        // todo: force waiting for tracking thread to dump state?
        // and wake up tracking thread, otherwise can lose data
        memhawk->InvalidateModulesCache();
    }
    return res;
}

} // namespace memhawk

// overrides section
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
    const auto savedErrno = errno;
    const auto result = memhawk::hawk_posix_memalign(memptr, alignment, size);
    errno = savedErrno;
    return result;
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
    memhawk::hawk_free(ptr);
}

void hawk_free_sized(void* ptr, size_t /*size*/)
{
    memhawk::hawk_free(ptr);
}

void hawk_free_aligned_sized(void* ptr, size_t /*alignment*/, size_t /*size*/)
{
    memhawk::hawk_free(ptr);
}

size_t hawk_malloc_usable_size(void* ptr)
{
    return memhawk::hawk_malloc_usable_size(ptr);
}

void* hawk_dlopen(const char* file, int mode)
{
    return memhawk::hawk_dlopen(file, mode);
}

int hawk_dlclose(void* handle)
{
    return memhawk::hawk_dlclose(handle);
}

void* HawkInternalNew(size_t size) noexcept(false)
{
    auto ptr = memhawk::hawk_malloc(size);
    if (unlikely(!ptr))
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

void HawkInternalDelete(void* p) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteSized(void* p, size_t /*size*/) noexcept
{
    memhawk::hawk_free(p);
}

void* HawkInternalNewArray(size_t size) noexcept(false)
{
    auto ptr = memhawk::hawk_malloc(size);
    if (unlikely(!ptr))
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

void HawkInternalDeleteArray(void* p) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteArraySized(void* p, size_t /*size*/) noexcept
{
    memhawk::hawk_free(p);
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
    memhawk::hawk_free(p);
}

void HawkInternalDeleteArrayNothrow(void* p, const std::nothrow_t& /*nt*/) noexcept
{
    memhawk::hawk_free(p);
}

void* HawkInternalNewAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    auto ptr = memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
    if (unlikely(!ptr))
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

void* HawkInternalNewAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteSizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    memhawk::hawk_free(p);
}

void* HawkInternalNewArrayAligned(size_t size, std::align_val_t alignment) noexcept(false)
{
    auto ptr = memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
    if (unlikely(!ptr))
    {
        throw std::bad_alloc{};
    }
    return ptr;
}

void* HawkInternalNewArrayAlignedNothrow(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return memhawk::hawk_aligned_alloc(static_cast<size_t>(alignment), size);
}

void HawkInternalDeleteArrayAligned(void* p, std::align_val_t /*alignment*/) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteArrayAlignedNothrow(void* p, std::align_val_t /*alignment*/, const std::nothrow_t&) noexcept
{
    memhawk::hawk_free(p);
}

void HawkInternalDeleteArraySizedAligned(void* p, size_t /*size*/, std::align_val_t /*alignment*/) noexcept
{
    memhawk::hawk_free(p);
}

} // extern "C"
