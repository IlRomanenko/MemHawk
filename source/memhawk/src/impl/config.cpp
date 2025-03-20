#include "config.h"

#include "log.h"

#include <absl/base/attributes.h>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace memhawk
{

ABSL_CONST_INIT Config gl_config = {};

constexpr const char* OptionsEnvName = "MEMHAWK_OPTS";

constexpr const char* OptionDelim = ":";
constexpr const char* OptionKeyValueDelim = "=";


constexpr const char* TrackDepthName = "track_depth";
constexpr const char* LruStackSizeName = "lru_size";
constexpr const char* CollapseRecursionDepthName = "collapse_recursion_depth";
constexpr const char* MaxPostponedName = "max_postponed";
constexpr const char* LoggingLevelName = "logging_level";
constexpr const char* MainLogIntoFileName = "main_log_file";
constexpr const char* LogDirName = "log_dir";
constexpr const char* TrackingWorkerName = "tracking_worker";
constexpr const char* TrackerBySizeCountName = "tracker_by_size_count";
constexpr const char* TrackerByTotalCountName = "tracker_by_total_count";

template <typename... Args>
void PrintError(fmt::format_string<Args...> fmt, Args... args)
{
    const auto str = fmt::format(fmt, std::forward<Args>(args)...);
    LogError(fStr, str.c_str());
}

void ParseValue(std::string_view key, std::string_view valueStr, size_t& result)
{
    size_t value{};
    auto res = std::from_chars(valueStr.begin(), valueStr.end(), value);
    if (res.ec == std::errc()) {
        result = value;
    } else {
        PrintError("Failed to parse key: {} with value: {}", key, valueStr);
    }
}

// TODO: perhaps, there is more accurate approach without actually writing your own parser
void InitConfig()
{
    const auto str = getenv(OptionsEnvName);
    if (!str) {
        gl_config = {};
        return;
    }

    const auto strView = std::string_view(str);
    auto splitIt = boost::algorithm::make_split_iterator(strView, boost::algorithm::first_finder(OptionDelim));
    for (; splitIt != decltype(splitIt)(); splitIt++) {
        const auto elemView = std::string_view(splitIt->begin(), splitIt->size());
        auto separatorPos = elemView.find(OptionKeyValueDelim);
        if (separatorPos == std::string_view::npos) {
            PrintError("Incorrect element without `=` separator: {}", elemView);
            continue;
        }
        const auto key = elemView.substr(0, separatorPos);
        const auto valueStr = elemView.substr(separatorPos + 1);

        if (key == TrackDepthName) {
            ParseValue(key, valueStr, gl_config.TrackDepth);
            if (gl_config.TrackDepth > MaxUnwindDepth) {
                PrintError("Can't set track depth more than: {}", MaxUnwindDepth);
                gl_config.TrackDepth = MaxUnwindDepth;
            }
            if (gl_config.TrackDepth < MinUnwindDepth) {
                PrintError("Can't set track depth less than: {}", MinUnwindDepth);
                gl_config.TrackDepth = MinUnwindDepth;
            }

        } else if (key == LruStackSizeName) {
            ParseValue(key, valueStr, gl_config.LruStackSize);
        } else if (key == CollapseRecursionDepthName) {
            ParseValue(key, valueStr, gl_config.CollapseRecursionDepth);
        } else if (key == MaxPostponedName) {
            ParseValue(key, valueStr, gl_config.MaxPostponed);
            if (gl_config.MaxPostponed < MinPostponedSize) {
                PrintError("Can't set postponed size less than: {}", MinPostponedSize);
                gl_config.MaxPostponed = MinPostponedSize;
            }
        } else if (key == LoggingLevelName) {
            size_t value{};
            ParseValue(key, valueStr, value);
            gl_config.LoggingLevel = static_cast<LogLevel>(value);
        } else if (key == MainLogIntoFileName) {
            size_t value{};
            ParseValue(key, valueStr, value);
            gl_config.MainLogIntoFile = value > 0;
        } else if (key == LogDirName) {
            gl_config.LogDir = valueStr;
        } else if (key == TrackingWorkerName) {
            size_t value{};
            ParseValue(key, valueStr, value);
            gl_config.StartTrackingWorker = value > 0;
        } else if (key == TrackerBySizeCountName) {
            ParseValue(key, valueStr, gl_config.TrackerBySizeCount);
        } else if (key == TrackerByTotalCountName) {
            ParseValue(key, valueStr, gl_config.TrackerByTotalCount);
        } else {
            PrintError("Unknown parameter: {}", key);
        }
    }
}

} // namespace memhawk
