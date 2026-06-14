#include "alloc_info.h"
#include "config.h"
#include "impl/memhawk.h"
#include "mock.h"
#include "stacktrace.h"
#include "thread_tracker.h"

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

using namespace std::chrono_literals;
using namespace testing;

namespace memhawk
{

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
        m_memhawk->PostponedConstruct();
    }

    void SetDefaultExpectations()
    {
        EXPECT_CALL(*m_writerMock, FlushData).WillRepeatedly(Return());
        EXPECT_CALL(*m_writerMock, UpdateModules).WillRepeatedly(Return());
        EXPECT_CALL(*m_writerMock, AccountSnapshot).WillRepeatedly(Return());
    }

    void SetAccountingExpectations()
    {
        EXPECT_CALL(*m_writerMock, FlushData).WillRepeatedly(Invoke([&]() {
            const std::scoped_lock lock(m_eventMt);
            m_eventVersion++;
            m_eventCv.notify_all();
        }));
        EXPECT_CALL(*m_writerMock, UpdateModules).WillRepeatedly(Return());
        EXPECT_CALL(*m_writerMock, AccountSnapshot)
            .WillRepeatedly(Invoke([&](const auto& summaries, const auto& /*total*/) {
                const std::scoped_lock lock(m_mt);
                for (const auto& [traceId, summary] : summaries)
                {
                    auto it = m_summaries.find(traceId);
                    if (it == m_summaries.end())
                    {
                        it = m_summaries.insert({traceId, {}}).first;
                    }
                    it->second += summary;
                }
                return;
            }));
    }

    void StartThreadsAndWaitForFinish(const std::function<void()>& func, size_t threadsCount)
    {
        std::vector<std::thread> threads;
        threads.reserve(threadsCount);
        for (size_t i = 0; i < threadsCount; i++)
        {
            threads.emplace_back(func);
        }

        for (auto& th: threads)
        {
            if (th.joinable())
            {
                th.join();
            }
        }
    }

    bool WaitForFlush(std::chrono::system_clock::duration timeout = 3s)
    {
        std::unique_lock lock(m_eventMt);
        auto curVersion = m_eventVersion;
        return m_eventCv.wait_for(lock, timeout, [&]() { return curVersion < m_eventVersion; });
    }

protected:
    MemHawkConfig m_cfg{};
    std::unique_ptr<WriterStrategyMock> m_writerMock;
    std::unique_ptr<MemHawk> m_memhawk;

    std::mutex m_eventMt;
    std::condition_variable m_eventCv;
    uint32_t m_eventVersion{};

    std::mutex m_mt;
    SummariesMap m_summaries;
};

TEST_F(MemHawkFixture, CreateAndConstruct_ExpectOk)
{
    SetDefaultExpectations();
    SetUpMemHawk();
}

TEST_F(MemHawkFixture, AccountAllocs_ExpectOk)
{
    constexpr int64_t TestAllocations = 100'000;
    constexpr int64_t AllocationSize = 127;
    constexpr size_t Offset = 16;
    constexpr int64_t TestThreads = 4;

    constexpr auto TotalAllocations = TestAllocations * TestThreads;
    constexpr int64_t ExpectedSize = TotalAllocations * AllocationSize;

    SetAccountingExpectations();
    SetUpMemHawk();

    auto testLambda = [&]() {
        for (size_t i = 0; i < TestAllocations; i++)
        {
            AllocInfo info{AllocationSize, Offset};
            Stacktrace stacktrace{};
            m_memhawk->TrackAlloc(info, stacktrace, true);
        }
    };

    StartThreadsAndWaitForFinish(testLambda, TestThreads);
    EXPECT_TRUE(WaitForFlush());
    // stop memhawk processing
    m_memhawk->Stop();

    const std::scoped_lock lock(m_mt);
    EXPECT_EQ(m_summaries.size(), 1);
    for (const auto& [_, summary]: m_summaries)
    {
        EXPECT_EQ(summary.active, TotalAllocations);
        EXPECT_EQ(summary.totalCount, TotalAllocations);
        EXPECT_EQ(summary.size, ExpectedSize);
        EXPECT_EQ(summary.overhead, Offset * TotalAllocations);
        EXPECT_EQ(summary.totalBytes, ExpectedSize);
    }
}

