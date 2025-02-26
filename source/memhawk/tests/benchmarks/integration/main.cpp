#include <benchmark/benchmark.h>

#include <list>

static void BM_MT_ListAllocs(benchmark::State& state)
{
    std::list<int> dq;
    // Perform setup here
    for (auto _ : state) {
        // This code gets timed
        for (auto i = 0; i < state.range(); i++) {
            dq.emplace_back(i);
        }
        for (auto i = 0; i < state.range(); i++) {
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


// Run the benchmark
BENCHMARK_MAIN();
