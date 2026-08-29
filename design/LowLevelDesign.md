# Low-Level Design — Portfolio Health

## 1. Modules (v2.0.0)

| Header                              | Impl                                | Responsibility                                 |
| ----------------------------------- | ----------------------------------- | ---------------------------------------------- |
| `include/HttpClient.hpp`            | `src/HttpClient.cpp`                | Thread-safe libcurl wrapper.                   |
| `include/UpstoxClient.hpp`          | `src/UpstoxClient.cpp`              | Upstox REST client: positions, holdings, news, and LTP. |
| `include/StreamingClient.hpp`       | `src/StreamingClient.cpp`           | WebSocket streaming for live quotes & holdings (v2.0.0+). |
| `include/Position.hpp`              | (header-only)                       | Domain struct mirroring the API payload.       |
| `include/PortfolioLoader.hpp`       | `src/PortfolioLoader.cpp`           | CSV / JSON reader for user-supplied portfolios.|
| `include/TechnicalIndicators.hpp`   | `src/TechnicalIndicators.cpp`       | Indicators: RSI, MACD, Bollinger Bands, Stochastic, ATR (v2.0.0+). |
| `include/MLModels.hpp`              | `src/MLModels.cpp`                  | Predictive models for price & trend forecast (v2.0.0+). |
| `include/SignalGenerator.hpp`       | `src/SignalGenerator.cpp`           | Buy/Sell/Hold consensus signal generation (v2.0.0+). |
| `include/OrdersAPI.hpp`             | `src/OrdersAPI.cpp`                 | Upstox Orders API integration with audit logging (v2.0.0+). |
| `include/NotificationService.hpp`   | `src/NotificationService.cpp`       | Push notifications for signals & alerts (v2.0.0+). |
| `include/PortfolioHealth.hpp`       | `src/PortfolioHealth.cpp`           | Analytics, report renderer, scoring, alerts.   |
| `include/WebServer.hpp`             | `src/WebServer.cpp`                 | Local browser UI, JSON routes, sentiment, metrics. |
| `src/main.cpp`                      | —                                   | CLI, terminal tabs, and `--web` entry point.   |

The Alerts tab joins `/api/holdings` with `/api/news` by `instrument_token` /
`instrument_key`. Holdings are the authoritative list: missing news produces a
neutral `Hold / wait` row with zero articles. Each row receives a 1-based serial
number, and the `Why` cell links to the matching article URL when available.
The Alerts renderer does not create image elements or display article thumbnails.
It omits the raw instrument-key Stock column and renders the serial number and
company name as the holding identity fields.

### v2.0.0 Module Enhancements

**StreamingClient** manages WebSocket connections to Upstox Market Data Feed V3.
It uses the authorization URL from `UpstoxClient::getMarketFeedAuth()` and decodes
binary Protobuf frames using the official schema. Streaming state is maintained
per symbol, with automatic reconnection on network failure. Thread-safe quote
cache serves the latest price for each subscribed symbol.

**TechnicalIndicators** calculates:
- **RSI** — Wilder's Relative Strength Index; overbought >70, oversold <30
- **MACD** — Moving Average Convergence Divergence with signal line and histogram
- **Bollinger Bands** — Upper/lower bands ± 2 std dev from SMA(20)
- **Stochastic** — %K and %D with overbought >80, oversold <20
- **ATR** — Average True Range for volatility measurement
- **EMA** — Exponential Moving Averages for trend lines

All indicators are computed from chronological OHLC data and cached for 5 minutes.

**MLModels** loads pre-trained model weights and runs inference on recent OHLC 
data. Models predict:
- Next-day close price (with confidence interval)
- Trend probability (up/down/neutral)
- Volatility forecast
- Mean reversion probability

Models are loaded from `config/ml_models/` and support ONNX Runtime or custom 
C++ inference. Inference results cache for 1 hour unless market volatility 
spikes beyond configured thresholds.

