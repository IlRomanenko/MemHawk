#include "log.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

extern int gl_logFile;

void LogInit(const char* filename)
{
    gl_logFile = open(filename, O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (gl_logFile < 0) {
        int error = errno;
        LogError("Failed to initialise logging: " fStr, strerror(error));
        // // perhaps, abort should be called in that case
        // abort();
    }
}

void LogDeinit()
{
    close(gl_logFile);
    gl_logFile = STDERR_FILENO;
}

void LogPrint(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vdprintf(gl_logFile, fmt, args);
    va_end(args);
}
