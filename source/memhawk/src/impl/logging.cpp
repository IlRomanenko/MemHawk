#include "logging.h"

#include "config.h"

#include <absl/base/attributes.h>
#include <fmt/format.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace memhawk
{
namespace
{
ABSL_CONST_INIT int gl_logFile = STDERR_FILENO;
LoggingConfig gl_loggingConfig;
} // namespace

void LogInit(LoggingConfig cfg)
{
    gl_loggingConfig = std::move(cfg);

    gl_loggingLevel = *gl_loggingConfig.LoggingLevel;
    if (*gl_loggingConfig.MainLogIntoFile)
    {
        const auto filename = GetProcessLogName("main_log");
        gl_logFile = open(filename.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (gl_logFile < 0)
        {
            const auto error = errno;
            gl_logFile = STDERR_FILENO;
            constexpr size_t BufSize = 512;
            char buffer[BufSize];
            LogPrint("Failed to initialise logging: " fStr ", error: ", filename.c_str(),
                     strerror_r(error, buffer, 512));
            exit(-1);
        }
    }
    else
    {
        gl_logFile = STDERR_FILENO;
    }
}

void LogDeinit()
{
    if (*gl_loggingConfig.MainLogIntoFile)
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

std::string GetProcessLogName(const char* suffix, const char* fileext)
{
    return fmt::format("{}/memhawk_{}_{}_{}.{}", *gl_loggingConfig.LogDir, program_invocation_short_name, getpid(),
                       suffix, fileext);
}


} // namespace memhawk
