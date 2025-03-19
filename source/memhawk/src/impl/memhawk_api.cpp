#include "log.h"
#include "config.h"

#include <memhawk/memhawk_api.h>

#include <cerrno>
#include <cstring>


void SetLogLevel(unsigned int level)
{
    if (level > static_cast<int>(memhawk::LogLevel::Error)) {
        LogError("Incorrect LogLevel : " fU32, level);
        return;
    }
    memhawk::gl_config.LoggingLevel = static_cast<memhawk::LogLevel>(level);
}
