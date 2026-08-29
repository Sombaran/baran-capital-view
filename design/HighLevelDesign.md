# High-Level Design — Portfolio Health Web Service

## 1. Purpose

Give an Indian retail trader an at-a-glance answer to a single question —
**"Is my portfolio healthy right now?"** — through a local C++ web dashboard
or the terminal interface, with optional automated trade execution support.

The service pulls positions and long-term holdings from Upstox (both via REST 
and real-time WebSocket streaming), fetches news for the account holdings, 
filters it to symbols in `config/holding.csv`, and exposes analytics, advisory 
sentiment signals, technical indicators, ML-driven predictions, and Prometheus 
metrics. The Alerts view uses long-term holdings as its authoritative row set, 
so a holding without a matching article is still represented with a neutral 
review state.

### v2.0.1 Security hardening and API safety

**Stock API hardening** — The live Upstox integration now restricts all outbound
API calls to trusted HTTPS hosts, rejects non-HTTPS URLs, and blocks arbitrary
base URLs that could route trading traffic to untrusted destinations. The code
validates the symbol, quantity, and price before any simulated or real order is
accepted, and it keeps sensitive token handling inside the secure helper layer
instead of exposing it through logs or loose config parsing.

**Versioned release patching** — The application version now follows a semver
pattern (`x.x.x`) and the right-side release popup calls out the applied fix in
plain language so operators can see what changed without opening the docs. The
project is now tagged as `2.0.1` for this hardening patch.

### v2.0.3 Runtime reliability, async scheduling, and browser UX hardening

The 2.0.3 patch makes the backend Python workflow portable across working
directories by resolving the project-local script path and running it with
absolute file references, so the Deeper analysis CLI remains reliable even when
launched outside the repository root. The application now also includes a safe
local-first internal task scheduler and async worker queue for back-end refresh
and analysis work, which reduces UI blocking without introducing a distributed
attack surface around the stock API. The web UI now treats empty or stale news
payloads as recoverable states instead of failing the page, the fundamentals
popup layout is tightened for consistent viewport alignment, and expired or
unauthorized API responses are surfaced as secure redirect or retry states
instead of exposing raw HTTP failures.

### v2.0.2 Runtime TLS consistency

The default build lets system libcurl own its verified HTTPS stack and does not
directly mix Conan OpenSSL headers with a different system OpenSSL ABI. This
avoids the certificate-store crashes seen in the live CLI and Web UI on hosts
without matching OpenSSL development files. OpenSSL-backed helper features are
enabled only when matching development files are available. HTTP redirects are
disabled for authenticated API requests so bearer credentials cannot be
forwarded outside the approved Upstox hosts.

### v2.0.0 Enhancements

**Real-time Streaming** — WebSocket connections to Upstox Market Data Feed V3 
provide live quote updates and holdings changes without polling overhead. The 
streaming infrastructure is thread-safe and integrates with the existing 
authentication model.

**Technical Indicators** — Expanded indicator suite includes RSI, MACD, 
Bollinger Bands, Stochastic Oscillator, and ATR, all computed in C++ for 
low-latency analysis. Indicators feed into signal generation and ML models.

**Machine Learning Models** — Python-based predictive models (trained on 
historical data) run server-side and integrate with C++ signal generation to 
produce consensus buy/sell/hold recommendations with confidence scores.

**Signal Generation Engine** — Autonomous system that combines technical 
indicators, ML predictions, news sentiment, and portfolio risk metrics to 
generate actionable trading signals (Buy/Sell/Hold) with associated risk 
scores and opportunity confidence levels.

**Mobile Notifications & Alerts** — Push notifications via configurable 
endpoints alert users to price movements, generated signals, portfolio health 
degradation, and important news events. Notification preferences are 
user-configurable per alert type.

