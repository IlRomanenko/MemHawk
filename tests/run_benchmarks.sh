#!/bin/bash

RED='\033[0;31m'
NC='\033[0m' # No Color

print_colored()
{
    echo -e "${RED}$1${NC}"
}

print_colored "System malloc"
./bench_allocs
echo

print_colored "libmemhawk.so + dwarf unwinding"
LD_PRELOAD=./libmemhawk.so ./bench_allocs
echo

print_colored "libmemhawk.so + frame unwinding"
MEMHAWK_OPTS=absl_stacktrace=1 LD_PRELOAD=./libmemhawk.so ./bench_allocs
echo

print_colored "libmemhawk.so + frame unwinding + zero recursion collapsing"
MEMHAWK_OPTS=absl_stacktrace=1:collapse_recursion_depth=0 LD_PRELOAD=./libmemhawk.so ./bench_allocs
echo

print_colored "tcmalloc.so + heap profiling"
PERFTOOLS_VERBOSE=-1 HEAPPROFILE=/tmp/tcmalloc_heap_profile.hprof  LD_PRELOAD=/usr/lib/libtcmalloc_and_profiler.so ./bench_allocs
echo

print_colored "heaptrack.so"
LD_PRELOAD=/usr/lib/heaptrack/libheaptrack_preload.so ./bench_allocs
echo

print_colored "jemalloc.so + heap sampling"
MALLOC_CONF="prof:true,prof_prefix:/tmp/jeprof.out,prof_active:true" LD_PRELOAD=/usr/lib/libjemalloc.so ./bench_allocs
echo
