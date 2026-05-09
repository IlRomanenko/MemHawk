#pragma once

#include "parse_value.h"

#include <string_view>

namespace memhawk
{
namespace config
{

template <typename T, typename Meta>
class ConfigVar
{
public:
    constexpr ConfigVar() noexcept = default;

    constexpr explicit ConfigVar(T defaultValue) noexcept : m_value{std::move(defaultValue)}
    {
    }

    constexpr ConfigVar(const ConfigVar& other) noexcept
    {
        m_value = other.m_value;
        m_set = other.m_set;
    }

    constexpr ConfigVar(ConfigVar&& other) noexcept
    {
        m_value = std::move(other.m_value);
        m_set = other.m_set;
    }

    ConfigVar& operator=(const ConfigVar& other) noexcept = default;
    ConfigVar& operator=(ConfigVar&& other) noexcept = default;

    ConfigVar& operator=(const T& value)
    {
        m_value = value;
        m_set = true;
        return *this;
    }

    constexpr T& operator*() noexcept
    {
        return m_value;
    }

    constexpr const T& operator*() const noexcept
    {
        return m_value;
    }

    constexpr T* operator->() noexcept
    {
        return &m_value;
    }

    constexpr const T* operator->() const noexcept
    {
        return &m_value;
    }

    constexpr const T& Value() const noexcept
    {
        return m_value;
    }

    bool ParseValue(std::string_view str)
    {
        const bool ok = config::ParseValue(str, m_value);
        if (ok)
        {
            m_set = true;
        }
        return ok;
    }

    static constexpr const char* GetName() noexcept
    {
        return Meta::ParamName;
    }

    bool IsValid() const noexcept
    {
        return m_set || !Meta::IsRequired;
    }

    void SetFlag() noexcept
    {
        m_set = true;
    }

    bool operator==(const ConfigVar&) const = default;

private:
    T m_value = {};
    bool m_set{false};
};

// clang-format off
#define CONFIG_VAR(type, name, key, required, defaultValue)  \
    struct MetaData_## name ##_t { \
        static constexpr const char* ParamName = key; \
        static constexpr bool IsRequired = required; \
    }; \
    memhawk::config::ConfigVar<type, MetaData_## name ##_t> name{defaultValue} // NOLINT(bugprone-macro-parentheses)

#define CONFIG_VAR_OPT(type, name, key, defaultValue) CONFIG_VAR(type, name, key, false, defaultValue)
#define CONFIG_VAR_DEFAULT(type, name) CONFIG_VAR(type, name, #name, true, {})

} // namespace config
} // namespace memhawk
