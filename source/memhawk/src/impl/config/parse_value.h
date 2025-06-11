#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace memhawk
{
namespace config
{
template <typename T>
inline bool ParseValue(std::string_view str, T& value);

template <typename T>
inline bool ParseValue(std::string_view str, T& value) requires(std::is_integral_v<T>)
{
    auto res = std::from_chars(str.begin(), str.end(), value);
    return res.ec == std::errc();
}

template <typename T>
inline bool ParseValue(std::string_view str, T& value) requires(std::is_enum_v<T>)
{
    std::underlying_type_t<T> v = {};
    auto res = ParseValue(str, v);
    if (!res)
    {
        return res;
    }
    value = static_cast<T>(v);
    return true;
}

template <>
inline bool ParseValue<std::string_view>(std::string_view str, std::string_view& value)
{
    value = str;
    return true;
}

template <>
inline bool ParseValue<std::string>(std::string_view str, std::string& value)
{
    value = str;
    return true;
}

template <>
inline bool ParseValue<bool>(std::string_view str, bool& value)
{
    if (str == "true")
    {
        value = true;
        return true;
    }
    if (str == "false")
    {
        value = false;
        return true;
    }
    int32_t v = 0;
    if (!ParseValue(str, v))
    {
        return false;
    }
    value = v > 0;
    return true;
}

template <typename T>
inline bool ParseValue(std::string_view str, std::optional<T>& value)
{
    T obj{};
    if (!ParseValue(str, obj))
    {
        return false;
    }
    value = obj;
    return true;
}

} // namespace config
} // namespace memhawk
