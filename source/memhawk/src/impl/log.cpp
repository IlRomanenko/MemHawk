#include "log.h"

#include "config.h"

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

ABSL_CONST_INIT int gl_logFile = STDERR_FILENO;

void LogInit(const char* filename)
{
    if (gl_config.MainLogIntoFile) {
        gl_logFile = open(filename, O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (gl_logFile < 0) {
            int error = errno;
            gl_logFile = STDERR_FILENO;
            constexpr size_t BufSize = 512;
            char buffer[BufSize];
            LogPrint("Failed to initialise logging: " fStr ", error: ", filename, strerror_r(error, buffer, 512));
            abort();
        }
    } else {
        gl_logFile = STDERR_FILENO;
    }
}

void LogDeinit()
{
    if (gl_config.MainLogIntoFile) {
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
