#pragma once

#include "i_stacktrace_tracker.h"
#include "writers/i_writer.h"

#include <gmock/gmock.h>

namespace memhawk
{
using namespace testing;

class WriterStrategyMock : public writers::IWriterStrategy
{
public:
    MOCK_METHOD(void, UpdateModules, (), (override));
    MOCK_METHOD(void, AccountSnapshot, (const SummariesMap&, const AllocSummary&), (override));
    MOCK_METHOD(void, FlushData, (), (override));
};

class WritersFactoryMock : public writers::IWritersFactory
{
public:
    MOCK_METHOD(std::unique_ptr<writers::IWriterStrategy>, CreateWritersAdaptor,
                (const WritersConfig&, std::shared_ptr<IStacktraceFinder>), (override));
};

class StacktraceFinderMock : public IStacktraceFinder
{
public:
    MOCK_METHOD(std::optional<Stacktrace>, GetStacktraceFromId, (uint32_t), (override));
};

} // namespace memhawk
