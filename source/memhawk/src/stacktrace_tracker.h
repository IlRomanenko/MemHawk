#pragma once

#include "i_stacktrace_tracker.h"
#include "stacktrace.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <boost/container/devector.hpp>

#include <cstdint>
#include <memory>
#include <optional>

class StacktraceTracker : public IStacktraceTracker
{
public:
    void PostponedConstruct();

    uint32_t InsertStacktrace(Stacktrace&& trace) override;
    std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) override;

    size_t StacktracesCount();

    StacktraceTracker(bool ctrDump = false) : dump(ctrDump) {}
    ~StacktraceTracker();

private:

    struct TraceNode{
        void* ptr{};
        uint32_t parent{};
        bool leaf{};

        absl::flat_hash_map<void*, uint32_t> edges;

        explicit TraceNode(void* ctrPtr, uint32_t ctrParent, bool ctrLeaf)
            : ptr(ctrPtr), parent(ctrParent), leaf(ctrLeaf)
        {
        }
    };

    struct Storage
    {
        boost::container::devector<TraceNode> nodes;
        boost::container::devector<uint32_t> leafsId;
    };

private:
    absl::Mutex m_mt;
    bool dump{false};
    std::unique_ptr<Storage> m_storage;
};
