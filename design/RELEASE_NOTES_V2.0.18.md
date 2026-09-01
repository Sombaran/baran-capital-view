# Release Notes v2.0.18

**Version:** 2.0.18

## Summary

This patch improves Deeper analysis accuracy and keeps the web dashboard reliable and responsive across its views.

## Analysis improvements

- Deeper analysis now runs `stock_alert_nlp.py` without `--fast`, enabling the transformer sentiment model when it is installed and available.
- The existing keyword-based classifier remains the fallback when model loading or external news retrieval fails.
- Analysis continues in the server background worker, so the browser receives a running state instead of blocking the dashboard.
- Saved portfolio news and fresh Python recommendations remain compared per live holding.

## Web UI and security

- Shared dashboard refresh behavior continues to use bounded server snapshots and browser `no-store` requests.
- News and Overview retain their identifier and valuation fallback protections from v2.0.17.
- Stock API credentials remain server-side; existing HTTPS, trusted-host, and HttpOnly SameSite session protections are unchanged.
- The post-login release popup summarizes the accuracy-first analysis change and data reliability fixes.

## Verification

The C++ regression target and Python regression suite should be run before deployment:

```bash
cmake --build build --target portfolio_health_tests -- -j2
ctest --test-dir build --output-on-failure --tests-regex portfolio_health_tests
pytest -q tests/python
```
