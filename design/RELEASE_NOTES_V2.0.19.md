# Release Notes v2.0.19

**Version:** 2.0.19

## Summary

This patch fixes Overview market value remaining equal to the local CSV snapshot when Upstox does not return an explicit current-value field.

## Fixes

- Long-term holdings now derive market value from live `last_price * quantity * multiplier` when `current_value` and `market_value` are absent.
- Explicit broker-reported current valuation remains preferred when provided.
- The local CSV remains a fallback only when live holdings cannot be fetched.
- Backend logs identify when local fallback data is being served.
- The fallback payload includes a warning so the source is visible in the JSON/Data health view.
- Existing HTTPS, trusted-host, token-isolation, and secure-session behavior remains unchanged.

## Verification

- C++ regression tests: all passed.
- Python regression tests: all passed.