**Orders API Integration** — Authenticated integration with Upstox Orders API 
allows automated trade placement when signals meet configured criteria. All 
orders are logged for audit, support manual override with 2FA, and can be 
automatically canceled if market conditions change significantly.
The Alerts page renders only text and table content; article thumbnails or other
attached images are intentionally excluded from that view.
The raw instrument-key Stock column is also excluded from the Alerts table; the
serial number and company name identify each holding instead.
The service exports review-action gauges for Prometheus. Prometheus rules can
forward threshold crossings to Alertmanager or Grafana notification channels;
these triggers are advisory and do not place broker orders.
This integration is exposed through the web service's `/metrics` route; the
browser Alerts tab does not directly fire Prometheus or Grafana notifications.
The dashboard also provides a read-only Data health view and browser-local
acknowledgement state for Alerts, without adding write calls to Upstox.
The RSI tool provides guided chronological input, sample values, and readable
interpretation feedback while continuing to use the local calculation endpoint.
Deeper analysis distinguishes missing news (`No recent news`) from genuine
neutral news (`Neutral news`) so a data gap is never presented as a hold signal.
It iterates the live holdings universe, so all 51 holdings remain visible, and
identifies saved portfolio news as a fallback when fresh Python news is absent.
The Web UI displays the CMake project version, and News presents articles in
descending publication-time order.
The CLI exposes the same build identifier through `--version`, and Web startup
logs include the version so the running binary is unambiguous.
Credentials are supplied through the environment or an external secret
manager; repository scripts and JSON configuration contain no live secrets.
The client restricts authenticated traffic to approved Upstox HTTPS hosts and
the web service remains bound to loopback.
The myFolio Python analyzer uses configured API providers first and falls back to
Google News RSS when providers are rate-limited, rejected, or unavailable;
all provider queries include the configured market query suffix.
Its implementation and dependencies are stored under myFolio, so Deeper
analysis has no runtime dependency on the sibling `market` directory.
Deeper analysis refreshes its live comparison at most every five minutes,
preventing an open-session result from being reused after the market closes.
An explicit post-session CLI job atomically updates the report-shaped
`config/holding.csv` from the latest Upstox holdings.
News filtering normalizes whitespace and case for both the portfolio CSV and
live holding symbols, ensuring that only Sombaran Portfolio holdings are shown.
After authentication, the dashboard shows a concise, dismissible release popup
once per version; its seen marker is kept in browser local storage.
The Overview dashboard displays a manual Refresh action alongside its summary
metrics and hides the implementation refresh interval from users while
allowing holdings to render when optional news or positions data is unavailable,
with the partial state surfaced in the status line.
Dashboard DOM observers update metric content only when values change, avoiding
browser main-thread loops that can block tab rendering.
Market Value uses the broker-reported holding value when available and shows
two decimal places for INR precision.
For live mode, the server derives each holding's `current_value` from the
authenticated Upstox holding and sends that normalized value to the browser.
The same live total is exposed to Deeper Analysis so its values stay aligned
with the portfolio snapshot.
Deeper Analysis groups recommendations into category counts and lets the user
select a category to inspect its stock names. News is limited to portfolio
instrument keys and articles published in the previous 30 days.
Fallback snapshots apply the same filtering boundary and never expose the raw
saved news export.
Holdings responses identify whether data came from `upstox-live` or the local
fallback, and API responses are marked non-cacheable.
retaining cached synchronization to reduce unnecessary API traffic. If an
overnight token expiry or transient Upstox news failure prevents refresh, the
server serves the filtered saved-news export until live news is available
again.

Non-goals:

* Historical / EOD analytics — Upstox news is limited to its recent-news window.
* Full risk model. The health score is a heuristic, deliberately simple.
* Direct position modification through the UI; order placement is API-only and signal-driven.

---

## 2. Component view (v2.0.0)