**SignalGenerator** combines multiple data sources:
- Technical indicator consensus (majority vote weighted by indicator confidence)
- ML model probability
- News sentiment score
- Portfolio risk metrics
- Relative strength within sector

It produces signals with:
- Action: Buy / Sell / Hold / Wait
- Confidence: 0–100
- Reason: Enumeration of contributing factors
- Risk Score: 0–100 (portfolio impact)

Signal thresholds are configurable per symbol, sector, and market condition.

**OrdersAPI** wraps Upstox `/v2/orders/regular` endpoint:
- Validates order against portfolio risk limits
- Supports limit/market/stop-loss orders
- Simulates order impact on portfolio health before execution
- Logs all orders (buy/sell/cancel) with request/response
- Supports automatic order cancellation after <N> minutes with adverse price move

Order execution requires:
- Signal confidence >= configured threshold
- Portfolio risk metrics within limits
- Manual approval via 2FA (configurable)
- Market hours validation

**NotificationService** dispatches alerts via:
- WebSocket to connected browser clients (low-latency)
- HTTP POST to configured webhooks (Slack, Discord, IFTTT, etc.)
- Firebase Cloud Messaging for mobile apps (with device token registration)
- Email via configured SMTP server
- SMS via Twilio or similar (optional)

Notification types include:
- `SIGNAL_GENERATED` — Buy/Sell opportunity detected
- `PRICE_ALERT` — Breakout/breakdown on configured levels
- `PORTFOLIO_ALERT` — Health score change, concentration risk
- `NEWS_ALERT` — Important news for portfolio holdings
The `/metrics` response exports `portfolio_news_review_total` with an `action`
label (`consider`, `risk`, or `hold`). Prometheus or Grafana can evaluate these
gauges and send notifications through Alertmanager or a configured contact
point; no order endpoint is invoked.
The browser UI displays the underlying review data, while Prometheus/Grafana
perform rule evaluation and notification triggering after scraping `/metrics`.
The dashboard enhancement adds a Data health tab that fetches `/api/holdings`
and `/api/news` concurrently. Alert acknowledgement is keyed by rendered row
and stored in browser local storage, avoiding server-side write contention.
The RSI panel gives live validation hints based on `period + 1`, provides a
sample input action, and renders the returned interpretation through an
accessible live status region. Calculation remains delegated to `/api/rsi`.
The Python analyzer retries a second fast-mode symbol query when the first
provider response contains no articles, applies `query_suffix` to provider
queries, and falls back once per stock to Google News RSS when all API
providers are unavailable. Its recommendation contract uses
`No recent news` for missing evidence and `Neutral news` for article text with
no directional keywords; C++ treats both as non-directional.
The myFolio analyzer and its `requirements.txt` are local to this project;
config lookup uses the project directory and does not depend on the caller's
working directory.
Deeper analysis joins from `holdings.positions`, not from saved-news keys. This
keeps the result complete for all 51 holdings. A missing fresh recommendation
uses the saved article/alert signal only as an explicitly labeled fallback;
`No recent news` is reserved for a holding with no usable saved or fresh news.
The build passes `PROJECT_VERSION` as `PORTFOLIO_HEALTH_VERSION`; the server
injects it into the dashboard header. The News renderer sorts flattened article
records by numeric `published_time` descending, leaving undated records last.
The server normalizes portfolio symbols by trimming whitespace and uppercasing
before matching `config/holding.csv` to live holdings, preventing unrelated
news keys from entering `/api/news`.
The authenticated page injects a version-aware `What is new` dialog. Its close
button stores `myfolio-release-seen` in local storage, so repeat visits for the
same version do not create another request or interrupt the dashboard.
The Overview refresh metric is a manual action that clears the browser cache
and requests a new snapshot. The existing five-second snapshot cache remains
the REST synchronization boundary; failed
positions requests produce an empty error payload rather than invalidating the
holdings snapshot, and the browser tolerates unavailable news.
Metric normalization is idempotent before being invoked by the dashboard
mutation observer.
The Overview market-value formatter prefers `current_value`, `market_value`,
or `value`, then falls back to `last_price * quantity * multiplier`, and keeps
two decimal places.
The live snapshot normalizer populates `current_value` from the parsed Upstox
holdings before `/api/holdings` is returned, preventing stale local CSV values
from controlling the Web UI total.
Values are matched by `instrument_token`, and Deeper Analysis normalizes
symbols before joining Python recommendations to live holdings.
Deeper Analysis results carry a five-minute server-side freshness window;
expired results are rebuilt from a live holdings/news snapshot.
`--update-holding <path>` preserves the CSV header and row order, updates
broker-derived values by symbol, writes a process-specific temporary file, and
renames it into place only after a complete write.
The response includes `category_counts` and `category_stocks`; the browser
renders these as selectable category cards. News filtering rejects missing,
future, and older-than-30-day `published_time` values before scoring.
The local fallback applies the same age filter and only returns retained
articles, preventing raw saved news from entering `/api/news`.
HTTP responses use `no-store`/`no-cache`; holdings payloads identify their
source as `upstox-live` or `local-fallback`.
news refreshes fall back to filtered `config/portfolio_news.json`, while true
tick-level updates require the Upstox WebSocket and Protobuf subscription path.
The API key, API secret, and access token are supplied through environment
variables or an external secret manager; the access token is read from
`UPSTOX_ACCESS_TOKEN` and never stored by repository scripts. The HTTP client
never enables verbose tracing, and the Upstox client rejects unapproved base
URLs before sending a bearer token.
Successful login returns `303 See Other` with the session cookie and redirects
to `/` for consistent browser behavior.

