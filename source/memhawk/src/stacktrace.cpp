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

#define UNW_LOCAL_ONLY 1
#include <libunwind.h>

#include <xxhash.h>

#include <lz4.h>


bool CompressedStacktrace::operator==(const CompressedStacktrace& other) const
{
    if (hash != other.hash) {
        return false;
    }
    return data == other.data;
}

bool CompressedStacktrace::operator<(const CompressedStacktrace& other) const
{
    if (hash == other.hash) {
        return data < other.data;
    }
    return hash < other.hash;
}

void CompressedStacktrace::RecalculateHash()
{
    hash = XXH3_64bits(data.data(), data.size() * sizeof(uint64_t));
}

Stacktrace::Stacktrace(void** data, size_t size)
{
    m_skip = 0;
    m_size = std::min(MaxUnwindSize, size);
    std::copy(data, std::next(data, static_cast<ssize_t>(m_size)), m_data);
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
    auto last = std::next(m_data, resultSize);
    resultSize = std::distance(m_data, last);
    while (resultSize > 0 && m_data[resultSize - 1] == nullptr) {
        resultSize--;
    }

    if (resultSize <= 0 || static_cast<size_t>(resultSize) < skip) {
        LogWarning("Failed to unwind, got empty stacktrace");
        return;
    }

    m_size = static_cast<size_t>(resultSize);
    m_skip = skip;
    // remove duplicates -> not interested in recursion
    SqueezeRecursion(4);
}

void Stacktrace::Compress(CompressedStacktrace& result) const
{
    const auto span = GetTrace();
    result.data.resize(span.size());
    memcpy(result.data.data(), span.data(), span.size() * sizeof(void*));
    result.RecalculateHash();
}

// Simple recursion squeezing algorithm with O(N^2) asymptotic
void Stacktrace::SqueezeRecursion(size_t depth)
{
    size_t begin = m_skip;
    size_t left = m_skip;
    size_t cur = left + 1;
    size_t end = m_size;
    while (cur < end) {
        size_t cycleSize = 0;

        //  begin         left    cur    end
        // [  |    ....    |   ... | .... | ]
        for (size_t spanSize = 1; spanSize < depth && cur + spanSize < end; spanSize++) {
            bool matched = true;
            if (begin + spanSize > left + 1) {
                break;
            }
            for (size_t p = 0; p < spanSize; p++) {
                if (m_data[cur + p] != m_data[left - spanSize + p + 1]) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                cycleSize = spanSize;
                break;
            }
        }
        if (cycleSize > 0) {
            cur += cycleSize;
        } else {
            left++;
            m_data[left] = m_data[cur];
            cur++;
        }
    }
    end = left + 1;

    m_size = static_cast<size_t>(end);
}

void Stacktrace::Setup()
{
    if (unw_set_caching_policy(unw_local_addr_space, UNW_CACHE_PER_THREAD)) {
        LogWarning("Failed to enable per-thread libunwind caching");
    }
    if (unw_set_cache_size(unw_local_addr_space, 1024, 0)) {
        LogWarning("Failed to set libunwind cache size");
    }
}

void Stacktrace::ShrinkBySize(size_t size)
{
    if (m_skip + size >= m_size) {
        // trace already has smaller length
        return;
    }
    m_size = m_skip + size;
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
        
        stream << fmt::format("{}: {} + {:x}: {}\n", ip, elfName, offset, buf);
    }
    return stream.str();
}

bool Stacktrace::operator==(const Stacktrace& rhs) const
{
    return m_size == rhs.m_size && m_skip == rhs.m_skip && memcmp(m_data, rhs.m_data, m_size) == 0;
}
