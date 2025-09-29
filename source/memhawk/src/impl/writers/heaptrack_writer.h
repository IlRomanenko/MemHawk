#pragma once

#include "config.h"
#include "i_stacktrace_tracker.h"
#include "thread_tracker.h"
#include "writers/i_writer.h"
#include "stacktrace_tree.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <cstdint>
#include <fstream>

namespace memhawk
{
namespace writers
{

class HeaptrackWriter : public IWriterStrategy
{

public:
    explicit HeaptrackWriter(HeaptrackWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder);

    HeaptrackWriter(const HeaptrackWriter&) = delete;
    HeaptrackWriter& operator=(const HeaptrackWriter&) = delete;

    void UpdateModules() override;
    void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) override;
    void FlushData() override;

private:
    void WriteVersion();
    void WriteSystemInfo();
    void WriteRSS();
    void WriteTimestamp();
    void WriteExe();
    void WriteCommandLine();
    void WriteAllocation(uint32_t traceId, int64_t size, uint32_t index);
    void WriteDeallocation(uint32_t traceId);
    void WriteTrace(uint32_t traceId);

private:
    HeaptrackWriterConfig m_cfg;
    std::shared_ptr<IStacktraceFinder> m_finder;

    std::unique_ptr<std::ofstream> m_ofstream;

    struct TraceSummary
    {
        int64_t current{};
        int64_t prev{};
    };

    absl::flat_hash_map<uint32_t, TraceSummary> m_summaries;
    absl::flat_hash_set<uint32_t> m_changedSummaries;
    // traceId -> TraceTree index
    absl::flat_hash_map<uint32_t, StacktraceTree::NodeId> m_writtenTraces;

    bool m_updateModules{true};

    StacktraceTree m_tree;
};

} // namespace writers
} // namespace memhawk
