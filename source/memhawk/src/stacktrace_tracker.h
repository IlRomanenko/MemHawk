#pragma once

#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <boost/container/devector.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace memhawk
{

class StacktraceTracker : public IStacktraceTracker
{
public:
    StacktraceTracker(bool dump = false);
    ~StacktraceTracker();

    void PostponedConstruct();

    uint32_t InsertStacktrace(Stacktrace&& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount() override;

private:
    struct TraceNode
    {
        void* ptr{};
        uint32_t parent{};
        bool leaf{};

        explicit TraceNode(void* ctrPtr, uint32_t ctrParent, bool ctrLeaf)
            : ptr(ctrPtr), parent(ctrParent), leaf(ctrLeaf)
        {
        }
    };

    struct Storage
    {
        boost::container::devector<TraceNode> nodes;
        // <nodeId --> ptr -- > nextNodeId>
        absl::flat_hash_map<std::pair<uint32_t, void*>, uint32_t> edges;
        boost::container::devector<uint32_t> leafsId;
    };

private:
    absl::Mutex m_mt;
    bool m_dump{false};
    std::unique_ptr<Storage> m_storage;
};

} // namespace memhawk
