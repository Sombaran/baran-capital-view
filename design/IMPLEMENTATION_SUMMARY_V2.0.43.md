# Baran Capital View v2.0.43 - Implementation Summary

## Problem

The News and Global market news tabs showed the same Upstox holdings-category information. The second UI tab called the legacy route directly and did not provide broader market coverage.

## Fix

- Consolidated navigation into one **Market news** tab using the existing authenticated portfolio news snapshot.
- Removed the duplicate UI entry and its redundant browser request while retaining `/api/global-news` for compatibility.
- Improved shared offscreen rendering and honored reduced-motion preferences across dashboard pages.
- Added the v2.0.43 fix summary to the post-login release popup.
- Preserved the current session, server-side Stock API credentials, and news endpoint security boundaries.

## Validation

- Added a regression test for the consolidated navigation and legacy backend route.
- Run the focused Python and C++ test suites and validate the CMake build.
