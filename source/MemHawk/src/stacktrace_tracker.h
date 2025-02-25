#pragma once

#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <cstdint>
#include <deque>
#include <optional>

class StacktraceTracker
{
public:
    void PostponedConstruct();

    std::optional<Stacktrace> GetStacktraceFromHash(uint32_t traceHash);
    void SaveStacktrace(Stacktrace&& trace);

    size_t StacktracesCount();

private:
    struct Storage
    {
        absl::flat_hash_map<uint32_t, uint32_t> m_stacktraces;
        // reversed m_stacktraces map, utilizes continious nature of m_traceId
        std::deque<Stacktrace> m_reversedStacktraces;
    };

private:
    absl::Mutex m_mt;
    std::unique_ptr<Storage> m_storage;
};
