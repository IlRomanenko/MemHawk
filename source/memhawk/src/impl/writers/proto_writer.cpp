
#include "proto_writer.h"

#include "logging.h"

#include <google/protobuf/arena.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/delimited_message_util.h>

#include <chrono>
#include <cstdint>
#include <elf.h>
#include <endian.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <link.h>
#include <system_error>
#include <unistd.h>
#include <zstd.h>

namespace memhawk
{
namespace writers
{

namespace
{
std::string GetPrognameFullPath()
{
    std::error_code ec{};
    auto res = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec)
    {
        return program_invocation_name;
    }
    return res.native();
}

int iterate_dl_headers(struct dl_phdr_info* info, size_t /*size*/, void* data)
{
    auto snapshot = reinterpret_cast<proto::Snapshot*>(data);
    auto* loadedSo = snapshot->add_loadedso();
    loadedSo->set_addr(info->dlpi_addr);
    std::string fileName{info->dlpi_name};
    if (fileName.empty())
    {
        fileName = GetPrognameFullPath();
    }
    loadedSo->set_filename(fileName);
    for (size_t i = 0; i < info->dlpi_phnum; i++)
    {
        auto* segment = loadedSo->add_segments();
        segment->set_addr(info->dlpi_phdr[i].p_vaddr);
        segment->set_size(info->dlpi_phdr[i].p_memsz);
    }

    return 0;
}
} // namespace

ProtobufWriter::ProtobufWriter(ProtobufWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder)
    : m_cfg{std::move(cfg)}, m_finder(std::move(finder))
{
    m_startTime = std::chrono::system_clock::now();

    auto filename = GetProcessLogName("protobuf", "binpb");
    if (m_cfg.Filename->has_value())
    {
        filename = m_cfg.Filename->value(); // NOLINT(bugprone-unchecked-optional-access)
    }
    m_ofstream = std::make_unique<std::ofstream>(filename, std::ios_base::out | std::ios_base::binary);
    WriteProcessInfo();
    // account empty allocation in order to correctly start time tracking
    ProtobufWriter::AccountSnapshot({{0, {}}}, {});
    ProtobufWriter::FlushData();
}

void ProtobufWriter::UpdateModules()
{
    m_updateModules = true;
}

void ProtobufWriter::WriteUint64BigEndian(uint64_t value)
{
    const uint64_t be = htobe64(value);
    m_ofstream->write(reinterpret_cast<const char*>(&be), sizeof(be));
}

void ProtobufWriter::WriteMessage(google::protobuf::MessageLite* message)
{
    const auto originalSize = message->ByteSizeLong();
    m_serializationBuffer.resize(originalSize);
    message->SerializeWithCachedSizesToArray(m_serializationBuffer.data());

    const auto compressBound = ZSTD_compressBound(originalSize);
    m_compressionBuffer.resize(compressBound);
    const auto compressedSize =
        ZSTD_compress(m_compressionBuffer.data(), compressBound, m_serializationBuffer.data(), originalSize, ZSTD_fast);

    WriteUint64BigEndian(compressedSize);
    m_ofstream->write(m_compressionBuffer.data(), static_cast<std::streamsize>(compressedSize));
    m_ofstream->flush();
}

void ProtobufWriter::WriteProcessInfo()
{
    auto processInfo = google::protobuf::Arena::Create<proto::ProcessInfo>(&m_arena);
    FillProcessInfo(processInfo);
    WriteMessage(processInfo);
    m_arena.Reset();
}

void ProtobufWriter::AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total)
{
    m_total += total;
    for (const auto& [traceId, summary] : summaries)
    {
        m_changedSummaries.insert(traceId);
        auto it = m_localSummaries.find(traceId);
        if (it == m_localSummaries.end())
        {
            m_localSummaries.insert({traceId, summary});
            continue;
        }
        it->second += summary;
    }
}

void ProtobufWriter::FlushData()
{
    proto::Snapshot* snapshot = google::protobuf::Arena::Create<proto::Snapshot>(&m_arena);

    if (m_updateModules)
    {
        dl_iterate_phdr(iterate_dl_headers, snapshot);
        m_updateModules = false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    snapshot->set_timestamp(static_cast<uint64_t>(nowNs));

    FillAllocSummary(snapshot->mutable_total(), m_total);
    for (const auto& traceId : m_changedSummaries)
    {
        if (!m_writtenTraces.contains(traceId))
        {
            AddStacktrace(traceId, snapshot);
            m_writtenTraces.insert(traceId);
        }
        auto* changedSummary = snapshot->add_changed();
        FillChangedSummary(traceId, changedSummary);
    }

    WriteMessage(snapshot);
    m_changedSummaries.clear();
    m_arena.Reset();
}

void ProtobufWriter::FillProcessInfo(proto::ProcessInfo* info)
{
    *info->mutable_processshortname() = program_invocation_short_name;
    *info->mutable_processfullpath() = GetPrognameFullPath();
    info->set_pid(static_cast<uint32_t>(getpid()));
    const auto startNs = std::chrono::duration_cast<std::chrono::nanoseconds>(m_startTime.time_since_epoch()).count();
    info->set_starttimestamp(static_cast<uint64_t>(startNs));
}

void ProtobufWriter::FillAllocSummary(proto::AllocSummary* protoSummary, const AllocSummary& summary)
{
    protoSummary->set_active(summary.active);
    protoSummary->set_overhead(summary.overhead);
    protoSummary->set_size(summary.size);
    protoSummary->set_totalbytes(summary.totalBytes);
    protoSummary->set_totalcount(summary.totalCount);
}

void ProtobufWriter::FillChangedSummary(uint32_t traceId, proto::TracedAllocSummary* tracedSummary)
{
    const auto& summary = m_localSummaries[traceId];
    tracedSummary->set_traceid(traceId);
    FillAllocSummary(tracedSummary->mutable_actual(), summary);
}

void ProtobufWriter::AddStacktrace(uint32_t traceId, proto::Snapshot* snapshot)
{
    auto trace = m_finder->GetStacktraceFromId(traceId);
    if (!trace)
    {
        return;
    }
    auto* stacktrace = snapshot->add_stacktraces();
    stacktrace->set_traceid(traceId);
    for (const auto& ptr : trace->GetTrace())
    {
        auto ptrValue = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr)) - 1;
        auto ptrIt = m_ptrMap.find(ptrValue);
        if (ptrIt == m_ptrMap.end())
        {
            const auto id = static_cast<uint32_t>(m_ptrMap.size());
            ptrIt = m_ptrMap.insert({ptrValue, id}).first;
            auto* ptrId = snapshot->add_ptrids();
            ptrId->set_ptraddr(ptrValue);
            ptrId->set_ptrid(id);
        }
        stacktrace->add_ptrid(ptrIt->second);
    }
}

} // namespace writers
} // namespace memhawk
