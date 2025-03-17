#include "stacktrace_tracker.h"

#include "config.h"
#include "log_name.h"
#include "stacktrace.h"

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptors.hpp>
#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>

namespace memhawk
{

void StacktraceTracker::PostponedConstruct()
{
    m_storage = std::make_unique<Storage>();
    // add root node
    m_storage->nodes.push_back(TraceNode{nullptr, 0, false});
}

size_t StacktraceTracker::StacktracesCount()
{
    absl::MutexLock lock(&m_mt);
    return m_storage->leafsId.size();
}

StacktraceTracker::StacktraceTracker(bool dump) : m_dump(dump)
{
}

StacktraceTracker::~StacktraceTracker()
{
    if (!m_dump) {
        return;
    }
    std::ofstream result(GetProcessLogName("inner_stacktraces"), std::ios_base::out | std::ios_base::trunc);
    result << "Inner stacktraces:" << "\n";
    for (const auto& traceId : m_storage->leafsId) {
        const auto trace = GetStacktraceFromId(traceId).value();
        const auto stacktrace = trace.Describe();
        result << "traceId: " << traceId << "\n" << stacktrace << "\n\n";
    }
    result.flush();
    result.close();
}

uint32_t StacktraceTracker::InsertStacktrace(Stacktrace&& trace)
{
    absl::MutexLock lock(&m_mt);

    const auto span = trace.GetTrace();
    const auto reversed = boost::adaptors::reverse(span);

    uint32_t nodeId = 0;
    for (const auto& ptr : reversed) {
        auto nextNodeIt = m_storage->edges.find({nodeId, ptr});
        if (nextNodeIt == m_storage->edges.end()) {
            uint32_t nextNodeId = m_storage->nodes.size();
            m_storage->nodes.push_back(TraceNode{ptr, nodeId, false});
            nextNodeIt = m_storage->edges.insert({{nodeId, ptr}, nextNodeId}).first;
        }
        nodeId = nextNodeIt->second;
    }
    // check if wasn't marked previously
    if (!m_storage->nodes[nodeId].leaf) {
        m_storage->nodes[nodeId].leaf = true;
        m_storage->leafsId.push_back(nodeId);
    }
    return nodeId;
}

std::optional<Stacktrace> StacktraceTracker::GetStacktraceFromId(uint32_t traceId)
{
    absl::MutexLock lock(&m_mt);
    std::array<void*, MaxUnwindDepth> trace;
    size_t traceIt = 0;
    if (traceId >= m_storage->nodes.size()) {
        return {};
    }
    auto nodeId = traceId;
    if (!m_storage->nodes[nodeId].leaf) {
        return {};
    }
    while (nodeId != 0) {
        const auto& node = m_storage->nodes[nodeId];
        trace[traceIt] = node.ptr;
        traceIt++;
        nodeId = node.parent;
    }
    return Stacktrace(trace.data(), traceIt);
}

} // namespace memhawk
