# Release Notes v2.0.48

**Version:** 2.0.48  
**Date:** October 3, 2026

## Long-running data availability

- Preserve the last successful live snapshot when an Upstox holdings refresh fails, rather than replacing fresher data with the static CSV fallback.
- Resolve the holdings CSV and saved-news files from the project directory, so offline fallback does not depend on the server's current working directory.
- Use the local CSV as a bootstrap fallback when no successful live snapshot exists.
- Clearly label local fallback data and provide token renewal/restart guidance when Upstox reports an expired or unauthorized token.

## Compatibility and security

- Keep automatic snapshot caching and bounded retry backoff in place.
- Keep API credentials server-side; fallback changes do not add unauthenticated routes or expose broker responses.

## Release transparency and tests

- Added a v2.0.48 fix summary to the post-login release popup.
- Added regressions verifying last-known-good snapshot precedence and project-root file resolution.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.48`.
