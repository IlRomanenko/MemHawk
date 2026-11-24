#pragma once

#include "config/describe_struct.h"

#include <cstdint>

namespace memhawk
{

namespace logging
{
enum class LogLevel : uint8_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Off = 5,
};
} // namespace logging

namespace config
{
template <>
struct ValueDescriber<logging::LogLevel>
{
    static std::string Describe(const logging::LogLevel& level)
    {
        switch (level)
        {
        case logging::LogLevel::Trace:
            return "Trace";
        case logging::LogLevel::Debug:
            return "Debug";
        case logging::LogLevel::Info:
            return "Info";
        case logging::LogLevel::Warning:
            return "Warning";
        case logging::LogLevel::Error:
            return "Error";
        case logging::LogLevel::Off:
            return "Off";
        default:
            return "Unknown";
        }
    }
};
} // namespace config

} // namespace memhawk
