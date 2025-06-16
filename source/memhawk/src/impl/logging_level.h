#pragma once

#include "config/describe_struct.h"

#include <cstdint>

namespace memhawk
{

enum class LogLevel : uint8_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Off = 5
};

namespace config
{
template <>
struct ValueDescriber<LogLevel>
{
    static std::string Describe(const LogLevel& level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return "Trace";
        case LogLevel::Debug:
            return "Debug";
        case LogLevel::Info:
            return "Info";
        case LogLevel::Warning:
            return "Warning";
        case LogLevel::Error:
            return "Error";
        case LogLevel::Off:
            return "Off";
        default:
            return "Unknown";
        }
    }
};
} // namespace config

} // namespace memhawk
