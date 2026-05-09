#include "stacktrace_tracker.h"

#include "config.h"
#include "logging.h"
#include "stacktrace.h"
#include "stacktrace_tree.h"

#include <absl/base/internal/spinlock.h>
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
    m_storage->nodes.push_back(TraceNode{m_storage->tree.GetPtrId(0), 0});
}

size_t StacktraceTracker::StacktracesCount()
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    return m_storage->leafsId.size();
}

StacktraceTracker::StacktraceTracker(StacktraceTrackerConfig cfg) : m_cfg{std::move(cfg)}
{
}

void StacktraceTracker::Describe()
{
    LogInfo("StacktraceTracker: nodes: " fSzt ", edges: " fSzt ", ptrMap: " fSzt, m_storage->nodes.size(),
            m_storage->tree.GetEdgesCount(), m_storage->tree.GetPtrMapSize());
}

StacktraceTracker::~StacktraceTracker()
{
    if (!*m_cfg.DumpStacktraces)
    {
        return;
    }
    auto filename = GetProcessLogName("external_stacktraces");
    if (m_cfg.Filename.Value().has_value())
    {
        filename = *m_cfg.Filename.Value(); // NOLINT(bugprone-unchecked-optional-access)
    }
    std::ofstream result(filename, std::ios_base::out | std::ios_base::trunc);
    result << "External stacktraces:" << "\n";
    for (const auto& traceId : m_storage->leafsId)
    {
        const auto stacktrace = GetStacktrace(traceId);
        const auto traceStr = stacktrace.Describe();
        result << "traceId: " << traceId << "\n" << traceStr << "\n\n";
    }
    result.flush();
    result.close();
}

uint32_t StacktraceTracker::InsertStacktrace(const Stacktrace& trace)
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    const StacktraceTree::NodeId nodeId = m_storage->tree.index(
        trace, [this](uintptr_t /*ptrValue*/, StacktraceTree::PtrId ptrId, StacktraceTree::NodeId parent) {
            m_storage->nodes.push_back(TraceNode{.id = ptrId, .parent = parent.value()});
        });
    m_storage->leafsId.insert(nodeId.value());
    return nodeId.value();
}

std::optional<Stacktrace> StacktraceTracker::GetStacktraceFromId(uint32_t traceId)
{
    const absl::base_internal::SpinLockHolder lock(&m_mt);
    if (traceId >= m_storage->nodes.size())
    {
        return {};
    }
    if (!m_storage->leafsId.contains(traceId))
    {
        return {};
    }
    return GetStacktrace(traceId);
}

Stacktrace StacktraceTracker::GetStacktrace(uint32_t traceId)
{
    std::array<void*, MaxUnwindDepth> trace{};
    size_t traceIt = 0;
    auto nodeId = traceId;
    while (nodeId != 0)
    {
        const auto& node = m_storage->nodes[nodeId];
        const auto ptrValue = m_storage->tree.GetPtrValue(node.id);;
        trace[traceIt] = reinterpret_cast<void*>(ptrValue);
        traceIt++;
        nodeId = node.parent;
    }
    return Stacktrace{trace.data(), traceIt};
}

} // namespace memhawk
