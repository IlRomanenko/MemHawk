#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <iterator>
#include <queue>
#include <span>
#include <vector>

namespace graph
{

struct RetrievedNode
{
    int64_t total{};
    int64_t value{};
    uint32_t depth{};
    uint32_t key{};

    bool operator<=>(const RetrievedNode&) const = default;
};

class CompressedTrie
{
public:
    CompressedTrie()
    {
        m_nodes.push_back(Node{});
    }

    void Append(std::span<const uint32_t> path, int64_t value)
    {
        uint32_t root = 0;

        while (!path.empty())
        {
            const uint32_t matched = SplitPath(root, path);
            path = path.subspan(matched);

            if (path.empty())
            {
                break;
            }

            auto& node = m_nodes[root];
            node.total += value;

            const uint32_t key = path.front();
            auto it = node.edges.find(key);
            if (it == node.edges.end())
            {
                const uint32_t nextNodeId = m_nodes.size();
                m_nodes.emplace_back(Node{
                    .path = CreatePath(path),
                    .edges = {},
                    .total = 0,
                    .value = 0,
                });
                it = node.edges.insert({key, nextNodeId}).first;
            }
            root = it->second;
        }
        m_nodes[root].total += value;
        m_nodes[root].value += value;
    }

    std::vector<RetrievedNode> GetTop(size_t count) const
    {
        struct QueueItem
        {
            int64_t neg_total{};
            uint32_t depth{};
            NodeOnPath node;

            bool operator>(const QueueItem& other) const
            {
                const auto pack = [](const QueueItem& item) { return std::tie(item.neg_total, item.depth); };
                return pack(*this) > pack(other);
            }
        };

        // priority queue with minimum on top
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> queue;

        // add root node
        queue.push({
            .neg_total = -m_nodes[0].total,
            .depth = 0,
            .node = NodeOnPath{.nodeId = 0, .pathSize = 0},
        });

        absl::flat_hash_set<NodeOnPath> selectedNodes;
        while (selectedNodes.size() < count && !queue.empty())
        {
            const QueueItem item = queue.top();
            queue.pop();
            selectedNodes.insert(item.node);

            const auto& node = m_nodes[item.node.nodeId];
            if (item.node.pathSize != node.path.size())
            {
                continue;
            }

            const auto depth = item.depth;

            for (const auto& [_, childId] : node.edges)
            {
                const auto& childNode = m_nodes[childId];
                // iterate over all compressed nodes
                for (uint32_t pos = 0; pos < childNode.path.size(); pos++)
                {
                    queue.push({
                        .neg_total = -childNode.total,
                        .depth = depth + pos + 1,
                        .node = NodeOnPath{.nodeId = childId, .pathSize = pos + 1},
                    });
                }
            }
        }
        std::vector<RetrievedNode> result;
        RetrieveNodes(0, 0, selectedNodes, result);
        return result;
    }

    void Stats()
    {
        std::map<uint32_t, size_t> pathStat;
        std::map<uint32_t, size_t> children;

        std::function<void(uint32_t)> dfs = [&](uint32_t nodeId) {
            const auto& node = m_nodes[nodeId];
            children[node.edges.size()]++;
            pathStat[node.path.size()]++;
            for (const auto& [_, childId] : node.edges)
            {
                dfs(childId);
            }
        };
        dfs(0);

        std::cout << "PathStat" << "\n\n";
        for (size_t i = 0; i < 256; i++)
        {
            if (pathStat[i] != 0)
            {
                std::cout << i << ' ' << pathStat[i] << '\n';
            }
        }

        std::cout << "\n\n\n\n";

        std::cout << "Children" << "\n\n";
        for (size_t i = 0; i < 256; i++)
        {
            if (children[i] != 0)
            {
                std::cout << i << ' ' << children[i] << '\n';
            }
        }
        std::cout << "\n";
        std::cout << "Nodes:  " << m_nodes.size() << "\n";
        std::cout << "Pathes: " << m_paths.size() << "\n";
        std::cout << "\n";
    }

protected:
    struct PathSpan
    {
        uint32_t begin{};
        uint32_t end{};

        size_t size() const
        {
            return end - begin;
        }

        PathSpan offset(uint32_t count) const
        {
            return PathSpan{begin + count, end};
        }

        PathSpan first(uint32_t count) const
        {
            return PathSpan{begin, begin + count};
        }
    };

