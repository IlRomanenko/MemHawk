#pragma once

#include <fmt/format.h>

#include <cerrno>
#include <string>
#include <unistd.h>

inline std::string GetProcessLogName(const char* suffix)
{
    return fmt::format("memhawk_log_{}_{}_{}.log", program_invocation_short_name, getpid(), suffix);
}
