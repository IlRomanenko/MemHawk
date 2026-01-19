#include "stacktrace.h"

#include "algo.h"
#include "config.h"
#include "logging.h"
#include "macros.h"

#include <absl/base/attributes.h>
#include <absl/debugging/stacktrace.h>
#include <absl/types/span.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <sstream>
#include <utility>

#define UNW_LOCAL_ONLY 1
#include <libunwind-x86_64.h>
#include <libunwind.h>
#include <xxhash.h>

namespace memhawk
{

#ifdef MEMHAWK_SELF_PROFILING
ABSL_CONST_INIT static inline thread_local std::array<void*, 8> m_retPtrs = {};
#endif

RecursiveStacktrace::RecursiveStacktrace(size_t capacity, bool useAbsl)
{
#ifdef MEMHAWK_SELF_PROFILING
    const auto level = guard.Level();
    const auto unwindStacktrace = [&]() {
        trace.UnwindStacktrace(capacity, useAbsl, 1);
        const auto span = trace.GetTrace();
        if (likely(span.size() > 1))
        {
            m_retPtrs[level] = span[1];
        }
    };
    if (likely(guard))
    {
        unwindStacktrace();
    }
    else
    {
        // unwind for local allocations, perhaps frame pointer unwinding can be used, but i'm not sure
        unwindStacktrace();

        // shrink stacktrace in order to contain only memhawk frames
        trace.ShrinkByPtr(m_retPtrs[level - 1]); // level can't be less than 1
        if (level > 1 && !trace.GetTrace().empty())
        {
            // don't interested in previous memhawk call
            // malloc->trace->malloc and free->trace->malloc will be squashed into trace->malloc
            trace.ShrinkBySize(trace.GetTrace().size() - 1);
        }
    }
#else
    // unwind only for external allocations
    if (likely(guard))
    {
        trace.UnwindStacktrace(capacity, useAbsl, 1);
    }
#endif
}

RecursiveStacktrace::~RecursiveStacktrace()
{
#ifdef MEMHAWK_SELF_PROFILING
    m_retPtrs[guard.Level()] = nullptr;
#endif
}

bool CompressedStacktrace::operator==(const CompressedStacktrace& other) const
{
    if (hash != other.hash)
    {
        return false;
    }
    return data == other.data;
}

bool CompressedStacktrace::operator<(const CompressedStacktrace& other) const
{
    if (hash == other.hash)
    {
        return data < other.data;
    }
    return hash < other.hash;
}

void CompressedStacktrace::RecalculateHash()
{
    hash = XXH3_64bits(data.data(), data.size() * sizeof(uintptr_t));
}

Stacktrace::Stacktrace() = default; // NOLINT(cppcoreguidelines-pro-type-member-init)

Stacktrace::Stacktrace(void* const* data, size_t size) // NOLINT(cppcoreguidelines-pro-type-member-init)
{
    m_skip = 0;
    m_size = std::min(MaxUnwindDepth, size);
    memcpy(reinterpret_cast<void*>(m_data), reinterpret_cast<const void*>(data), m_size * sizeof(void*));
}

Stacktrace Stacktrace::Unwind(size_t capacity, bool useAbsl, size_t skip)
{
    Stacktrace trace{};
    trace.UnwindStacktrace(capacity, useAbsl, skip);
    return trace;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void Stacktrace::UnwindStacktrace(size_t capacity, bool useAbsl, size_t skip)
{
    const size_t size = std::min(capacity, MaxUnwindDepth);
    int resultSize = 0;
    if (useAbsl)
    {
        // unwind by frame-pointer
        resultSize = absl::GetStackTrace(m_data, static_cast<int>(size), 0);
    }
    else
    {
        // unwind by dwarf
        resultSize = unw_backtrace(m_data, static_cast<int>(size));
    }
    while (likely(resultSize > 0) && unlikely(m_data[static_cast<size_t>(resultSize - 1)] == nullptr))
    {
        resultSize--;
    }

    if (std::cmp_less(resultSize, skip))
    {
        LogWarning("Failed to unwind, got empty stacktrace");
        return;
    }

    m_size = static_cast<size_t>(resultSize);
    m_skip = skip;
}

void Stacktrace::CollapseRecursion(size_t depth)
{
    // remove duplicates -> not interested in recursion
    const auto newSize = memhawk::CollapseRecursion(GetTrace(), depth);
    ShrinkBySize(newSize);
}

void Stacktrace::Compress(CompressedStacktrace& result) const
{
    const auto span = GetTrace();
    result.data.resize(span.size());
    const auto begin = reinterpret_cast<const void*>(span.data());
    if (likely(!span.empty()))
    {
        memcpy(result.data.data(), begin, span.size() * sizeof(void*));
    }
    result.RecalculateHash();
}

void Stacktrace::Setup()
{
    if (unw_set_caching_policy(unw_local_addr_space, UNW_CACHE_PER_THREAD))
    {
        LogWarning("Failed to enable per-thread libunwind caching");
    }
    if (unw_set_cache_size(unw_local_addr_space, 1024, 0))
    {
        LogWarning("Failed to set libunwind cache size");
    }
}

void Stacktrace::ShrinkBySize(size_t size)
{
    if (m_skip + size >= m_size)
    {
        // trace already has smaller length
        return;
    }
    m_size = m_skip + size;
}

void Stacktrace::Skip(size_t size)
{
    m_skip += size;
    if (m_skip > m_size)
    {
        m_skip = m_size;
    }
}

void Stacktrace::ShrinkByPtr(void* ptr)
{
    const auto span = GetTrace();
    for (size_t i = 2; i < span.size(); i++)
    {
        if (span[i] == ptr)
        {
            // don't included matched pointer
            ShrinkBySize(i);
            return;
        }
    }
}

void Stacktrace::CoarseToFunctionsStart()
{
    auto span = GetTrace();
    for (auto& ip : span)
    {
        unw_proc_info_t info{};
        unw_get_proc_info_by_ip(unw_local_addr_space, reinterpret_cast<unw_word_t>(ip), &info, nullptr);
        ip = reinterpret_cast<void*>(info.start_ip);
    }
}

absl::Span<void*> Stacktrace::GetTrace()
{
    return absl::MakeSpan(m_data + m_skip, m_size - m_skip);
}

absl::Span<void* const> Stacktrace::GetTrace() const
{
    return absl::MakeConstSpan(m_data + m_skip, m_size - m_skip);
}

std::string Stacktrace::Describe() const
{
    char buf[512];
    char elfName[512];
    const auto span = GetTrace();
    std::stringstream stream;
    for (const auto& ip : span)
    {
        unw_get_proc_name_by_ip(unw_local_addr_space, reinterpret_cast<unw_word_t>(ip), buf, 512, nullptr, nullptr);

        unw_word_t offset{};
        unw_get_elf_filename_by_ip(unw_local_addr_space, reinterpret_cast<unw_word_t>(ip), elfName, 512, &offset,
                                   nullptr);
        Dl_info info{};
        // use dladdr instead of the libunwind function, as the offset returned by libunwind can sometimes be misleading
        if (dladdr(ip, &info))
        {
            offset = reinterpret_cast<unw_word_t>(ip) - reinterpret_cast<unw_word_t>(info.dli_fbase);
        }

        stream << fmt::format("{}: {} + 0x{:x} : {}\n", ip, elfName, offset, buf);
    }
    return stream.str();
}

bool Stacktrace::operator==(const Stacktrace& rhs) const
{
    return GetTrace() == rhs.GetTrace();
}

} // namespace memhawk
