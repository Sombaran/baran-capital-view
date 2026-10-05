# Release Notes v2.0.45

**Version:** 2.0.45  
**Date:** October 3, 2026

## Deeper analysis consistency

- Analyze each normalized trading symbol once, even when the broker holdings response contains repeated positions.
- Aggregate market value across unique instrument positions for a repeated symbol.
- Read Python recommendation results as structured JSON instead of manually parsing CSV rows.
- Include the normalized category with each result row and keep the symbol category lists unique.
- Derive the dashboard's category counts and category symbol lists from the same unique result rows shown in the evidence table.
- Remove the redundant browser fetch observer that maintained a second, independently assembled set of category data.
- Keep the fresh Python recommendation separate from the saved portfolio signal so a saved-news fallback is not presented as a Python result.

## Compatibility and security

- Preserve existing authenticated endpoints, session controls, server-side API credentials, and advisory-only signal behavior.
- Continue escaping dynamic symbol, category, and analysis content before adding it to the dashboard.

## Release transparency and tests

- Added a v2.0.45 fix summary to the post-login release popup.
- Added C++ coverage for symbol normalization/deduplication and Python coverage for structured result parsing and UI consistency.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.45`.
