# Release Notes v2.0.29

**Version:** 2.0.29  
**Date:** September 20, 2026

## Coverage reporting

- Added opt-in GCC/Clang coverage instrumentation through CMake.
- Added the `coverage` target for CTest execution, lcov percentage summaries,
  and an HTML report at `build-coverage/coverage/html/index.html`.
- Added the `coverage-gcov` target for raw per-source `.gcov` files.
- Documented the Conan dependency and the system `lcov` prerequisite.

## Dashboard optimization

- Added a shared post-render optimization pass across all dashboard pages.
- Marked the live status region for assistive technology updates.
- Added lazy image loading and asynchronous image decoding for rendered views.
- Added stable table wrapping after each view render to reduce layout work and
  preserve readable tables on narrow screens.
- Kept the existing browser-local cache and responsive navigation behavior.

## Version and release transparency

- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.29`.
- Updated the right-side post-login release popup with the coverage, UI, and
  regression-test summary.
- The popup remains version-aware and is shown once per release version.

## Unit tests

- Added a regression test confirming that dashboard symbol keys are trimmed,
  uppercased, and normalized consistently before matching API data.

## Compatibility

All changes are backward compatible with existing portfolio files, saved news,
configuration files, API credentials, and browser sessions.
