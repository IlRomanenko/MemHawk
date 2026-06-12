#include "impl/owned_guards.h"

#include <gtest/gtest.h>

#include <emmintrin.h>
#include <latch>
#include <mutex>
#include <thread>

using namespace testing;

namespace memhawk
{

class OwnedGuardsFixture : public testing::Test
{
public:
    void RunInThreads(size_t threadsCount, const std::function<void()>& func)
    {
        std::vector<std::thread> threads;
        threads.reserve(threadsCount);

        for (size_t i = 0; i < threadsCount; i++)
        {
            threads.emplace_back(func);
        }
        JoinAllThreads(threads);
    }

    void JoinAllThreads(std::vector<std::thread>& threads)
    {
        for (auto& th : threads)
        {
            if (th.joinable())
            {
                th.join();
            }
        }
    }
};

TEST_F(OwnedGuardsFixture, Ctr_ExpectOk)
{
    EXPECT_NO_THROW(const OwnedGuards guardsOwner);
}

TEST_F(OwnedGuardsFixture, SingleThread_ExpectCalled)
{
    OwnedGuards guardsOwner;
    std::atomic<uint64_t> value;
    {
        const auto guard = guardsOwner.Register([&value] { value++; });
        EXPECT_EQ(value, 0);
    }
    // updated on guard dtor
    EXPECT_EQ(value, 1);
}

TEST_F(OwnedGuardsFixture, MultiThread_ExpectCalled)
{
    constexpr const size_t ThreadsCount = 16;
    OwnedGuards guardsOwner;
    std::atomic<uint64_t> value;

    RunInThreads(ThreadsCount, [&] { const auto guard = guardsOwner.Register([&value] { value++; }); });
    EXPECT_EQ(value, ThreadsCount);
}

TEST_F(OwnedGuardsFixture, MultiThread_Tls_ExpectCalled)
{
    constexpr const size_t ThreadsCount = 16;
    OwnedGuards guardsOwner;
    std::atomic<uint64_t> value;

    RunInThreads(ThreadsCount, [&] { const thread_local auto guard = guardsOwner.Register([&value] { value++; }); });
    EXPECT_EQ(value, ThreadsCount);
}

TEST_F(OwnedGuardsFixture, MultiThread_DrainedOwner_ExpectCalled)
{
    constexpr const size_t ThreadsCount = 16;
    std::atomic<uint64_t> value;
    auto guardsOwner = std::make_unique<OwnedGuards>();

    std::vector<std::thread> threads;

    std::latch registerLatch(ThreadsCount + 1);
    std::latch deinitLatch(ThreadsCount + 1);

    threads.reserve(ThreadsCount);
    for (size_t i = 0; i < ThreadsCount; i++)
    {
        threads.emplace_back([&] {
            const thread_local auto guard = guardsOwner->Register([&value] { value++; });
            registerLatch.arrive_and_wait();
            // guardsOwner will be drained
            deinitLatch.arrive_and_wait();
        });
    }
    registerLatch.arrive_and_wait();
    guardsOwner->Drain();
    deinitLatch.arrive_and_wait();
    JoinAllThreads(threads);

    EXPECT_EQ(value, ThreadsCount);
}

TEST_F(OwnedGuardsFixture, MultiThread_GuardOutlivesOwner_ExpectCalled)
{
    constexpr const size_t ThreadsCount = 16;
    std::atomic<uint64_t> value;
    std::mutex ownerMt;
    auto guardsOwner = std::make_unique<OwnedGuards>();

    std::vector<std::thread> threads;

    std::latch registerLatch(ThreadsCount + 1);
    std::latch deinitLatch(ThreadsCount + 1);

    threads.reserve(ThreadsCount);
    for (size_t i = 0; i < ThreadsCount; i++)
    {
        threads.emplace_back([&] {
            {
                const std::scoped_lock lock(ownerMt);
                const thread_local auto guard = guardsOwner->Register([&value] { value++; });
            }
            registerLatch.arrive_and_wait();
            // guardsOwner will be deleted
            deinitLatch.arrive_and_wait();
        });
    }
    registerLatch.arrive_and_wait();
    {
        const std::scoped_lock lock(ownerMt);
        guardsOwner.reset();
    }
    deinitLatch.arrive_and_wait();
    JoinAllThreads(threads);

    EXPECT_EQ(value, ThreadsCount);
}

TEST_F(OwnedGuardsFixture, MultiThread_DrainAndThreadExit_ExpectCalledOnce)
{
    constexpr size_t Iterations = 100;
    constexpr size_t ThreadsCount = 4;

    for (size_t it = 0; it < Iterations; it++)
    {
        OwnedGuards owner;
        std::atomic<uint64_t> value{0};
        std::latch registerLatch(ThreadsCount + 1);

        std::vector<std::thread> threads;
        threads.reserve(ThreadsCount);
        for (size_t i = 0; i < ThreadsCount; ++i)
        {
            threads.emplace_back([&] {
                const auto guard = owner.Register([&value] { value++; });
                registerLatch.arrive_and_wait();
                // guard dtor is concurrent with owner.Drain
            });
        }

        registerLatch.arrive_and_wait();
        owner.Drain(); // concurrent with thread exits

        JoinAllThreads(threads);
        EXPECT_EQ(value.load(), ThreadsCount);
    }
}

} // namespace memhawk
