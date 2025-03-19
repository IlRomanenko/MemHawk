#pragma once

#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <array>
#include <cstdint>
#include <optional>

namespace memhawk
{

bool IsFixedTrackerId(uint32_t traceId);

class StacktraceTrackerFixed : public IStacktraceTracker
{
public:
    StacktraceTrackerFixed(bool dump = false);
    ~StacktraceTrackerFixed();

    uint32_t InsertStacktrace(Stacktrace&& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount() override;

private:
    uint32_t InsertStacktraceUnlocked(Stacktrace&& trace);

    static constexpr size_t StorageSize = 128;
private:
    absl::Mutex m_mt;

    // std::array<uint32_t, StorageSize> m_storage
    std::array<Stacktrace, StorageSize> m_stacktraces;
    size_t m_size{};

    bool m_dump{false};
};

} // namespace memhawk
