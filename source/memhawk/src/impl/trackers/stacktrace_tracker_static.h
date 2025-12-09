#pragma once

#include "config.h"
#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace memhawk
{

bool IsFixedTrackerId(uint32_t traceId);

class StaticStacktraceTracker : public IStacktraceTracker
{
public:
    static constexpr const size_t MaxStacktraceLength = 6;

    static constexpr size_t ElementsCount = 512;
    static constexpr size_t StorageSize = ElementsCount * MaxStacktraceLength * 2;

    static constexpr uint32_t FixedTrackerIdFlag = 1UL << (std::numeric_limits<uint32_t>::digits - 1);
    static constexpr const uint32_t UnknownTraceId = (ElementsCount + 1) ^ FixedTrackerIdFlag;

public:
    explicit StaticStacktraceTracker(StacktraceTrackerConfig cfg);
    ~StaticStacktraceTracker() override;

    uint32_t InsertStacktrace(const Stacktrace& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount() override;

    size_t GetStorageSize();

private:
    uint32_t InsertStacktraceUnlocked(absl::Span<void* const> trace);
    absl::Span<const uint32_t> GetTraceSpan(uint32_t traceId);
    Stacktrace GetStacktrace(uint32_t traceId);

    struct TraceElement
    {
        uint64_t hash{};
        absl::Span<const uint32_t> trace;
        bool operator==(const TraceElement& rhs) const;
    };

private:
    StacktraceTrackerConfig m_cfg;

    absl::base_internal::SpinLock m_mt;

    // Use static arrays because memory can't be allocated upon inserting into StaticStacktraceTracker
    // due to recursion problem
    std::array<uint32_t, StorageSize> m_storage{};
    std::array<TraceElement, ElementsCount> m_elements{};
    std::array<uint32_t, MaxStacktraceLength * 8> m_compressedTrace{};
    std::array<uint64_t, MaxStacktraceLength * 4> m_decompressedTrace{};

    uint32_t m_size{};
    uint32_t m_storageSize{};
};

} // namespace memhawk
