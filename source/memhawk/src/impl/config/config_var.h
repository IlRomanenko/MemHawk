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
    constexpr ConfigVar(T defaultValue) : m_value{std::move(defaultValue)}
    {
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

    ConfigVar& operator=(const T& value)
    {
        m_value = value;
        m_set = true;
        return *this;
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

    static constexpr const char* GetDefaultValue() 
    {
        return Meta::DefaultValue;
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
        static constexpr const char* DefaultValue = #defaultValue; \
    }; \
    memhawk::config::ConfigVar<type, MetaData_## name ##_t> name{defaultValue} // NOLINT(bugprone-macro-parentheses)

#define CONFIG_VAR_OPT(type, name, key, defaultValue) CONFIG_VAR(type, name, key, false, defaultValue)
#define CONFIG_VAR_DEFAULT(type, name) CONFIG_VAR(type, name, #name, true, {})

} // namespace config
} // namespace memhawk
