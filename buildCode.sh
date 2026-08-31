#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

build_dir="${BUILD_DIR:-build}"
build_system="${BUILD_SYSTEM:-cmake}"
rebuild=0

usage() {
  cat <<'EOF'
Usage: ./buildCode.sh [1|2] [--rebuild] [--cmake|--bazel]
  1 = CMake build
  2 = Bazel build
  --rebuild = remove stale build artifacts before building
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    1)
      build_system="cmake"
      ;;
    2)
      build_system="bazel"
      ;;
    --rebuild)
      rebuild=1
      ;;
    --cmake)
      build_system="cmake"
      ;;
    --bazel)
      build_system="bazel"
      ;;
    --build-system)
      shift
      case "${1:-}" in
        1|cmake)
          build_system="cmake"
          ;;
        2|bazel)
          build_system="bazel"
          ;;
        *)
          echo "Unsupported build system: ${1:-}"
          usage
          exit 2
          ;;
      esac
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1"
      usage
      exit 2
      ;;
  esac
  shift
done

if [[ "$build_system" == "bazel" ]]; then
  if ! command -v bazel >/dev/null 2>&1 && ! command -v bazelisk >/dev/null 2>&1; then
    echo "Bazel is not installed or not on PATH. Install Bazel or Bazelisk, or run: ./buildCode.sh 1 --rebuild" >&2
    exit 1
  fi

  bazel_bin="$(command -v bazel || command -v bazelisk)"

  if [[ "$rebuild" -eq 1 ]]; then
    "$bazel_bin" clean --expunge >/dev/null 2>&1 || true
  fi
  "$bazel_bin" build //:portfolio_health
  exit 0
fi

if [[ "$rebuild" -eq 1 ]]; then
  rm -rf "$build_dir"
fi

if ! conan profile show default >/dev/null 2>&1; then
  conan profile detect --force
fi

conan install . --output-folder="$build_dir/conan" --build=missing
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$build_dir/conan/build/Release/generators/conan_toolchain.cmake"
cmake --build "$build_dir" -j"${BUILD_JOBS:-$(nproc)}"