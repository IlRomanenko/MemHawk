#pragma once

#include "config.h"
#include <fmt/format.h>

#include <cerrno>
#include <string>
#include <unistd.h>

namespace memhawk
{

inline std::string GetProcessLogName(const char* suffix)
{
    return fmt::format("{}/memhawk_{}_{}_{}.log", gl_config.LogDir, program_invocation_short_name, getpid(), suffix);
}

} // namespace memhawk
