# MemHawk

MemHawk is a high-performance C++ library designed for summarizing memory allocations extremely fast—without locking a global mutex. It helps you quickly identify the top-10 memory allocation traces that are consuming the most memory — perfect for tracking down leaks in your application.

## Features

* Statically Linked & Zero External Dependencies:
MemHawk is self-contained. It’s built for static linking, meaning you won’t have to worry about external libraries or dependency conflicts.

* Extremely Fast with Minimal Global Locks:
MemHawk is designed to be incredibly efficient. Its minimal-lock design ensures that even in multi-threaded environments, the overhead is kept to a bare minimum.

* Top-10 Trace Summaries: Instantly identify the top-10 allocation traces by memory consumption. This focused insight makes it easier to locate and resolve memory leaks.

* CMake Integration: The library is designed using modern CMake practices, making it straightforward to integrate into your existing projects.

## Getting Started

### Prerequisites

A C++ compiler with C++17 (or later) support.

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

### As a Linked Library

MemHawk can also be linked into your application. In that case, it's recommended to list it as the very first dependency in your target's link libraries. This ensures that MemHawk's interception mechanisms are set up before any other libraries.

## License

MemHawk is licensed under the Apache-2.0 License. See the LICENSE file for details.
