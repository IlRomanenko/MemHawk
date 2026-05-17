
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

#include <atomic>
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

static ABSL_CONST_INIT std::atomic<bool> gl_initialised = false;
static ABSL_CONST_INIT std::atomic<bool> gl_dlInitialised = false;
static ABSL_CONST_INIT std::atomic<bool> gl_memhawkReady = false;
static ABSL_CONST_INIT std::unique_ptr<MemHawk> gl_memhawk = nullptr;
static ABSL_CONST_INIT UnwindConfig gl_unwind = {};

ABSL_ATTRIBUTE_ALWAYS_INLINE MemHawk* GetMemHawk()
{
    if (likely(gl_memhawkReady.load(std::memory_order_acquire)))
    {
        return gl_memhawk.get();
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

// Allocate memory via mmap before memhawk is initialised
void* mmap_malloc(size_t size)
{
    auto totalSize = size + AdditionalSize;
    auto ptr =
        absl::base_internal::DirectMmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (unlikely(ptr == MAP_FAILED))
    {
        return nullptr;
    }
    memset(ptr, 0, totalSize);
    AllocInfo* info = new (ptr) AllocInfo{size, AdditionalSize};
    // set specific TraceId, that shouldn't be used by memhawk
    info->traceId = InvalidTraceId;
    ptr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    return ptr;
}

void mmap_free(void* ptr, const AllocInfo* info)
{
    absl::base_internal::DirectMunmap(ptr, info->size + info->offset);
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
    mmap_free(origPtr, info);

    return realloced;
}

void* mmap_alloc_aligned(size_t size, size_t alignment)
{
    const size_t totalSize = size + AdditionalSize + alignment;
    // allocate enough memory in order to find suitably aligned pointer for user data
    auto ptr =
        absl::base_internal::DirectMmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (unlikely(ptr == MAP_FAILED))
    {
        return nullptr;
    }
    memset(ptr, 0, totalSize);

    // reserve memory for AllocInfo
    const auto shiftedPtr = reinterpret_cast<char*>(ptr) + AdditionalSize;
    // find first aligned addr
    const auto alignedPtrValue = align_ceil(reinterpret_cast<uintptr_t>(shiftedPtr), alignment);
    void* alignedPtr = reinterpret_cast<void*>(alignedPtrValue); // start of user data

    const auto offset = alignedPtrValue - reinterpret_cast<uintptr_t>(ptr);
    // NOLINTNEXTLINE(misc-const-correctness) false positive
    void* infoLocation = reinterpret_cast<char*>(alignedPtr) - AdditionalSize;

    // save `totalSize - offset` as user size instead of `size`
    // it's necessary in order to correctly unmap full memory region
    AllocInfo* info = new (infoLocation) AllocInfo{totalSize - offset, static_cast<uint32_t>(offset)};
    // set specific TraceId, that shouldn't be used by memhawk
    info->traceId = InvalidTraceId;

    return alignedPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_malloc(size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_malloc(size);
    }
    LogTrace("requested: " fSzt, size);

    void* raw = hooks::malloc(size + AdditionalSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + AdditionalSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, size, AdditionalSize, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_aligned_alloc(size_t align, size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_alloc_aligned(size, align);
    }
    LogTrace("requested: " fSzt, size);

    const auto alignedSize = align_ceil(AdditionalSize, align);
    void* raw = hooks::aligned_alloc(align, size + alignedSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + alignedSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, size, static_cast<uint32_t>(alignedSize), stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE int hawk_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
    {
        *memptr = mmap_alloc_aligned(size, alignment);
        return 0;
    }
    LogTrace("requested: " fSzt, size);

    const auto alignedSize = align_ceil(AdditionalSize, alignment);
    const auto res = hooks::posix_memalign(memptr, alignment, size + alignedSize);
    if (unlikely(res != 0))
    {
        return res;
    }
    *memptr = reinterpret_cast<char*>(*memptr) + alignedSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(*memptr, size, static_cast<uint32_t>(alignedSize), stacktrace);
    return res;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_calloc(size_t nm, size_t size)
{
    size_t bytes{};
    // add check similar to glibc
    if (unlikely(__builtin_mul_overflow(nm, size, &bytes)))
    {
        errno = ENOMEM;
        return nullptr;
    }

    if (unlikely(!hooks::CheckInitialized()))
    {
        return mmap_malloc(nm * size);
    }
    LogTrace("requested: " fSzt " " fSzt, nm, size);

    void* raw = hooks::calloc(1UL, nm * size + AdditionalSize);
    if (unlikely(!raw))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(raw) + AdditionalSize;

    RecursiveStacktrace stacktrace(*hooks::gl_unwind.TrackDepth, *hooks::gl_unwind.UseAbslStacktraces);
    TrackAllocation(userPtr, nm * size, AdditionalSize, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_realloc(void* ptr, size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
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
            // copy user data
            memcpy(clearPtr, ptr, std::min(info->size, size));
            mmap_free(origPtr, info);
            return clearPtr;
        }

        if (auto memhawk = hooks::GetMemHawk(); likely(memhawk))
        {
            memhawk->TrackDealloc(*info, stacktrace.IsExternal());
        }
    }
    void* realloced = hooks::realloc(origPtr, size + AdditionalSize);
    if (unlikely(!realloced))
    {
        return nullptr;
    }
    void* userPtr = reinterpret_cast<char*>(realloced) + AdditionalSize;
    TrackAllocation(userPtr, size, AdditionalSize, stacktrace);
    return userPtr;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE void* hawk_valloc(size_t size)
{
    if (unlikely(!hooks::CheckInitialized()))
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
    if (unlikely(!hooks::CheckInitialized()))
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
        mmap_free(ptr, info);
        return;
    }
    // highly unlikely situation, got pointer
    // that wasn't allocated from memhawk during statics initialisation
    // and memhawk wasn't constructed yet
    if (unlikely(!hooks::CheckInitialized()))
    {
        // will be printed into stderr
        LogError("Got pointer from static-initialisation, that wasn't allocated via memhawk: " fPtr, ptr);
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
