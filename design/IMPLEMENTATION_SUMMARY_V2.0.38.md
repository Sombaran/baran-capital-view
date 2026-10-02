# Baran Capital View v2.0.38 - Implementation Summary

## Problem

When snapshot generation failed, `/api/holdings` could return an error object without `status: error`. The browser treated the missing `data` field as an empty portfolio and displayed zero live holdings and zero Day P&L. Periodic refreshes also retried failed requests at the normal refresh interval.

## Fix

- Return a consistent, sanitized error envelope for failed server snapshots.
- Reject error, malformed, and shape-invalid API payloads in the dashboard client instead of caching them as successful data.
- Show a clear authentication/data-unavailable state with a manual retry action.
- Back off automatic retries for transient and authentication failures; manual refresh and wake recovery clear the backoff.
- Add the fix summary to the versioned post-login popup.

## Validation

- Runtime and C++ test targets build successfully; the C++ test suite passes.
- Python regression tests pass.
- Embedded dashboard JavaScript parses successfully.
