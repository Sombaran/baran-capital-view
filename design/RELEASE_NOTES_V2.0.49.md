# Release Notes v2.0.49

**Version:** 2.0.49  
**Date:** October 5, 2026

## Long-running dashboard stability

- Handle up to eight local browser connections concurrently so a slow Upstox refresh does not block static UI or other dashboard requests.
- Return the last successful snapshot to readers while another request is refreshing it.
- If no live snapshot exists yet, use the project-root local holdings and saved-news files as a bootstrap view while refresh proceeds.
- Keep the last rendered dashboard view visible with a warning when a background refresh fails, rather than replacing the page with an unavailable-data panel.
- Retain the snapshot cache, retry cooldown, stale-data labels, and manual-refresh rate limit.

## Compatibility and security

- Continue binding the web server to loopback and requiring the existing authenticated session for data endpoints.
- Bound worker concurrency; reject excess concurrent connections with HTTP 503 rather than creating unlimited threads.
- Keep stock API credentials and upstream requests on the server.

## Release transparency and tests

- Added a v2.0.49 fix summary to the post-login release popup.
- Added regression coverage for bounded concurrent request handling and non-blocking snapshot reads.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.49`.
