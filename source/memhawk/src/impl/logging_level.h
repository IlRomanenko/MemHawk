#pragma once

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

} // namespace memhawk
