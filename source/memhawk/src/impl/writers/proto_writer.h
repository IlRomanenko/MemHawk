#pragma once

#include "config.h"
#include "i_stacktrace_tracker.h"
#include "thread_tracker.h"
#include "writers/i_writer.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/arena.h>
#include <protos/snapshot.pb.h>

#include <fstream>

namespace memhawk
{
namespace writers
{

class ProtobufWriter : public IWriterStrategy
{

public:
    explicit ProtobufWriter(ProtobufWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder);
    ~ProtobufWriter() override;

    ProtobufWriter(const ProtobufWriter&) = delete;
    ProtobufWriter& operator=(const ProtobufWriter&) = delete;

    void UpdateModules() override;
    void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) override;
    void FlushData() override;

private:

    void FillChangedSummary(uint32_t traceId, protos::TracedAllocSummary* tracedSummary);

    void AddStacktrace(uint32_t traceId, protos::Snapshot* snapshot);

private:
    ProtobufWriterConfig m_cfg;
    std::shared_ptr<IStacktraceFinder> m_finder;

    std::ofstream m_file;

    google::protobuf::Arena m_arena;

    SummariesMap m_localSummaries;
    absl::flat_hash_set<uint32_t> m_changedSummaries;
    absl::flat_hash_set<uint32_t> m_writtenTraces;
    absl::flat_hash_map<uint64_t, uint32_t> m_ptrMap;

    bool m_updateModules{true};
};

} // namespace writers
} // namespace memhawk
