#include "graph.h"

#include <gtest/gtest.h>

using namespace testing;

namespace graph
{

struct NodeInspected
{
    std::vector<uint32_t> path;
    absl::flat_hash_map<uint32_t, uint32_t> edges;
    int64_t total{};
    int64_t value{};

    bool operator==(const NodeInspected&) const = default;
};

class TrieInspector : public CompressedTrie
{
public:
    NodeInspected GetNode(uint32_t nodeId)
    {
        EXPECT_GE(m_nodes.size(), nodeId);
        const auto& span = m_nodes[nodeId].path;
        NodeInspected result{
            .path = {m_paths.begin() + span.begin, m_paths.begin() + span.end},
            .edges = m_nodes[nodeId].edges,
            .total = m_nodes[nodeId].total,
            .value = m_nodes[nodeId].value,
        };
        return result;
    }

    size_t GetNodesCount() const
    {
        return m_nodes.size();
    }
};

class TrieFixture : public Test
{
protected:
    TrieInspector m_trie;
};

TEST_F(TrieFixture, Append_EmptyPath)
{
    m_trie.Append(std::span<const uint32_t>({}), 7);
    EXPECT_EQ(m_trie.GetNodesCount(), 1); // root node always exists

    const auto rootNode = m_trie.GetNode(0);
    EXPECT_EQ(m_trie.GetNodesCount(), 1);
    EXPECT_EQ(rootNode.total, 7);
    EXPECT_EQ(rootNode.value, 7);
    EXPECT_EQ(rootNode.path.size(), 0);
    EXPECT_EQ(rootNode.edges.size(), 0);
}

TEST_F(TrieFixture, Append_SinglePath_NoSplitting)
{
    m_trie.Append(std::span<const uint32_t>({8, 2, 3}), 10);
    EXPECT_EQ(m_trie.GetNodesCount(), 2);

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{8, 1}}, .total = 10, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedNewNode = NodeInspected{.path = {8, 2, 3}, .edges = {}, .total = 10, .value = 10};
    EXPECT_EQ(m_trie.GetNode(1), expectedNewNode);
}

TEST_F(TrieFixture, Append_SamePath_Accumulate)
{
    m_trie.Append(std::span<const uint32_t>({8, 2, 3}), 10);
    m_trie.Append(std::span<const uint32_t>({8, 2, 3}), 42);
    EXPECT_EQ(m_trie.GetNodesCount(), 2);

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{8, 1}}, .total = 52, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedNewNode = NodeInspected{.path = {8, 2, 3}, .edges = {}, .total = 52, .value = 52};
    EXPECT_EQ(m_trie.GetNode(1), expectedNewNode);
}

TEST_F(TrieFixture, Append_SharedPrefix_SplitLast)
{
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 10);
    m_trie.Append(std::span<const uint32_t>({1, 2, 4}), 5);
    EXPECT_EQ(m_trie.GetNodesCount(), 4);

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{1, 1}}, .total = 15, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedCommonNode = NodeInspected{.path = {1, 2}, .edges = {{3, 2}, {4, 3}}, .total = 15, .value = 0};
    EXPECT_EQ(m_trie.GetNode(1), expectedCommonNode);

    const auto expectedLeftNode = NodeInspected{.path = {3}, .edges = {}, .total = 10, .value = 10};
    EXPECT_EQ(m_trie.GetNode(2), expectedLeftNode);

    const auto expectedRightNode = NodeInspected{.path = {4}, .edges = {}, .total = 5, .value = 5};
    EXPECT_EQ(m_trie.GetNode(3), expectedRightNode);
}

TEST_F(TrieFixture, Append_LongerAfterShorter)
{
    m_trie.Append(std::span<const uint32_t>({1, 2}), 10);
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 5);
    EXPECT_EQ(m_trie.GetNodesCount(), 3);

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{1, 1}}, .total = 15, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedCommonNode = NodeInspected{.path = {1, 2}, .edges = {{3, 2}}, .total = 15, .value = 10};
    EXPECT_EQ(m_trie.GetNode(1), expectedCommonNode);

    const auto expectedLongNode = NodeInspected{.path = {3}, .edges = {}, .total = 5, .value = 5};
    EXPECT_EQ(m_trie.GetNode(2), expectedLongNode);
}

TEST_F(TrieFixture, Append_DoubleSplit)
{
    const auto paths = std::vector<std::vector<uint32_t>>{
        {1, 2, 3, 4},
        {1, 2, 5},
        {1, 6},
    };
    const auto values = std::vector<int64_t>{10, 5, 3};
    for (size_t i = 0; i < paths.size(); i++)
    {
        m_trie.Append(paths[i], values[i]);
    }

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{1, 1}}, .total = 18, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedCommonNode = NodeInspected{.path = {1}, .edges = {{2, 4}, {6, 5}}, .total = 18, .value = 0};
    EXPECT_EQ(m_trie.GetNode(1), expectedCommonNode);

    const auto expectedSecondCommonNode =
        NodeInspected{.path = {2}, .edges = {{3, 2}, {5, 3}}, .total = 15, .value = 0};
    EXPECT_EQ(m_trie.GetNode(4), expectedSecondCommonNode);

    const auto expectedLeftLeafNode = NodeInspected{.path = {3, 4}, .edges = {}, .total = 10, .value = 10};
    EXPECT_EQ(m_trie.GetNode(2), expectedLeftLeafNode);

    const auto expectedRightLeafNode = NodeInspected{.path = {5}, .edges = {}, .total = 5, .value = 5};
    EXPECT_EQ(m_trie.GetNode(3), expectedRightLeafNode);

    const auto expectedRightMostLeafNode = NodeInspected{.path = {6}, .edges = {}, .total = 3, .value = 3};
    EXPECT_EQ(m_trie.GetNode(5), expectedRightMostLeafNode);
}

