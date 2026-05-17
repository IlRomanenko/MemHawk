#pragma once

#include "config.h"
#include "recursion_guard.h"

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

    static Stacktrace Unwind(size_t capacity, bool useAbsl, size_t skip = 1);

    void Compress(CompressedStacktrace& result) const;

    void CoarseToFunctionsStart();
    void CollapseRecursion(size_t depth);

    void Skip(size_t size);
    void ShrinkBySize(size_t size);
    void ShrinkByPtr(void* ptr);
    std::string Describe() const;
    absl::Span<void*> GetTrace();
    absl::Span<void* const> GetTrace() const;

    void UnwindStacktrace(size_t capacity, bool useAbsl, size_t skip);

    bool operator==(const Stacktrace& rhs) const;

private:
    size_t m_size{};
    size_t m_skip{};

    void* m_data[MaxUnwindDepth]; // don't initialise memory
};

struct RecursiveStacktrace
{
    RecursiveStacktrace(size_t capacity, bool useAbsl);

    ~RecursiveStacktrace();

    Stacktrace& GetStacktrace()
    {
        return trace;
    }

    bool IsExternal() const
    {
        return guard.IsFirst();
    }

private:
    RecursionGuard<AllocTag> guard;
    Stacktrace trace;
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
