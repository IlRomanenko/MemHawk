
#include "hawk_malloc.h"

#include "impl/alloc_info.h"
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
#include <sys/mman.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <exception>
#include <pthread.h>
#include <regex>
#include <unistd.h>

namespace memhawk
{

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
        if (!ret && Type == HookType::Optional)
        {
            return;
        }
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

#define HOOK(name, type)                                                                                               \
    struct name##_t : public hook<decltype(&::name), name##_t, type>                                                   \
    {                                                                                                                  \
        static constexpr const char* identifier = #name;                                                               \
    } name

// Allocation hooks
HOOK(malloc, HookType::Required);
HOOK(free, HookType::Required);
HOOK(calloc, HookType::Required);
HOOK(realloc, HookType::Required);
HOOK(posix_memalign, HookType::Optional);
HOOK(valloc, HookType::Optional);
HOOK(aligned_alloc, HookType::Optional);
// Proc maps hooks
HOOK(dlopen, HookType::Required);
HOOK(dlclose, HookType::Required);

#pragma GCC diagnostic pop
#undef HOOK

static ABSL_CONST_INIT bool gl_initialised = false;
static ABSL_CONST_INIT bool gl_dlInitialised = false;
static ABSL_CONST_INIT bool gl_memhawkReady = false;
static ABSL_CONST_INIT std::unique_ptr<MemHawk> gl_memhawk = nullptr;
static ABSL_CONST_INIT UnwindConfig gl_unwind = {};

MemHawk* GetMemHawk()
{
    if (likely(gl_memhawkReady))
    {
        return gl_memhawk.get();
    }
    return nullptr;
}

void InitDlHooks()
{
    if (gl_dlInitialised)
    {
        return;
    }
    hooks::dlopen.init();
    hooks::dlclose.init();
    gl_dlInitialised = true;
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
try
{
    if (prognameRegex.empty())
    {
        return true;
    }
    const std::regex pattern(std::string{prognameRegex});
    return std::regex_match(program_invocation_name, pattern);
}
catch (const std::exception& ex)
{
    LogError("Incorrect progname regex: " fStr, ex.what());
    abort();
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
        Stacktrace::Setup();

        LogInfo("[" fI32 "]", getpid());
        gl_memhawk = std::make_unique<MemHawk>(*cfg.MemHawk, std::make_unique<writers::WritersFactory>());
        gl_memhawkReady = true;

        // after that can save traces as postponed into memhawk
        gl_initialised = true;
        gl_memhawk->PostponedConstruct();

        // setup atfork handlers
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
        gl_memhawk.reset();
    }
    LogDeinit();
}

}; // namespace hooks

constexpr size_t AdditionalSize = sizeof(AllocInfo);
// Ensure, that we won't ruin malloc invariant
static_assert(alignof(max_align_t) == sizeof(AllocInfo));

