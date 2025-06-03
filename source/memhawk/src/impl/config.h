#pragma once

#include "config/config_var.h"
#include "logging_level.h"

#include <boost/hana.hpp>

#include <cstddef>
#include <cstdint>

namespace memhawk
{

constexpr size_t MinUnwindDepth = 3;
constexpr size_t MaxUnwindDepth = 64;

constexpr size_t MinPostponedSize = 64;

struct StacktraceTrackerConfig
{
    CONFIG_VAR_OPT(bool, DumpStacktraces, "dump", false);
    CONFIG_VAR_OPT(std::optional<std::string>, Filename, "filename", {});
};

struct TextWriterConfig
{
    CONFIG_VAR_OPT(size_t, TrackerBySizeCount, "by_size_count", 25);
    CONFIG_VAR_OPT(size_t, TrackerByTotalCount, "by_total_count", 10);
};

struct WritersConfig
{
    CONFIG_VAR_OPT(TextWriterConfig, TextWriter, "text_writer", {});
};

struct MemHawkConfig
{
    CONFIG_VAR_OPT(bool, TrackingWorker, "tracking_worker", true);
    CONFIG_VAR_OPT(size_t, MaxPostponed, "max_postponed", 512);
    CONFIG_VAR_OPT(uint64_t, TrackerDumpingPeriodMs, "dumping_period", 1000);
    CONFIG_VAR_OPT(size_t, LruStackSize, "lru_size", 256);
    CONFIG_VAR_OPT(StacktraceTrackerConfig, InnerTracker, "inn_tracker", {});
    CONFIG_VAR_OPT(StacktraceTrackerConfig, ExternalTracker, "ext_tracker", {});
    CONFIG_VAR_OPT(WritersConfig, Writers, "writers", {});
    CONFIG_VAR_OPT(size_t, CollapseRecursionDepth, "collapse_depth", 8);
};

struct LoggingConfig
{
    CONFIG_VAR_OPT(bool, MainLogIntoFile, "main_log", true);
    CONFIG_VAR_OPT(LogLevel, LoggingLevel, "log_level", LogLevel::Info);
    CONFIG_VAR_OPT(std::string, LogDir, "log_dir", "./");
};

struct UnwindConfig
{
    CONFIG_VAR_OPT(bool, UseAbslStacktraces, "absl", false);
    CONFIG_VAR_OPT(size_t, TrackDepth, "track_depth", 32);
};

struct MainConfig
{
    CONFIG_VAR_OPT(std::string, PrognameRegex, "progname_regex", "");
    CONFIG_VAR_OPT(MemHawkConfig, MemHawk, "memhawk", {});
    CONFIG_VAR_OPT(LoggingConfig, Logging, "logging", {});
    CONFIG_VAR_OPT(UnwindConfig, Unwind, "unwind", {});
    CONFIG_VAR_OPT(bool, Help, "help", false);
};

MainConfig ParseConfig();

} // namespace memhawk

BOOST_HANA_ADAPT_STRUCT(memhawk::StacktraceTrackerConfig, DumpStacktraces, Filename);
BOOST_HANA_ADAPT_STRUCT(memhawk::TextWriterConfig, TrackerBySizeCount, TrackerByTotalCount);
BOOST_HANA_ADAPT_STRUCT(memhawk::WritersConfig, TextWriter);
BOOST_HANA_ADAPT_STRUCT(memhawk::MemHawkConfig, TrackingWorker, MaxPostponed, TrackerDumpingPeriodMs, LruStackSize,
                        InnerTracker, ExternalTracker, Writers, CollapseRecursionDepth);
BOOST_HANA_ADAPT_STRUCT(memhawk::LoggingConfig, MainLogIntoFile, LoggingLevel, LogDir);
BOOST_HANA_ADAPT_STRUCT(memhawk::UnwindConfig, UseAbslStacktraces, TrackDepth);
BOOST_HANA_ADAPT_STRUCT(memhawk::MainConfig, PrognameRegex, MemHawk, Logging, Unwind, Help);
