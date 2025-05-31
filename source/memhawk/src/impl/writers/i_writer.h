#pragma once
#include "thread_tracker.h"

namespace memhawk
{

class IWriter
{
public:
    virtual ~IWriter() = default;
    virtual void PostponedConstruct() = 0;

    virtual void AccountThreadTracker(ThreadTracker* tracker) = 0;
    virtual void FlushData() = 0;
};
} // namespace memhawk
