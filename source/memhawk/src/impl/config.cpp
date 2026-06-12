#include "config.h"

#include "config/describe_struct.h"
#include "config/parse_struct.h"
#include "logging.h"

#include <absl/base/attributes.h>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>

#include <cstdlib>
#include <unistd.h>

namespace memhawk
{

constexpr const char* OptionsEnvName = "MEMHAWK_OPTS";

template <typename... Args>
void PrintMessage(fmt::format_string<Args...> fmt, Args... args)
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
        PrintMessage("Failed to find field: {}, during parsing key: {}\n", err.field, err.key);
        break;
    case config::ParseErrorType::ValueNotParsed:
        PrintMessage("Failed to parse value: {}, field: {}, during parsing key: {}\n", err.value, err.field, err.key);
        break;
    case config::ParseErrorType::ValueNotFound:
        PrintMessage("Incorrect option format: {}, failed to find value\n", err.key);
        break;
    case config::ParseErrorType::RequiredFieldMissing:
        PrintMessage("Required field missing, key: {}, field: {}\n", err.key, err.field);
        break;
    }
}

void OnKey(const config::DescribeContext& ctx)
{
    PrintMessage("{} = {}\n", ctx.key, ctx.value);
}

bool ValidateConfig(MainConfig& cfg)
{
    bool valid = true;
    if (*cfg.Unwind->TrackDepth > MaxUnwindDepth)
    {
        PrintMessage("Can't set track depth more than: {}", MaxUnwindDepth);
        cfg.Unwind->TrackDepth = MaxUnwindDepth;
        valid = false;
    }
    if (*cfg.Unwind->TrackDepth < MinUnwindDepth)
    {
        PrintMessage("Can't set track depth less than: {}", MinUnwindDepth);
        cfg.Unwind->TrackDepth = MinUnwindDepth;
        valid = false;
    }
    if (*cfg.MemHawk->MaxPostponed < MinPostponedSize)
    {
        PrintMessage("Can't set postponed size less than: {}", MinPostponedSize);
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
    if (*cfg.Help)
    {
        PrintMessage("full set of fields in config:\n");
        config::DescribeStruct(MainConfig{}, OnKey);
        exit(0);
    }
    return cfg;
}

} // namespace memhawk
