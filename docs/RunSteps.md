# Run Steps

Step-by-step walkthrough — from a fresh checkout to a live health
report.

---

## 1. Install prerequisites

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev
```

### RHEL / CentOS / Fedora

```bash
sudo dnf install -y gcc-c++ cmake libcurl-devel
```

### macOS (Homebrew)

```bash
brew install cmake curl
```

### Windows (MSVC + vcpkg)

```powershell
vcpkg install curl
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

`nlohmann/json` ships in-tree (`third_party/nlohmann/json.hpp`), so
nothing else is required.

---

## 2. Build

### Option A — one-shot script

```bash
cd /home/ritup2404/baran-capital-view
./run.sh
```

`run.sh` builds and then picks an input source in this order:

1. Explicit CLI args (`./run.sh --portfolio path.csv`).
2. `MY_PORTFOLIO` env var pointing at a file.
3. Local `config/my_portfolio.csv` if present (default when you first clone).
4. `UPSTOX_ACCESS_TOKEN` → live account.
5. Fallback: offline demo against `config/sample_positions.json`.

### Option B — manual CMake

```bash
cd /home/ritup2404/baran-capital-view
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artefact: `build/portfolio_health`.

### Optional CMake flags

| Flag                                     | Effect                                              |
| ---------------------------------------- | --------------------------------------------------- |
| `-DCMAKE_BUILD_TYPE=Debug`               | Debug symbols, no optimisations.                     |
| `-DPORTFOLIO_HEALTH_FETCH_JSON=ON`       | Ignore the vendored header, pull from GitHub.        |
| `-DCMAKE_TOOLCHAIN_FILE=<vcpkg.cmake>`   | Use vcpkg for libcurl on Windows.                    |

---

## 3. Try it offline first

Verify the binary is happy against the bundled sample payload:

```bash
./build/portfolio_health --file config/sample_positions.json
```

Expected: a fixed-width report with score `54 / 100`, four open
positions, and one high-concentration alert.

JSON mode for tooling / dashboards:

```bash
./build/portfolio_health --file config/sample_positions.json --json | jq .score
# → 54
```

---

## 3.5 Analyse **your own** portfolio

1. Open `config/my_portfolio.csv` (or copy it somewhere else) and describe
   your holdings:

   ```csv
   symbol,quantity,average_price,last_price
   INFY,50,1500.00,1525.00
   RELIANCE,30,2450.00,2510.00
   ITC,200,420.00,445.00
   ```

   * `symbol`, `quantity`, `average_price` are mandatory.
   * `last_price` is optional — if you skip it, P&L for that row will
     read as 0 (see §3.6 to fetch it automatically).
   * Full column list & JSON variant:
     [docs/API_Reference.md#3-portfolio-file-format](API_Reference.md#3-portfolio-file-format).

2. Run the analysis:

   ```bash
   ./build/portfolio_health --portfolio config/my_portfolio.csv
   ./build/portfolio_health --portfolio config/my_portfolio.json --json
   ```

3. Or point `run.sh` at a file anywhere on disk:

   ```bash
   MY_PORTFOLIO=/data/prod_book.csv ./run.sh
   ```

## 3.6 Auto-fill last prices from Upstox

If you'd rather not maintain `last_price` by hand, add an
`instrument_key` column (Upstox instrument key, e.g.
`NSE_EQ|INE009A01021`) and pass `--fetch-ltp`:

```bash
export UPSTOX_ACCESS_TOKEN="eyJhbGciOi..."
./build/portfolio_health --portfolio config/my_portfolio.csv --fetch-ltp
```

The tool issues a single batched
`GET /v2/market-quote/ltp?instrument_key=...` call and updates the LTP
of every returned row before analytics run. Rows with no
`instrument_key`, or where the API returns no price, keep whatever
`last_price` you supplied (or the average-price default).

---

## 4. Get an Upstox access token

1. Sign in to the [Upstox developer console](https://account.upstox.com/developer/apps)
   and create an app. Note the **API Key** and **API Secret**.
2. Follow the
   [OAuth 2.0 flow](https://upstox.com/developer/api-documentation/authentication):
   1. Direct the user to
      `https://api.upstox.com/v2/login/authorization/dialog?client_id=<API_KEY>&redirect_uri=<REDIRECT>&response_type=code`.
   2. Exchange the returned `code` for an access token via
      `POST https://api.upstox.com/v2/login/authorization/token`.
