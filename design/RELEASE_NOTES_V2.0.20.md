# Release Notes v2.0.20

**Version:** 2.0.20

## Fix

Upstox HTTP 401 means the access token is invalid or expired. The Overview now keeps the holdings list available but displays `Unavailable - renew token` for Market value instead of presenting the local CSV amount as live data.

The backend logs the failed live request and the fallback source. No bearer token is exposed in the browser or logs.

## Verification

The C++ regression suite passes after the change.
