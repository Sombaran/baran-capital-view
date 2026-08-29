# API Reference

Three surfaces are documented here:

1. The **upstream Upstox REST endpoints** the tool consumes.
2. The **portfolio file format** — the CSV / JSON shape you write to
   describe your own holdings.
3. The **internal C++, CLI and JSON API** the tool exposes.

---

## 1. Upstox — Get Positions

Source of truth:
<https://upstox.com/developer/api-documentation/get-positions>

### 1.1 Endpoint

```
GET https://api.upstox.com/v2/portfolio/short-term-positions
```

Returns the day's open positions (both intraday and overnight legs) for
the authenticated user.

### 1.2 Request headers

| Header          | Required | Value                                            |
| --------------- | -------- | ------------------------------------------------ |
| `Accept`        | yes      | `application/json`                               |
| `Api-Version`   | yes      | `2.0`                                            |
| `Authorization` | yes      | `Bearer <access_token>`                          |

The access token is obtained through the
[Upstox OAuth 2.0 flow](https://upstox.com/developer/api-documentation/authentication).

### 1.3 Response envelope

```json
{
  "status": "success",
  "data":   [ Position, Position, ... ]
}
```

`status` is `"success"` on 2xx. Any other value indicates an application
error — the transport still returned 200, so check the field explicitly.

### 1.4 `Position` object (the fields this tool reads)

| Field                     | Type    | Meaning                                                                       |
| ------------------------- | ------- | ----------------------------------------------------------------------------- |
| `exchange`                | string  | Exchange code — `NSE`, `BSE`, `NFO`, …                                        |
| `trading_symbol`          | string  | Human-readable ticker. Legacy name `tradingsymbol` also accepted.             |
| `instrument_token`        | string  | Upstox instrument key, e.g. `NSE_EQ\|INE009A01021`.                           |
| `product`                 | string  | `I` intraday, `D` delivery/CNC, `M` margin.                                   |
| `multiplier`              | int     | Contract / lot multiplier (defaults to 1 for equities).                       |
| `quantity`                | int     | Net position quantity (buy − sell). Signed: positive long, negative short.    |
| `overnight_quantity`      | int     | Carry-forward quantity from the previous session.                             |
| `day_buy_quantity`        | int     | Quantity bought today.                                                        |
| `day_sell_quantity`       | int     | Quantity sold today.                                                          |
| `average_price`           | number  | Weighted-avg price of the net leg.                                            |
| `last_price`              | number  | LTP.                                                                          |
| `close_price`             | number  | Previous-day close (used for the day P&L calculation).                        |
| `buy_price` / `sell_price`| number  | Avg buy/sell price for today.                                                 |
| `buy_value` / `sell_value`| number  | Cash value of today's buys/sells.                                             |
| `unrealised`              | number  | MTM P&L on the open leg.                                                      |
| `realised`                | number  | Booked P&L for the session (may be non-zero even when `quantity == 0`).       |
| `pnl`                     | number  | `unrealised + realised`, as sent by the broker.                               |
| `value`                   | number  | Signed exposure of the open leg.                                              |

> The Upstox payload contains additional fields (`day_buy_value`,
> `overnight_buy_amount`, …). This tool ignores them; they are not
> discarded from the raw dump (`--raw`).

### 1.5 Sample response (excerpt)

See [`sample_positions.json`](../config/sample_positions.json) for a full,
tool-parsable example. Minimal shape:

```json
{
  "status": "success",
  "data": [
    {
      "exchange": "NSE",
      "trading_symbol": "INFY",
      "instrument_token": "NSE_EQ|INE009A01021",
      "product": "D",
      "quantity": 100,
      "average_price": 1500.0,
      "last_price": 1525.0,
      "close_price": 1510.0,
      "unrealised": 2500.0,
      "realised": 0.0,
      "multiplier": 1
    }
  ]
}
```

### 1.6 Error responses

Non-2xx responses use Upstox's standard error envelope. This tool wraps
them into `PositionsResult::error` (see below) rather than exposing the
raw structure.

### 1.7 Upstox — Get LTP (used by `--fetch-ltp`)

Source of truth:
<https://upstox.com/developer/api-documentation/get-ltp>

```
GET https://api.upstox.com/v2/market-quote/ltp?instrument_key=<key1>,<key2>,...
```

Headers are identical to §1.2. `|` inside `instrument_key` values is URL-
encoded as `%7C` by this tool.

Response:

```json
{
  "status": "success",
  "data": {
    "NSE_EQ:INFY": {
      "last_price": 1525.0,
      "instrument_token": "NSE_EQ|INE009A01021"
    },
    "NSE_EQ:TCS": {
      "last_price": 3720.0,
      "instrument_token": "NSE_EQ|INE467B01029"
    }
  }
}
```

The tool keys the returned prices on `data[*].instrument_token` (the
canonical instrument key), so the outer JSON keys don't have to match
your input verbatim.

---

## 3. Portfolio file format

Used with `--portfolio <path>`. The extension picks the parser (`.csv`,
`.tsv`, `.json`); anything else is sniffed from the first non-whitespace
character (`{` or `[` → JSON, otherwise CSV).

### 3.1 Columns / fields

| Name              | Required | Type   | Default        | Notes                                                     |
| ----------------- | -------- | ------ | -------------- | --------------------------------------------------------- |
| `symbol`          | yes      | string | —              | Ticker as shown by your broker (e.g. `INFY`).              |
| `quantity`        | yes      | int    | —              | Signed. Negative = short position.                         |
| `average_price`   | yes      | number | —              | Weighted-avg cost of the net leg.                          |
| `last_price`      | no       | number | `average_price`| Current LTP. Missing rows produce P&L=0 unless `--fetch-ltp` is used. |
| `close_price`     | no       | number | `last_price`   | Previous-day close (for day P&L).                          |
| `exchange`        | no       | string | `NSE`          | `NSE`, `BSE`, `NFO`, ...                                   |
| `product`         | no       | string | `D`            | `D` delivery/CNC, `I` intraday, `M` margin.                |
| `multiplier`      | no       | int    | 1              | Lot / contract multiplier.                                 |
| `instrument_key`  | no       | string | —              | Upstox instrument key. **Required only for `--fetch-ltp`.**|

### 3.2 CSV shape

* Header row required.
* Column order is arbitrary — the parser matches by name (case-insensitive).
* Blank lines and lines starting with `#` are ignored.
* Simple `,` splitting; wrap fields containing commas in double quotes.

```csv
# my_portfolio.csv
symbol,quantity,average_price,last_price,instrument_key
INFY,50,1500.00,1525.00,NSE_EQ|INE009A01021
RELIANCE,30,2450.00,2510.00,NSE_EQ|INE002A01018
ITC,200,420.00,445.00,NSE_EQ|INE154A01025
```

### 3.3 JSON shape

Either a top-level array or an object with a `portfolio` array:

```json
{
  "portfolio": [
    { "symbol": "INFY", "quantity": 50, "average_price": 1500, "last_price": 1525 },
    { "symbol": "RELIANCE", "quantity": 30, "average_price": 2450, "last_price": 2510 }
  ]
}
```

For JSON, `trading_symbol` is accepted as an alias for `symbol`, and
`instrument_token` for `instrument_key`.

### 3.4 Warnings vs errors

* Missing required column / field → hard error, exit 1.
* Missing `last_price` on a row → warning on stderr; row still analysed
  (with P&L 0) unless `--fetch-ltp` supplies the value.
* Empty `symbol` → warning, row skipped.

---

## 4. Internal API — C++

Namespace: `folio`.

### 4.1 `HttpClient` — `include/HttpClient.hpp`

```cpp
struct HttpResponse {
    long        statusCode;   // HTTP status (0 on transport failure)
    std::string body;         // response body
    std::string error;        // non-empty on transport-level failure
    bool ok() const;          // error empty && 200..299
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers,
                     long timeoutSeconds = 30) const;
};
```

Thread-safe (per-request easy handle). `curl_global_init` runs once via
`CurlGlobal::ensureInit()`.

### 4.2 `UpstoxClient` — `include/UpstoxClient.hpp`

```cpp
struct PositionsResult {
    bool                  ok;
    long                  httpStatus;
    std::string           error;       // human-readable error, filled on failure
    std::vector<Position> positions;   // parsed positions on success
    std::string           rawBody;     // raw JSON (for --raw and logging)
};

struct LtpResult {
    bool ok;
    long httpStatus;
    std::string error;
    std::unordered_map<std::string, double> ltp;   // key = instrument_key
    std::string rawBody;
};

class UpstoxClient {
public:
    UpstoxClient(std::string accessToken,
                 std::string baseUrl = "https://api.upstox.com");

    PositionsResult getPositions() const;
    LtpResult       fetchLtp(const std::vector<std::string>& instrumentKeys) const;
};
```

### 4.3 `Position` — `include/Position.hpp`

See the field table in §1.4 and the "Data model" section of
[LowLevelDesign](../design/LowLevelDesign.md#2-data-model).

Convenience methods:

```cpp
double marketValue() const;  // lastPrice * quantity * multiplier (signed)
double exposure()    const;  // |marketValue()|
double totalPnl()    const;  // unrealised + realised
```

### 4.4 `PortfolioHealth` — `include/PortfolioHealth.hpp`

```cpp
struct Contributor { std::string label; double value; };

struct PortfolioHealth {
    std::size_t openPositions, longCount, shortCount,
                winningCount, losingCount;

    double grossExposure, netExposure, longExposure, shortExposure;
    double investedCapital;
    double totalUnrealised, totalRealised, totalPnl, dayPnl;
    double pnlPercent, concentrationPct, herfindahlIndex;

    int         healthScore;   // 0..100
    std::string healthGrade;   // "A".."F"
    std::vector<std::string> alerts;

    std::vector<Contributor> topGainers, topLosers, topExposures;
};

PortfolioHealth analyze(const std::vector<Position>& positions);
void printReport(const std::vector<Position>& positions,
                 const PortfolioHealth& health);
```

Scoring formulas and alert thresholds live in
[PortfolioHealthModel](../design/PortfolioHealthModel.md).

### 4.5 `PortfolioLoader` — `include/PortfolioLoader.hpp`

```cpp
struct PortfolioLoadResult {
    bool ok;
    std::string error;
    std::vector<Position> positions;    // ready for analyze()
    std::vector<std::string> warnings;
};

PortfolioLoadResult loadPortfolio(const std::string& path);
```

Reads a CSV or JSON file (see §3), applies defaults, and computes
`unrealised`, `pnl`, `value` so the loaded positions plug straight into
`analyze()`.

---

## 5. CLI

```
portfolio_health [--portfolio <path>] [--file <path>]
                 [--fetch-ltp] [--raw] [--json] [--base <url>]
```

| Flag             | Description                                                                       |
| ---------------- | --------------------------------------------------------------------------------- |
| `--portfolio P`  | User-supplied portfolio (CSV or JSON, format in §3).                              |
| `--file P`       | Raw Upstox positions payload (JSON) — offline replay.                             |
| `--fetch-ltp`    | Fill missing `last_price` in `--portfolio` mode from Upstox LTP. Needs a token and `instrument_key`. |
| `--raw`          | Print the raw broker payload and exit (only valid in live / `--file` modes).       |
| `--json`         | Emit the analytics as JSON (§6).                                                  |
| `--base URL`     | Override the API base (default `https://api.upstox.com`).                          |
| `-h`, `--help`   | Print usage.                                                                       |

`--portfolio` and `--file` are mutually exclusive.

Exit codes:

| Code | Meaning                                                             |
| ---- | ------------------------------------------------------------------- |
| 0    | Report generated successfully.                                      |
| 1    | Runtime failure (no token / network error / bad payload / read err).|
| 2    | Usage error (unknown flag, missing value, incompatible combination).|

Environment:

| Variable                | Purpose                                                 |
| ----------------------- | ------------------------------------------------------- |
| `UPSTOX_ACCESS_TOKEN`   | Bearer token. Required for live mode and `--fetch-ltp`. |
| `MY_PORTFOLIO`          | Used by `run.sh` to point at a portfolio file.          |

---

## 6. JSON output (`--json`)

Stable schema:

```json
{
  "score": 68,
  "grade": "C",
  "open_positions": 4,
  "long_count": 3,
  "short_count": 1,
  "winning_count": 2,
  "losing_count": 2,
  "invested_capital": 622000.0,
  "gross_exposure":   629000.0,
  "net_exposure":     439000.0,
  "long_exposure":    534000.0,
  "short_exposure":    95000.0,
  "total_unrealised": 1000.0,
  "total_realised":   1200.0,
  "total_pnl":        2200.0,
  "day_pnl":          1000.0,
  "pnl_percent":      0.35,
  "top_concentration_pct": 47.30,
  "herfindahl_index":      0.3231,
  "alerts": [
    "High single-name concentration (47.30%)."
  ],
  "top_gainers":   [{"symbol": "RELIANCE", "value": 4000.0}, ...],
  "top_losers":    [{"symbol": "HINDUNILVR", "value": -3000.0}, ...],
  "top_exposures": [{"symbol": "TCS", "value": 297500.0}, ...]
}
```

Field names are `snake_case`, values are numbers (no locale formatting).
Safe to `jq` / feed into dashboards.
