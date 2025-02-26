#include "log.h"

#include <MemHawk/memhawk_api.h>

#include <cerrno>
#include <csignal>
#include <cstring>

void SetLogLevel(unsigned int level)
{
    if (level > static_cast<int>(LogLevel::Error)) {
        LogError("Incorrect LogLevel : " fU32, level);
        return;
    }
    gl_logLevel = static_cast<LogLevel>(level);
}