TEST_F(TrieFixture, Append_NegativeValues)
{
    const std::vector<std::vector<uint32_t>> paths = {
        {1, 2},
        {1, 2},
        {1, 3},
    };
    const std::vector<int64_t> values = {10, -3, -5};

    for (size_t i = 0; i < paths.size(); i++)
    {
        m_trie.Append(paths[i], values[i]);
    }

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{1, 1}}, .total = 2, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedCommonNode = NodeInspected{.path = {1}, .edges = {{2, 2}, {3, 3}}, .total = 2, .value = 0};
    EXPECT_EQ(m_trie.GetNode(1), expectedCommonNode);

    const auto expectedLeftLeafNode = NodeInspected{.path = {2}, .edges = {}, .total = 7, .value = 7};
    EXPECT_EQ(m_trie.GetNode(2), expectedLeftLeafNode);

    const auto expectedRightLeafNode = NodeInspected{.path = {3}, .edges = {}, .total = -5, .value = -5};
    EXPECT_EQ(m_trie.GetNode(3), expectedRightLeafNode);
}

TEST_F(TrieFixture, Append_DisjointAtRoot)
{
    m_trie.Append(std::span<const uint32_t>({1}), 10);
    m_trie.Append(std::span<const uint32_t>({2}), 20);

    EXPECT_EQ(m_trie.GetNodesCount(), 3);

    const auto expectedRoot = NodeInspected{.path = {}, .edges = {{1, 1}, {2, 2}}, .total = 30, .value = 0};
    EXPECT_EQ(m_trie.GetNode(0), expectedRoot);

    const auto expectedLeftNode = NodeInspected{.path = {1}, .edges = {}, .total = 10, .value = 10};
    EXPECT_EQ(m_trie.GetNode(1), expectedLeftNode);

    const auto expectedRightNode = NodeInspected{.path = {2}, .edges = {}, .total = 20, .value = 20};
    EXPECT_EQ(m_trie.GetNode(2), expectedRightNode);
}

TEST_F(TrieFixture, GetTop_EmptyTrie_RootEmitted)
{
    EXPECT_EQ(m_trie.GetTop(10).size(), 1);
}

TEST_F(TrieFixture, GetTop_SinglePath_CountLimit)
{
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 10);
    EXPECT_EQ(m_trie.GetTop(1).size(), 1);
    EXPECT_EQ(m_trie.GetTop(2).size(), 2);
    EXPECT_EQ(m_trie.GetTop(3).size(), 3);
    EXPECT_EQ(m_trie.GetTop(4).size(), 4);
    EXPECT_EQ(m_trie.GetTop(5).size(), 4);
}

TEST_F(TrieFixture, GetTop_SinglePath_CorrectPath)
{
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 10);
    const auto expected = std::vector<RetrievedNode>{
        RetrievedNode{.total = 10, .value = 0, .depth = 0, .key = 0},
        RetrievedNode{.total = 10, .value = 0, .depth = 1, .key = 1},
        RetrievedNode{.total = 10, .value = 0, .depth = 2, .key = 2},
        RetrievedNode{.total = 10, .value = 10, .depth = 3, .key = 3},
    };
    const auto result = m_trie.GetTop(100);
    EXPECT_EQ(result, expected);
}

TEST_F(TrieFixture, GetTop_SharedPrefix_CountLimit)
{
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 10);
    m_trie.Append(std::span<const uint32_t>({1, 2, 4}), 5);
    for (const size_t size : std::vector<size_t>{1, 2, 3, 4, 5})
    {
        EXPECT_EQ(size, m_trie.GetTop(size).size());
    }
    EXPECT_EQ(5, m_trie.GetTop(6).size());
}

TEST_F(TrieFixture, GetTop_SharedPrefix_CorrectStructure)
{
    m_trie.Append(std::span<const uint32_t>({1, 2, 3}), 10);
    m_trie.Append(std::span<const uint32_t>({1, 2, 4}), 5);
    m_trie.Append(std::span<const uint32_t>({1, 5}), 3);
    const auto expected = std::vector<RetrievedNode>{
        RetrievedNode{.total = 18, .value = 0, .depth = 0, .key = 0},
        RetrievedNode{.total = 18, .value = 0, .depth = 1, .key = 1},
        RetrievedNode{.total = 15, .value = 0, .depth = 2, .key = 2},
        RetrievedNode{.total = 10, .value = 10, .depth = 3, .key = 3},
        RetrievedNode{.total = 5, .value = 5, .depth = 3, .key = 4},
        RetrievedNode{.total = 3, .value = 3, .depth = 2, .key = 5},
    };
    const auto result = m_trie.GetTop(100);
    EXPECT_EQ(result, expected);
}

} // namespace graph
