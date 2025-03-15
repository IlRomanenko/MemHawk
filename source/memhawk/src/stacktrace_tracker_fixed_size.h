#pragma once

#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <boost/container/static_vector.hpp>

#include <cstdint>
#include <optional>

bool IsFixedTrackerId(uint32_t traceId);

class StacktraceTrackerFixed : public IStacktraceTracker
{
public:
    uint32_t InsertStacktrace(Stacktrace&& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount();

    StacktraceTrackerFixed() {}
    ~StacktraceTrackerFixed();

private:

    uint32_t InsertStacktraceUnlocked(Stacktrace&& trace);

private:
    absl::Mutex m_mt;

    std::array<Stacktrace, 128> m_stacktraces;
    size_t m_size{};
};
