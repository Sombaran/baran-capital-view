# Release Notes v2.0.32

**Version:** 2.0.32  
**Date:** September 20, 2026

## Stock API and request hardening

- Rejected unsafe RSI periods before arithmetic to prevent overflow and
  out-of-bounds access.
- Validated and capped market quote and OHLC instrument-key batches before
  forwarding requests to Upstox.
- Replaced predictable PID-based temporary analysis paths with securely created
  temporary files.
- Bounded per-symbol analysis result and error caches to prevent unbounded
  memory growth from authenticated requests.

## Browser output safety

- Removed unsafe dynamic article links unless their scheme is `http` or
  `https`.
- Preserved `noopener noreferrer` on external links.
- Kept all credentials server-side and unchanged API authentication behavior.

## Tests and release transparency

- Added regression coverage for zero, oversized, and overflow-sized RSI periods.
- Added a 2.0.32 security summary to the right-side post-login popup.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.32`.

## Compatibility

Valid portfolio workflows and existing broker API calls remain unchanged.
Malformed or oversized requests now fail safely before external API access.
