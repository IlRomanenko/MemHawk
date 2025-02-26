#pragma once

#include <cstdint>
#include <span>
#include <stdlib.h>
#include <string>

constexpr size_t MaxUnwindSize = 64;

class Stacktrace
{
public:
    Stacktrace() = default;
    Stacktrace(void** data, size_t size);

    static Stacktrace Unwind(size_t capacity, size_t skip = 1);
    static void Setup();


    void ShrinkBySize(size_t size);
    void ShrinkByPtr(void* ptr);
    uint32_t Hash() const;
    std::string Describe() const;
    std::span<void*> GetTrace();
    std::span<void* const> GetTrace() const;

private:
    void UnwindStacktrace(size_t capacity, size_t skip);

private:
    void* m_data[MaxUnwindSize];
    size_t m_size{};
    size_t m_skip{};
    uint32_t m_hash{};
};
