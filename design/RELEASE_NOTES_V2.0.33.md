# Release Notes v2.0.33

**Version:** 2.0.33  
**Date:** September 20, 2026

## Dashboard navigation stability

- Fixed a child-list observer loop caused by repeatedly appending the existing
  RSI panel to the Deeper analysis view.
- Tab navigation now remains responsive while Deeper analysis and its technical
  tool render.
- RSI placement is idempotent and only changes the DOM when its position is
  incorrect.
- Existing refresh, filtering, sorting, retry, and cache behavior is retained.

## Security and compatibility

- Preserved the request validation, safe temporary-file handling, bounded
  analysis caches, and HTTP(S)-only dynamic links from v2.0.32.
- No valid Stock API request or authentication behavior was changed.

## Release transparency

- Added a 2.0.33 navigation stability summary to the post-login right-side
  popup.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.33`.
