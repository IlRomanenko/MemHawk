#pragma once

#include "config/config_var.h"
#include "config/parse_struct.h"

#include <boost/hana/for_each.hpp>
#include <fmt/format.h>

#include <functional>
#include <string>
#include <type_traits>

namespace memhawk
{
namespace config
{

template <typename T>
struct ValueDescriber;

template <typename T>
struct ValueDescriber
{
    static std::string Describe(const T& value)
    {
        return fmt::format("{}", value);
    }
};

template <typename T, typename Meta>
struct ValueDescriber<ConfigVar<T, Meta>>
{
    static std::string Describe(const ConfigVar<T, Meta>& value)
    {
        return ValueDescriber<T>::Describe(*value);
    }
};

template <typename T>
struct ValueDescriber<std::optional<T>>
{
    static std::string Describe(const std::optional<T>& value)
    {
        if (value.has_value())
        {
            return ValueDescriber<T>::Describe(*value);
        }
        return "{}";
    }
};

struct DescribeContext
{
    std::string_view key;
    std::string_view field;
    std::string_view value;
};

template <typename T>
inline void DescribeStruct(const T& object, std::function<void(const DescribeContext&)> onKey,
                           const std::string& keyPath = "")
{
    boost::hana::for_each(boost::hana::accessors<T>(), [&](const auto& pair) {
        auto& member = boost::hana::second(pair)(object);
        const auto memberName = member.GetName();
        auto fullPath = keyPath + OptionNestedDelim + memberName;
        if (keyPath.empty())
        {
            fullPath = memberName;
        }
        if constexpr (boost::hana::Struct<std::decay_t<decltype(*member)>>::value)
        {
            DescribeStruct(*member, onKey, fullPath);
        }
        else
        {
            const auto valueStr = ValueDescriber<std::decay_t<decltype(member)>>::Describe(member); 
            onKey({.key = fullPath, .field = memberName, .value = valueStr});
        }
    });
}

} // namespace config
} // namespace memhawk
