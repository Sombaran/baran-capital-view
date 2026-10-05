# baran-capital-view v2.0.49 - Implementation Summary

## Problem

The local HTTP server handled one connection at a time. A portfolio snapshot refresh could make several sequential Upstox requests, each with a bounded network timeout, preventing the server from accepting or serving other browser requests during that period. Concurrent requests also waited for the snapshot refresh mutex.

## Fix

- Process requests on detached worker threads with an explicit maximum of eight active connections.
- Return HTTP 503 when the bounded worker capacity is exhausted.
- Use `try_to_lock` for snapshot refresh ownership. Concurrent readers receive a labeled last-known-good snapshot, or local bootstrap data when no snapshot exists, rather than waiting on the upstream call.
- Keep already-rendered data visible with an inline warning when a background refresh fails.
- Preserve the existing authentication, cache, backoff, and manual-refresh controls.
- Add the fix summary to the versioned post-login popup.

## Validation

- Add regression checks for bounded workers, overload rejection, and nonblocking snapshot reads.
- Build C++ targets and run CTest and the complete Python test suite.
- Verify executable version and patch formatting.
