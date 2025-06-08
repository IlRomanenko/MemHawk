#include "impl/spinlock.h"

#include <absl/base/internal/spinlock.h>
#include <benchmark/benchmark.h>

#include <mutex>

void BM_SpinLock_MemHawk(benchmark::State& state)
{
    memhawk::SpinLock spinlock;
    for (auto _ : state) // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        std::lock_guard lock(spinlock);
        benchmark::DoNotOptimize(lock);
    }
}

void BM_SpinLock_Absl(benchmark::State& state)
{
    absl::base_internal::SpinLock spinlock;
    for (auto _ : state) // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        absl::base_internal::SpinLockHolder lock(&spinlock);
        benchmark::DoNotOptimize(lock);
    }
}

// clang-format off

BENCHMARK(BM_SpinLock_Absl)
    ->ThreadRange(1, 16)
    ->UseRealTime();

BENCHMARK(BM_SpinLock_MemHawk)
    ->ThreadRange(1, 16)
    ->UseRealTime();

// clang-format on
