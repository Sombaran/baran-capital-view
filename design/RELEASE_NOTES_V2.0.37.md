# Release Notes v2.0.37

**Version:** 2.0.37  
**Date:** October 2, 2026

## Market holidays and special sessions

- Added the authenticated Upstox `/v2/market/holidays` client request.
- Added a session-protected `/api/market-holidays` route with strict response validation and a 12-hour server-side cache.
- The IST market-status badge identifies NSE trading holidays and uses the published NSE open/close timestamps for special sessions.
- When the holiday feed is unavailable, the badge retains the existing weekday and normal market-hours calculation; cached valid data remains available during transient upstream failures.

## Security and compatibility

- Upstox credentials remain server-side; the browser receives only the validated holiday calendar through the existing authenticated session boundary.
- No existing broker routes, holdings data, or login behavior changed.

## Release transparency and tests

- Added a v2.0.37 summary to the right-side popup shown after login.
- Added response-shape tests for valid, empty, malformed, and invalid-date holiday payloads.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.37`.
