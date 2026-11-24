#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

FOLDERS=(source tests)

find "${FOLDERS[@]}" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \) -print0 |
    xargs -0 -r clang-format -i
