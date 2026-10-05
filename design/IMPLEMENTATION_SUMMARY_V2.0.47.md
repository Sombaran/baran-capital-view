# baran-capital-view v2.0.47 - Implementation Summary

## Problem

The manual-refresh request handling removed query strings globally. Several existing API routes, including `/api/fundamentals?symbol=...`, rely on query strings for route selection and parameter parsing, so those requests fell through to `not found`. The Fundamentals popup's four-column summary also exceeded its available width.

## Fix

- Preserve query strings for existing query-routed APIs.
- Strip `refresh=1` only for the allowlisted holdings, positions, and news snapshot endpoints.
- Apply fixed table layout, wrapping, and responsive cell sizing to the Fundamentals popup summary.
- Add the fix summary to the versioned post-login popup.

## Validation

- Add regression checks for all query-based route families and Fundamentals popup overflow.
- Build C++ targets and run CTest and the complete Python suite.
- Verify executable version and patch formatting.
