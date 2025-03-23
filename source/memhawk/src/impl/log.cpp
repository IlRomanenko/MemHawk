#include "log.h"

#include "config.h"
#include "log_name.h"

#include <absl/base/attributes.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace memhawk
{
namespace
{
ABSL_CONST_INIT int gl_logFile = STDERR_FILENO;
} // namespace

void LogInit()
{
    if (gl_config.MainLogIntoFile)
    {
        const auto filename = GetProcessLogName("main_log", gl_config);
        gl_logFile = open(filename.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (gl_logFile < 0)
        {
            const auto error = errno;
            gl_logFile = STDERR_FILENO;
            constexpr size_t BufSize = 512;
            char buffer[BufSize];
            LogPrint("Failed to initialise logging: " fStr ", error: ", filename.c_str(),
                     strerror_r(error, buffer, 512));
            abort();
        }
    }
    else
    {
        gl_logFile = STDERR_FILENO;
    }
}

void LogDeinit()
{
    if (gl_config.MainLogIntoFile)
    {
        fsync(gl_logFile);
        close(gl_logFile);
    }
    gl_logFile = STDERR_FILENO;
}

void LogPrint(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vdprintf(gl_logFile, fmt, args);
    va_end(args);
}

} // namespace memhawk
