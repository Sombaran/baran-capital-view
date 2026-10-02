# Release Notes v2.0.41

**Version:** 2.0.41  
**Date:** October 2, 2026

## Som Baran Portfolio download

- Added a Download subview under Operations for exporting the current holdings as CSV.
- Reuses the dashboard's authenticated `/api/holdings` payload and cache; no new API route or broker request is introduced when the data is already loaded.
- Exports symbol, company, exchange, quantity, average/last price, and market value.
- Escapes CSV fields, neutralizes formula-leading text, uses a fixed filename prefix, and revokes the temporary object URL after download.
- Indicates whether the export came from the live Upstox portfolio or local fallback.

## Release transparency and tests

- Added a v2.0.41 summary to the right-side popup shown after login.
- Added regression coverage for the Download subview and export safety markers.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.41`.
