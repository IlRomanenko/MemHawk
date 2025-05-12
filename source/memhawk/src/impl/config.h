#pragma once

#include "log.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace memhawk
{

constexpr size_t MinUnwindDepth = 3;
constexpr size_t MaxUnwindDepth = 64;

constexpr size_t MinPostponedSize = 64;

struct Config
{
    constexpr Config() = default;

    size_t TrackDepth = 32;
    size_t LruStackSize = 256;
    size_t CollapseRecursionDepth = 4;
    size_t MaxPostponed = 512;
    LogLevel LoggingLevel = LogLevel::Info;
    bool MainLogIntoFile = true;
    bool StartTrackingWorker = true;
    bool StartSignalWorker = false; // todo
    uint64_t TrackerDumpingPeriodMs = 1000;
    size_t TrackerBySizeCount = 25;
    size_t TrackerByTotalCount = 10;

    bool DumpAllInnerStacktraces = false;
    bool DumpAllExternalStacktraces = false;
    bool UseAbslStacktraces = false;

    std::string_view PrognameRegex;

    std::string_view LogDir = "./";
};

inline Config gl_config{};

void InitConfig();

} // namespace memhawk
