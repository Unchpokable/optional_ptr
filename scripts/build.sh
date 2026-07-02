#!/usr/bin/env bash
# Configure, build and run tests with Ninja.
# Usage: ./build.sh [Debug|Release]
set -euo pipefail

config="${1:-Debug}"

if [[ "${config}" != "Debug" && "${config}" != "Release" ]]; then
    echo "Usage: $(basename "$0") [Debug|Release]" >&2
    exit 1
fi

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root_dir}/build/${config}"

cmake -S "${root_dir}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE="${config}"
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure -C "${config}"
