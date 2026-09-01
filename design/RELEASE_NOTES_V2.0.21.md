# Release Notes v2.0.21

**Version:** 2.0.21

## Fix

Long-running dashboard sessions now retry transient Upstox DNS-resolution, connection, and timeout failures before using local fallback data. Each request uses bounded retry attempts with short backoff and refreshed DNS cache lifetime.

HTTP authentication failures such as 401 and 403 are not retried as network errors. They remain visible so the access token can be renewed.

No bearer token is written to diagnostics. HTTPS, trusted-host validation, and secure session handling remain unchanged.

## Verification

C++ and Python regression suites pass after the change.
