# Release Notes v2.0.42

**Version:** 2.0.42  
**Date:** October 2, 2026

## Upstox credential precedence

- Fixed `run.sh` sourcing `~/.upstox.env` before inspecting the launching environment, which could replace a newly supplied shell access token with a stale file value and lead to HTTP 401 responses across dashboard views.
- The protected file is now used only when required credential variables are missing, and values already supplied by the parent process are restored after sourcing it.
- Restart the web process after token rotation; the running C++ client retains its startup credentials.
- Added a launcher regression test proving a fresh shell access token takes precedence over an older file token.

## Compatibility and security

- Existing credential files still supply missing values; explicit process environment values now have priority.
- Tokens remain server-side and are not printed by the launcher or copied into browser state.

## Release transparency

- Added the v2.0.42 fix summary to the right-side popup shown after login.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.42`.