TEST_F(MemHawkFixture, AccountDeallocs_ExpectOk)
{
    constexpr int64_t TestAllocations = 100'000;
    constexpr int64_t AllocationSize = 127;
    constexpr size_t Offset = 16;
    constexpr int64_t TestThreads = 4;

    constexpr auto TotalAllocations = TestAllocations * TestThreads;
    constexpr int64_t ExpectedSize = -TotalAllocations * AllocationSize;

    SetAccountingExpectations();
    SetUpMemHawk();

    auto testLambda = [&]() {
        for (size_t i = 0; i < TestAllocations; i++)
        {
            const AllocInfo info{AllocationSize, Offset};
            m_memhawk->TrackDealloc(info, true);
        }
    };
    StartThreadsAndWaitForFinish(testLambda, TestThreads);
    EXPECT_TRUE(WaitForFlush());
    // stop memhawk processing
    m_memhawk->Stop();

    const std::scoped_lock lock(m_mt);
    EXPECT_EQ(m_summaries.size(), 1);
    for (const auto& [_, summary]: m_summaries)
    {
        EXPECT_EQ(summary.active, -TotalAllocations);
        EXPECT_EQ(summary.size, ExpectedSize);
        EXPECT_EQ(summary.overhead, -Offset * TotalAllocations);

        // below fields are updated only by TrackAlloc 
        EXPECT_EQ(summary.totalCount, 0);
        EXPECT_EQ(summary.totalBytes, 0);
    }
}

TEST_F(MemHawkFixture, AllocsAndDeallocs_ExpectOk)
{
    constexpr int64_t TestAllocations = 100'000;
    constexpr int64_t AllocationSize = 127;
    constexpr size_t Offset = 16;
    constexpr int64_t TestThreads = 4;

    constexpr auto TotalAllocations = TestAllocations * TestThreads;

    SetAccountingExpectations();
    SetUpMemHawk();

    auto testLambda = [&]() {
        for (size_t i = 0; i < TestAllocations; i++)
        {
            AllocInfo info{AllocationSize, Offset};
            Stacktrace stacktrace{};
            m_memhawk->TrackAlloc(info, stacktrace, true);
            // dealloc allocated memory
            m_memhawk->TrackDealloc(info, true);
        }
    };
    StartThreadsAndWaitForFinish(testLambda, TestThreads);
    EXPECT_TRUE(WaitForFlush());
    // stop memhawk processing
    m_memhawk->Stop();

    const std::scoped_lock lock(m_mt);
    EXPECT_EQ(m_summaries.size(), 1);
    for (const auto& [_, summary]: m_summaries)
    {
        EXPECT_EQ(summary.active, 0);
        EXPECT_EQ(summary.size, 0);
        EXPECT_EQ(summary.overhead, 0);

        // below fields are updated only by TrackAlloc 
        EXPECT_EQ(summary.totalCount, TotalAllocations);
        EXPECT_EQ(summary.totalBytes, TotalAllocations * AllocationSize);
    }
}

TEST_F(MemHawkFixture, AllocsAndDeallocs_FromMainThread_ExpectOk)
{
    constexpr int64_t TestAllocations = 100'000;
    constexpr int64_t AllocationSize = 127;
    constexpr size_t Offset = 16;

    SetAccountingExpectations();
    SetUpMemHawk();
    for (size_t i = 0; i < TestAllocations; i++)
    {
        AllocInfo info{AllocationSize, Offset};
        Stacktrace stacktrace{};
        m_memhawk->TrackAlloc(info, stacktrace, true);
        // dealloc allocated memory
        m_memhawk->TrackDealloc(info, true);
    }
    EXPECT_TRUE(WaitForFlush());
    // stop memhawk processing
    m_memhawk->Stop();

    const std::scoped_lock lock(m_mt);
    EXPECT_EQ(m_summaries.size(), 1);
    for (const auto& [_, summary]: m_summaries)
    {
        EXPECT_EQ(summary.active, 0);
        EXPECT_EQ(summary.size, 0);
        EXPECT_EQ(summary.overhead, 0);

        // below fields are updated only by TrackAlloc 
        EXPECT_EQ(summary.totalCount, TestAllocations);
        EXPECT_EQ(summary.totalBytes, TestAllocations * AllocationSize);
    }
}

} // namespace memhawk
