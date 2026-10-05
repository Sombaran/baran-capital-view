# Release Notes v2.0.50

**Version:** 2.0.50  
**Date:** October 5, 2026

## Running the current application build

- Fix `run.sh` reusing an existing executable after source changes by rebuilding when project source files or build metadata are newer than `build/portfolio_health`.
- Use the existing incremental CMake/Bazel build path on ordinary startup; no clean rebuild is performed unless `--rebuild` is explicitly requested.
- Preserve command-line arguments after an optional `--rebuild`.
- Keep normal web log capture and secure credential precedence behavior unchanged.

## Release transparency and tests

- Added a v2.0.50 fix summary to the post-login popup.
- Added launcher tests for stale-source rebuild detection and `--rebuild` argument handling.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.50`.
