#pragma once

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <optional>

namespace memhawk
{

namespace bmi = boost::multi_index;

// Simple and fast lru-cache with focus on minimal allocations
template <typename Key, typename Value>
class LruCache
{
public:
    explicit LruCache(size_t capacity) : m_capacity(capacity)
    {
    }

    // returns evicted key if capacity is exceeded
    std::optional<Key> Insert(Key key, Value value)
    {
        auto& byKey = m_index.template get<TagByKey>();
        if (m_index.size() >= m_capacity)
        {
            auto& byOrder = m_index.template get<TagByOrder>();
            const auto it = m_index.template project<TagByKey>(byOrder.begin());
            m_index.modify(it, [&key, &value](auto& node) {
                std::swap(node.key, key);
                std::swap(node.value, value);
            });
            Relocate(it);
            return {std::move(key)};
        }
        const auto [insertIt, _] = byKey.insert({std::move(key), std::move(value)});
        Relocate(insertIt);
        return {};
    }

    std::optional<Value> Touch(const Key& key)
    {
        const auto& byKey = m_index.template get<TagByKey>();
        const auto it = byKey.find(key);
        if (it != byKey.end())
        {
            Relocate(it);
            return it->value;
        }
        return {};
    }

    bool Erase(const Key& key)
    {
        auto& byKey = m_index.template get<TagByKey>();
        return byKey.erase(key) > 0;
    }

    size_t Size() const
    {
        return m_index.size();
    }

private:
    struct IndexValue
    {
        Key key;
        Value value;
    };

    struct TagByOrder;
    struct TagByKey;

    // clang-format off
    using Index = boost::multi_index_container<
        IndexValue,
        bmi::indexed_by<
            bmi::hashed_unique<
                bmi::tag<TagByKey>,
                bmi::member<IndexValue, Key, &IndexValue::key>
            >,
            bmi::sequenced<
                bmi::tag<TagByOrder>
            >
        >
    >;
    // clang-format on

    void Relocate(Index::const_iterator it)
    {
        auto& byOrder = m_index.template get<TagByOrder>();
        const auto orderIt = m_index.template project<TagByOrder>(it);
        byOrder.relocate(byOrder.end(), orderIt);
    }

private:
    Index m_index;
    size_t m_capacity{};
};

} // namespace memhawk
