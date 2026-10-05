# baran-capital-view v2.0.48 - Implementation Summary

## Problem

The long-running UI could become unavailable when a live holdings request failed and the server could not resolve the relative local holdings path from its launch directory. When a previous live snapshot existed, the fallback order also preferred the older static CSV over that more recent snapshot.

## Fix

- Return a clearly marked last-known-good snapshot before loading the static CSV fallback.
- Resolve holdings and saved-news paths against the project root, independent of the process working directory.
- Continue to use the local CSV when no successful snapshot exists; explain token renewal when authentication has expired.
- Add the fix summary to the versioned post-login popup.

## Validation

- Add regression checks for fallback ordering, root-based config paths, and popup wiring.
- Build C++ targets and run CTest and the complete Python test suite.
- Verify executable version and patch formatting.
