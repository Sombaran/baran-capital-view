# Baran Capital View v2.0.39 - Implementation Summary

## Web diagnostics

The server already emits operational messages on stdout/stderr, but `run.sh --web` did not persist them. The web launch now redirects both streams to a private user-state log. `PORTFOLIO_WEB_LOG` can override the destination; non-web CLI commands are unchanged.

## UI efficiency

The dashboard now pauses recurring holdings/news refreshes while hidden. The dedicated Deeper analysis poll also waits until the page is visible. Existing visibility recovery refreshes the active view when the user returns.

## Validation

- Runtime and C++ test targets build; CTest passes.
- All 10 Python tests pass, including launcher tests for output capture, mode-`0600` permissions, and non-web behavior.
- Existing Stock API security boundaries and response handling are unchanged.
