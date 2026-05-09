#include "stacktrace_tracker_static.h"

#include "compression.h"
#include "config.h"
#include "logging.h"
#include "stacktrace.h"

#include <absl/base/internal/spinlock.h>
#include <absl/types/span.h>
#include <boost/range/algorithm/find.hpp>
#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <xxhash.h>

namespace memhawk
{


bool IsFixedTrackerId(uint32_t traceId)
{
    return traceId & StaticStacktraceTracker::FixedTrackerIdFlag;
}

StaticStacktraceTracker::StaticStacktraceTracker(StacktraceTrackerConfig cfg) : m_cfg{std::move(cfg)}
{
}

StaticStacktraceTracker::~StaticStacktraceTracker()
{
    if (!*m_cfg.DumpStacktraces)
    {
        return;
    }
    auto filename = GetProcessLogName("inner_stacktraces");
    if (m_cfg.Filename.Value().has_value())
    {
        filename = *m_cfg.Filename.Value(); // NOLINT(bugprone-unchecked-optional-access)
    }
    std::ofstream result(filename, std::ios_base::out | std::ios_base::trunc);
    result << "Inner stacktraces:" << "\n";
    for (size_t i = 0; i < m_size; i++)
    {
        const auto stacktrace = GetStacktrace(i);
        const auto traceStr = stacktrace.Describe();
        result << "traceId: " << i << "\n" << traceStr << "\n\n";
    }
    result.flush();
    result.close();
}

size_t StaticStacktraceTracker::StacktracesCount()
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    return m_size;
}

size_t StaticStacktraceTracker::GetStorageSize()
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    return m_storageSize;
}

uint32_t StaticStacktraceTracker::InsertStacktrace(const Stacktrace& trace)
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    auto span = trace.GetTrace();
    if (span.size() > MaxStacktraceLength) {
        span = span.subspan(0, MaxStacktraceLength);
    }
    auto traceId = InsertStacktraceUnlocked(span);
    return traceId ^ FixedTrackerIdFlag;
}

bool StaticStacktraceTracker::TraceElement::operator==(const TraceElement& rhs) const
{
    if (hash != rhs.hash)
    {
        return false;
    }
    return trace == rhs.trace;
}

uint32_t StaticStacktraceTracker::InsertStacktraceUnlocked(absl::Span<void* const> span)
{
    const auto uspan = absl::MakeConstSpan(reinterpret_cast<const uint64_t*>(span.data()), span.size());

    m_compressedTrace = {};
    const uint32_t compressedSize = bit_packing::Compress(uspan, m_compressedTrace.data());
    const auto compressedSpan = absl::MakeConstSpan(m_compressedTrace.begin(), compressedSize);
    const uint64_t hash = XXH3_64bits(compressedSpan.data(), compressedSpan.size() * sizeof(uint32_t));

    const TraceElement cur{.hash = hash, .trace = compressedSpan};

    const auto elementsSpan = absl::MakeConstSpan(m_elements.begin(), m_size);
    const auto curIt = boost::range::find(elementsSpan, cur);
    if (curIt == elementsSpan.end())
    {
        if (m_storageSize + compressedSize < StorageSize && m_size + 1 < ElementsCount)
        {
            m_elements[m_size] = TraceElement{
                .hash = hash, .trace = absl::MakeConstSpan(m_storage.data() + m_storageSize, compressedSize)};
            memcpy(m_storage.data() + m_storageSize, compressedSpan.data(), compressedSize * sizeof(uint32_t));
            m_storageSize += compressedSize;
            m_size++;
        }
        else
        {
            // todo: refactor
            return UnknownTraceId ^ FixedTrackerIdFlag;
        }
    }

    return std::distance(elementsSpan.begin(), curIt);
}

std::optional<Stacktrace> StaticStacktraceTracker::GetStacktraceFromId(uint32_t traceId)
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    if (!IsFixedTrackerId(traceId))
    {
        return {};
    }
    traceId ^= FixedTrackerIdFlag;
    if (traceId >= m_size)
    {
        return {};
    }
    return GetStacktrace(traceId);
}

Stacktrace StaticStacktraceTracker::GetStacktrace(uint32_t traceId)
{
    m_decompressedTrace = {};
    const auto size = bit_packing::Decompress(m_elements[traceId].trace, m_decompressedTrace.data());
    return Stacktrace{reinterpret_cast<void**>(m_decompressedTrace.data()), size};
}

} // namespace memhawk
