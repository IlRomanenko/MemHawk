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

#define UNW_LOCAL_ONLY 1
#include <libunwind.h>
#include <xxhash.h>

namespace memhawk
{

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

    if (resultSize <= 0 || static_cast<size_t>(resultSize) < skip)
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
    if (likely(span.size() != 0))
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

        stream << fmt::format("{}: {} + {:x}: {}\n", ip, elfName, offset, buf);
    }
    return stream.str();
}

bool Stacktrace::operator==(const Stacktrace& rhs) const
{
    return GetTrace() == rhs.GetTrace();
}

} // namespace memhawk
