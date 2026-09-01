# Release Notes v2.0.24

**Version:** 2.0.24

## New dashboard tabs

- **Global market news** requests broad market news from the authenticated Upstox feed using the existing server-side client. It is not limited to configured holdings.
- **Summary Dashboard** combines holdings count, live valuation state, portfolio news coverage, and positive/risk review signals from existing dashboard snapshots.

## Security and performance

Global news requests remain authenticated and server-side; bearer credentials are never sent to browser JavaScript. Summary Dashboard reuses existing cached snapshot data and does not add broker requests. Existing no-store refresh, sleep/wake recovery, HTTPS, trusted-host, and secure-session behavior remains unchanged.

If the access token or network is unavailable, Global market news shows a safe error state and Summary identifies live valuation as unavailable rather than presenting fallback values as live.
