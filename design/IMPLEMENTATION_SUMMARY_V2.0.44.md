# Baran Capital View v2.0.44 - Implementation Summary

## Problem

The dashboard refreshed its shared Upstox snapshot every five seconds even though the browser polls every fifteen seconds. This can increase API traffic and make rate limits more likely. Additionally, failure to fetch news could fail a holdings request, and an upstream outage with no local fallback hid the last usable portfolio snapshot.

## Fix

- Increased the shared server snapshot cache lifetime to 30 seconds.
- Added server-side backoff after failed holdings fetches (30 seconds for transient failures; five minutes for authentication failures).
- Return the last successful snapshot, explicitly labeled with its source, age, and refresh warning, when live refresh and local fallback are unavailable.
- Keep valid holdings visible when news cannot be fetched; show a clear warning rather than failing the Overview.
- Label cached Market news and expose its warning.
- Added a versioned fix summary to the post-login popup.

## Validation

- Added regression assertions for cache lifetime, retry throttling, stale snapshot disclosure, and holdings/news failure isolation.
- Build the C++ targets and run the C++ and focused Python test suites.