auto align_ceil(auto value, size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

// Allocate memory via mmap before memhawk is initialised
void* mmap_malloc(size_t size)
{
    auto totalSize = size + AdditionalSize;
    auto ptr =
        absl::base_internal::DirectMmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    memset(ptr, 0, size);

    AllocInfo* info = new (ptr) AllocInfo{size, AdditionalSize};
    // set specific TraceId, that shouldn't be used by memhawk
    info->traceId = InvalidTraceId;
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    return ptr;
}

void mmap_free(void* ptr, size_t size)
{
    absl::base_internal::DirectMunmap(ptr, size);
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
    if (unlikely(!ptr))
    {
        return nullptr;
    }
    memcpy(realloced, ptr, info->size);
    mmap_free(origPtr, info->size);

    return realloced;
}

void* mmap_alloc_aligned(size_t size, size_t alignment)
{
    auto alignedSize = align_ceil(AdditionalSize, alignment);
    const size_t totalSize = size + alignedSize * 2;
    // allocate enough memory in order to find suitably aligned pointer for user data
    void* ptr = mmap_malloc(totalSize);
    if (unlikely(!ptr))
    {
        return nullptr;
    }
    // reserve memory for AllocInfo
    auto shiftedPtr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    // find first aligned addr
    auto alignedPtrValue = align_ceil(reinterpret_cast<uintptr_t>(shiftedPtr), alignment);
    void* alignedPtr = reinterpret_cast<void*>(alignedPtrValue);

    AllocInfo* info = new (reinterpret_cast<char*>(alignedPtr) - AdditionalSize)
        AllocInfo{totalSize, static_cast<uint32_t>(alignedPtrValue - reinterpret_cast<uintptr_t>(ptr))};
    // set specific TraceId, that shouldn't be used by memhawk
    info->traceId = InvalidTraceId;

    return alignedPtr;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_malloc(size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        return mmap_malloc(size);
    }
    LogTrace("requested: " fSzt, size);

    auto totalSize = size + AdditionalSize;
    void* ptr = hooks::malloc(totalSize);
    if (unlikely(!ptr))
    {
        return nullptr;
    }
    AllocInfo* info = new (ptr) AllocInfo{size, AdditionalSize};
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;

    LogTrace("result: " fPtr, ptr);

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
    return ptr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_aligned_alloc(size_t align, size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        return mmap_alloc_aligned(size, align);
    }
    LogTrace("requested: " fSzt, size);

    auto alignedSize = align_ceil(AdditionalSize, align);
    void* ptr = hooks::aligned_alloc(align, (size + alignedSize));
    if (unlikely(!ptr))
    {
        return nullptr;
    }
    ptr = reinterpret_cast<char*>(ptr) + alignedSize;

    const auto allocInfoPtr = reinterpret_cast<char*>(ptr) - AdditionalSize;
    AllocInfo* info = new (allocInfoPtr) AllocInfo{size, static_cast<uint32_t>(alignedSize)};

    LogTrace("result: " fPtr, ptr);

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
    return ptr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE int hawk_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        *memptr = mmap_alloc_aligned(size, alignment);
        return 0;
    }
    LogTrace("requested: " fSzt, size);

    auto alignedSize = align_ceil(AdditionalSize, alignment);
    const auto res = hooks::posix_memalign(memptr, alignment, size + alignedSize);
    LogTrace("result: " fPtr, *memptr);
    if (unlikely(res != 0))
    {
        return res;
    }

    *memptr = reinterpret_cast<char*>(*memptr) + alignedSize;
    const auto allocInfoPtr = reinterpret_cast<char*>(*memptr) - AdditionalSize;
    AllocInfo* info = new (allocInfoPtr) AllocInfo{size, static_cast<uint32_t>(alignedSize)};

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
    return res;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_calloc(size_t nm, size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        return mmap_malloc(nm * size);
    }
    LogTrace("requested: " fSzt " " fSzt, nm, size);

    const size_t totalSize = nm * size + AdditionalSize;
    void* ptr = hooks::calloc(1UL, totalSize);
    if (unlikely(!ptr))
    {
        return ptr;
    }

    AllocInfo* info = new (ptr) AllocInfo{nm * size, AdditionalSize};
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    LogTrace("result: " fPtr, ptr);

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
    return ptr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_realloc(void* ptr, size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        return mmap_realloc(ptr, size);
    }
    LogTrace("requested: " fPtr " " fSzt, ptr, size);

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);

    auto origPtr = ptr;

    if (ptr)
    {
        AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
        origPtr = reinterpret_cast<char*>(ptr) - info->offset;

        // check if pointer was allocated during statics initialisation
        if (unlikely(info->traceId == InvalidTraceId))
        {
            // account realloc by malloc, launder pointer
            void* clearPtr = hawk_malloc(size);
            if (!clearPtr)
            {
                return nullptr;
            }
            memcpy(clearPtr, ptr, info->size);
            mmap_free(ptr, info->size);
            return clearPtr;
        }

        if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
        {
            memhawk->TrackDealloc(*info, stacktrace.IsExternal());
        }
    }
    void* realloced = hooks::realloc(origPtr, size + AdditionalSize);
    if (unlikely(!realloced))
    {
        return nullptr;
    }

    AllocInfo* info = new (realloced) AllocInfo{size, AdditionalSize};
    realloced = reinterpret_cast<char*>(realloced) + AdditionalSize;
    LogTrace("result: " fPtr, realloced);

    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        auto& trace = stacktrace.GetStacktrace();
        memhawk->TrackAlloc(*info, trace, stacktrace.IsExternal());
    }
    return realloced;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_valloc(size_t size)
{
    if (unlikely(!hooks::gl_initialised))
    {
        // will be printed into stderr
        LogError("valloc is not supported yet on statics initialisation");
        abort();
    }
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
    if (unlikely(!hooks::gl_initialised))
    {
        // will be printed into stderr
        LogError("pvalloc is not supported yet on statics initialisation");
        abort();
    }
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
    size = align_ceil(size, szPageSize);
    return hawk_aligned_alloc(szPageSize, size);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void hawk_free(void* ptr)
{
    // skip nullptr
    if (!ptr)
    {
        return;
    }
    AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    ptr = reinterpret_cast<char*>(ptr) - info->offset;

    // check if pointer was allocated during statics initialisation
    if (unlikely(info->traceId == InvalidTraceId))
    {
        mmap_free(ptr, info->size);
        return;
    }
    // highly unlikely situation, got pointer
    // that wasn't allocated from memhawk during statics initialisation
    // and memhawk wasn't constructed yet
    if (unlikely(!hooks::gl_initialised))
    {
        // will be printed into stderr
        LogError("Got pointer from static-initialisation, that wasn't allocated via memhawk: " fPtr, ptr);
        abort();
    }

    LogTrace("requested: " fPtr, ptr);

    const RecursiveStacktrace stacktrace(MinUnwindDepth, *hooks::gl_unwind.UseAbslStacktraces);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk && stacktrace.IsTrackable()))
    {
        memhawk->TrackDealloc(*info, stacktrace.IsExternal());
    }

    hooks::free(ptr);
}

size_t hawk_malloc_usable_size(void* ptr)
{
    const AllocInfo* info = reinterpret_cast<AllocInfo*>(reinterpret_cast<char*>(ptr) - AdditionalSize);
    return info->size;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_dlopen(const char* file, int mode)
{
    if (unlikely(!hooks::gl_dlInitialised))
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
    if (unlikely(!hooks::gl_dlInitialised))
    {
        hooks::InitDlHooks();
    }
    const int res = hooks::dlclose(handle);
    if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
    {
        // todo: force waiting for tracking thread to dump state?
        // and wake up tracking thread, otherwise can loose data
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
