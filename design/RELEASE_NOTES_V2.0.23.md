# Release Notes v2.0.23

**Version:** 2.0.23

## Fix

Deeper analysis now retries transient API failures up to three times with bounded backoff before showing an error. This allows the page to recover when a suspended browser request or temporary network interruption occurs after system sleep.

The existing server-side worker remains asynchronous and can rebuild an expired or failed analysis result on the next request. Stock API credentials remain server-side, and the retry path does not expose or log bearer tokens.

## Verification

C++ and Python regression suites pass after the change.