```
             ┌─────────────────────────────────────────┐
             │                CLI (main)               │
             │  argv → Options { file, raw, json, … }  │
             └────────────┬──────────────┬─────────────┘
                          │              │
        offline (--file)  │              │ live (default)
                          ▼              ▼
                ┌───────────────┐  ┌───────────────────┐
                │ file reader   │  │  UpstoxClient     │
                │ + JSON parse  │  │  (Bearer auth)    │
                └───────┬───────┘  │  + StreamingClient│
                        │          │  (WebSocket auth) │
                        │          └─────────┬─────────┘
                        │                    │
                        ▼                    ▼
                ┌───────────────────────────────────────┐
                │           std::vector<Position>       │
                │         + Real-time quotes            │
                └────────────────────┬──────────────────┘
                                     ▼
                     ┌──────────────────────────────────┐
                     │    TechnicalIndicators::         │
                     │  Calculate(RSI, MACD, BB, etc)   │
                     └───────────────┬──────────────────┘
                                     ▼
                     ┌──────────────────────────────────┐
                     │      MLModels::Predict()         │
                     │  (Price movements, trends)       │
                     └───────────────┬──────────────────┘
                                     ▼
                     ┌──────────────────────────────────┐
                     │   SignalGenerator::Generate()    │
                     │  (Consensus Buy/Sell/Hold)       │
                     └───────────────┬──────────────────┘
                                     ├─────────────────┐
                                     ▼                 ▼
                    ┌──────────────────┐   ┌─────────────────────┐
                    │ PortfolioHealth::│   │ NotificationService │
                    │   analyze()      │   │   ::Send()          │
                    └────────┬─────────┘   └──────────┬──────────┘
                             │                        │ WebSocket/HTTP
                             ▼                        ▼
                    ┌──────────────────┐   Mobile/Desktop Clients
                    │  OrdersAPI::     │
                    │  Execute()       │   (if signal >= threshold)
                    │ Upstox Orders    │
                    └─────────┬────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │   WebServer      │
                    │  JSON routes     │
                    │ Prometheus /     │
                    │  /metrics        │
                    └──────────────────┘
```

All modules use thread-safe design patterns and async I/O where applicable.
Real-time streaming is optional and controlled via configuration.
Order execution requires explicit user authorization with configurable thresholds.

The current deployment also starts `WebServer` with `--web`. It serves the
embedded dashboard and JSON routes on `127.0.0.1:8080`; `/metrics` exposes
portfolio gauges for Prometheus and Grafana. The browser talks only to this
local server, so the Upstox bearer token remains server-side.

The Deeper analysis tab is intentionally on demand. Its authenticated API
route reads `config/portfolio_news.json`, invokes `stock_alert_nlp.py`
with `config/holding.csv`, and combines the Python recommendation with the
saved news signal by mapping Upstox instrument keys through live holdings.

## 3. End-to-end flow (v2.0.0)

1. **Entry point** — `--web` starts `WebServer`; no arguments starts the
  terminal menu, while explicit flags remain available for automation.
  
2. **Payload acquisition** — either
   * `--file` → read local JSON, or
   * live → `UpstoxClient` performs authenticated GETs via `HttpClient` and 
     optionally `StreamingClient` establishes WebSocket for real-time updates.
   
3. **Deserialisation** — the JSON `data` array is mapped 1:1 into
   `std::vector<Position>` with optional streaming quote enhancements 
   (see [LowLevelDesign](LowLevelDesign.md)).
   
4. **Technical Analysis** — `TechnicalIndicators` module computes:
   * RSI (Relative Strength Index) for momentum
   * MACD for trend detection
   * Bollinger Bands for volatility
   * Stochastic Oscillator for overbought/oversold
   * ATR for volatility measurement
   
5. **Machine Learning** — `MLModels` module invokes trained models to predict:
   * Short-term price movements
   * Trend continuation probability
   * Volatility forecast
   
6. **Signal Generation** — `SignalGenerator` combines:
   * Technical indicator consensus
   * ML model predictions
   * News sentiment analysis
   * Portfolio risk metrics
   → Produces Buy/Sell/Hold signals with confidence scores
   
