# Release Notes v2.0.17

**Version:** 2.0.17

## Summary

This patch fixes the recurring empty News view and incorrect Overview market value while preserving the server-side Stock API security boundary.

## Fixes

- Saved portfolio news remains visible when live broker news is unavailable.
- News filtering falls back safely when saved articles use identifiers that do not match the current holdings snapshot.
- Local holdings now retain the CSV `Current Value` column instead of silently dropping it.
- Overview market value continues to prefer broker-reported `current_value` or `market_value`, then valid reported exposure, then a price-times-quantity fallback.
- Browser status reports stale or expired access tokens without exposing credentials.
- Backend logs identify HTTP 401/403 and authentication failures so the token can be renewed.
- The post-login right-side release popup now documents these actual fixes.

## Security

Stock API requests remain server-side only. Credentials are not rendered into browser responses, and existing HTTPS, trusted-host, and HttpOnly SameSite session protections remain unchanged.

## Verification

The C++ regression target builds successfully and all 24 tests pass with CTest.
