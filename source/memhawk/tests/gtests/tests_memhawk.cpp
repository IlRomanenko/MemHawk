
#include "config.h"
#include "impl/memhawk.h"
#include "writers/i_writer.h"

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace testing;

namespace memhawk
{

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

class MemHawkFixture : public testing::Test
{
public:
    void SetUp() override
    {
        m_writerMock = std::make_unique<WriterStrategyMock>();
    }

    void SetUpMemHawk()
    {
        auto factory = std::make_unique<WritersFactoryMock>();
        EXPECT_CALL(*factory, CreateWritersAdaptor).WillOnce(Return(ByMove(std::move(m_writerMock))));
        m_memhawk = std::make_unique<MemHawk>(m_cfg, std::move(factory));
    }

    void SetDefaultExpectations()
    {
        EXPECT_CALL(*m_writerMock, FlushData).WillRepeatedly(Return());
        EXPECT_CALL(*m_writerMock, UpdateModules).WillRepeatedly(Return());
        EXPECT_CALL(*m_writerMock, AccountSnapshot).WillRepeatedly(Return());
    }

protected:
    MemHawkConfig m_cfg{};
    std::unique_ptr<WriterStrategyMock> m_writerMock;
    std::unique_ptr<MemHawk> m_memhawk;
};

TEST_F(MemHawkFixture, CreateAndConstruct_ExpectOk)
{
    SetDefaultExpectations();
    SetUpMemHawk();
    m_memhawk->PostponedConstruct();
}

} // namespace memhawk
