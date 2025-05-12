#include "impl/algo.h"
#include "impl/config.h"

#include <absl/types/span.h>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstring>
#include <string_view>

void SetUpData(std::string_view input, void** output)
{
    for (size_t i = 0; i < input.size(); i++)
    {
        output[i] = reinterpret_cast<void*>(static_cast<uint64_t>(input[i]));
    }
}

const std::string_view TestData = "abacabacffdghjkqweqwelabdfabdfqlabacabacffdghjkqweqwelabdfabdfq";

void TestCollapseRecursion(benchmark::State& state, size_t(*func)(absl::Span<void*>, size_t))
{
    alignas(64) void* testData[memhawk::MaxUnwindDepth];
    alignas(64) void* input[memhawk::MaxUnwindDepth];
    SetUpData(TestData, testData);
    const auto depth = static_cast<size_t>(state.range(0));

    for (auto _ : state) // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        memcpy(reinterpret_cast<void*>(input), reinterpret_cast<void*>(testData), TestData.size() * sizeof(void*));
        size_t result = func(absl::MakeSpan(input), depth);
        benchmark::DoNotOptimize(result);
    }
}

void BM_CollapseRecursionNaive(benchmark::State& state)
{
    TestCollapseRecursion(state, memhawk::CollapseRecursionNaive);
}

void BM_CollapseRecursion(benchmark::State& state)
{
    TestCollapseRecursion(state, memhawk::CollapseRecursion);
}

void BM_CollapseRecursionOpt(benchmark::State& state)
{
    TestCollapseRecursion(state, memhawk::CollapseRecursionOpt);
}


// clang-format off

BENCHMARK(BM_CollapseRecursionNaive)
    ->DenseRange(0, 6, 2)
    ->UseRealTime();

BENCHMARK(BM_CollapseRecursion)
    ->DenseRange(0, 6, 2)
    ->UseRealTime();

BENCHMARK(BM_CollapseRecursionOpt)
    ->DenseRange(0, 6, 2)
    ->UseRealTime();

// clang-format on
