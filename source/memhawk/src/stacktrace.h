#pragma once

#include <absl/types/span.h>
#include <boost/container_hash/hash.hpp>

#include <cstdint>
#include <stdlib.h>
#include <string>

constexpr size_t MaxUnwindSize = 64;

class Stacktrace;

struct CompressedStacktrace
{
    std::vector<uint64_t> data;
    size_t hash{};

    CompressedStacktrace() = default;
    CompressedStacktrace(const CompressedStacktrace& other) = delete;
    CompressedStacktrace& operator=(const CompressedStacktrace&) = delete;
    CompressedStacktrace(CompressedStacktrace&& other) = default;
    CompressedStacktrace& operator=(CompressedStacktrace&&) = default;

    bool operator==(const CompressedStacktrace& other) const;
    bool operator<(const CompressedStacktrace& other) const;
    void RecalculateHash();

};

namespace boost
{
template <>
struct hash<CompressedStacktrace>
{
    size_t operator()(const CompressedStacktrace& trace) const
    {
        return trace.hash;
    }
};
} // namespace boost

class Stacktrace
{
public:
    Stacktrace() = default;
    Stacktrace(void** data, size_t size);

    static Stacktrace Unwind(size_t capacity, size_t skip = 1);
    static void Setup();

    void Compress(CompressedStacktrace& result) const;

    void ShrinkBySize(size_t size);
    void ShrinkByPtr(void* ptr);
    std::string Describe() const;
    absl::Span<void*> GetTrace();
    absl::Span<void* const> GetTrace() const;

    bool operator==(const Stacktrace& rhs) const;

private:
    void UnwindStacktrace(size_t capacity, size_t skip);
    void SqueezeRecursion(size_t depth);

private:
    void* m_data[MaxUnwindSize];
    size_t m_size{};
    size_t m_skip{};
};
