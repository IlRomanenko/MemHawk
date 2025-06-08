#pragma once
#include "alloc_info.h"
#include "config.h"
#include "thread_tracker.h"

namespace memhawk
{

namespace writers
{

class IWriterStrategy
{
public:
    virtual ~IWriterStrategy() = default;

    virtual void UpdateModules() = 0;
    virtual void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) = 0;
    virtual void FlushData() = 0;
};

class IWritersFactory
{
public:
    virtual ~IWritersFactory() = default;
    virtual std::unique_ptr<IWriterStrategy> CreateWritersAdaptor(const WritersConfig& cfg,
                                                                  std::shared_ptr<IStacktraceFinder> finder) = 0;
};

} // namespace writers
} // namespace memhawk
