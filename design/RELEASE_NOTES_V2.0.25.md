# Release Notes v2.0.25

**Version:** 2.0.25

## Global market news fix

- Uses the broad Upstox `global` news category instead of the invalid `all` category.
- Preserves the backend's actual HTTP, access-token, or network error in the tab.
- Shows an actionable message and Refresh path instead of the generic `Unable to load this view` panel.
- Safely supports both array and instrument-keyed news payloads.

The Summary Dashboard continues to reuse existing snapshots. Stock API credentials remain server-side, with existing HTTPS, trusted-host, retry, and secure-session protections unchanged.

The login release popup now summarizes both new dashboard views and their error-handling behavior.
