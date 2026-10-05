# Release Notes v2.0.47

**Version:** 2.0.47  
**Date:** October 3, 2026

## Fundamentals and API routing

- Preserve query strings on query-based API routes, fixing Fundamentals requests incorrectly returning `not found`.
- Keep the `refresh=1` query limited to the existing holdings, positions, and news snapshot routes.
- Constrain the Fundamentals summary table to the popup width and allow long labels and reasons to wrap on desktop and mobile.

## Compatibility and security

- No broker endpoint or API credential handling changed; fundamentals remain authenticated and credentials remain server-side.
- Route matching continues to validate symbols before starting analysis or making upstream requests.

## Release transparency and tests

- Added a v2.0.47 fix summary to the post-login release popup.
- Added regressions for query-preserving route behavior and responsive fundamentals popup layout.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.47`.
