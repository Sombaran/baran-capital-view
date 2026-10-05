# baran-capital-view v2.0.50 - Implementation Summary

## Root cause

The screenshot showed runtime version `2.0.48`, although the project had already advanced to `2.0.49`. `run.sh` only built when the executable was missing or `--rebuild` was supplied, so normal startup could keep launching an older binary that did not include the newer recovery and stability fixes.

## Fix

- Check application source and build metadata timestamps before normal startup.
- Run the existing incremental build when files are newer than the executable.
- Keep clean rebuild opt-in and correctly consume `--rebuild` before forwarding app arguments.
- Add the fix summary to the versioned post-login popup.

## Validation

- Add launcher tests for incremental rebuild detection and explicit rebuild argument forwarding.
- Run Python tests, C++ build/CTest, and verify the executable release version.
