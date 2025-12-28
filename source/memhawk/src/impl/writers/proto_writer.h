#pragma once

#include "alloc_info.h"
#include "config.h"
#include "i_stacktrace_tracker.h"
#include "thread_tracker.h"
#include "writers/i_writer.h"

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message_lite.h>
#include <proto/snapshot.pb.h>

#include <chrono>
#include <cstdint>
#include <fstream>

namespace memhawk
{
namespace writers
{

class ProtobufWriter : public IWriterStrategy
{

public:
    explicit ProtobufWriter(ProtobufWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder);

    ProtobufWriter(const ProtobufWriter&) = delete;
    ProtobufWriter& operator=(const ProtobufWriter&) = delete;

    void UpdateModules() override;
    void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) override;
    void FlushData() override;

private:

    void WriteUint64BigEndian(uint64_t value);
    void WriteProcessInfo();
    void WriteMessage(google::protobuf::MessageLite* message);
    void FillAllocSummary(proto::AllocSummary* protoSummary, const AllocSummary& summary);
    void FillProcessInfo(proto::ProcessInfo* info);
    void FillChangedSummary(uint32_t traceId, proto::TracedAllocSummary* tracedSummary);

    void AddStacktrace(uint32_t traceId, proto::Snapshot* snapshot);

private:
    ProtobufWriterConfig m_cfg;
    std::shared_ptr<IStacktraceFinder> m_finder;

    std::unique_ptr<std::ofstream> m_ofstream;
    std::vector<uint8_t> m_serializationBuffer;
    std::vector<char> m_compressionBuffer;

    google::protobuf::Arena m_arena;

    AllocSummary m_total;
    SummariesMap m_localSummaries;
    absl::flat_hash_set<uint32_t> m_changedSummaries;
    absl::flat_hash_set<uint32_t> m_writtenTraces;
    absl::flat_hash_map<uint64_t, uint32_t> m_ptrMap;

    bool m_updateModules{true};

    std::chrono::system_clock::time_point m_startTime;
};

} // namespace writers
} // namespace memhawk
