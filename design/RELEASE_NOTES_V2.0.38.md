# Release Notes v2.0.38

**Version:** 2.0.38  
**Date:** October 2, 2026

## Live data reliability

- Fixed an API snapshot error response that lacked an explicit error status and could be interpreted by the browser as an empty, valid portfolio.
- Failed or malformed holdings responses now render an actionable unavailable/authentication state instead of zero holdings and zero P&L.
- Automatic retries back off for 30 seconds on transient failures and five minutes on 401/403 responses. Manual Refresh clears the backoff.
- Snapshot error details remain in server logs; the browser receives a safe status message.

## Compatibility and security

- Existing successful holdings, positions, news, and market-holiday response contracts are unchanged.
- Access tokens remain server-side. This change does not attempt to renew credentials automatically.

## Release transparency and tests

- Added the v2.0.38 summary to the right-side popup shown after login.
- Existing C++ and Python regression suites remain the validation gates.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.38`.
