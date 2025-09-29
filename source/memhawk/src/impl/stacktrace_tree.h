#pragma once

#include "stacktrace.h"
#include "tag_type.h"

#include <absl/container/flat_hash_map.h>
#include <boost/range/adaptor/reversed.hpp>

#include <cstdint>

namespace memhawk
{


class StacktraceTree
{
public:
    using NodeId = IdTagType<StacktraceTree, uint32_t>;

    NodeId index(const Stacktrace& stacktrace, std::function<void(uintptr_t, NodeId)> onNewNode)
    {
        NodeId nodeId{0};

        const auto span = stacktrace.GetTrace();
        const auto reversed = boost::adaptors::reverse(span);
        for (const auto ptr : reversed)
        {
            if (!ptr)
            {
                continue;
            }
            const auto ptrValue = reinterpret_cast<uintptr_t>(ptr);

            auto ptrValueIt = m_ptrMap.find(ptrValue);
            if (ptrValueIt == m_ptrMap.end())
            {
                uint32_t ptrId = static_cast<uint32_t>(m_ptrMap.size());
                ptrValueIt = m_ptrMap.insert({ptrValue, PtrId{ptrId}}).first;
            }

            const auto ptrValueId = ptrValueIt->second;

            auto edgeIt = m_edges.find({nodeId, ptrValueId});
            if (edgeIt == m_edges.end())
            {
                onNewNode(ptrValue, nodeId);
                edgeIt = m_edges.insert({{nodeId, ptrValueId}, NodeId{m_nodesCounter}}).first;
                m_nodesCounter++;
            }
            nodeId = edgeIt->second;
        }
        return nodeId;
    }

private:
    using PtrId = IdTagType<struct PtrIdTag, uint32_t>;

    absl::flat_hash_map<std::pair<NodeId, PtrId>, NodeId> m_edges;
    uint32_t m_nodesCounter = 1;
    absl::flat_hash_map<uintptr_t, PtrId> m_ptrMap;
};

} // namespace memhawk
