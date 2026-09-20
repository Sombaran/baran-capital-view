# Release Notes v2.0.30

**Version:** 2.0.30  
**Date:** September 20, 2026

## Deeper analysis redesign

- Replaced the explanatory workflow block with a focused review dashboard.
- Added summary counts for positive review, risk review, hold/wait, and total
  compared stocks.
- Added signal-category controls that reveal the associated stocks.
- Combined saved news, fresh NLP recommendation, review action, and rationale
  in one evidence table.
- Kept the RSI tool available as a secondary technical check instead of making
  it the primary content of the page.

## Dashboard usability

- Applied consistent responsive rendering and table wrapping across all pages.
- Improved status announcements for assistive technology.
- Preserved existing filtering, sorting, refresh, retry, and cache behavior.
- Kept the UI compatible with desktop and narrow mobile layouts.

## Unit tests

- Strengthened the Deeper analysis category-order regression test so the UI and
  backend continue to share the same five canonical categories.

## Release popup and versioning

- Updated the post-login right-side popup with the 2.0.30 fix summary.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.30`.
- The popup remains version-aware and appears once for the new release version.

## Compatibility

The release keeps the existing Deeper analysis API response, configuration
files, portfolio files, sessions, and coverage commands backward compatible.
