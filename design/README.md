# Portfolio Health — C++ Web UI and CLI

**Version:** `2.0.9`

A C++17 portfolio service with a browser UI and command-line interface. It analyzes a live Upstox account and gives you a **0–100 health score** with P&L, exposure, concentration, diversification, holdings news, and advisory sentiment signals.

### Key Features (v2.0.0+)

- **Stream holdings + live quotes** via WebSocket API
- **Process data in C++/Python** with technical indicators (RSI, MACD, Bollinger Bands) and ML models
- **Generate insights** with signals, risk scores, and sentiment overlays
- **Push to UI** with Angular dashboard and mobile notifications
- **Automate execution** with Orders API when signals meet criteria
- **Ship a versioned runtime contract** in x.x.x format, with the UI popup and build metadata reading the same semver value for release transparency
- **Expose the core implementation as a shared library** so the CLI executable stays lightweight without duplicating source objects

Works with:
- **Portfolio files** (CSV/JSON) — no broker account needed
- **Live Upstox accounts** — connect via OAuth token
- **Offline broker payloads** — analyze saved data

---

## Quick Start

Build and start the local web UI:

```bash
./run.sh --web
```

Set `UPSTOX_API_KEY`, `UPSTOX_API_SECRET`, and `UPSTOX_ACCESS_TOKEN` in the
shell or a `chmod 600` `~/.upstox.env` file before starting live mode. Never
put credentials in `run.sh`, JSON config, source control, or command arguments.
Open [http://127.0.0.1:8080](http://127.0.0.1:8080).
Use `./build/portfolio_health --version` to print the running build version.
Credentials are read only from the environment and are never stored in JSON.

The web UI requires a secret code configured outside the repository:

```bash
printf '%s\n' '070923' > .folio_login_code
chmod 600 .folio_login_code
./run.sh --web
```

The login page has only a secret-code field: there is no phone-number or
sign-up flow. Successful login creates an HttpOnly session cookie with a
one-hour lifetime. Logout
invalidates that session and redirects to `/login`. Do not commit the code or
put it in browser JavaScript.

The browser UI provides Overview, News, Alerts, Deeper analysis, Positions, JSON, and Config tabs. Holdings, positions, and news refresh automatically every 5 seconds while a tab is open. News is filtered to normalized symbols in `config/holding.csv` for the Sombaran Portfolio; the Alerts tab lists every holding, adds a serial number, links each available `Why` explanation to its News article, and provides advisory C++ keyword sentiment without placing orders.

### Architecture guidance

The recommended production pattern for baran-capital-view is a local monolith with an internal task scheduler and async worker queue. This approach keeps the stock API, the browser UI, and the analysis jobs tightly controlled in one trusted process, instead of expanding the system into a distributed workflow mesh too early.

The local-first design provides:
- a single entry point for credential handling
- safe background refresh jobs that do not block the UI
- a clear upgrade path to a distributed worker model only when scaling and isolation become real requirements

### Release notes

* `2.0.8` — Added Bazel build compatibility alongside the existing CMake path, tightened repository hygiene by ignoring generated Python and secret files, refreshed the right-side fix summary to cover the build and repo-cleanup patch, and updated the build helper to select the appropriate backend without changing the live stock API security or the browser contract.
* `2.0.7` — Fixed the broken Alerts tab and duplicate browser-rendering logic, corrected the Deeper analysis category labels and action normalization so the counts and buttons match the same canonical values, refreshed the right-side fix summary to document the shipped patch, preserved the stock API hardening and local-first design, and kept the build metadata aligned with the x.x.x semver format.
* `2.0.6` — Fixed the live Market Value mismatch by preferring the broker-reported `current_value`/`market_value` before falling back to price × quantity, refreshed the right-side release summary so it describes the actual fix shipped in this patch, preserved the upstream stock API hardening and local-first design, and kept the build metadata aligned with the x.x.x semver format.
* `2.0.5` — Fixed the login secret validation so `.folio_login_code` and `FOLIO_LOGIN_CODE` values are trimmed, URL-decoded, and compared safely before a session is created; refreshed the right-side release popup to summarize the fix; preserved the stock API hardening and local-first architecture; and kept the build metadata aligned with the x.x.x semver format.
* `2.0.4` — Tightened the stock API boundary by validating Upstox hosts, disabling unsafe redirects, and blocking non-HTTPS or untrusted broker routes; refreshed the versioned right-side fix summary popup to reflect the current patch; preserved the local-first task scheduler and saved-news fallbacks; added gtest and pytest regression coverage for security and Python analysis flows; and kept the web and CLI release notes consistent with the x.x.x semver format.
* `2.0.3` — Fixed the Deeper analysis backend CLI pathing issue so the Python runner works from any working directory, hardened the external API calls against missing/expired auth and invalid host patterns, improved the news page fallback behavior for empty or stale results, aligned the fundamentals popup layout for the browser UI, introduced a local-first internal task scheduler and async worker queue for background jobs, and refreshed the right-side summary popup to document the actual production fixes.
* `2.0.2` — Fixed the live TLS crash caused by mixing Conan OpenSSL headers with system libcurl's OpenSSL ABI. The default build now lets system libcurl own its verified HTTPS stack and uses OpenSSL-backed helpers only when matching development files are available; redirects to untrusted hosts are blocked.
* `2.0.1` — Hardened the live stock API path with HTTPS-only enforcement, trusted-Host allowlisting for Upstox endpoints, strict input validation for symbols/quantities/prices, secure memory-safe token handling, and a release-notes summary popup that explains the applied security fix in the right-side UI.
* `2.0.0` — Added real-time WebSocket streaming for holdings and market quotes, expanded technical indicators (RSI, MACD, Bollinger Bands, Stochastic), integrated ML models for predictive analysis, added signal generation engine with automated risk scoring, implemented Orders API integration for automated trade execution, added push notifications for mobile/desktop alerts, and created comprehensive WebSocket streaming infrastructure.
* `1.13.0` — Removed embedded broker and news-provider credentials, blocked bearer-token forwarding to untrusted API hosts, disabled credential-bearing HTTP traces, and stopped exposing upstream response bodies in errors.
* `1.12.0` — Moved the Deeper analysis Python implementation and dependencies into baran-capital-view so it no longer runs code from the sibling market directory.
* `1.11.0` — Made Deeper analysis refresh its live snapshot every five minutes, added atomic end-of-day `holding.csv` updates, and made the baran-capital-view Python launcher independent of the current working directory.
* `1.10.0` — Fixed the Deeper analysis iterator error and added a Google News RSS fallback with configured India-market query context when API providers are rate-limited, rejected, or unavailable.
* `1.9.9` — Prevented raw saved news from bypassing Sombaran portfolio and 30-day filtering during fallback; Deeper Analysis categories now score only retained recent articles.
* `1.9.8` — Added Deeper Analysis news categorization counts with clickable stock lists and enforced a maximum 30-day article window for portfolio news.
* `1.9.7` — Disabled API/browser caching and labeled holdings as `upstox-live` or `local-fallback` so invalid-token data is never presented as a live API snapshot.
* `1.9.6` — Matched live Upstox holdings by instrument key, exposed the live portfolio market-value total, and normalized Deeper Analysis symbol matching.
* `1.9.5` — Normalized authenticated Upstox holdings with server-derived `current_value` fields so Web UI Market Value uses the live API holdings response.
* `1.9.4` — Updated Sombaran Portfolio Market Value to use reported holding values when available and display exact paise precision: `INR 490,597.61` for the current CSV snapshot.
* `1.9.3` — Fixed an infinite dashboard DOM-observer loop that caused Firefox to report the page as slow and left every tab stuck on Loading.
* `1.9.2` — Kept the Web UI usable when news or positions requests fail; holdings continue to render and the status line identifies partial data.
* `1.9.1` — Changed successful login from `204 No Content` to a browser-compatible `303 See Other` redirect so Firefox can follow the session login reliably.
* `1.9.0` — Corrected Overview stock and market-value calculations and restored the empty fourth metric slot as a manual Refresh button.
* `1.8.1` — Configured the three Upstox environment exports used by `run.sh --web`; replace them in the local script when rotating credentials.
* `1.8.0` — Added explicit environment exports and validation for the Upstox API key, API secret, and access token when starting the Web UI.
* `1.7.0` — Moved the Upstox access token to `UPSTOX_ACCESS_TOKEN` and added saved-news fallback when an overnight token expiry or transient Upstox news failure occurs. Browser responses now disable caching.
* `1.6.0` — Fixed Deeper analysis to render every live holding, including the full 51-stock portfolio, and to use an explicit saved-news fallback when fresh Python news is unavailable. News remains restricted to Sombaran Portfolio holdings.
* `1.5.1` — Removed the visible refresh-rate card from the Web UI. The dashboard continues using cached background synchronization to avoid unnecessary API traffic.
* `1.5.0` — Added a dismissible, version-aware What is new popup after login. It appears once per version and uses browser-local state so it does not add server requests.
* `1.4.2` — Hardened News filtering by normalizing portfolio symbols before matching, ensuring only Sombaran Portfolio holdings reach the News UI/API.
* `1.4.1` — Added the code-derived version label to the Web UI and sorted News articles newest first using their publication timestamps.
* `1.4.0` — Fixed Deeper analysis so missing news is reported as `No recent news` instead of incorrectly appearing as `Is ok to hold`; fast mode now retries a second symbol query and labels keyword-free articles as `Neutral news`.
* `1.3.1` — Improved the RSI momentum tool with input-order guidance, live price-count hints, sample data, accessible result status, and clearer interpretations.
* `1.3.0` — Added a Data health dashboard and browser-local Alerts acknowledgement controls. Health checks run holdings and news requests in parallel to keep the UI responsive.
* `1.2.1` — Clarified that Prometheus/Grafana monitoring is exposed by the web server through `/metrics`; alert notifications are triggered externally, not by a dashboard button.
* `1.2.0` — Added Prometheus review-action gauges and a ready-to-use rule file for notification triggers through Alertmanager or Grafana.
* `1.1.2` — Removed the raw instrument-key `Stock` column from the Alerts table; serial numbers and company names remain visible.
* `1.1.1` — Confirmed the Alerts page is text/table-only and does not render attached news images.
* `1.1.0` — Alert rows now cover the complete holdings list, include serial numbers, and link the `Why` explanation to matching News articles. Holdings without recent news remain visible with a neutral review state.

The authenticated web API also exposes Upstox market quotes:
`/api/market-quotes?instrument_key=...` returns the full v2 quote snapshot,
including OHLC, depth, volume, and circuit limits. `/api/market-quote/ohlc`
uses the v3 OHLC endpoint and requires `interval=1d`, `I1`, or `I30`. Both
routes accept comma-separated instrument keys and support up to 500 symbols.
The authenticated web API also exposes Upstox market quotes:
`/api/market-quotes?instrument_key=...` returns the full v2 quote snapshot,
including OHLC, depth, volume, and circuit limits. `/api/market-quote/ohlc`
uses the v3 OHLC endpoint and requires `interval=1d`, `I1`, or `I30`. Both
routes accept comma-separated instrument keys and support up to 500 symbols.

### Real-Time Streaming (v2.0.0+)

`/api/market-feed/authorize` obtains the one-time authorized `wss://` URL for
the Upstox Market Data Feed V3. The feed itself uses binary Protobuf frames;
use a WebSocket client with the official `MarketDataFeed.proto` schema to
subscribe and decode live updates.

`/api/stream/holdings` streams real-time holdings updates via WebSocket, allowing
live monitoring of portfolio changes without polling.

`/api/stream/quotes` establishes a WebSocket connection for streaming live market
quotes for subscribed symbols.

### Technical Indicators & Analysis (v2.0.0+)

`/api/technical-indicators?symbol=...&interval=1d` returns calculated technical
indicators including:
- RSI (Relative Strength Index) — Momentum indicator for overbought/oversold conditions
- MACD (Moving Average Convergence Divergence) — Trend-following momentum indicator
- Bollinger Bands — Volatility indicator with upper/lower bands
- Stochastic Oscillator — Momentum indicator comparing closing price to price range
- ATR (Average True Range) — Volatility measurement

`/api/rsi?closes=<oldest,...,latest>&period=14` calculates Wilder RSI locally
from chronological OHLC close values and returns its value, interpretation,
and one-line reason. Values above 70 are labelled overbought, below 30
oversold, and the rest neutral; RSI is a review signal, not a trade guarantee.

### Signal Generation & Insights (v2.0.0+)

`/api/signals?symbol=...` generates trading signals based on technical indicators,
ML predictions, and portfolio metrics:
- Buy/Sell/Hold signals derived from multiple indicator consensus
- Risk scoring combining volatility, concentration, and sector analysis
- Sentiment overlays integrating news analysis and expert signals

`/api/insights` provides comprehensive portfolio insights including:
- Identified opportunities and risks
- Sector concentration analysis
- Correlation matrices between holdings
- Recommended actions with confidence scores

### Mobile Notifications & Push (v2.0.0+)

`/api/notifications/subscribe` registers device endpoints for push notifications

`/api/notifications` configures notification preferences for:
- Price alerts (breakout/breakdown)
- Signal alerts (generated trade opportunities)
- Risk alerts (portfolio health degradation)
- News alerts (important holding developments)

### Orders API Integration (v2.0.0+)

`/api/orders/simulate?symbol=...&quantity=...&price=...` simulates order execution
without placing real trades, showing estimated impact on portfolio health.

`/api/orders/place?symbol=...&quantity=...&order_type=...` places actual orders
when:
- Signal confidence exceeds configured threshold
- Portfolio risk metrics remain within limits
- Manual override approval is provided (requires 2FA)

Orders are placed through the Upstox API with full audit logging and can be
automatically canceled if market conditions change significantly.

The **Fundamentals** tab is available for the Sombaran Portfolio. Select a
holding to load its Upstox Fundamentals data by ISIN: balance sheet, income
statement, cash flow, and corporate actions. These statements complement the
news and ratio analysis; they are informational and do not place orders.

The **Deeper analysis** tab runs only when selected. The C++ server reads the
saved `config/portfolio_news.json`, invokes `stock_alert_nlp.py` against
`config/holding.csv` in fast mode, using one news query per stock and the
lightweight scorer, then compares both signals by stock. It reports
API providers are tried first; when they return no usable articles, the
analyzer uses Google News RSS with the configured `query_suffix` context. It
reports
`Buy / review`, `Hold / review`, or `Sell / review`; these are informational
signals, not investment advice or order instructions. Every live holding is
included in the comparison. If fresh Python news is unavailable but saved
portfolio news exists, the UI identifies that saved-news fallback explicitly;
`No recent news` is shown only when neither source has usable articles.
Deeper analysis refreshes its live holdings and news comparison every five
minutes, so results remain current as the market moves from open to closed.
The local analyzer entry point is [stock_alert_nlp.py](stock_alert_nlp.py); install
its dependencies from [requirements.txt](requirements.txt) and run it from any
working directory.

After the NSE session closes, refresh the report-shaped holdings CSV with:

```cron
30 15 * * 1-5 cd /opt/portfolio_health && . $HOME/.upstox.env && ./build/portfolio_health --update-holding config/holding.csv >> /var/log/portfolio_health-holdings.log 2>&1
```

When a holding is clicked on Overview, the server also queries Upstox
Fundamentals using the holding ISIN:
`/v2/fundamentals/{isin}/profile` and
`/v2/fundamentals/{isin}/key-ratios`. The detail panel shows sector/profile
data and P/E, P/B, ROA, ROE, ROCE, and EV/EBITDA against sector benchmarks.
News and fundamentals must agree for a Buy or Sell review signal; otherwise
the result remains Hold / review.

For the terminal menu, run the binary without arguments:

```bash
./build/portfolio_health
```

Force a rebuild any time with `./run.sh --rebuild`.

---

## Build with CMake

```bash
cmake -S . -B build
cmake --build build
```

The binary is `build/portfolio_health`. Then run it directly or via CMake targets:

```bash
cmake --build build --target run        # sample offline data (no token)
cmake --build build --target run-csv    # your config/my_portfolio.csv
cmake --build build --target run-json   # your config/my_portfolio.json
cmake --build build --target run-live   # live account (needs token)
```

`nlohmann/json` is vendored in `third_party/`, so the build works offline.

## Build with Conan 2

Conan generates the CMake toolchain and dependency files. By default, the
recipe uses the system `libcurl`, which avoids rebuilding libcurl on older
Linux hosts:

```bash
conan install . --output-folder=build/conan --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/build/Release/generators/conan_toolchain.cmake
cmake --build build
```

On a host that can build Conan's libcurl package, enable it explicitly:

```bash
conan install . --output-folder=build/conan --build=missing \
  -o use_conan_libcurl=True
```

### Prerequisites

| Tool         | Debian/Ubuntu                      | RHEL/Fedora                 |
| ------------ | ---------------------------------- | --------------------------- |
| CMake ≥ 3.16 | `apt install cmake`                | `dnf install cmake`         |
| C++ compiler | `apt install build-essential`      | `dnf install gcc-c++`       |
| libcurl -dev | `apt install libcurl4-openssl-dev` | `dnf install libcurl-devel` |

---

## Three Ways to Use

### 1. Your own portfolio (no token needed)

Edit [config/my_portfolio.csv](config/my_portfolio.csv) with your holdings:

```csv
symbol,quantity,average_price,last_price
INFY,50,1500.00,1525.00
RELIANCE,30,2450.00,2510.00
ITC,200,420.00,445.00
```

Only `symbol`, `quantity`, `average_price` are required. Then:

```bash
./build/portfolio_health --portfolio config/my_portfolio.csv
```

Add live prices from Upstox (needs a token and `instrument_key` per row):

```bash
export UPSTOX_ACCESS_TOKEN='eyJ...'
./build/portfolio_health --portfolio config/my_portfolio.csv --fetch-ltp
```

### 2. Live Upstox account

```bash
export UPSTOX_ACCESS_TOKEN='eyJ...'
./build/portfolio_health
```

Fetch long-term holdings and holdings news directly:

```bash
./build/portfolio_health --holdings
./build/portfolio_health --news
```

`--news` saves the filtered response to `config/portfolio_news.json`. The web UI uses the same live endpoints and filtering logic.

Get a token from the [Upstox Developer Console](https://developer.upstox.com).
Use the three environment variables in `run.sh` for live mode; see [Token setup](#token-setup) below.

### 3. Sample data (offline demo)

```bash
./build/portfolio_health --file config/sample_positions.json
```

---

## Sample Output

```
===================== Portfolio Health =====================
Score            : 63 / 100  (grade C)
Open positions   : 6  (long 6, short 0)
Winners / losers : 4 / 2

-- Exposure ------------------------------------------------
Invested capital : INR 5,34,500.00
Gross exposure   : INR 5,85,500.00
Net exposure     : INR 5,85,500.00
Top-1 concentr.  : 18.25%

-- P&L -----------------------------------------------------
Unrealised P&L   : INR   23,450.00  (+4.4%)
Realised   P&L   : INR       500.00
Total      P&L   : INR   23,950.00  (+4.5%)
============================================================
```

Add `--json` for machine-readable output.

---

## CLI Reference

```
portfolio_health [--portfolio <file>] [--file <path>]
                 [--fetch-ltp] [--raw] [--holdings] [--news]
                 [--update-holding <path>]
                 [--web] [--json]
                 [--config <path>] [--base <url>]
```

| Flag               | Meaning                                                        |
| ------------------ | -------------------------------------------------------------- |
| `--portfolio PATH` | Your own portfolio (CSV or JSON).                              |
| `--file PATH`      | Raw Upstox positions payload (JSON) — offline replay.          |
| `--fetch-ltp`      | Fill missing `last_price` from Upstox LTP (needs token + key). |
| `--raw`            | Print the raw broker payload and exit.                         |
| `--holdings`       | Fetch long-term holdings from Upstox.                          |
| `--news`           | Fetch and save news filtered to `config/holding.csv`.           |
| `--update-holding PATH` | Refresh broker values in the report-shaped holdings CSV.   |
| `--web`            | Start the browser UI on `127.0.0.1:8080`.                      |
| `--json`           | Emit analytics as JSON.                                        |
| `--config PATH`    | JSON file holding the access token (see below).                |
| `--base URL`       | Override the API base (default `https://api.upstox.com`).      |

Without `--portfolio` or `--file`, the tool queries Upstox live and needs
`$UPSTOX_ACCESS_TOKEN`.

## Web UI and Monitoring

The local C++ web server exposes:

| Route | Purpose |
|-------|---------|
| `/` | Browser dashboard |
| `/api/holdings` | Live long-term holdings JSON |
| `/api/positions` | Live open positions JSON |
| `/api/news` | News filtered to `config/holding.csv`, with sentiment and signals |
| `/api/deeper-analysis` | On-demand comparison of saved and Python news signals |
| `/api/fundamentals?symbol=SYMBOL` | On-demand four-statement fundamentals |
| `/metrics` | Prometheus text-format portfolio metrics |

The **Data health** dashboard view is on demand. It checks the holdings and
news APIs concurrently and reports loaded holdings, article coverage, news
decisions, and missing prices without adding another background polling loop.

Prometheus can scrape `http://127.0.0.1:8080/metrics`. Grafana can use Prometheus as its data source for portfolio value, total P&L, holding count, matching news article count, and `portfolio_news_review_total{action=...}`. Load [config/prometheus-alerts.yml](config/prometheus-alerts.yml) into Prometheus with an Alertmanager URL, or recreate the same expressions in Grafana Alerting.

Monitoring is implemented in the web service, not as a direct Alerts-page
button: Prometheus or Grafana scrapes `/metrics`, evaluates a rule, and then
triggers its configured notification channel.

The browser dashboard uses cached background synchronization for live updates.
The Data health
tab checks holdings, news article, decision, and missing-price counts with
parallel requests. Alert acknowledgement is stored only in the browser's
local storage and does not change broker or server data. It is not a
tick-by-tick market feed; use the Upstox market-feed WebSocket when sub-second
price streaming is required.

The C++ server keeps a shared 5-second snapshot, so simultaneous dashboard
requests reuse the same Upstox response. If a live news refresh fails, the
server filters and serves `config/portfolio_news.json` until a fresh request
succeeds, keeping the News page available across overnight token expiry.

News alerts use a transparent C++ keyword heuristic with newer articles weighted more heavily. The result combines sentiment, article agreement, and confidence to produce `Consider adding - review`, `Hold / wait`, or `Do not add - review risk`. These are not investment advice and are not connected to order placement.

### Notifications with Prometheus and Grafana

Prometheus can evaluate rules against the C++ `/metrics` endpoint, but it is
not a message-delivery service. Grafana Alerting can send matching alerts by
email or webhook. For WhatsApp, configure the Grafana webhook contact point
to call an approved provider such as Twilio WhatsApp or Meta WhatsApp Cloud
API. Keep provider credentials in Grafana or the webhook service, never in
`config/config.json` or browser JavaScript.

Typical flow:

```text
C++ /metrics -> Prometheus rule -> Grafana Alerting -> email
                                             \-> webhook -> WhatsApp provider
```

The included rules alert when `portfolio_total_pnl` falls below a limit or a
risk-review news signal exists. These notifications should ask the user to
review the article; they should not automatically buy or sell securities.

---

## Token setup

Keep the expiring credential outside JSON and export it in the process environment:

```bash
export UPSTOX_ACCESS_TOKEN='eyJ...'
./load_config.sh
```

```bash
./build/portfolio_health                                   # live account
./build/portfolio_health --portfolio config/my_portfolio.csv --fetch-ltp
```

`config/config.json` contains endpoint settings only. Refresh the environment
variable with a newly issued token before the next live session. The saved
news export provides continuity but is clearly not a fresh market feed.

---

## Documentation

| File | For |
|------|-----|
| [docs/RunSteps.md](docs/RunSteps.md) | Detailed install → build → run walkthrough |
| [docs/API_Reference.md](docs/API_Reference.md) | CLI flags, JSON schema, file formats |
| [docs/JSON_CONFIG.md](docs/JSON_CONFIG.md) | JSON credentials & automation |
| [docs/UPSTOX_TOKEN_SETUP.txt](docs/UPSTOX_TOKEN_SETUP.txt) | Getting an access token |
| [design/HighLevelDesign.md](design/HighLevelDesign.md) | Architecture & flow |
| [design/LowLevelDesign.md](design/LowLevelDesign.md) | Classes, threading, error paths |
| [design/PortfolioHealthModel.md](design/PortfolioHealthModel.md) | Health-score formula |

---

## Project Layout

```
baran-capital-view/
├── CMakeLists.txt          ← build + run targets
├── run.sh                  ← one-shot build + run
├── config/                 ← credentials, portfolios, and sample data
│   ├── my_portfolio.csv    ← your holdings (EDIT ME)
│   ├── my_portfolio.json   ← alternative JSON shape
│   ├── sample_positions.json ← offline demo payload
│   ├── config.json.example  ← JSON credential template
│   └── portfolio_news.json  ← generated filtered news (git-ignored)
├── load_config.sh          ← load token from config/config.json
│
├── include/                ← headers
├── src/                    ← implementation
│   ├── WebServer.cpp        ← localhost web UI and API routes
│   └── ...
├── third_party/nlohmann/   ← vendored nlohmann/json
├── design/                 ← design docs
└── docs/                   ← reference docs
```

---

## Technology

- **Language:** C++17
- **Build:** CMake 3.16+
- **HTTP:** libcurl (thread-safe)
- **JSON:** nlohmann/json (single-header, vendored)
- **Web server:** C++ POSIX sockets, localhost only
- **Monitoring:** Prometheus text exposition at `/metrics`

---

## FAQ

**Do I need a token?** No — start with sample data or your own CSV. A token is only needed for the live account or `--fetch-ltp`.

**How do I enter my portfolio?** Edit `config/my_portfolio.csv` (6 example stocks included).

**Windows?** Use WSL2, or build with MSVC — `CMakeLists.txt` supports it.

**Token expired?** Tokens last until 03:30 IST next day. Get a new one; automate with `refresh_token` (see [docs/JSON_CONFIG.md](docs/JSON_CONFIG.md)).

---

## License

Uses [nlohmann/json](https://github.com/nlohmann/json) (MIT) and libcurl (curl license).
Upstox® is a trademark of RKSV Securities India Pvt Ltd.
