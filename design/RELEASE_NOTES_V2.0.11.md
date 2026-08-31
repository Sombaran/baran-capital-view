# Release Notes v2.0.11

**Version**: 2.0.11

## Summary

Version 2.0.11 adds optimized, accessible arrow sorting to the Overview, Alerts, Deeper analysis, and Fundamentals tables. Sorting operates on rows already loaded in the browser and does not add Stock API requests or expose credentials.

## UI Changes

- Added up/down sorting arrows to sortable table headers.
- Added numeric-aware sorting for quantities, prices, P&L, confidence, and other numeric values.
- Added case-insensitive natural text sorting for company and symbol fields.
- Preserved serial numbers after rows are reordered.
- Added keyboard-focusable sort controls with accessible labels.
- Kept existing Overview confidence ordering and search, refresh, hover, and detail workflows intact.

## Security

- Sorting is client-side only and does not send user-controlled sort parameters to the Stock API.
- Existing server-side API access, HTTPS restrictions, session handling, and escaping remain unchanged.
- Table labels and values are escaped before being inserted into sort controls.

## Validation

- Clean CMake build completed successfully.
- Existing C++ regression tests remain passing.
- Bazel dependency configuration remains independent of the browser sorting behavior.

## Files

- `src/WebServer.cpp`: shared sortable table behavior, styling, and release popup summary.
- `CMakeLists.txt`, `conanfile.py`, `MODULE.bazel`: version `2.0.11`.
- `README.md`, `design/README.md`: current release and feature documentation.