All symbols live in `namespace folio`.

---

## 2. Data model

```cpp
struct Position {
    std::string exchange;          // "NSE", "BSE", "NFO", …
    std::string tradingSymbol;     // "INFY"
    std::string instrumentToken;   // "NSE_EQ|INE009A01021"
    std::string product;           // "I" (intraday), "D" (delivery/CNC), "M" (margin)

    long   quantity;               // signed: >0 long, <0 short, 0 flat
    long   overnightQuantity;
    long   dayBuyQuantity;
    long   daySellQuantity;

    double averagePrice;           // avg price of the net leg
    double lastPrice;              // LTP
    double closePrice;             // previous-day close (day P&L input)
    double buyPrice, sellPrice;
    double buyValue, sellValue;

    double unrealised;             // MTM on the open leg
    double realised;               // booked P&L for the session
    double pnl;                    // as sent by the broker (unreal + real)
    double value;                  // signed exposure of the open leg

    int multiplier;                // lot size / contract multiplier (>=1)

    double marketValue() const;    // last * qty * multiplier   (signed)
    double exposure()    const;    // |marketValue()|
    double totalPnl()    const;    // unrealised + realised
};
```

### 2.1 JSON → `Position` mapping

The parser (`UpstoxClient::parseOne`) is defensive:

* Missing key      → default (0 / "" / 1 for multiplier).
* Null value       → default.
* String-encoded number ("0.0") → parsed via `std::stod` / `std::stol`.
* Both `trading_symbol` and legacy `tradingsymbol` accepted.
* `multiplier == 0` is treated as `1` (avoids divisor-of-zero downstream).

---

## 3. HTTP layer

`HttpClient` uses **one `CURL*` easy-handle per request**, so instances
are safe to share across threads. `curl_global_init` runs exactly once
per process via `std::call_once`; cleanup is registered with
`std::atexit`.

Timeouts:

* Total request : 30 s (`getPositions` uses default).
* Connect       : 15 s.

Redirects are followed. User-Agent is set explicitly
(`portfolio-health/1.0`).

`HttpResponse::ok()` requires no transport error **and** HTTP 2xx.

---

## 4. `UpstoxClient`

```cpp
UpstoxClient(std::string accessToken,
             std::string baseUrl = "https://api.upstox.com");

PositionsResult getPositions() const;
PositionsResult getHoldings() const;
NewsResult       getNews(const std::string& category = "holdings") const;
LtpResult       fetchLtp(const std::vector<std::string>& instrumentKeys) const;
```

