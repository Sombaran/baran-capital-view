# Release Notes v2.0.15

**Version**: 2.0.15

## Summary

Version 2.0.15 improves the Alerts page for faster interpretation of portfolio news decisions.

## Alerts Improvements

- Replaced the generic `Should I add more?` heading with `Portfolio news review`.
- Removed the duplicate `Confidence order` control from Alerts.
- Added hover and keyboard-focus stock lists to `Consider adding`, `Risk review`, and `Hold / wait` metrics.
- Stock lists are derived from the displayed alert rows and remain consistent with the table.
- Existing search, refresh, sorting, article links, and alert calculations remain unchanged.

## Security

- No new Stock API requests are made for hover or focus behavior.
- Stock names are read from already-rendered, escaped table content.
- Credentials and upstream API payloads remain server-side.

## Validation

- CMake and Bazel regression workflows remain available.
- Existing C++ and Python regression tests should pass before deployment.
