# Release Notes v2.0.31

**Version:** 2.0.31  
**Date:** September 20, 2026

## Deeper analysis visual correction

- Added a responsive visual system for the review summary cards, signal
  categories, evidence table, and analysis status state.
- Prevented labels and values from collapsing into adjacent text.
- Moved the RSI tool below the primary review content so it remains available
  without competing with the main result.
- Kept the existing API, category order, filtering, sorting, and retry behavior.

## Dashboard polish

- Shared dashboard styling now handles the new analysis components on desktop,
  tablet, and mobile widths.
- Evidence tables remain horizontally readable on narrow screens.
- Status and source information are visually separated from the action counts.

## Release popup and versioning

- Added a 2.0.31 visual-fix summary to the right-side popup shown after login.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.31`.

## Compatibility

The change is backward compatible with existing configuration, portfolio files,
API responses, sessions, and coverage commands.
