# Release Notes v2.0.36

**Version:** 2.0.36  
**Date:** October 1, 2026

## Consistent serial headers

- Replaced `#` with `S.No` across Overview, Alerts, Fundamentals, and analysis tables.
- Kept serial columns out of sortable data fields.
- Made serial-number insertion idempotent so refreshes and sorts do not duplicate the column.

## Compatibility and security

- No API routes, authentication behavior, broker credentials, or stock-data request handling changed.
- The existing responsive dashboard improvements and live Day P&L correction remain included.

## Release transparency

- Added the v2.0.36 summary to the right-side popup shown after login.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.36`.
