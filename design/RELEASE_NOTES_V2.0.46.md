# Release Notes v2.0.46

**Version:** 2.0.46  
**Date:** October 3, 2026

## Dashboard refresh reliability

- Fixed Refresh and Retry now so they can initiate a live snapshot refresh even when the server is applying automatic retry backoff after an upstream failure.
- Added a server-side 10-second minimum interval between forced manual refresh attempts to avoid excessive Stock API requests.
- Show refresh progress in the toolbar and disable duplicate clicks until the request finishes.
- Preserve the existing automatic snapshot cache, bounded transient/authentication backoff, and last-known-good data behavior.

## Compatibility and security

- Refresh remains behind the existing authenticated web session; API credentials remain server-side.
- The manual refresh flag is accepted only on existing snapshot-backed API routes and does not change public API access.

## Release transparency and tests

- Added a v2.0.46 fix summary to the post-login release popup.
- Added regression checks for the client manual-refresh request, server cooldown bypass/rate limit, and documented release wiring.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.46`.
