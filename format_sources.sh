#!/bin/bash
FOLDERS="source tests"

find $FOLDERS -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
