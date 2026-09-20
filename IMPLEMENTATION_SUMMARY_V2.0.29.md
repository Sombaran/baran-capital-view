# Baran Capital View v2.0.29 - Implementation Summary

## Overview

Version 2.0.29 exposes repeatable C++ coverage reports, aligns release metadata,
and updates the post-login release summary. The dashboard also receives a
shared post-render optimization pass so the same rendering safeguards apply to
every page.

## Coverage reports

- `PORTFOLIO_HEALTH_ENABLE_COVERAGE=ON` instruments the core library, CLI, and
  C++ test target with GCC/Clang coverage flags.
- `coverage` runs CTest, writes the lcov data file, prints the coverage
  percentage, and generates HTML.
- `coverage-gcov` runs CTest and writes raw gcov output.
- `nlohmann_json/3.11.3` remains resolved through `conanfile.py`; lcov is a
  system reporting prerequisite.

## Web UI optimization

`dashboardUiOptimization()` is injected after login and observes the shared
`#view` container used by Overview, News, Alerts, Deeper analysis,
Fundamentals, Positions, JSON, Data health, Config, Summary Dashboard, and
Global market news. It:

- marks the status region as an ARIA live region;
- marks completed renders as no longer busy;
- enables lazy loading and asynchronous decoding for images; and
- applies stable table wrappers after dynamic renders.

## Release summary

The version-aware right-side popup now summarizes the 2.0.29 coverage reports,
shared dashboard improvements, metadata alignment, and regression test update.
It continues to use browser-local version state and does not make an extra API
request.

## Tests

Added a C++ regression test for canonical symbol normalization, which protects
the dashboard's symbol matching behavior across whitespace and case variants.
