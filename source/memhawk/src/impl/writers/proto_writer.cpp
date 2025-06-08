
#include "proto_writer.h"

#include "logging.h"

#include <google/protobuf/arena.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <protos/snapshot.pb.h>

#include <cstdint>
#include <fstream>
#include <ios>
#include <link.h>

namespace memhawk
{
namespace writers
{

namespace
{
int iterate_dl_headers(struct dl_phdr_info* info, size_t /*size*/, void* data)
{
    auto snapshot = reinterpret_cast<protos::Snapshot*>(data);
    auto* loadedSo = snapshot->add_loadedso();
    loadedSo->set_addr(info->dlpi_addr);
    loadedSo->set_filename(std::string{info->dlpi_name});
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
    auto filename = GetProcessLogName("protobuf", "binpb");
    if (m_cfg.Filename->has_value())
    {
        filename = m_cfg.Filename->value(); // NOLINT(bugprone-unchecked-optional-access)
    }
    m_ofstream = std::make_unique<std::ofstream>(filename, std::ios_base::out | std::ios_base::binary);
    m_ostream = std::make_unique<google::protobuf::io::OstreamOutputStream>(m_ofstream.get());
    m_codedStream = std::make_unique<google::protobuf::io::CodedOutputStream>(m_ostream.get());
}

void ProtobufWriter::UpdateModules()
{
    m_updateModules = true;
}

void ProtobufWriter::AccountSnapshot(const SummariesMap& summaries, const AllocSummary& /*total*/)
{
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
    protos::Snapshot* snapshot = google::protobuf::Arena::Create<protos::Snapshot>(&m_arena);

    if (m_updateModules)
    {
        dl_iterate_phdr(iterate_dl_headers, snapshot);
        m_updateModules = false;
    }

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
    google::protobuf::util::SerializeDelimitedToCodedStream(*snapshot, m_codedStream.get());
    m_ofstream->flush();
    m_changedSummaries.clear();
    m_arena.Reset();
}

void ProtobufWriter::FillChangedSummary(uint32_t traceId, protos::TracedAllocSummary* tracedSummary)
{
    const auto& summary = m_localSummaries[traceId];

    tracedSummary->set_traceid(traceId);
    auto* actual = tracedSummary->mutable_actual();
    actual->set_active(summary.active);
    actual->set_overhead(summary.overhead);
    actual->set_size(summary.size);
    actual->set_totalbytes(summary.totalBytes);
    actual->set_totalcount(summary.totalCount);
}

void ProtobufWriter::AddStacktrace(uint32_t traceId, protos::Snapshot* snapshot)
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
        auto ptrValue = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
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
