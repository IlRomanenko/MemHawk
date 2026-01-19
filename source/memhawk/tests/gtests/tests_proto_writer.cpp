#include "alloc_info.h"
#include "config.h"
#include "mock.h"
#include "proto/snapshot.pb.h"
#include "writers/proto_writer.h"

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <system_error>
#include <zstd.h>

using namespace testing;

namespace memhawk
{
namespace
{
constexpr const uint32_t TraceId1 = 1;
constexpr const uint32_t TraceId2 = 2;
constexpr const uint32_t TraceId3 = 3;

constexpr const AllocSummary Summary1{0, 1, 2, 3, 4};
constexpr const AllocSummary Summary2{10, 11, 12, 13, 14};
constexpr const AllocSummary Summary3{20, 21, 22, 23, 24};
} // namespace

class ProtobufWriterFixture : public testing::Test
{
public:
    void SetUp() override
    {
        m_filename = std::filesystem::temp_directory_path() / "memhawk_test.binpb";
        if (std::filesystem::exists(m_filename))
        {
            std::error_code ec{};
            std::filesystem::remove(m_filename, ec);
            EXPECT_FALSE(ec);
        }
        m_cfg.Enabled = true;
        m_cfg.Filename = m_filename;

        m_finderMock = std::make_shared<StacktraceFinderMock>();

        EXPECT_CALL(*m_finderMock, GetStacktraceFromId).WillRepeatedly(Return(std::nullopt));
    }

    void TearDown() override
    {
        if (std::filesystem::exists(m_filename))
        {
            std::error_code ec{};
            std::filesystem::remove(m_filename, ec);
            EXPECT_FALSE(ec);
        }
    }

    std::optional<uint64_t> ReadUint64BigEndian(std::ifstream& stream)
    {
        constexpr size_t ValueSize = 8;
        char buf[ValueSize];
        if (!stream.read(buf, ValueSize))
        {
            // eof
            EXPECT_EQ(0, stream.gcount());
            return {};
        }
        EXPECT_EQ(8, stream.gcount());
        uint64_t value = 0;
        for (size_t i = 0; i < ValueSize; i++)
        {
            value <<= 8;
            value |= static_cast<uint8_t>(buf[i]);
        }
        return value;
    }

    template <typename T>
    std::optional<T> ReadMessage(std::ifstream& stream)
    {
        const auto messageSizeOpt = ReadUint64BigEndian(stream);
        if (!messageSizeOpt)
        {
            return {};
        }
        const auto compressedMessageSize = messageSizeOpt.value();
        std::vector<char> compressed;
        compressed.resize(compressedMessageSize);
        EXPECT_TRUE(stream.read(compressed.data(), static_cast<std::streamsize>(compressedMessageSize)));
        EXPECT_EQ(stream.gcount(), compressedMessageSize);
        const auto frameContentSize = ZSTD_getFrameContentSize(compressed.data(), compressedMessageSize);
        EXPECT_NE(frameContentSize, ZSTD_CONTENTSIZE_ERROR);
        EXPECT_NE(frameContentSize, ZSTD_CONTENTSIZE_UNKNOWN);
        std::vector<uint8_t> decompressed;
        decompressed.resize(frameContentSize);
        const auto decompressedSize =
            ZSTD_decompress(decompressed.data(), frameContentSize, compressed.data(), compressedMessageSize);
        EXPECT_FALSE(ZSTD_isError(decompressedSize));
        T result{};
        EXPECT_TRUE(result.ParseFromArray(decompressed.data(), static_cast<int>(decompressedSize)));
        return result;
    }

    void ReadSnapshots(std::vector<proto::Snapshot>& snapshots)
    {
        std::ifstream stream(m_filename, std::ios_base::in | std::ios_base::binary);
        auto processInfo = ReadMessage<proto::ProcessInfo>(stream);
        EXPECT_TRUE(processInfo);

        std::optional<proto::Snapshot> snapshot;
        do
        {
            snapshot = ReadMessage<proto::Snapshot>(stream);
            if (snapshot)
            {
                snapshots.push_back(snapshot.value());
            }
        } while (snapshot);
    }

protected:
    ProtobufWriterConfig m_cfg{};
    std::string m_filename;
    std::unique_ptr<writers::ProtobufWriter> m_writer;

    std::shared_ptr<StacktraceFinderMock> m_finderMock;
};

TEST_F(ProtobufWriterFixture, CreateAndConstruct_ExpectOk)
{
    writers::ProtobufWriter writer{m_cfg, m_finderMock};
}

TEST_F(ProtobufWriterFixture, AccountSnapshot_ExpectOk)
{
    {
        writers::ProtobufWriter writer{m_cfg, m_finderMock};
        writer.AccountSnapshot({{TraceId1, Summary1}}, Summary1);
        writer.FlushData();
    }
    std::vector<proto::Snapshot> snapshots;
    ReadSnapshots(snapshots);

    EXPECT_EQ(snapshots.size(), 1);
}

TEST_F(ProtobufWriterFixture, AccountSnapshot_MultipleCalls_ExpectOk)
{
    {
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId1))).WillOnce(Return(std::nullopt));
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId2))).WillOnce(Return(std::nullopt));
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId3))).WillOnce(Return(std::nullopt));
        writers::ProtobufWriter writer{m_cfg, m_finderMock};
        writer.AccountSnapshot({{TraceId1, Summary1}}, Summary1);
        writer.AccountSnapshot({{TraceId2, Summary2}}, Summary2);
        writer.AccountSnapshot({{TraceId3, Summary3}}, Summary3);
        writer.FlushData();
    }
    std::vector<proto::Snapshot> snapshots;
    ReadSnapshots(snapshots);

    ASSERT_EQ(snapshots.size(), 1);
    EXPECT_EQ(snapshots.front().changed_size(), 3);
    EXPECT_EQ(snapshots.front().ptrids_size(), 0);
    EXPECT_EQ(snapshots.front().stacktraces_size(), 0);
}

TEST_F(ProtobufWriterFixture, AccountSnapshot_MultipleWrites_ExpectOk)
{
    {
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId1))).WillOnce(Return(std::nullopt));
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId2))).WillOnce(Return(std::nullopt));
        EXPECT_CALL(*m_finderMock, GetStacktraceFromId(Eq(TraceId3))).WillOnce(Return(std::nullopt));
        writers::ProtobufWriter writer{m_cfg, m_finderMock};
        writer.AccountSnapshot({{TraceId1, Summary1}}, Summary1);
        writer.FlushData();
        writer.AccountSnapshot({{TraceId2, Summary2}}, Summary2);
        writer.FlushData();
        writer.AccountSnapshot({{TraceId3, Summary3}}, Summary3);
        writer.FlushData();
    }
    std::vector<proto::Snapshot> snapshots;
    ReadSnapshots(snapshots);

    ASSERT_EQ(snapshots.size(), 3);
    for (const auto& snapshot : snapshots)
    {
        EXPECT_EQ(snapshot.changed_size(), 1);
        EXPECT_EQ(snapshot.ptrids_size(), 0);
        EXPECT_EQ(snapshot.stacktraces_size(), 0);
    }
}

} // namespace memhawk
