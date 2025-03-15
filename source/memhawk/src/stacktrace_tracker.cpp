#include "stacktrace_tracker.h"

#include "log.h"
#include "stacktrace.h"

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptors.hpp>
#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

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

StacktraceTracker::~StacktraceTracker()
{
    LogInfo(fSzt, m_storage->nodes.size());
    if (!dump) {
        return;
    }
    std::ofstream result(fmt::format("res_{}.txt", getpid()), std::ios_base::out | std::ios_base::trunc);

    for (const auto& traceId : m_storage->leafsId) {
        const auto trace = GetStacktraceFromId(traceId).value();
        auto span = trace.GetTrace();
        std::stringstream str;
        for (const auto& ptr : span) {
            str << reinterpret_cast<uintptr_t>(ptr) << ' ';
        }
        result << str.str() << "\n";
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
        auto nextNodeIt = m_storage->nodes[nodeId].edges.find(ptr);
        if (nextNodeIt == m_storage->nodes[nodeId].edges.end()) {
            uint32_t nextNodeId = m_storage->nodes.size();
            m_storage->nodes.push_back(TraceNode{ptr, nodeId, false});
            nextNodeIt = m_storage->nodes[nodeId].edges.insert({ptr, nextNodeId}).first;
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
    std::array<void*, MaxUnwindSize> trace;
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
