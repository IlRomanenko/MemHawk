# MemHawk - a high performance memory profiler

![GitHub License](https://img.shields.io/github/license/IlRomanenko/MemHawk) ![Asan Status](https://img.shields.io/github/actions/workflow/status/IlRomanenko/MemHawk/sanitizers-asan.yaml?label=Asan) ![Tsan Status](https://img.shields.io/github/actions/workflow/status/IlRomanenko/MemHawk/sanitizers-tsan.yaml?label=Tsan) ![Clang-tidy Status](https://img.shields.io/github/actions/workflow/status/IlRomanenko/MemHawk/clang-tidy.yaml?label=Clang-tidy)

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

### As a Linked Library

MemHawk can also be linked into your application. In that case, it's recommended to list it as the very first dependency in your target's link libraries. This ensures that MemHawk's interception mechanisms are set up before any other libraries.

## License

MemHawk is licensed under the Apache-2.0 License. See the LICENSE file for details.
