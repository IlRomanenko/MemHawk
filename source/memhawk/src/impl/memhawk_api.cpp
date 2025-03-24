#include "config.h"
#include "log.h"

#include <memhawk/memhawk_api.h>

#include <cerrno>
#include <cstring>

extern "C" {

void SetLogLevel(unsigned int level)
{
    if (level > static_cast<int>(memhawk::LogLevel::Off))
    {
        LogError("Incorrect LogLevel : " fU32, level);
        return;
    }
    memhawk::gl_config.LoggingLevel = static_cast<memhawk::LogLevel>(level);
}

} // extern "C"
