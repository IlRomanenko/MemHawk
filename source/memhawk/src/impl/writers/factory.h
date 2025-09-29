#pragma once
#include "writers/adaptor.h"
#include "writers/heaptrack_writer.h"
#include "writers/i_writer.h"
#include "writers/proto_writer.h"
#include "writers/text_writer.h"

namespace memhawk
{
namespace writers
{

class WritersFactory : public IWritersFactory
{
public:
    std::unique_ptr<IWriterStrategy> CreateWritersAdaptor(const WritersConfig& cfg,
                                                          std::shared_ptr<IStacktraceFinder> finder) override
    {
        auto adaptor = std::make_unique<WritersAdaptor>();
        if (*cfg.TextWriter->Enabled)
        {
            adaptor->AddWriter(std::make_unique<TextWriter>(*cfg.TextWriter, finder));
        }
        if (*cfg.ProtobufWriter->Enabled)
        {
            adaptor->AddWriter(std::make_unique<ProtobufWriter>(*cfg.ProtobufWriter, finder));
        }
        if (*cfg.HeaptrackWriter->Enabled)
        {
            adaptor->AddWriter(std::make_unique<HeaptrackWriter>(*cfg.HeaptrackWriter, finder));
        }
        return adaptor;
    }
};

} // namespace writers
} // namespace memhawk
