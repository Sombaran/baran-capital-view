# baran-capital-view v2.0.46 - Implementation Summary

## Problem

The dashboard's Refresh control cleared client-side retry state, but the server still applied its upstream snapshot cooldown. When data was unavailable, clicking Refresh or Retry now therefore did not necessarily make a new Stock API request.

## Fix

- Added a manual refresh flag for the existing authenticated holdings, positions, and news snapshot endpoints.
- Manual refresh bypasses the cached snapshot and automatic retry cooldown for one refresh attempt.
- Added a server-side 10-second minimum interval for forced manual refreshes.
- Show refresh progress and disable the toolbar button while the current view reloads.
- Preserve automatic polling, cache lifetime, error handling, and last-known-good snapshot behavior.
- Add the fix summary to the versioned post-login popup.

## Validation

- Add Python regression checks for refresh UI state and manual request behavior.
- Build C++ targets and run CTest and the complete Python test suite.
- Verify executable version and patch formatting.
