#pragma once

#include <cstdint>

namespace memhawk
{

enum class LogLevel : uint8_t
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Off = 4
};

void LogInit(const char* filename);
void LogDeinit();
void LogPrint(const char* fmt, ...);

#define LogEx(level, format, ...)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (memhawk::LogLevel::level >= memhawk::gl_config.LoggingLevel)                                               \
        {                                                                                                              \
            memhawk::LogPrint("[%s]:%s :: " format "\n", #level, __func__, ##__VA_ARGS__);                             \
        }                                                                                                              \
    } while (0)

#define LogError(format, ...) LogEx(Error, format, ##__VA_ARGS__)
#define LogWarning(format, ...) LogEx(Warning, format, ##__VA_ARGS__)
#define LogInfo(format, ...) LogEx(Info, format, ##__VA_ARGS__)
#define LogDebug(format, ...) LogEx(Debug, format, ##__VA_ARGS__)

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
