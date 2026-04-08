#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-Release}"

/usr/local/bin/cmake --build "$BUILD_DIR" -j4
ctest --test-dir "$BUILD_DIR" --output-on-failure -R correctness_harness