Request headers (both endpoints):

```
Accept:        application/json
Api-Version:   2.0
Authorization: Bearer <accessToken>
```

`getPositions` hits `/v2/portfolio/short-term-positions`; `getHoldings`
hits `/v2/portfolio/long-term-holdings`; `getNews` hits
`/v2/news?category=holdings`; `fetchLtp`
hits `/v2/market-quote/ltp?instrument_key=<csv>` (`|` inside keys is
URL-encoded to `%7C`, all keys sent in a single batch).

`PositionsResult` carries the parsed positions **plus** the raw body and
HTTP status, so callers can `--raw`-dump the payload or log it for
support. `LtpResult::ltp` is a map from `instrument_key` to LTP.

Failure modes surface as strings in `PositionsResult::error`:

| Cause                        | `.error` prefix                  | `ok` |
| ---------------------------- | -------------------------------- | ---- |
| libcurl transport error      | `"network error: ..."`           | false |
| non-2xx HTTP                 | `"HTTP <code> from Upstox: ..."` | false |
| JSON parse failure           | `"JSON parse failure: ..."`      | false |
| `status != "success"`        | `"Upstox status='...' body=..."` | false |
| `data` missing / not array   | `"'data' field missing ..."`     | false |

---

## 5. Analytics

`PortfolioHealth::analyze(const std::vector<Position>&)` walks the
snapshot **twice**:

1. First pass — over open legs (`quantity != 0`) — accumulates exposure,
   invested capital, unrealised P&L, day P&L; counts long/short/win/lose;
   builds contributor lists.
2. Second pass — over **all** rows — sums realised P&L (which the broker
   reports even for closed intraday legs where `quantity == 0`).

Derived metrics:

* `pnlPercent = totalPnl / investedCapital * 100`
* `herfindahlIndex = Σ w_iˆ2` with `w_i = |mv_i| / grossExposure`
* `concentrationPct = max_i(|mv_i|) / grossExposure * 100`
* `healthScore = pnlScore + winScore + diversityScore` clipped to [0,100]

Formula & alert thresholds:
[PortfolioHealthModel.md](PortfolioHealthModel.md).

Top-N lists are sorted once, then truncated to 5.

---

## 6. Rendering

`printReport()`:

* Uses only `<iostream>` and `<iomanip>` — no `std::locale` dependency.
* Indian-numbering grouping (`fmtInr`) is done by hand to keep behaviour
  identical across systems that may lack the `en_IN` locale.
* Table columns are set with `std::setw` for a fixed-width look.

`emitJson()` (in `main.cpp`) writes a `nlohmann::json` object with
`dump(2)`. It's stable enough to consume from `jq` / dashboards.

---

## 7. CLI

```
portfolio_health [--raw] [--json] [--file <path>] [--base <url>]
                 [--portfolio <path>] [--fetch-ltp] [--holdings]
                 [--news] [--web] [--config <path>]
```

* Arguments are parsed in a single linear pass — no getopt dependency.
* Unknown flags → usage on stderr, exit 2.
* Missing token in live mode → exit 1 with a hint pointing at the docs.
* Successful run → exit 0.
* Any downstream error (network, parse, etc.) → exit 1.

---

## 8. Threading

The tool itself is single-threaded. `HttpClient` is written to be
thread-safe so future callers (batch of tokens, multi-account) can reuse
one instance across worker threads.

---

## 9. Testing hooks

* `--file` mode loads any JSON payload with the same envelope
  (`{"status": "success", "data": [...]}`), enabling deterministic tests
  without a live token or network.
* `--portfolio` mode loads a user CSV / JSON — the same code path is
  exercised in unit tests via `PortfolioLoader::loadPortfolio`.
* `config/sample_positions.json` covers the shapes exercised by the analytics:
  a long overnight winner, a long overnight loser, a long intraday
  winner, a fully-closed intraday leg (realised only), a short position.
