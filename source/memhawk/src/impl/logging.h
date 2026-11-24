#pragma once

#include "logging/log_api.h" // IWYU pragma: export

namespace memhawk
{

#define LogEx(level, ...)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (memhawk::logging::LogLevel::level >= memhawk::logging::gl_loggingLevel)                                    \
        {                                                                                                              \
            memhawk::logging::LogMessage("[", #level, "] (", __func__, ":", __LINE__, ") ", ##__VA_ARGS__);            \
        }                                                                                                              \
    } while (0)

#define LogError(...) LogEx(Error, ##__VA_ARGS__)
#define LogWarning(...) LogEx(Warning, ##__VA_ARGS__)
#define LogInfo(...) LogEx(Info, ##__VA_ARGS__)
#define LogDebug(...) LogEx(Debug, ##__VA_ARGS__)

#ifdef VERBOSE_DEBUG
#define LogTrace(format, ...) LogEx(Trace, format, ##__VA_ARGS__)
#else
#define LogTrace(...)
#endif

} // namespace memhawk