7. **Risk Scoring** — Dynamic calculation of:
   * Position concentration risk
   * Sector concentration risk
   * Volatility risk
   * Correlation-based diversification risk
   → Factors into signal thresholds and order execution guards
   
8. **News processing** — live holdings are matched to `config/holding.csv`; each
  article receives a keyword sentiment score and advisory signal.
  
9. **Notifications** — `NotificationService` pushes:
   * Generated trading signals (if configured)
   * Price breakout/breakdown alerts
   * Portfolio health degradation warnings
   * Important news events
   → To mobile apps, desktop clients, or configured webhooks
   
10. **Order Execution** (if enabled) — `OrdersAPI` module:
    * Validates signal confidence against threshold
    * Checks portfolio risk limits
    * Simulates order impact
    * Executes via Upstox Orders API (with audit logging)
    * Supports automatic cancellation on adverse price movements
    
11. **Rendering** — terminal reports, JSON, browser tabs, and Prometheus
  metrics are generated from the live responses with signal overlays.
  
12. **Deeper analysis** — only when selected, the server runs the Python news
  analyzer and compares its result with the saved portfolio news.
  
13. **Fundamentals** — the user selects a holding and the server retrieves its
  balance sheet, income statement, cash flow, and corporate actions by ISIN.
  
14. **End-of-day update** — a scheduled `--update-holding` command fetches
  long-term holdings and atomically replaces the latest broker values in the
  configured holdings CSV.

## 4. Cross-cutting concerns

| Concern         | Approach                                                            |
| --------------- | ------------------------------------------------------------------- |
| Auth            | Upstox credentials exported by `run.sh` and read from environment; never sent to the browser. |
| Secrets         | Token is read from env only. Not echoed. `--raw` prints response, never the request. |
| Network errors  | Surfaced as `PositionsResult::error` — non-zero exit, no stack trace. |
| JSON drift      | Numeric parsers tolerate string-encoded numbers; `trading_symbol` and `tradingsymbol` are both accepted. |
| Offline testing | `--file` mode; ships `config/sample_positions.json`.                 |
| Portability     | POSIX + libcurl. Build tested on Linux; CMake also targets MSVC / MinGW paths. |
| Currency        | INR fixed-width formatting is done in-tree — no locale dependency.   |

## 5. Deployment shape

Single C++ binary (`portfolio_health`) with a localhost-only web server. It
uses `UPSTOX_ACCESS_TOKEN` for authentication and writes only
the explicit `config/portfolio_news.json` export when `--news` is used.

## 6. Extension points

* **New endpoint** — add a method to `UpstoxClient` and expose a route in
  `WebServer`.
* **Real-time streaming** — `StreamingClient` establishes WebSocket connections
  using the market data authorization from Upstox API.
* **Technical indicators** — expand `TechnicalIndicators` module with new
  indicators; integrate with signal generation.
* **Machine learning** — replace or augment ML models in `MLModels`; train on
  your own historical data or integrate external prediction services.
* **Signal customization** — modify `SignalGenerator` thresholds, weights, and
  consensus logic to match your risk tolerance.
* **Order automation** — configure `OrdersAPI` with different execution strategies,
  risk limits, and cancellation policies.
* **Notification channels** — extend `NotificationService` to support additional
  endpoints (Slack, Discord, Email, SMS, etc.).
* **Richer sentiment** — replace the keyword scorer with ONNX Runtime or a
  separate model service while keeping signals advisory.
* **Monitoring** — add metrics to `WebServer` for new Grafana panels.
* **Alternate broker** — implement a sibling client that yields the
  common `Position` struct; `analyze()` stays untouched.
* **Different score** — `PortfolioHealth::analyze` is the only place the
  scoring formula lives. See [PortfolioHealthModel](PortfolioHealthModel.md).
