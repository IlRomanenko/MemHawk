#pragma once

#include "config.h"

#include <fmt/format.h>

#include <string>
#include <unistd.h>

namespace memhawk
{

inline std::string GetProcessLogName(const char* suffix, Config& cfg)
{
    return fmt::format("{}/memhawk_{}_{}_{}.log", cfg.LogDir, program_invocation_short_name, getpid(), suffix);
}

} // namespace memhawk
