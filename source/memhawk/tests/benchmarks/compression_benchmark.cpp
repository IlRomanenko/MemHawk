#include <absl/base/attributes.h>
#include <absl/base/optimization.h>
#include <benchmark/benchmark.h>

#include <cstdint>
#include <ios>
#include <list>
#include <fstream>
#include <vector>


std::vector<std::vector<uintptr_t>> PrepareTestdata()
{
    std::vector<std::vector<uintptr_t>> result;
    std::ifstream file("stacktraces.dump.txt", std::ios_base::in);
    size_t traceSize{};
    size_t value{};
    while (file >> traceSize) {
        std::vector<uint64_t> trace;
        trace.resize(traceSize);
        for (size_t i = 0; i < traceSize; i++) {
            file >> value;
            trace.push_back(value);
        }
        result.push_back(std::move(trace));
    }
    return result;
}

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
// Run the benchmark
BENCHMARK_MAIN();
