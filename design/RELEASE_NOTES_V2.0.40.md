# Release Notes v2.0.40

**Version:** 2.0.40  
**Date:** October 2, 2026

## Operations workspace

- Combined the Positions, Data health, and Config top-level tabs into one Operations tab.
- Added keyboard-accessible in-page subviews for Positions, Data health, and Configuration.
- Reused the existing view renderers and authenticated API routes; no new broker requests or credential paths were added.
- Skipped automatic live-data polling while Configuration is selected.

## Compatibility and security

- Existing positions, diagnostics, and configuration content remains available.
- Existing login/session behavior and Stock API boundaries are unchanged.

## Release transparency and tests

- Added a v2.0.40 summary to the right-side popup shown after login.
- Added a regression check for the three Operations subviews.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.40`.
