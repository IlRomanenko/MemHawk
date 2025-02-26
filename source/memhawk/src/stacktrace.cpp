#include "stacktrace.h"

#include "log.h"

#include <boost/container_hash/hash.hpp>
#include <fmt/format.h>
#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <sstream>
#include <stdio.h>
#include <xxhash.h>

#define UNW_LOCAL_ONLY 1
#include <libunwind.h>

Stacktrace::Stacktrace(void** data, size_t size)
{
    m_skip = 0;
    m_size = std::min(MaxUnwindSize, size);
    std::copy(data, std::next(data, m_size), m_data);
    RecalculateHash();
}

Stacktrace Stacktrace::Unwind(size_t capacity, size_t skip)
{
    Stacktrace trace{};
    trace.UnwindStacktrace(capacity, skip);
    return trace;
}

inline void Stacktrace::UnwindStacktrace(size_t capacity, size_t skip)
{
    size_t size = std::min(capacity, MaxUnwindSize);
    auto resultSize = unw_backtrace(m_data, size);
    // remove duplicates -> not interested in recursion
    // auto last = std::unique(m_data, std::next(m_data, resultSize));
    // todo: Create correct mechanishm for removing recursion
    auto last = std::next(m_data, resultSize);
    resultSize = std::distance(m_data, last);
    while (resultSize > 0 && m_data[resultSize - 1] == nullptr) {
        resultSize--;
    }

    if (resultSize <= 0 || static_cast<size_t>(resultSize) < skip) {
        LogWarning("Failed to unwind, got empty stacktrace");
        return;
    }
    m_size = resultSize;
    m_skip = skip;
    RecalculateHash();
}

void Stacktrace::Setup()
{
    // todo: Create defines for it and specify on compile time
    if (unw_set_caching_policy(unw_local_addr_space, UNW_CACHE_PER_THREAD)) {
        fprintf(stderr, "WARNING: Failed to enable per-thread libunwind caching.\n");
    }
    if (unw_set_cache_size(unw_local_addr_space, 1024, 0)) {
        fprintf(stderr, "WARNING: Failed to set libunwind cache size.\n");
    }
}

void Stacktrace::ShrinkBySize(size_t size)
{
    if (m_skip + size >= m_size) {
        // trace already has smaller length
        return;
    }
    m_size = m_skip + size;
    RecalculateHash();
}

void Stacktrace::ShrinkByPtr(void* ptr)
{
    const auto span = GetTrace();
    for (size_t i = 2; i < span.size(); i++) {
        if (span[i] == ptr) {
            // don't included matched pointer
            ShrinkBySize(i);
            return;
        }
    }
}

absl::Span<void*> Stacktrace::GetTrace()
{
    return absl::MakeSpan(m_data + m_skip, m_size - m_skip);
}

absl::Span<void* const> Stacktrace::GetTrace() const
{
    return absl::MakeConstSpan(m_data + m_skip, m_data + m_size);
}

void Stacktrace::RecalculateHash()
{
    auto span = GetTrace();
    m_hash = static_cast<uint32_t>(boost::hash_range(span.begin(), span.end()));
}

std::string Stacktrace::Describe() const
{
    char buf[512];
    char elfName[512];
    auto span = GetTrace();
    std::stringstream stream;
    for (const auto& ip : span) {
        unw_get_proc_name_by_ip(unw_local_addr_space, reinterpret_cast<unw_word_t>(ip), buf, 512, nullptr, nullptr);

        unw_word_t offset{};
        unw_get_elf_filename_by_ip(unw_local_addr_space, reinterpret_cast<unw_word_t>(ip), elfName, 512, &offset,
                                   nullptr);

        stream << fmt::format("{} + {:x}: {}\n", elfName, offset, buf);
    }
    return stream.str();
}

uint32_t Stacktrace::Hash() const
{
    return m_hash;
}
