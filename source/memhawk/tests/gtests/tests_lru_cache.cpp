
#include "impl/lru_cache.h"

#include <gtest/gtest.h>

#include <cstddef>

struct MovableKey
{
    size_t id{};

    MovableKey() = default;

    explicit MovableKey(size_t value) : id{value}
    {
    }

    MovableKey(const MovableKey&) = delete;
    MovableKey& operator=(const MovableKey&) = delete;

    MovableKey(MovableKey&& rhs) noexcept : id{rhs.id}
    {
    }

    MovableKey& operator=(MovableKey&& rhs) noexcept
    {
        id = rhs.id;
        return *this;
    }

    bool operator==(const MovableKey& rhs) const
    {
        return id == rhs.id;
    }
};

namespace boost
{
template <>
struct hash<MovableKey>
{
    size_t operator()(const MovableKey& key) const
    {
        return key.id;
    }
};
} // namespace boost

namespace memhawk
{

TEST(LruCache, CheckNonExistingKey_ExpectNotFound)
{
    memhawk::LruCache<MovableKey, int> cache(1);
    const MovableKey key{2};
    EXPECT_FALSE(cache.Touch(key));
}

TEST(LruCache, CheckExisting_ExpectFound)
{
    memhawk::LruCache<MovableKey, int> cache(1);
    const MovableKey key{1};
    EXPECT_FALSE(cache.Touch(key));
    // check that nothing was evicted
    EXPECT_FALSE(cache.Insert(MovableKey{1}, 1));
    EXPECT_TRUE(cache.Touch(key));
}

TEST(LruCache, AddMultipleKeys_ExpectLastFound)
{
    constexpr size_t Capacity = 3;
    memhawk::LruCache<MovableKey, size_t> cache(Capacity);
    for (size_t i = 0; i < Capacity; i++)
    {
        EXPECT_FALSE(cache.Insert(MovableKey{i}, i));
    }
    for (size_t i = Capacity; i < Capacity * 2; i++)
    {
        auto elem = cache.Insert(MovableKey{i}, i);
        ASSERT_TRUE(elem);
        EXPECT_EQ(elem->id, i - Capacity); // NOLINT(bugprone-unchecked-optional-access)
    }
    for (size_t i = Capacity; i < Capacity * 2; i++)
    {
        EXPECT_TRUE(cache.Touch(MovableKey{i}));
    }
}

TEST(LruCache, AddKeysAndTouch_ExpectTouchedPreserved)
{
    constexpr size_t Capacity = 10;
    memhawk::LruCache<MovableKey, size_t> cache(Capacity);
    for (size_t i = 0; i < Capacity; i++)
    {
        EXPECT_FALSE(cache.Insert(MovableKey{i}, i));
    }
    for (size_t i = Capacity; i < Capacity * 10; i++)
    {
        EXPECT_TRUE(cache.Touch(MovableKey{0}));
        EXPECT_TRUE(cache.Touch(MovableKey{1}));
        auto elem = cache.Insert(MovableKey{i}, i);
        ASSERT_TRUE(elem);
        EXPECT_EQ(elem->id, i - Capacity + 2); // NOLINT(bugprone-unchecked-optional-access)
    }
}

} // namespace memhawk
