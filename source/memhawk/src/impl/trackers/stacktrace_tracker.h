#pragma once

#include "config.h"
#include "i_stacktrace_tracker.h"
#include "stacktrace.h"
#include "stacktrace_tree.h"

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

    uint32_t InsertStacktrace(const Stacktrace& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount() override;

    void Describe();

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
        StacktraceTree tree;

        absl::flat_hash_set<uint32_t> leafsId;
    };

    Stacktrace GetStacktrace(uint32_t traceId);

private:
    StacktraceTrackerConfig m_cfg;

    absl::base_internal::SpinLock m_mt;
    std::unique_ptr<Storage> m_storage;
};

} // namespace memhawk
