#pragma once

#include "HttpClient.hpp"
#include "Position.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace folio {

struct PositionsResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;                  // human-readable error, populated on failure
    std::vector<Position> positions;    // parsed positions on success
    std::string rawBody;                // raw JSON (kept for debugging / --raw)
};

struct LtpResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;
    // Keyed by instrument_key (e.g. "NSE_EQ|INE009A01021"). Empty if the
    // instrument was not returned by the API.
    std::unordered_map<std::string, double> ltp;
    std::string rawBody;
};

struct MarketQuoteResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;
    std::string rawBody;
};

struct WebSocketAuthorizationResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;
    std::string authorizedRedirectUri;
    std::string rawBody;
};

struct NewsResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;
    std::string rawBody;
};

struct FundamentalsResult {
    bool ok = false;
    long httpStatus = 0;
    std::string error;
    std::string profileBody;
    std::string ratiosBody;
    std::string balanceSheetBody;
    std::string incomeStatementBody;
    std::string cashFlowBody;
    std::string corporateActionsBody;
};

// Small client around the Upstox v2 REST surface.
//   GET /v2/portfolio/short-term-positions   -> getPositions()
//   GET /v2/market-quote/ltp                 -> fetchLtp()
class UpstoxClient {
public:
    // baseUrl typically "https://api.upstox.com" (no trailing slash).
    UpstoxClient(std::string accessToken,
                 std::string baseUrl = "https://api.upstox.com");

    PositionsResult getPositions() const;

    // Fetch long-term holdings from the authenticated Upstox account.
    PositionsResult getHoldings() const;

    NewsResult getNews(const std::string& category = "holdings") const;

    // Fetch company profile and key ratios for an ISIN.
    FundamentalsResult getFundamentals(const std::string& isin) const;

    // Fetch the last-traded price for a batch of instrument keys.
    // Upstox accepts a comma-separated list, so this issues a single
    // HTTP call.
    LtpResult fetchLtp(const std::vector<std::string>& instrumentKeys) const;

    // Fetch complete v2 market snapshots, including OHLC, depth, volume and
    // circuit limits, for up to 500 instrument keys.
    MarketQuoteResult fetchMarketQuotes(const std::vector<std::string>& instrumentKeys) const;

    // Fetch v3 previous/live OHLC candles for 1d, I1 or I30 intervals.
    MarketQuoteResult fetchOhlcQuotes(const std::vector<std::string>& instrumentKeys,
                                      const std::string& interval = "1d") const;

    // Get the one-time authorized wss:// URL for Market Data Feed V3.
    WebSocketAuthorizationResult authorizeMarketDataFeed() const;

private:
    HttpClient  http_;
    std::string accessToken_;
    std::string baseUrl_;
};

} // namespace folio
