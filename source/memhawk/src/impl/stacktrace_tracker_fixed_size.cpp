#include "stacktrace_tracker_fixed_size.h"

#include "config.h"
#include "log_name.h"
#include "stacktrace.h"

#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>

namespace memhawk
{

constexpr uint32_t FixedTrackerIdFlag = 1ul << (std::numeric_limits<uint32_t>::digits - 1);

bool IsFixedTrackerId(uint32_t traceId)
{
    return traceId & FixedTrackerIdFlag;
}

size_t StacktraceTrackerFixed::StacktracesCount()
{
    absl::MutexLock lock(&m_mt);
    return m_size;
}

StacktraceTrackerFixed::StacktraceTrackerFixed(bool dump) : m_dump(dump)
{
}

StacktraceTrackerFixed::~StacktraceTrackerFixed()
{
    if (!m_dump) {
        return;
    }
    std::ofstream result(GetProcessLogName("inner_stacktraces", gl_config), std::ios_base::out | std::ios_base::trunc);
    result << "Inner stacktraces:" << "\n";
    for (size_t i = 0; i < m_size; i++) {
        const auto stacktrace = m_stacktraces[i].Describe();
        result << "traceId: " << i << "\n" << stacktrace << "\n\n";
    }
    result.flush();
    result.close();
}

uint32_t StacktraceTrackerFixed::InsertStacktrace(Stacktrace&& trace)
{
    absl::MutexLock lock(&m_mt);
    auto traceId = InsertStacktraceUnlocked(std::move(trace));
    return traceId ^ FixedTrackerIdFlag;
}

uint32_t StacktraceTrackerFixed::InsertStacktraceUnlocked(Stacktrace&& trace)
{
    std::optional<size_t> foundPos{};
    for (size_t i = 0; i < m_size; i++) {
        if (trace == m_stacktraces[i]) {
            foundPos = i;
            break;
        }
    }
    if (!foundPos) {
        if (m_size < StorageSize) {
            foundPos = m_size;
            m_size++;
            m_stacktraces[foundPos.value()] = std::move(trace);
        } else {
            return std::numeric_limits<uint32_t>::max();
        }
    }
    return foundPos.value();
}

std::optional<Stacktrace> StacktraceTrackerFixed::GetStacktraceFromId(uint32_t traceId)
{
    absl::MutexLock lock(&m_mt);
    if (!IsFixedTrackerId(traceId)) {
        return {};
    }
    traceId ^= FixedTrackerIdFlag;
    if (traceId >= m_size) {
        return {};
    }
    return m_stacktraces[traceId];
}

} // namespace memhawk
