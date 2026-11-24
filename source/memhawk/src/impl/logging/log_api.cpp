#include "log_api.h"

#include "config.h"

#include <absl/base/attributes.h>
#include <fmt/format.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace memhawk::logging
{
namespace
{
ABSL_CONST_INIT int gl_logFile = STDERR_FILENO;
ABSL_CONST_INIT LoggingConfig gl_loggingConfig = {};
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
            LogMessage("Failed to initialise logging: ", filename, ", error: ", strerror_r(error, buffer, BufSize));
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

std::string GetProcessLogName(const char* suffix, const char* fileext)
{
    return fmt::format("{}/memhawk_{}_{}_{}.{}", *gl_loggingConfig.LogDir, program_invocation_short_name, getpid(),
                       suffix, fileext);
}

void WriteMessage(std::string_view data)
{
    while (!data.empty())
    {
        const auto written = ::write(gl_logFile, data.data(), data.size());
        if (written > 0)
        {
            data = data.substr(static_cast<size_t>(written));
        }
        else if (errno == EINTR)
        {
            // try again
            continue;
        }
        else if (written == 0 || errno == EAGAIN) // NOLINTNEXTLINE(bugprone-branch-clone)
        {
            // actually it's possible to have EAGAIN or written == 0 during normal workflow
            // but it's better to skip internal memhawk logs than to hang external process
            return;
        }
        else
        {
            // any other error
            return;
        }
    }
}

} // namespace memhawk::logging
