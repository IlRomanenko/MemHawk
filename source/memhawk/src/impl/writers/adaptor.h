#pragma once

#include "alloc_info.h"
#include "writers/i_writer.h"

namespace memhawk
{
namespace writers
{

class WritersAdaptor : public IWriterStrategy
{
public:
    WritersAdaptor() = default;
    WritersAdaptor(const WritersAdaptor&) = delete;
    WritersAdaptor& operator=(const WritersAdaptor&) = delete;

    void UpdateModules() override
    {
        for (auto& writer : m_writers)
        {
            writer->UpdateModules();
        }
    }

    void AccountSnapshot(const SummariesMap& summaries, const AllocSummary& total) override
    {
        for (auto& writer : m_writers)
        {
            writer->AccountSnapshot(summaries, total);
        }
    }

    void FlushData() override
    {
        for (auto& writer : m_writers)
        {
            writer->FlushData();
        }
    }

    void AddWriter(std::unique_ptr<IWriterStrategy> writer)
    {
        m_writers.push_back(std::move(writer));
    }

private:
    std::vector<std::unique_ptr<IWriterStrategy>> m_writers;
};

} // namespace writers
} // namespace memhawk
