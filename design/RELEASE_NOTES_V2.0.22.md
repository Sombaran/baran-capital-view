# Release Notes v2.0.22

**Version:** 2.0.22

## Fix

The browser dashboard now recovers after the computer sleeps or the tab is suspended. When the page becomes visible again, it clears interrupted browser data and refreshes the active view.

This prevents Deeper analysis from remaining on `Unable to load this view` after a wake-related request failure. The server-side analysis worker can start a fresh run on the next request, while the existing bounded HTTP retries handle temporary Upstox network failures.

## Security

The recovery path sends only the existing authenticated API requests. It does not expose, persist, or log the Upstox bearer token. HTTPS, trusted-host validation, and secure session cookies remain unchanged.

## Verification

The C++ regression target builds successfully. Existing C++ and Python regression suites should pass before deployment.
