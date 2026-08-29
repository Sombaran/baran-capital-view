# myFolio

Version: 2.0.3

myFolio is a C++17 portfolio analysis and monitoring application for live and saved market data. It blends portfolio health scoring, fundamental analysis, C++/Python analytics, and browser-based reporting while keeping stock API access constrained and secure.

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

Current release: 2.0.3

## Build

```bash
cd /home/ritup2404/myFolio
./buildCode.sh --rebuild
```

## Run web UI

```bash
cd /home/ritup2404/myFolio
./run.sh --web
```

## Security notes

- API traffic is restricted to HTTPS-only access for stock and broker endpoints
- Host allowlisting prevents credential forwarding to untrusted destinations
- Secrets are read from environment variables or local protected files only
- Browser session login uses a private secret code outside version-controlled files
- Redirection, stale data, and unauthorized responses fail safely without exposing sensitive details

## Architecture recommendation

The best architecture for myFolio is a local monolith first, with an internal task scheduler and async worker queue for non-blocking background work.

Recommended pattern:

- Local monolith: the main C++ web/UI service remains the single operational boundary
- Internal task scheduler: scheduled refresh, analysis, and notification tasks run in-process
- Async worker queue: long-running jobs are handled without blocking the main request loop
- Distributed version later: only if there is a real need for multi-node scaling or horizontal isolation

This keeps the broker API and stock data behind one trusted boundary and avoids exposing new attack surfaces.

## Recent fix summary

Version 2.0.3 includes:

- fixed Deeper analysis Python path resolution from any working directory
- hardened API calls for missing/expired authentication and invalid hosts
- improved news fallback behavior when empty or stale payloads are returned
- aligned the fundamentals popup layout on narrow screens
- added a local-first task scheduler and async queue model for background analysis work
- refreshed the right-side summary popup with the actual applied fix set
- updated project documentation to reflect the current release version and design notes

## Project structure

- src/ — C++ application implementation
- include/ — public interfaces and models
- design/ — architecture and release documentation
- config/ — portfolio and service configuration
- stock_alert_nlp.py — Python analysis and news scoring utility
