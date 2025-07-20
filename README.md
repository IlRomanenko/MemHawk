# MemHawk - a high performance memory profiler

![GitHub License](https://img.shields.io/github/license/IlRomanenko/MemHawk) [![Asan Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-asan.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-asan.yaml) [![Tsan Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-tsan.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-tsan.yaml) [![Clang-tidy Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/clang-tidy.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/clang-tidy.yaml)

## Overview

MemHawk traces all memory allocations and summaries them by stack traces. It helps you quickly identify the top-N memory allocation traces that are consuming the most memory — perfect for tracking down leaks in your application.

## Features

* 🚀 **Extremely Fast**: Designed with a minimal-locking, thread-local architecture to ensure the lowest possible overhead, even in highly concurrent applications.

* 🎯 **100% Accurate**: Tracks every malloc/free to give you a complete and precise picture of your application's memory usage. No sampling, no missed events.

* 💡 **Smart Aggregation**: Instead of logging millions of individual events, MemHawk groups allocations by unique stack traces, making it easy to spot the top memory consumers.

* 🔧 **Flexible Unwinding**: Supports stack unwinding via both DWARF debug info (for any binary) and frame pointers (-fno-omit-frame-pointer) for even greater speed.

* 📦 **Zero Runtime Dependencies & Statically Linked**: MemHawk is compiled into a single, self-contained library. All its components (like libunwind, absl, xxHash) are built-in. You don't need to install anything on the target machine—just copy libmemhawk.so and you're ready to profile.

## Benchmarks

Performance was tested on an Intel(R) Core(TM) i9-9900KF CPU @ 3.60GHz. The difference in speed becomes even more noticeable as the number of threads increases. The benchmark source is available in `tests/bench_allocs.cpp`.

**N.B.** Jemalloc is the fastest option, but it performs probabilistic sampling instead of full profiling and omits information about total allocation count on given trace.

| Profiler / Allocator                          | Workers | Time    | Speedup |
| --------------------------------------------- | ------- | ------- | ------- |
| tcmalloc.so + heap profiling                  | 16      | 50991ms | 82.8x   |
| **heaptrack.so**                              | 16      | 25512ms | 41.4x   |
| **libmemhawk.so + dwarf unwinding (default)** | 16      | 1555ms  | 2.52x   |
| libmemhawk.so + frame unwinding               | 16      | 1182ms  | 1.91x   |
| libmemhawk.so + dwarf unwinding + jemalloc    | 16      | 1120ms  | 1.81x   |
| libmemhawk.so + frame unwinding + jemalloc    | 16      | 843ms   | 1.36x   |
| **system malloc (baseline)**                  | 16      | 616ms   | 1x      |
| jemalloc.so + heap sampling                   | 16      | 311ms   | 0.5x    |

## How It Works

MemHawk achieves its speed by avoiding global locks on the critical path. Each application thread writes allocation data to a thread-local cache. A background worker thread then asynchronously collects and aggregates this data, generating a global summary with minimal impact on the application's performance.

This design makes it exceptionally well-suited for profiling highly concurrent, multi-threaded applications.

## Getting Started

### Prerequisites

*   A C++ compiler with C++20 support (GCC 11+, Clang 12+).
*   CMake 3.25 or newer.

## Building MemHawk

These commands will configure, build, and install the library into an artifacts directory within the project root.
The recommended configuration uses Clang, as it is the primary compiler used for development and testing.

```bash
git clone https://github.com/IlRomanenko/MemHawk.git
cd MemHawk
mkdir build && cd build
cmake -B $(pwd)/build --preset ReleaseClang -DCMAKE_INSTALL_PREFIX=$(pwd)/artifacts
cmake --build $(pwd)/build --parallel $(nproc)
cmake --install $(pwd)/build
```

## Usage

### Primary usage via LD_PRELOAD

```bash
LD_PRELOAD=/path/to/libmemhawk.so ./your_application
```

### Add as dependency via patchelf

With patchelf memhawk can be injected even into binaries with suid/guid, where LD_PRELOAD is often disabled for security reasons.

```(bash)
patchelf --add-needed /path/to/libmemhawk.so ./your_application
./your_application <your app args>
```

### As a Linked Library

MemHawk can also be linked into your application. In that case, it's recommended to list it as the very first dependency in your target's link libraries. This ensures that MemHawk's interception mechanisms are set up before any other libraries.

## Configuration

MemHawk's behavior can be controlled via the `MEMHAWK_OPTS` environment variable. Multiple options can be separated by a colon (`:`).

To see a full list of available options and their default values, run:
```bash
MEMHAWK_OPTS=help=1 LD_PRELOAD=./libmemhawk.so date
```

**Example:** Change the log directory and the summary report period.
```bash
MEMHAWK_OPTS="logging.log_dir=/tmp:memhawk.dumping_period=5000" \
  LD_PRELOAD=./libmemhawk.so ./your_application
```

## Examples

### 1. Basic Profiling (with a real-world application)
Let's find allocation hotspots in a real-world application, like filelight (a disk usage visualizer).

#### Step 1: Run the application with MemHawk
```bash
LD_PRELOAD=./libmemhawk.so filelight
```

#### Step 2: Analyze the results
After starting filelight, MemHawk will create several log files. The most important one is `memhawk_filelight_<pid>_summary.log`. It contains aggregated statistics for all allocations.

```log
2025-07-20T17:22:13.1626456+03:00
Application heap: size:   332.598 mb, active:      6089431, total:     11116384, average:       57.272 bytes, overhead:    92.918 mb
MemHawk heap:     size:    48.140 mb, active:       204186, total:       210301, average:      247.217 bytes, overhead:     3.117 mb
Total traces: 173462, updated since last time: 18569
External index:
ByActiveSize
TraceId:    440633, size:    82.386 mb, active:      1199826, total:      1199826, average:       72.000 bytes, overhead:    18.308 mb
TraceId:    440636, size:    40.886 mb, active:      1198535, total:      1198535, average:       35.771 bytes, overhead:    18.288 mb
...
ByTotalCount
TraceId:    440645, size:     0.000 mb, active:            0, total:      1206461, average:        0.000 bytes, overhead:     0.000 mb
TraceId:    440637, size:    27.462 mb, active:      1199826, total:      1199826, average:       24.000 bytes, overhead:    18.308 mb
...
```
**How to read this:**

**ByActiveSize:** Shows which code is responsible for the largest amount of active (i.e., not yet freed) memory. TraceId 440633 is our primary suspect for a memory leak, as it holds over 82 MB.

**ByTotalCount:** Shows the code that performs the most allocations overall. This helps find hotspots for optimization, even if they don't cause leaks.

#### Step 3: Find the source code
Now, let's find TraceId: 440633 in the `memhawk_filelight_<pid>_stacktraces.log` file to see the full stack trace.

```log
TraceId: 440633
0x55762bfb8659: /usr/bin/filelight + 31659: _ZN9Filelight11LocalLister4scanERK10QByteArrayS3_
0x55762bfba461: /usr/bin/filelight + 33461: _ZNSt17_Function_handlerIFvvEZN9Filelight11LocalLister4scanERK10QByteArrayS5_EUlvE_E9_M_invokeERKSt9_Any_data
0x55762bfb89a2: /usr/bin/filelight + 319a2: _ZN9Filelight11LocalLister4scanERK10QByteArrayS3_
0x55762bfba461: /usr/bin/filelight + 33461: _ZNSt17_Function_handlerIFvvEZN9Filelight11LocalLister4scanERK10QByteArrayS5_EUlvE_E9_M_invokeERKSt9_Any_data
0x55762bfb6482: /usr/bin/filelight + 2f482: _ZN9QRunnable16QGenericRunnable6HelperISt8functionIFvvEEE4implENS0_10HelperBase2OpEPS6_Pv
0x7ff13b7277b3: /usr/lib/libQt6Core.so.6.9.1 + 3277b3: 
0x7ff13b71ee69: /usr/lib/libQt6Core.so.6.9.1 + 31ee69: 
0x7ff13aea57eb: /usr/lib/libc.so.6 + 957eb: start_thread
0x7ff13af2918c: /usr/lib/libc.so.6 + 11918c: __clone3
```
The stack trace clearly points to the Filelight::LocalLister::scan function as the source of the allocations.

### 2. Advanced: Working with Stripped Binaries and addr2line
You can get full stack trace information even if your binary is stripped of its debug symbols.

#### Step 1: Separate the debug symbols (before profiling)
Use strip to create a clean binary and a separate file with the debug symbols.

```bash
# Save debug info to a separate file
strip --only-keep-debug ./bench_allocs -o symbols.sym

# Strip all symbols from the main binary
strip --strip-all ./bench_allocs -o stripped
```

#### Step 2: Run the profiler
```bash
LD_PRELOAD=./libmemhawk.so ./stripped
```
In stacktraces.log, you will see unresolved addresses:

```log
TraceId: 14
0x564cb099d44a: /path/to/stripped + 244a:
0x7fa1004e51a4: /usr/lib/libstdc++.so.6.0.34 + e51a4: execute_native_thread_routine
0x7fa1001807eb: /usr/lib/libc.so.6 + 957eb: start_thread
0x7fa10020418c: /usr/lib/libc.so.6 + 11918c: __clone3
```

#### Step 3: Resolve symbols using addr2line
Use the symbol file (symbols.sym) and the address from the log to find the exact function name, file, and even the chain of inlined calls.

```bash
addr2line -f -i -C -e ./symbols.sym 0x244a

bar()
/path/to/memhawk/tests/bench_allocs.cpp:28
foo()
/path/to/memhawk/tests/bench_allocs.cpp:34
worker()
/path/to/memhawk/tests/bench_allocs.cpp:48
```
The -i flag shows the full chain of inlined calls, making this method incredibly powerful for debugging production builds.

## License

MemHawk is licensed under the Apache-2.0 License. See the LICENSE file for details.
