#pragma once

#include "config.h"
#include "logging_level.h"

#include <string>

namespace memhawk
{

inline LogLevel gl_loggingLevel = LogLevel::Info;

void LogInit(LoggingConfig cfg);
void LogDeinit();
void LogPrint(const char* fmt, ...);

std::string GetProcessLogName(const char* suffix);

#define LogEx(level, format, ...)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (memhawk::LogLevel::level >= memhawk::gl_loggingLevel)                                                      \
        {                                                                                                              \
            memhawk::LogPrint("[%s]:%s :: " format "\n", #level, __func__, ##__VA_ARGS__);                             \
        }                                                                                                              \
    } while (0)

#define LogError(format, ...) LogEx(Error, format, ##__VA_ARGS__)
#define LogWarning(format, ...) LogEx(Warning, format, ##__VA_ARGS__)
#define LogInfo(format, ...) LogEx(Info, format, ##__VA_ARGS__)
#define LogDebug(format, ...) LogEx(Debug, format, ##__VA_ARGS__)

#ifdef VERBOSE_DEBUG
#define LogTrace(format, ...) LogEx(Trace, format, ##__VA_ARGS__)
#else
#define LogTrace(...)
#endif

// define our own version of specifiers, perhaps they can be replaced with PRI*
#define fU32 "%u"
#define fU64 "%lu"
#define fSzt "%zu"
#define fI32 "%d"
#define fI64 "%ld"
#define fStr "%s"
#define fPtr "%p"
#define fHex "%x"

} // namespace memhawk
