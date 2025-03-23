#pragma once

#include "stacktrace.h"

#include <cstdint>
#include <limits>

namespace memhawk
{

constexpr uint32_t InvalidTraceId = std::numeric_limits<uint32_t>::max();

class IStacktraceTracker
{
public:
    virtual ~IStacktraceTracker() = default;

    virtual uint32_t InsertStacktrace(Stacktrace&& trace) = 0;
    virtual std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) = 0;
    virtual size_t StacktracesCount() = 0;
};


} // namespace memhawk
