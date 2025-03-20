#pragma once

#include "config.h"

#include <absl/types/span.h>
#include <boost/container_hash/hash.hpp>

#include <cstdint>
#include <string>

namespace memhawk
{

class Stacktrace;

struct CompressedStacktrace
{
    std::vector<uintptr_t> data;
    size_t hash{};

    CompressedStacktrace() = default;
    ~CompressedStacktrace() = default;
    CompressedStacktrace(const CompressedStacktrace& other) = delete;
    CompressedStacktrace& operator=(const CompressedStacktrace&) = delete;
    CompressedStacktrace(CompressedStacktrace&& other) = default;
    CompressedStacktrace& operator=(CompressedStacktrace&&) = default;

    bool operator==(const CompressedStacktrace& other) const;
    bool operator<(const CompressedStacktrace& other) const;

    void RecalculateHash();
};

class Stacktrace
{
public:
    Stacktrace();
    Stacktrace(void* const* data, size_t size);

    static Stacktrace Unwind(size_t capacity, size_t collapseDepth, size_t skip = 1);
    static void Setup();

    void Compress(CompressedStacktrace& result) const;

    void CoarseToFunctionsStart();

    void Skip(size_t size);
    void ShrinkBySize(size_t size);
    void ShrinkByPtr(void* ptr);
    std::string Describe() const;
    absl::Span<void*> GetTrace();
    absl::Span<void* const> GetTrace() const;

    bool operator==(const Stacktrace& rhs) const;

private:
    void UnwindStacktrace(size_t capacity, size_t collapseDepth, size_t skip);

private:
    std::array<void*, MaxUnwindDepth> m_data; // don't initialise memory
    size_t m_size{};
    size_t m_skip{};
};

} // namespace memhawk

namespace boost
{
template <>
struct hash<memhawk::CompressedStacktrace>
{
    size_t operator()(const memhawk::CompressedStacktrace& trace) const
    {
        return trace.hash;
    }
};
} // namespace boost