3. The response contains `access_token`. Access tokens are typically
   valid until 03:30 IST on the next trading day — automate refresh if
   you cron this tool.

Export the token in the shell where you'll run the CLI:

```bash
export UPSTOX_ACCESS_TOKEN="eyJhbGciOi..."
```

**Never commit the token, never pass it on the CLI, never log it.**
This tool reads it from the environment only.

---

## 5. Run live

Formatted report:

```bash
./build/portfolio_health
```

Raw JSON body (useful when reporting issues):

```bash
./build/portfolio_health --raw > positions_$(date +%F).json
```

Machine-readable analytics:

```bash
./build/portfolio_health --json | tee health_$(date +%F).json
```

Behind a custom base URL (sandbox, corporate proxy, …):

```bash
./build/portfolio_health --base https://api-sandbox.upstox.com
```

---

## 6. Cron / systemd example

Snapshot the health report every 15 minutes during market hours:

```cron
*/15 9-15 * * 1-5   . $HOME/.upstox.env && \
                    /opt/portfolio_health/build/portfolio_health --json \
                    >> /var/log/portfolio_health.jsonl
```

Update the report-shaped holdings CSV after the NSE session closes:

```cron
30 15 * * 1-5 cd /opt/portfolio_health && . $HOME/.upstox.env && ./build/portfolio_health --update-holding config/holding.csv >> /var/log/portfolio_health-holdings.log 2>&1
```

`$HOME/.upstox.env` should be a `chmod 600` file that does
`export UPSTOX_ACCESS_TOKEN=...` and is rotated whenever the broker
refresh token expires.

---

## 7. Troubleshooting

| Symptom                                            | Likely cause & fix                                                                                     |
| -------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| `UPSTOX_ACCESS_TOKEN is not set`                   | Export the token (see §4) — or use `--portfolio <file>` / `--file <file>` (no token needed).           |
| `--fetch-ltp requires UPSTOX_ACCESS_TOKEN`         | Export the token before passing `--fetch-ltp`.                                                          |
| `CSV: required columns missing ...`                | Add `symbol,quantity,average_price` header row to your CSV.                                             |
| `CSV row N: stod: no conversion` (or similar)      | Non-numeric value in a numeric column. Check quotes / decimal separator (use `.`).                      |
| `warning: row N: last_price missing ...`           | Fill `last_price` in the file, or add `instrument_key` + pass `--fetch-ltp`.                            |
| `LTP fill failed: HTTP 401 from Upstox LTP`        | Token expired / wrong. Rerun OAuth.                                                                     |
| `HTTP 401 from Upstox: ...`                        | Token expired or wrong. Rerun the OAuth flow.                                                          |
| `HTTP 403 from Upstox: ...`                        | The account doesn't have API access, or the app is disabled in the developer console.                  |
| `network error: Could not resolve host: ...`       | Offline / firewalled machine. Use `--portfolio` or `--file` mode instead.                              |
| `network error: SSL certificate problem`           | System CA bundle is stale. Update `ca-certificates`.                                                    |
| Build error: `CURL not found`                      | Install the `-dev` / `-devel` package (see §1).                                                        |
| Build error: `nlohmann/json.hpp: No such file`     | Vendored copy missing — reclone, or pass `-DPORTFOLIO_HEALTH_FETCH_JSON=ON`.                           |
| Numbers look wrong (locale)                        | Not a locale issue — INR grouping is hand-rolled. File a bug with the raw payload attached (`--raw`).   |

---

## 8. Clean up

```bash
rm -rf build
```

The tool writes no files, no config, no cache. There is nothing else to
clean up.
