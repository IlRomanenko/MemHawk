#include <absl/base/attributes.h>
#include <absl/base/optimization.h>
#include <absl/debugging/stacktrace.h>
#include <benchmark/benchmark.h>
#include <boost/concept_check.hpp>

#include <libunwind.h>
#include <list>

static void BM_MT_ListAllocs(benchmark::State& state)
{
    std::list<int> dq;
    // Perform setup here
    for (auto value : state)
    {
        boost::ignore_unused_variable_warning(value);
        // This code gets timed
        for (auto i = 0; i < state.range(); i++)
        {
            dq.emplace_back(i);
        }
        for (auto i = 0; i < state.range(); i++)
        {
            dq.pop_front();
        }
    }
    state.SetComplexityN(state.range(0));
}

// Register the function as a benchmark
BENCHMARK(BM_MT_ListAllocs)
    ->Range(2 << 20, 2 << 20)
    ->ThreadRange(1, 16)
    ->UseRealTime()
    ->Complexity(benchmark::BigO::oN);


static constexpr int kMaxStackDepth = 100;
static constexpr int kCacheSize = (1 << 16);
int cacheArray[kCacheSize];
void* unwindArray[kMaxStackDepth];

ABSL_ATTRIBUTE_NOINLINE void unwind_stacktrace(benchmark::State& state, int64_t x, int64_t depth, bool abslUnwinder)
{
    if (x <= 0)
    {
        // Touch a significant amount of memory so that the stack is likely to be
        // not cached in the L1 cache.
        state.PauseTiming();
        for (int i = 0; i < kCacheSize; ++i)
        {
            benchmark::DoNotOptimize(cacheArray[i] = 100);
        }
        state.ResumeTiming();
        if (abslUnwinder)
        {
            benchmark::DoNotOptimize(absl::GetStackTrace(unwindArray, kMaxStackDepth, 0));
        }
        else
        {
            benchmark::DoNotOptimize(unw_backtrace(unwindArray, kMaxStackDepth));
        }
        return;
    }
    ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
    unwind_stacktrace(state, --x, depth, abslUnwinder);
}

static void BM_absl_StackTrace(benchmark::State& state)
{
    const auto depth = state.range(0);
    for (auto value : state)
    {
        boost::ignore_unused_variable_warning(value);
        unwind_stacktrace(state, depth, depth, true);
    }
}

static void BM_libunwind_StackTrace(benchmark::State& state)
{
    const auto depth = state.range(0);
    for (auto value : state)
    {
        boost::ignore_unused_variable_warning(value);
        unwind_stacktrace(state, depth, depth, false);
    }
}

BENCHMARK(BM_absl_StackTrace)->DenseRange(10, kMaxStackDepth, 30);
BENCHMARK(BM_libunwind_StackTrace)->DenseRange(10, kMaxStackDepth, 30);

// Run the benchmark
BENCHMARK_MAIN();
