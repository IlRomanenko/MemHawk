#pragma once

#include <utility>

namespace memhawk
{

template <typename Tag, typename T>
class IdTagType
{
public:
    explicit constexpr IdTagType(T v) : m_value(v)
    {
    }

    explicit constexpr operator T() const
    {
        return m_value;
    }

    T value() const
    {
        return m_value;
    }

    auto operator<=> (const IdTagType&) const = default;

    template <typename H>
    friend H AbslHashValue(H state, const IdTagType& v)
    {
        return H::combine(std::move(state), v.m_value);
    }

private:
    T m_value{};
};

template <typename T>
class GuardTag
{
private:
    explicit GuardTag() = default;
    friend T;
};

} // namespace memhawk
