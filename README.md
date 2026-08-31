# baran-capital-view

Version: 2.0.10

baran-capital-view is a C++17 portfolio analysis and monitoring application for live and saved market data. It blends portfolio health scoring, fundamental analysis, C++/Python analytics, and browser-based reporting while keeping stock API access constrained and secure.

## Highlights

- Portfolio health scoring for holdings, concentration, and diversification
- Live Upstox integration with hardened HTTPS and trusted-host validation
- Python-backed Deeper analysis for recent company/news context and recommendations
- Browser UI for overview, news, alerts, positions, config, and data inspection
- Release-notes popup summarizing the current code-change version and fixes
- Security-first handling around API tokens and external HTTP endpoints
- Local monolith architecture with internal async task scheduling for background work
- Safe extension path for future distributed workers only when real operational need exists

## Versioning format

The project follows semantic versioning in x.x.x format:

- MAJOR: breaking architecture or incompatible changes
- MINOR: new features or major enhancements
- PATCH: fixes, hardening, and stability improvements

Current release: 2.0.10

The build and the browser popup both read the same version identifier from the CMake project definition so the release notes, UI banner, and runtime binary stay aligned with the shipped code change set.

## Shared library layout

The C++ build now exposes a shared library target named `portfolio_health_core` for the service implementation, while the CLI executable remains a thin runtime wrapper. This keeps the runtime entry point small, avoids duplicate translation units, and preserves the same operational behavior for the web UI and CLI.

Build targets:

- `portfolio_health_core` — shared library containing the portfolio, API, notification, and analytics logic
- `portfolio_health` — executable entry point for the app and CLI commands
- `portfolio_health_tests` — gtest regression target for the secure stock API and portfolio logic

## Build

The project supports both the existing CMake workflow and a Bazel build path for compatibility with alternative CI pipelines.

```bash
cd /home/ritup2404/baran-capital-view

# 1 = CMake build with Conan integration
./buildCode.sh 1 --rebuild

# 2 = Bazel build (requires Bazel or Bazelisk to be installed locally)
./buildCode.sh 2 --rebuild

# legacy explicit flags remain supported
./buildCode.sh --cmake --rebuild
./buildCode.sh --bazel --rebuild
```

Use either the numeric selector (`1` or `2`) or the explicit flags for build backend selection in automation. If Bazel is not installed, prefer the CMake path with `1` to keep the default workflow functional.

Add the local login secret to your shell environment or a protected local file before starting the browser UI:

```bash
printf '%s\n' "export FOLIO_LOGIN_CODE='070923'" >> ~/.upstox.env
chmod 600 ~/.upstox.env
source ~/.upstox.env
./run.sh --web
```

## Testing

The project includes regression checks for both C++ and Python paths:

```bash
# C++ unit tests with gtest
cmake --build build --target portfolio_health_tests
ctest --test-dir build --output-on-failure

# Python unit tests with pytest
pytest -q
```

The C++ tests cover the secure symbol, quantity, and price validation helpers used by the stock order pipeline, while the Python tests cover the sentiment fallback and path resolution logic in the Deeper analysis workflow.

## Run web UI

```bash
cd /home/ritup2404/baran-capital-view
./run.sh --web
```

## Security notes

- API traffic is restricted to HTTPS-only access for stock and broker endpoints
- Host allowlisting prevents credential forwarding to untrusted destinations
- Secrets are read from environment variables or local protected files only
- Browser session login uses a private secret code outside version-controlled files
- Redirection, stale data, and unauthorized responses fail safely without exposing sensitive details

## Architecture recommendation

The best architecture for baran-capital-view is a local monolith first, with an internal task scheduler and async worker queue for non-blocking background work.

Recommended pattern:

- Local monolith: the main C++ web/UI service remains the single operational boundary
- Internal task scheduler: scheduled refresh, analysis, and notification tasks run in-process
- Async worker queue: long-running jobs are handled without blocking the main request loop
- Distributed version later: only if there is a real need for multi-node scaling or horizontal isolation

This keeps the broker API and stock data behind one trusted boundary and avoids exposing new attack surfaces.

## Recent fix summary

Version 2.0.10 includes:

- fixed the secret-login flow so `FOLIO_LOGIN_CODE` values are trimmed, URL-decoded, and compared safely before a session is created
- added env-backed secret handling through `~/.upstox.env` and protected local login files without exposing credentials in source control
- tightened repo hygiene by ignoring Bazel artifacts, local caches, and environment files so generated or secret data does not get pushed to GitHub
- kept the release popup and documentation aligned to the x.x.x code-change version while preserving the same HTTPS-only stock API boundary and local-first architecture
- optimized the browser tab rendering path so the Overview, Alerts, and Deeper analysis views stay consistent without reintroducing duplicate UI logic
- kept the local-first architecture, browser cache behavior, and saved-news fallback path intact so existing features remain stable
- refreshed the project documentation and runtime version metadata to the x.x.x semver format used in production releases

## Project structure

A simple industry-style project layout for this service is:

- app/ — entry points and runtime bootstrapping for the web UI and CLI
- src/ — C++ implementation, API clients, and service logic
- include/ — public headers and shared interfaces
- config/ — runtime configuration and portfolio data files
- design/ — architecture decisions, design docs, and release notes
- docs/ — operator and developer documentation
- scripts/ — build, run, and deployment helper scripts
- tests/ — unit and integration checks for stability and security
- stock_alert_nlp.py — Python analysis and news scoring utility

This structure keeps the stock API boundary, executable surface, and configuration clearly separated so operational changes stay easier to reason about and audit.
