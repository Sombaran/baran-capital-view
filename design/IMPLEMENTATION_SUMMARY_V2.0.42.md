# Baran Capital View v2.0.42 - Implementation Summary

## Problem

`run.sh` unconditionally sourced `~/.upstox.env` before starting the C++ server. If the user launched it with a newly refreshed `UPSTOX_ACCESS_TOKEN` in the shell while the protected file still contained an older value, the file replaced the fresh token. The server then received 401 responses across dashboard data views.

## Fix

- Removed unconditional credential-file sourcing.
- Load the protected environment file only when one or more required Upstox credentials are missing.
- Preserve any credential values already present in the launching process while loading missing values from the file.
- Document that a running process must be restarted after token rotation because its client retains the startup token.
- Keep credentials out of log output and browser state.
- Add a launcher test with a stale file token and a fresh shell token.

## Validation

The launcher regression verifies shell-token precedence. Full C++ and Python test suites remain the verification gates.
