#include "stacktrace_tracker.h"

#include "log.h"
#include "stacktrace.h"

#include <cstddef>

void StacktraceTracker::PostponedConstruct()
{
    m_storage = std::make_unique<Storage>();
}

std::optional<Stacktrace> StacktraceTracker::GetStacktraceFromHash(uint32_t traceHash)
{
    absl::MutexLock lock(&m_mt);
    auto traceIt = m_storage->m_stacktraces.find(traceHash);
    if (traceIt == m_storage->m_stacktraces.end()) {
        LogDebug("Not found stacktrace with hash: " fU32, traceHash);
        return {};
    }
    return m_storage->m_reversedStacktraces[traceIt->second];
}

void StacktraceTracker::SaveStacktrace(Stacktrace&& stacktrace)
{
    absl::MutexLock lock(&m_mt);
    const auto traceHash = stacktrace.Hash();

    auto traceIt = m_storage->m_stacktraces.find(traceHash);
    if (traceIt != m_storage->m_stacktraces.end()) {
        return;
    }
    auto traceId = m_storage->m_reversedStacktraces.size();
    m_storage->m_stacktraces.insert({traceHash, traceId});
    m_storage->m_reversedStacktraces.emplace_back(std::move(stacktrace));
}

size_t StacktraceTracker::StacktracesCount()
{
    absl::MutexLock lock(&m_mt);
    return m_storage->m_reversedStacktraces.size();
}
