#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

build_dir="${BUILD_DIR:-build}"
build_system="${BUILD_SYSTEM:-cmake}"
rebuild=0
skip_tests=0
coverage=0
coverage_target="coverage"
coverage_build_dir_set=0

usage() {
  cat <<'EOF'
Usage: ./buildCode.sh [1|2] [--rebuild] [--skip-tests] [--coverage|--coverage-gcov] [--cmake|--bazel]
  1 = CMake build
  2 = Bazel build
  --rebuild = remove stale build artifacts before building
  --skip-tests = build only; do not run regression tests
  --coverage = instrument a Debug build and generate the lcov HTML report
  --coverage-gcov = instrument a Debug build and generate raw gcov reports
  --coverage-build-dir <dir> = override the default build-coverage directory
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
    --skip-tests)
      skip_tests=1
      ;;
    --coverage)
      coverage=1
      coverage_target="coverage"
      build_system="cmake"
      ;;
    --coverage-gcov)
      coverage=1
      coverage_target="coverage-gcov"
      build_system="cmake"
      ;;
    --coverage-build-dir)
      shift
      build_dir="${1:-}"
      if [[ -z "$build_dir" ]]; then
        echo "--coverage-build-dir requires a directory" >&2
        usage
        exit 2
      fi
      coverage_build_dir_set=1
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

if [[ "$coverage" -eq 1 && "$coverage_build_dir_set" -eq 0 ]]; then
  build_dir="${COVERAGE_BUILD_DIR:-build-coverage}"
fi

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
  if [[ "$skip_tests" -eq 0 ]]; then
    "$bazel_bin" test //:portfolio_health_tests --test_output=errors
    pytest -q
  fi
  exit 0
fi

if [[ "$rebuild" -eq 1 ]]; then
  rm -rf "$build_dir"
fi

if ! conan profile show default >/dev/null 2>&1; then
  conan profile detect --force
fi

cmake_build_type="Release"
if [[ "$coverage" -eq 1 ]]; then
  cmake_build_type="Debug"
fi

conan install . --output-folder="$build_dir/conan" --build=missing \
  -s build_type="$cmake_build_type"
toolchain_dir="$build_dir/conan/build/$cmake_build_type/generators"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$cmake_build_type" \
  -DCMAKE_TOOLCHAIN_FILE="$toolchain_dir/conan_toolchain.cmake" \
  -DPORTFOLIO_HEALTH_ENABLE_COVERAGE="$([[ "$coverage" -eq 1 ]] && echo ON || echo OFF)"

if [[ "$coverage" -eq 1 ]]; then
  if [[ "$coverage_target" == "coverage" ]] && {
    ! command -v lcov >/dev/null 2>&1 || ! command -v genhtml >/dev/null 2>&1;
  }; then
    echo "Coverage HTML requires lcov and genhtml. Install lcov, then retry." >&2
    exit 1
  fi
  cmake --build "$build_dir" --target "$coverage_target" -j"${BUILD_JOBS:-$(nproc)}"
else
  cmake --build "$build_dir" -j"${BUILD_JOBS:-$(nproc)}"
fi

if [[ "$skip_tests" -eq 0 ]]; then
  ctest --test-dir "$build_dir" --output-on-failure
  pytest -q
fi