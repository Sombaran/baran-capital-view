# Release Notes v2.0.34

**Version:** 2.0.34  
**Date:** September 20, 2026

## Deeper analysis data consistency

- Fixed the contradiction where the page showed all signal categories as zero
  while all holdings were counted as hold/wait.
- Normalized transformer output such as `Is ok to hold` into the visible
  `Neutral news` category.
- Unknown model labels now fall back safely to `Neutral news` instead of being
  omitted from category counts.
- Summary counts and category stock lists now use the same canonical labels.

## Dashboard and security compatibility

- Preserved the responsive dashboard layout, navigation-loop fix, browser link
  hardening, Stock API validation, cache limits, and safe temporary files.
- No valid Stock API request, authentication flow, or existing portfolio data
  format was changed.

## Release transparency

- Added the 2.0.34 data-consistency summary to the post-login right-side popup.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.34`.
