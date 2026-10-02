# Release Notes v2.0.39

**Version:** 2.0.39  
**Date:** October 2, 2026

## Web log capture

- `./run.sh --web` now captures the C++ server's stdout and stderr in a per-user log file.
- Default path: `${XDG_STATE_HOME:-~/.local/state}/baran-capital-view/web-ui.log`.
- Set `PORTFOLIO_WEB_LOG` to select another path.
- The log directory is created with owner-only access, and the log file is set to mode `0600`.
- Non-web invocations keep their existing terminal output. The script refuses a log path that is a symlink or non-regular file.

## Dashboard efficiency

- Automatic portfolio polling pauses while the browser tab is hidden and resumes through the existing visibility recovery when the tab becomes visible.
- The Deeper analysis timer also skips hidden tabs, reducing unnecessary requests and background work.

## Tests and release transparency

- Added launcher tests covering stdout/stderr capture, log permissions, and unchanged non-web output.
- Added a v2.0.39 summary to the right-side popup shown after login.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.39`.
