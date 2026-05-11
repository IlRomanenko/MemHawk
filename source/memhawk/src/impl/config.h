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
    CONFIG_VAR_OPT(std::optional<std::string_view>, Filename, "filename", {});
};

struct TextWriterIndexConfig
{
    CONFIG_VAR_OPT(bool, Enabled, "enabled", true);
    CONFIG_VAR_OPT(size_t, TrackerBySizeCount, "by_size_count", 10);
    CONFIG_VAR_OPT(size_t, TrackerByTotalCount, "by_total_count", 10);
};

constexpr const TextWriterIndexConfig DefaultTextWriterInternalConfig{
    .Enabled = decltype(TextWriterIndexConfig::Enabled){false},
    .TrackerBySizeCount = decltype(TextWriterIndexConfig::TrackerBySizeCount){10},
    .TrackerByTotalCount = decltype(TextWriterIndexConfig::TrackerByTotalCount){10},
};

struct TextWriterConfig
{
    CONFIG_VAR_OPT(bool, Enabled, "enabled", false);
    CONFIG_VAR_OPT(std::optional<std::string_view>, Filename, "filename", {});
    CONFIG_VAR_OPT(TextWriterIndexConfig, ExternalTraces, "ext", {});
    CONFIG_VAR_OPT(TextWriterIndexConfig, InternalTraces, "int", DefaultTextWriterInternalConfig);
};

struct ProtobufWriterConfig
{
    CONFIG_VAR_OPT(bool, Enabled, "enabled", true);
    CONFIG_VAR_OPT(std::optional<std::string_view>, Filename, "filename", {});
};

struct HeaptrackWriterConfig
{
    CONFIG_VAR_OPT(bool, Enabled, "enabled", false);
    CONFIG_VAR_OPT(std::optional<std::string_view>, Filename, "filename", {});
};

struct WritersConfig
{
    CONFIG_VAR_OPT(TextWriterConfig, TextWriter, "text_writer", {});
    CONFIG_VAR_OPT(ProtobufWriterConfig, ProtobufWriter, "proto_writer", {});
    CONFIG_VAR_OPT(HeaptrackWriterConfig, HeaptrackWriter, "heaptrack_writer", {});
};

struct MemHawkConfig
{
    CONFIG_VAR_OPT(bool, TrackingWorker, "tracking_worker", true);
    CONFIG_VAR_OPT(size_t, MaxPostponed, "max_postponed", 512);
    CONFIG_VAR_OPT(uint64_t, TrackerDumpingPeriodMs, "dumping_period", 1000);
    CONFIG_VAR_OPT(size_t, LruStackSize, "lru_size", 256);
    CONFIG_VAR_OPT(StacktraceTrackerConfig, InnerTracker, "int_tracker", {});
    CONFIG_VAR_OPT(StacktraceTrackerConfig, ExternalTracker, "ext_tracker", {});
    CONFIG_VAR_OPT(WritersConfig, Writers, "writers", {});
    CONFIG_VAR_OPT(size_t, CollapseRecursionDepth, "collapse_depth", 8);
};

struct LoggingConfig
{
    CONFIG_VAR_OPT(bool, MainLogIntoFile, "main_log", true);
    CONFIG_VAR_OPT(LogLevel, LoggingLevel, "log_level", LogLevel::Info);
    CONFIG_VAR_OPT(std::string_view, LogDir, "log_dir", "./");
};

struct UnwindConfig
{
    CONFIG_VAR_OPT(bool, UseAbslStacktraces, "absl", false);
    CONFIG_VAR_OPT(size_t, TrackDepth, "track_depth", 32);
};

struct MainConfig
{
    CONFIG_VAR_OPT(std::string_view, PrognameRegex, "progname_regex", "");
    CONFIG_VAR_OPT(MemHawkConfig, MemHawk, "memhawk", {});
    CONFIG_VAR_OPT(LoggingConfig, Logging, "logging", {});
    CONFIG_VAR_OPT(UnwindConfig, Unwind, "unwind", {});
    CONFIG_VAR_OPT(bool, Help, "help", false);
};

MainConfig ParseConfig();

} // namespace memhawk

BOOST_HANA_ADAPT_STRUCT(memhawk::StacktraceTrackerConfig, DumpStacktraces, Filename);
BOOST_HANA_ADAPT_STRUCT(memhawk::TextWriterIndexConfig, Enabled, TrackerBySizeCount, TrackerByTotalCount);
BOOST_HANA_ADAPT_STRUCT(memhawk::TextWriterConfig, Enabled, Filename, ExternalTraces, InternalTraces);
BOOST_HANA_ADAPT_STRUCT(memhawk::ProtobufWriterConfig, Enabled, Filename);
BOOST_HANA_ADAPT_STRUCT(memhawk::HeaptrackWriterConfig, Enabled, Filename);
BOOST_HANA_ADAPT_STRUCT(memhawk::WritersConfig, TextWriter, ProtobufWriter, HeaptrackWriter);
BOOST_HANA_ADAPT_STRUCT(memhawk::MemHawkConfig, TrackingWorker, MaxPostponed, TrackerDumpingPeriodMs, LruStackSize,
                        InnerTracker, ExternalTracker, Writers, CollapseRecursionDepth);
BOOST_HANA_ADAPT_STRUCT(memhawk::LoggingConfig, MainLogIntoFile, LoggingLevel, LogDir);
BOOST_HANA_ADAPT_STRUCT(memhawk::UnwindConfig, UseAbslStacktraces, TrackDepth);
BOOST_HANA_ADAPT_STRUCT(memhawk::MainConfig, PrognameRegex, MemHawk, Logging, Unwind, Help);