* `config/my_portfolio.csv` / `config/my_portfolio.json` cover the loader happy path.
* `--json` provides a stable machine-readable output for regression
  diffing.

---

## 10. Build

* C++17, warnings as errors under `-Wall -Wextra -Wpedantic` (Clang/GCC)
  or `/W4 /permissive-` (MSVC).
* `find_package(CURL REQUIRED)` — libcurl comes from the system.
* JSON is resolved in this order: vendored →
  `find_package(nlohmann_json)` → `FetchContent` (opt-in with
  `-DPORTFOLIO_HEALTH_FETCH_JSON=ON`).

## Current Web UI

`WebServer` binds to `127.0.0.1:8080` and serves an embedded browser UI.
Start it with `./run.sh --web` or `./build/portfolio_health --web`.
The UI contains Overview, News, Alerts, Positions, JSON, and Config tabs.

The server exposes these routes:

| Route | Response |
|-------|----------|
| `/` | Browser dashboard |
| `/api/holdings` | Live long-term holdings JSON |
| `/api/positions` | Live open-positions JSON |
| `/api/news` | News filtered to symbols in `config/holding.csv` |
| `/api/deeper-analysis` | On-demand Python/news comparison |
| `/metrics` | Prometheus text exposition |

The bearer token stays in the C++ process and is never sent to the browser.

`/api/deeper-analysis` is not part of the snapshot refresh. It loads
`config/portfolio_news.json`, fetches holdings to map instrument keys to
symbols, invokes `stock_alert_nlp.py`, parses its CSV output, and
returns per-stock saved signal, Python recommendation, comparison, and action.

The `Fundamentals` tab calls `/api/fundamentals?symbol=...` on selection. The
server maps the symbol to its live holding ISIN and returns the Upstox profile,
key ratios, balance sheet, income statement, cash flow, and corporate actions.
The four statement requests use consolidated annual data with `fs=true` where
supported, so the UI can show recent history and detailed line items.

Clicked-stock analysis additionally calls the Upstox Fundamentals endpoints
`/v2/fundamentals/{isin}/profile` and
`/v2/fundamentals/{isin}/key-ratios`. Valuation ratios (P/E, P/B,
EV/EBITDA) are compared with lower-is-better logic; return ratios (ROA, ROE,
ROCE) use higher-is-better logic. A Buy or Sell review requires matching news
signals and a matching fundamentals signal. Missing fundamentals produce an
Unavailable signal and cannot independently trigger an action.

Web authentication uses the process environment variable `FOLIO_LOGIN_CODE`.
`POST /api/login` compares the submitted secret code server-side, creates a
random HttpOnly session cookie, and protects the dashboard/API routes.
`POST /api/logout` removes the session and redirects to `/login`. There is no
phone-number or sign-up route.

## News Sentiment and Alerts

News is requested from Upstox with `category=holdings`, then filtered against
`config/holding.csv`. Each article heading and summary is scored with the C++ keyword
heuristic. Newer articles receive a larger weight, and each stock combines
weighted sentiment, directional article coverage, and agreement:

$$s = \frac{positive\ matches - negative\ matches}{positive\ matches + negative\ matches}$$

The score is zero when no keywords match. A directional decision requires a
score of at least `0.30` or at most `-0.30` and confidence of at least `0.35`.
The UI shows `Consider adding - review`, `Do not add - review risk`, or
`Hold / wait`, with confidence and rationale. Signals are advisory only; no
order API is implemented.

## Prometheus and Grafana

`GET /metrics` exposes:

| Metric | Meaning |
|--------|---------|
| `portfolio_holdings_total` | Number of live long-term holdings |
| `portfolio_market_value` | Current market value |
| `portfolio_total_pnl` | Unrealised plus realised P&L |
| `portfolio_news_articles_total` | Matching news articles |

Prometheus can scrape `http://127.0.0.1:8080/metrics`, and Grafana can use
that Prometheus instance as its data source.
