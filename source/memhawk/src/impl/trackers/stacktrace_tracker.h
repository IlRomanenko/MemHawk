#pragma once

#include "config.h"
#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>

namespace memhawk
{

class StacktraceTracker : public IStacktraceTracker
{
public:
    explicit StacktraceTracker(StacktraceTrackerConfig cfg);
    ~StacktraceTracker() override;

    void PostponedConstruct();

    uint32_t InsertStacktrace(Stacktrace&& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount() override;

private:
    static constexpr uint64_t InvalidEdgeId = std::numeric_limits<uint32_t>::max();

    struct TraceNode
    {
        void* ptr{};
        uint32_t parent{};

        explicit TraceNode(void* ctrPtr, uint32_t ctrParent)
            : ptr(ctrPtr), parent(ctrParent)
        {
        }
    };

    struct Storage
    {
        std::deque<TraceNode> nodes;
        // <nodeId --> ptrId -- > nextNodeId>
        absl::flat_hash_map<std::pair<uint32_t, uint32_t>, uint32_t> edges;

        // ptrValue --> ptrId
        absl::flat_hash_map<uint64_t, uint32_t> ptrMap;
        absl::flat_hash_set<uint32_t> leafsId;

        uint32_t ptrCounter{1};
    };

    Stacktrace GetStacktrace(uint32_t traceId);

private:
    StacktraceTrackerConfig m_cfg;

    absl::base_internal::SpinLock m_mt;
    std::unique_ptr<Storage> m_storage;
};

} // namespace memhawk
