# baran-capital-view

Version: 2.0.12

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

Current release: 2.0.12

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

## Dependencies

All dependencies are managed through **Conan** (`conanfile.py`) for reproducible builds across platforms:

- **nlohmann_json 3.11.3** — JSON serialization/deserialization (managed via Conan since v2.0.10)
- **libcurl** — HTTP client (optional via Conan with `use_conan_libcurl` option)
- **OpenSSL** — TLS/HTTPS support (optional via Conan)
- **GTest** — C++ testing framework (automatically integrated)

The `conanfile.py` file is the single source of truth for all project requirements. Conan automatically generates CMake toolchain files and dependency metadata during the build process.

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

Version 2.0.12 removes the duplicate Overview confidence selector. The table
header arrows remain the single sorting control, while the shared filter now
filters data rows and cards without hiding table headers or contacting the
Stock API.

Version 2.0.11 adds optimized client-side arrow sorting to the Overview, Alerts,
Deeper analysis, and Fundamentals tables. Sorting uses the already loaded rows,
keeps serial numbers stable, and does not send new requests to the Stock API.

Version 2.0.10 included:

### Dependency Management (v2.0.10+)
- **Removed vendored nlohmann header**: Eliminated `third_party/nlohmann/json.hpp` vendored copy
- **Added Conan dependency**: `nlohmann_json/3.11.3` now managed through `conanfile.py`
- **Simplified CMakeLists.txt**: Reduced dependency resolution from 35+ lines to 2 lines using `find_package(nlohmann_json REQUIRED)`
- **Single source of truth**: All dependencies managed through Conan for consistency across CMake and Bazel builds

### API resilience and error handling
- **Fixed JSON parsing errors** that showed "unexpected character at line 1 column 1" on long-running sessions by validating all API responses before sending to the browser
- **Added robust error handling** for empty, malformed, or incomplete JSON payloads at both the C++ server and JavaScript client layers
- **Implemented fallback JSON** for all API endpoints (`/api/holdings`, `/api/news`, `/api/positions`, `/api/config`) to ensure valid responses even when data sources fail
- **Enhanced error messages** with detailed context displayed in a dedicated error panel instead of raw parse exceptions

### Browser UI optimization
- **Data health tab** now shows operational diagnostics: holdings loaded, news articles, alert decisions, and missing price checks
- **JSON tab** displays raw server payload with a clear "Runtime payload" label and explanatory context
- **Config tab** summarizes the secure configuration boundary: holdings source, news source, auth mode, and HTTPS-only model
- **Error display** improved with actionable guidance and browser console logging for debugging

### Security and code quality
- **Login flow hardening** ensures `FOLIO_LOGIN_CODE` values are trimmed, URL-decoded, and compared safely before session creation
- **Environment-backed secrets** can come from `~/.upstox.env` or protected local fallback files without exposing credentials in source control
- **Repository hygiene** keeps Bazel artifacts, local caches, generated files, and secret data out of GitHub
- **Single canonical tab render path** prevents duplicate UI logic and stale data conflicts in Overview, Alerts, and Deeper analysis views

### Operational stability
- **Existing features preserved**: browser cache behavior, saved-news fallback path, and local-first architecture remain unchanged
- **Version alignment** across release popup, documentation, CMake build, and runtime UI using x.x.x semantic versioning
- **All regression tests pass** with 100% success rate under the gtest suite

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
