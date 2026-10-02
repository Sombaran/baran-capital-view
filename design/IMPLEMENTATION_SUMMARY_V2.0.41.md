# Baran Capital View v2.0.41 - Implementation Summary

## Feature

Users need a local copy of the Som Baran Portfolio stock list without adding another endpoint or widening the Stock API surface.

## Implementation

- Added an Operations > Download subview.
- Exported the current authenticated holdings payload as CSV in the browser, reusing the dashboard cache where available.
- Quoted CSV cells, escaped embedded quotes, and prefixed formula-like untrusted text to prevent spreadsheet formula injection.
- Used a constant filename prefix and revoked the generated object URL after download.
- Added a post-login popup summary and aligned release metadata on `2.0.41`.

## Validation

- Added a regression check for the Download subview, authenticated holdings source, CSV escaping, formula protection, and object URL cleanup.
- Existing C++ and Python test suites remain the release gates.
