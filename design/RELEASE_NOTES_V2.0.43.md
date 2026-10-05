# Release Notes v2.0.43

**Version:** 2.0.43  
**Date:** October 2, 2026

## Unified Market news

- Replaced the duplicate News and Global market news navigation entries with one **Market news** tab.
- The view uses the existing authenticated `/api/news` portfolio snapshot, which is matched to configured holdings and sorted newest first.
- Removed the redundant browser-facing tab and request while preserving the legacy `/api/global-news` route for compatibility.
- Kept Upstox credentials server-side and preserved the existing authenticated news snapshot, session behavior, and error handling.

## Dashboard optimization

- Added deferred offscreen rendering for dashboard cards and articles to reduce work on long pages.
- Respect the browser's reduced-motion preference across dashboard transitions and animations.
- Retain the existing responsive layout and horizontal table scrolling across all dashboard pages.

## Release transparency and tests

- Added a v2.0.43 summary to the versioned popup displayed after successful login.
- Documented the consolidated tab, feed scope, API behavior, and security compatibility.
- Added a regression test for the single-tab UI and preserved backend route.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.43`.
