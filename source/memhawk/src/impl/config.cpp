#include "config.h"

#include "config/parse_struct.h"
#include "logging.h"

#include <absl/base/attributes.h>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>

// #include <charconv>
#include <cstdlib>
// #include <string_view>
#include <unistd.h>

// #include <utility>

namespace memhawk
{

constexpr const char* OptionsEnvName = "MEMHAWK_OPTS";

template <typename... Args>
void PrintError(fmt::format_string<Args...> fmt, Args... args)
{
    const auto str = fmt::format(fmt, std::forward<Args>(args)...);
    dprintf(STDERR_FILENO, fStr, str.c_str());
}

void OnError(const config::ParseError& err)
{
    switch (err.type)
    {

    case config::ParseErrorType::Ok:
        break;
    case config::ParseErrorType::FieldNotFound:
        PrintError("Failed to find field: {}, during parsing key: {}\n", err.field, err.key);
        break;
    case config::ParseErrorType::ValueNotParsed:
        PrintError("Failed to parse value: {}, field: {}, during parsing key: {}\n", err.value, err.field, err.key);
        break;
    case config::ParseErrorType::ValueNotFound:
        PrintError("Incorrect option format: {}, failed to find value\n", err.key);
        break;
    case config::ParseErrorType::RequiredFieldMissed:
        PrintError("Required filed missed, key: {}, field: {}\n", err.key, err.field);
        break;
    }
}

bool ValidateConfig(MainConfig& cfg)
{
    bool valid = true;
    if (*cfg.Unwind->TrackDepth > MaxUnwindDepth)
    {
        PrintError("Can't set track depth more than: {}", MaxUnwindDepth);
        cfg.Unwind->TrackDepth = MaxUnwindDepth;
        valid = false;
    }
    if (*cfg.Unwind->TrackDepth < MinUnwindDepth)
    {
        PrintError("Can't set track depth less than: {}", MinUnwindDepth);
        cfg.Unwind->TrackDepth = MinUnwindDepth;
        valid = false;
    }
    if (*cfg.MemHawk->MaxPostponed < MinPostponedSize)
    {
        PrintError("Can't set postponed size less than: {}", MinPostponedSize);
        *cfg.MemHawk->MaxPostponed = MinPostponedSize;
        valid = false;
    }
    return valid;
}

MainConfig ParseConfig()
{
    MainConfig cfg{};
    const auto str = getenv(OptionsEnvName);
    if (!str)
    {
        return {};
    }
    if (!config::ParseStruct(str, cfg, OnError))
    {
        exit(-1);
    }
    if (!ValidateConfig(cfg))
    {
        exit(-1);
    }
    return cfg;
}

} // namespace memhawk
