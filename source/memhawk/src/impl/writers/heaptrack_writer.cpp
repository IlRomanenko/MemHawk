
#include "heaptrack_writer.h"

#include "logging.h"
#include "stacktrace_tree.h"

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <ios>
#include <link.h>
#include <ostream>
#include <type_traits>

namespace memhawk
{
namespace writers
{

namespace
{

template <typename T>
void WriteHexValue(std::ostream& stream, T value)
{

    constexpr const char hexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    using Type = std::decay_t<T>;
    if constexpr (std::is_integral_v<Type>)
    {
        char buf[16]{};
        char* const start = buf;
        char* data = start;
        while (value > 15)
        {
            *data = hexChars[value % 16];
            value /= 16;
            data++;
        }
        *data = hexChars[value];
        data++;
        std::reverse(buf, data);
        *data = '\0';
        stream << start;
    }
    else
    {
        stream << value;
    }
}

template <typename T>
std::ostream& WriteHexLine(std::ostream& stream, T&& arg)
{
    WriteHexValue(stream, std::forward<T>(arg));
    return stream;
}

template <typename T, typename... Args>
std::ostream& WriteHexLine(std::ostream& stream, T&& arg, Args&&... args)
{
    WriteHexValue(stream, std::forward<T>(arg));
    stream << ' ';
    return WriteHexLine(stream, std::forward<Args>(args)...);
}

int iterate_dl_headers(struct dl_phdr_info* info, size_t /*size*/, void* data)
{
    auto stream = reinterpret_cast<std::ofstream*>(data);
    const char* fileName = info->dlpi_name;
    if (!fileName || !fileName[0])
    {
        fileName = "x\0";
    }
    WriteHexLine(*stream, "m", strlen(fileName), fileName, info->dlpi_addr);

    for (size_t i = 0; i < info->dlpi_phnum; i++)
    {
        const auto& phdr = info->dlpi_phdr[i];
        if (phdr.p_type == PT_LOAD)
        {
            WriteHexLine(*stream, "", phdr.p_vaddr, phdr.p_memsz);
        }
    }

    *stream << "\n";
    return 0;
}
} // namespace

HeaptrackWriter::HeaptrackWriter(HeaptrackWriterConfig cfg, std::shared_ptr<IStacktraceFinder> finder)
    : m_cfg{std::move(cfg)}, m_finder(std::move(finder))
{
    auto filename = GetProcessLogName("heaptrack", "txt");
    if (m_cfg.Filename->has_value())
    {
        filename = m_cfg.Filename->value(); // NOLINT(bugprone-unchecked-optional-access)
    }
    m_ofstream = std::make_unique<std::ofstream>(filename, std::ios_base::out | std::ios_base::binary);

    WriteVersion();
    WriteExe();
    WriteCommandLine();
    WriteSystemInfo();
}

void HeaptrackWriter::UpdateModules()
{
    m_updateModules = true;
}

void HeaptrackWriter::AccountSnapshot(const SummariesMap& summaries, const AllocSummary& /*total*/)
{
    for (const auto& [traceId, summary] : summaries)
    {
        m_changedSummaries.insert(traceId);
        auto it = m_summaries.find(traceId);
        if (it == m_summaries.end())
        {
            m_summaries.insert({traceId, {summary.size, 0}});
            continue;
        }
        it->second.current += summary.size;
    }
}

void HeaptrackWriter::WriteTimestamp()
{
    static const auto startTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    WriteHexLine(*m_ofstream, "c", static_cast<size_t>(elapsed)) << "\n";
}

void HeaptrackWriter::WriteRSS()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    WriteHexLine(*m_ofstream, "R", usage.ru_maxrss) << "\n"; // NOLINT(cppcoreguidelines-pro-type-union-access)
}

void HeaptrackWriter::WriteVersion()
{
    // hardcode latest heaptrack version
    WriteHexLine(*m_ofstream, "v", 0x10550ULL, 3ULL) << "\n";
}

void HeaptrackWriter::WriteExe()
{

    const int BUF_SIZE = 1023;
    char buf[BUF_SIZE + 1];

    const ssize_t size = readlink("/proc/self/exe", buf, BUF_SIZE);

    if (size > 0 && size < BUF_SIZE)
    {
        buf[size] = 0;
        WriteHexLine(*m_ofstream, "x", size, buf) << "\n";
    }
}

void HeaptrackWriter::WriteCommandLine()
{
    *m_ofstream << "X ";
    const int BUF_SIZE = 4096;
    char buf[BUF_SIZE + 1] = {0};

    // contains some amount of 0 terminated strings
    auto fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    auto bytesRead = read(fd, buf, BUF_SIZE);
    close(fd);

    const char* end = buf + bytesRead;
    for (const char* ptr = buf; ptr < end; ptr++)
    {
        auto cur = ptr;
        while (cur < end && *cur)
        {
            cur++;
            // skip until start of next 0-terminated section
        }
        *m_ofstream << std::string{ptr, cur};
        ptr = cur;
    }

    *m_ofstream << "\n";
}

void HeaptrackWriter::WriteSystemInfo()
{
    const auto pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const auto physPages = static_cast<size_t>(sysconf(_SC_PHYS_PAGES));
    WriteHexLine(*m_ofstream, "I", pageSize, physPages) << "\n";
}

void HeaptrackWriter::WriteAllocation(uint32_t traceId, int64_t size, uint32_t index)
{
    WriteHexLine(*m_ofstream, "+", size, index, traceId) << "\n";
}

void HeaptrackWriter::WriteDeallocation(uint32_t traceId)
{
    WriteHexLine(*m_ofstream, "-", traceId) << "\n";
}

void HeaptrackWriter::FlushData()
{
    WriteTimestamp();
    WriteRSS();

    if (m_updateModules)
    {
        *m_ofstream << "m 1 -" << "\n";
        dl_iterate_phdr(iterate_dl_headers, m_ofstream.get());
        m_updateModules = false;
    }

    for (const auto& traceId : m_changedSummaries)
    {
        auto traceIt = m_writtenTraces.find(traceId);
        if (traceIt == m_writtenTraces.end())
        {
            const auto stacktrace = m_finder->GetStacktraceFromId(traceId);
            if (!stacktrace)
            {
                continue;
            }
            const auto index = m_tree.index(*stacktrace, [this](uintptr_t ip, StacktraceTree::NodeId index) {
                --ip;
                WriteHexLine(*m_ofstream, "t", ip, index.value()) << "\n";
            });
            traceIt = m_writtenTraces.insert({traceId, index}).first;
        }

        auto& summary = m_summaries[traceId];

        const auto index = traceIt->second;
        if (summary.prev)
        {
            WriteDeallocation(traceId);
        }
        if (summary.current > 0)
        {
            WriteAllocation(traceId, summary.current, index.value());
        }
        summary.prev = summary.current;
    }

    m_ofstream->flush();
    m_changedSummaries.clear();
}


} // namespace writers
} // namespace memhawk
