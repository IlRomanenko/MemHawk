#pragma once

#include "stacktrace.h"
#include "tag_type.h"

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <boost/range/adaptor/reversed.hpp>

#include <cstdint>

namespace memhawk
{

class StacktraceTree
{
public:
    using NodeId = IdTagType<struct NodeIdTag, uint32_t>;
    using PtrId = IdTagType<struct PtrIdTag, uint32_t>;

    NodeId index(const Stacktrace& stacktrace, const std::function<void(uintptr_t, PtrId, NodeId)>& onNewNode)
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
            const auto ptrValueId = GetPtrId(ptrValue);

            auto edgeIt = m_edges.find({nodeId, ptrValueId});
            if (edgeIt == m_edges.end())
            {
                onNewNode(ptrValue, ptrValueId, nodeId);
                edgeIt = m_edges.insert({{nodeId, ptrValueId}, NodeId{m_nodesCounter}}).first;
                m_nodesCounter++;
            }
            nodeId = edgeIt->second;
        }
        return nodeId;
    }

    PtrId GetPtrId(uintptr_t ptrValue)
    {
        auto ptrValueIt = m_ptrMap.find(ptrValue);
        if (ptrValueIt == m_ptrMap.end())
        {
            const uint32_t ptrId = static_cast<uint32_t>(m_ptrMap.size());
            ptrValueIt = m_ptrMap.insert({ptrValue, PtrId{ptrId}}).first;
            // add rev mapping
            m_revPtrMap.insert({PtrId{ptrId}, ptrValue});
        }
        return ptrValueIt->second;
    }

    uintptr_t GetPtrValue(PtrId ptrId)
    {
        return m_revPtrMap.at(ptrId);
    }

    size_t GetEdgesCount()
    {
        return m_edges.size();
    }

    size_t GetPtrMapSize()
    {
        return m_ptrMap.size();
    }

private:
    uint32_t m_nodesCounter = 1;
    absl::btree_map<std::pair<NodeId, PtrId>, NodeId> m_edges;
    absl::flat_hash_map<uintptr_t, PtrId> m_ptrMap;
    absl::flat_hash_map<PtrId, uintptr_t> m_revPtrMap;
};

} // namespace memhawk
