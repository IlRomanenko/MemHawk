#pragma once

#include "stacktrace.h"

#include <cstdint>

class IStacktraceTracker
{
public:
    virtual ~IStacktraceTracker() = default;

    virtual uint32_t InsertStacktrace(Stacktrace&& trace) = 0;
    virtual std::optional<Stacktrace> GetStacktraceFromId(uint32_t traceId) = 0;
};
