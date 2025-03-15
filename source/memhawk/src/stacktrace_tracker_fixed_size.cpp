#include "stacktrace_tracker_fixed_size.h"

#include "stacktrace.h"
#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <optional>

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

StacktraceTrackerFixed::~StacktraceTrackerFixed()
{
    // if (!dump) {
    //     return;
    // }
    std::ofstream result(fmt::format("res_inner_{}.txt", getpid()), std::ios_base::out | std::ios_base::trunc);

    result << "Inner stacktraces:" << "\n";
    for (size_t i = 0; i < m_size; i++) {
        const auto stacktrace = m_stacktraces[i].Describe();
        result << "traceId: " << i << "\n" << stacktrace << "\n\n";
    }
    // for (const auto& traceId : m_storage->leafsId) {
    //     const auto trace = GetStacktraceFromId(traceId).value();
    //     auto span = trace.GetTrace();
    //     std::stringstream str;
    //     for (const auto& ptr : span) {
    //         str << reinterpret_cast<uintptr_t>(ptr) << ' ';
    //     }
    //     result << str.str() << "\n";
    // }
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
        if (m_size < 128) {
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
