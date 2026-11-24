#pragma once

#include "config.h"
#include "logging/log_level.h"
#include "logging/log_record.h"

#include <chrono>
#include <string>

namespace memhawk::logging
{

inline LogLevel gl_loggingLevel = LogLevel::Info;

void LogInit(LoggingConfig cfg);
void LogDeinit();
void WriteMessage(std::string_view data);

std::string GetProcessLogName(const char* suffix, const char* fileext = "log");

// Pass message fragments directly so argument formatting needs no heap storage.
template <logging::LogPart... Parts>
ABSL_ATTRIBUTE_NOINLINE void LogMessage(const Parts&... parts) noexcept
{
    const int savedErrno = errno;
    logging::LogRecord record;
    const auto now = std::chrono::system_clock::now();
    if (auto timestamp = logging::FormatUtcTimestamp(now); timestamp)
    {
        record.Append(*timestamp);
    }
    record.Append(" ");
    (record.Append(parts), ...);
    WriteMessage(record.Data());
    errno = savedErrno;
}

} // namespace memhawk::logging
