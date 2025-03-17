#pragma once

#include "log.h"

#include <cstddef>
#include <string_view>

namespace memhawk
{

constexpr size_t MinUnwindDepth = 8;
constexpr size_t MaxUnwindDepth = 64;

constexpr size_t MinPostponedSize = 64;

struct Config
{
    size_t TrackDepth = 32;
    size_t LruStackSize = 128;
    size_t CollapseRecursionDepth = 4;
    size_t MaxPostponed = 128;
    LogLevel LoggingLevel = LogLevel::Info;
    bool MainLogIntoFile = true;

    std::string_view LogDir = "./";
};

extern Config gl_config;

void InitConfig();

} // namespace memhawk
