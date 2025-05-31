

#include <absl/base/attributes.h>
#include <absl/base/optimization.h>
#include <absl/debugging/stacktrace.h>
#include <benchmark/benchmark.h>
#include <boost/concept_check.hpp>

#include <libunwind.h>

static constexpr int kMaxStackDepth = 64;
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

static void BM_StackTrace_absl(benchmark::State& state)
{
    const auto depth = state.range(0);
    for (auto value : state)
    {
        boost::ignore_unused_variable_warning(value);
        unwind_stacktrace(state, depth, depth, true);
    }
}

static void BM_StackTrace_libunwind(benchmark::State& state)
{
    const auto depth = state.range(0);
    for (auto value : state)
    {
        boost::ignore_unused_variable_warning(value);
        unwind_stacktrace(state, depth, depth, false);
    }
}

BENCHMARK(BM_StackTrace_absl)->DenseRange(10, kMaxStackDepth, 30);
BENCHMARK(BM_StackTrace_libunwind)->DenseRange(10, kMaxStackDepth, 30);
