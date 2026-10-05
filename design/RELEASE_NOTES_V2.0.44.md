# Release Notes v2.0.44

**Version:** 2.0.44  
**Date:** October 3, 2026

## Long-running dashboard reliability

- Extended the shared portfolio snapshot cache from 5 to 30 seconds to avoid repeatedly fetching holdings, positions, and news while the browser polls every 15 seconds.
- Added bounded server-side retry backoff after failed Upstox holdings requests: 30 seconds for transient errors and 5 minutes for expired or unauthorized tokens.
- If live refresh fails and local holdings are unavailable, serve the last successful snapshot with an explicit stale source, warning, and snapshot age rather than replacing it with an empty/error screen.
- A missing news feed no longer prevents a valid holdings snapshot from being returned. Overview continues to show holdings and identifies the news outage.
- Market news labels cached results and displays the stale-data warning when applicable.

## Compatibility and security

- Retained the existing authenticated API routes, server-side token handling, and local CSV fallback behavior.
- Stale portfolio values are identified as cached and are not labeled as current live data.
- Reduced upstream request volume without increasing browser polling frequency.

## Release transparency and tests

- Added a v2.0.44 summary to the post-login release popup.
- Added regression coverage for snapshot caching, retry behavior, news isolation, and stale-data UI labeling.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.44`.
