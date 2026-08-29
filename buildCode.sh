#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

build_dir="${BUILD_DIR:-build}"
if [[ "${1:-}" == "--rebuild" ]]; then
	rm -rf "$build_dir"
fi

if ! conan profile show default >/dev/null 2>&1; then
	conan profile detect --force
fi

conan install . --output-folder="$build_dir/conan" --build=missing
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE="$build_dir/conan/build/Release/generators/conan_toolchain.cmake"
cmake --build "$build_dir" -j"${BUILD_JOBS:-$(nproc)}"