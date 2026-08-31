# Release Notes v2.0.13

**Version**: 2.0.13

## Summary

Version 2.0.13 hardens dashboard JSON handling after transient or malformed holdings responses caused the UI to show `Invalid JSON response from holdings`.

## Fixes

- Browser API parsing now reads and validates response text safely.
- Holdings, positions, and news use shape-correct fallback objects when requests fail or return empty/non-JSON content.
- Server response validation returns valid JSON for empty or malformed snapshot data.
- Existing cache, refresh, filter, sorting, and tab behavior remains available.

## Security

- No credentials are sent to the browser or included in fallback messages.
- Sorting and filtering remain client-side and do not add Stock API requests.
- Existing HTTPS, host validation, session, and escaping controls are unchanged.

## Validation

- CMake build and C++ regression tests must pass before deployment.
- The UI should display an unavailable/empty state rather than a JSON parse error during temporary API failures.
