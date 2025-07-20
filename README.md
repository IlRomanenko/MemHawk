# MemHawk - a high performance memory profiler

![GitHub License](https://img.shields.io/github/license/IlRomanenko/MemHawk) [![Asan Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-asan.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-asan.yaml) [![Tsan Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-tsan.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/sanitizers-tsan.yaml) [![Clang-tidy Status](https://github.com/IlRomanenko/MemHawk/actions/workflows/clang-tidy.yaml/badge.svg)](https://github.com/IlRomanenko/MemHawk/actions/workflows/clang-tidy.yaml)

## Overview

MemHawk traces all memory allocations and summaries them by stack traces. It helps you quickly identify the top-N memory allocation traces that are consuming the most memory — perfect for tracking down leaks in your application.

## Features

* **Statically Linked & Zero External Dependencies:** MemHawk is self-contained. It’s built for static linking, meaning you won’t have to worry about external libraries or dependency conflicts.

* **Supports unwinding by dwarf info and by frame pointer:** Unwinds all binaries with dwarf info. Moreover unwinding speed can be increased for binaries built with `-fno-omit-frame-pointer`.

* **Extremely Fast with Minimal Global Locks:** MemHawk is designed to be incredibly efficient. Its minimal-lock design ensures that even in multi-threaded environments, the synchronization overhead is kept to a minimum.

* **Top-N Trace Summaries:** Instantly identify the top allocation traces by memory consumption. This focused insight makes it easier to locate and resolve memory leaks.

* **CMake Integration:** The library is designed using modern CMake practices, making it straightforward to integrate into your existing projects.

## Is it actually fast?

**Definitely.** And the difference in speed will become more noticeable as the number of cores and threads increases. Performance was tested on `Intel(R) Core(TM) i9-9900KF CPU @ 3.60GHz`.  Benchmark is located on `tests/run_benchmark.sh`.

**N.B.** Jemalloc is the fastest option, but it performs probabilistic sampling instead of full profiling and omits information about total allocation count on given trace.

| Allocator                       | Workers | Time    |
|---------------------------------|---------|---------|
| tcmalloc.so + heap profiling    | 16      | 50844ms |
| heaptrack.so                    | 16      | 28137ms |
| libmemhawk.so + dwarf unwinding | 16      | 2794ms  |
| libmemhawk.so + frame unwinding | 16      | 2186ms  |
| libmemhawk.so + frame unwinding + zero recursion collapsing | 16 | 1816ms |
| system malloc                   | 16      | 582ms   |
| jemalloc.so + heap sampling     | 16      | 336ms   |

## Getting Started

### Prerequisites

A C++ compiler with C++20 (or later) support.

CMake 3.25 or newer.

## Building MemHawk

```(bash)
git clone https://github.com/IlRomanenko/MemHawk.git
cd MemHawk
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Usage

### Primary usage via LD_PRELOAD

```(bash)
LD_PRELOAD=/path/to/libmemhawk.so ./your_application
```

### Add as dependency via patchelf

With patchelf memhawk can be injected even into binaries with suid/guid.

```(bash)
patchelf --add-needed /path/to/libmemhawk.so ./your_application
./your_application <your app args>
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

```bash
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

```bash
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

```bash
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

### As a Linked Library

MemHawk can also be linked into your application. In that case, it's recommended to list it as the very first dependency in your target's link libraries. This ensures that MemHawk's interception mechanisms are set up before any other libraries.

## License

MemHawk is licensed under the Apache-2.0 License. See the LICENSE file for details.