    struct Node
    {
        PathSpan path;
        absl::flat_hash_map<uint32_t, uint32_t> edges;
        int64_t total{};
        int64_t value{};
    };

private:
    template <typename T>
    class DequeSpan
    {
    public:
        explicit DequeSpan(const std::deque<T>& data, PathSpan path) : m_data{data}, m_span{path}
        {
        }

        size_t size() const
        {
            return m_span.size();
        }

        const T& operator[](size_t i) const
        {
            return m_data[m_span.begin + i];
        }

    private:
        const std::deque<T>& m_data; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        PathSpan m_span;
    };

    struct NodeOnPath
    {
        uint32_t nodeId{};
        uint32_t pathSize{};

        auto operator<=>(const NodeOnPath&) const = default;

        template <typename H>
        friend H AbslHashValue(H h, const NodeOnPath& item)
        {
            return H::combine(std::move(h), item.nodeId, item.pathSize);
        }
    };

    void RetrieveNodes(uint32_t nodeId, uint32_t depth, const absl::flat_hash_set<NodeOnPath>& selectedNodes,
                      std::vector<RetrievedNode>& result) const
    {
        const auto& node = m_nodes[nodeId];
        const auto nodePath = DequeSpan(m_paths, node.path);
        for (uint32_t pos = 0; pos < nodePath.size(); pos++)
        {
            const NodeOnPath nodeKey{.nodeId = nodeId, .pathSize = pos + 1};
            if (!selectedNodes.contains(nodeKey))
            {
                // further nodes were not selected and therefore it's not necessary to traverse them
                return;
            }
            int64_t value = 0;
            // check is it a leaf node on path
            if (pos + 1 == nodePath.size())
            {
                value = node.value;
            }
            result.push_back(RetrievedNode{
                .total = node.total,
                .value = value,
                .depth = depth + pos + 1,
                .key = nodePath[pos],
            });
        }
        if (nodeId == 0)
        {
            // manually add root node
            result.push_back(RetrievedNode{
                .total = node.total,
                .value = 0,
                .depth = depth,
                .key = 0,
            });
        }
        std::vector<std::pair<int64_t, uint32_t>> childrenOrdered;
        for (const auto& [key, childId] : node.edges)
        {
            const auto& childNode = m_nodes[childId];
            childrenOrdered.emplace_back(childNode.total, key);
        }
        // order children by subtree weight
        std::ranges::sort(childrenOrdered, std::greater<>{});
        for (const auto& [_, childKey] : childrenOrdered)
        {
            const auto childId = node.edges.at(childKey);
            RetrieveNodes(childId, depth + nodePath.size(), selectedNodes, result);
        }
    }

    PathSpan CreatePath(std::span<const uint32_t> path)
    {
        const uint32_t begin = m_paths.size();
        std::ranges::copy(path, std::back_inserter(m_paths));
        const uint32_t end = m_paths.size();
        return PathSpan{begin, end};
    }

    uint32_t SplitPath(uint32_t root, std::span<const uint32_t> path)
    {
        auto& node = m_nodes[root];
        const auto& nodePath = node.path;
        const auto& nodeSpan = DequeSpan(m_paths, nodePath);
        uint32_t matched = 0;
        const uint32_t maxMatched = std::min(path.size(), nodePath.size());

        for (; matched < maxMatched; matched++)
        {
            if (path[matched] != nodeSpan[matched])
            {
                break;
            }
        }

        // full path matched - nothing to split
        if (matched == nodePath.size())
        {
            return matched;
        }

        // [<matched prefix>, key, <leftover suffix>]
        const uint32_t key = m_paths[nodePath.begin + matched];

        PathSpan matchedSpan = nodePath.first(matched);
        PathSpan leftoverSpan = nodePath.offset(matched);

        const uint32_t leftoverNodeId = m_nodes.size();
        auto& leftoverNode = m_nodes.emplace_back();

        leftoverNode.path = leftoverSpan;
        leftoverNode.edges = std::move(node.edges);
        leftoverNode.total = node.total;
        leftoverNode.value = node.value;

        node.path = matchedSpan;
        node.edges = {{key, leftoverNodeId}};
        node.value = 0;
        // node.total accounts for whole subtree and therefore not changed 
        return matched;
    }

protected:
    // must be deque: SplitPath holds a node reference across emplace_back
    std::deque<Node> m_nodes;
    std::deque<uint32_t> m_paths;
};

} // namespace graph
