#include "UpstoxClient.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>
#include <map>
#include <utility>
#include <set>

namespace folio {

using nlohmann::json;

namespace {

// Best-effort numeric parsing: the Upstox payload can occasionally deliver
// numeric fields as JSON strings ("0"), so accept both without failing.
double asDouble(const json& j, const char* key, double fallback = 0.0) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    if (it->is_number()) return it->get<double>();
    if (it->is_string()) {
        try { return std::stod(it->get<std::string>()); }
        catch (...) { return fallback; }
    }
    return fallback;
}

long asLong(const json& j, const char* key, long fallback = 0) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    if (it->is_number_integer()) return it->get<long>();
    if (it->is_number()) return static_cast<long>(it->get<double>());
    if (it->is_string()) {
        try { return std::stol(it->get<std::string>()); }
        catch (...) { return fallback; }
    }
    return fallback;
}

std::string asString(const json& j, const char* key, const std::string& fb = {}) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fb;
    if (it->is_string()) return it->get<std::string>();
    return it->dump();
}

Position parseOne(const json& j) {
    Position p;
    p.exchange        = asString(j, "exchange");
    // Upstox has shipped both spellings across API versions.
    p.tradingSymbol   = asString(j, "trading_symbol",
                        asString(j, "tradingsymbol"));
    p.instrumentToken = asString(j, "instrument_token");
    p.product         = asString(j, "product");

    p.quantity          = asLong(j, "quantity");
    p.overnightQuantity = asLong(j, "overnight_quantity");
    p.dayBuyQuantity    = asLong(j, "day_buy_quantity");
    p.daySellQuantity   = asLong(j, "day_sell_quantity");

    p.averagePrice = asDouble(j, "average_price");
    p.lastPrice    = asDouble(j, "last_price");
    p.closePrice   = asDouble(j, "close_price");
    p.buyPrice     = asDouble(j, "buy_price");
    p.sellPrice    = asDouble(j, "sell_price");
    p.buyValue     = asDouble(j, "buy_value");
    p.sellValue    = asDouble(j, "sell_value");

    p.unrealised = asDouble(j, "unrealised");
    p.realised   = asDouble(j, "realised");
    p.pnl        = asDouble(j, "pnl");
    p.currentValue = asDouble(j, "current_value");
    if (std::fabs(p.currentValue) <= 0.0) p.currentValue = asDouble(j, "market_value");
    p.value = asDouble(j, "value");
    if (std::fabs(p.value) <= 0.0 && std::fabs(p.currentValue) > 0.0) p.value = p.currentValue;

    p.multiplier = static_cast<int>(asLong(j, "multiplier", 1));
    if (p.multiplier == 0) p.multiplier = 1;
    return p;
}

} // namespace

UpstoxClient::UpstoxClient(std::string accessToken, std::string baseUrl)
    : accessToken_(std::move(accessToken)), baseUrl_(std::move(baseUrl)) {}

PositionsResult UpstoxClient::getPositions() const {
    PositionsResult out;
    const std::string url = baseUrl_ + "/v2/portfolio/short-term-positions";

    const std::map<std::string, std::string> headers = {
        {"Accept",        "application/json"},
        {"Api-Version",   "2.0"},
        {"Authorization", "Bearer " + accessToken_},
    };

    const HttpResponse resp = http_.get(url, headers);
    out.httpStatus = resp.statusCode;
    out.rawBody    = resp.body;

    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox";
        return out;
    }

    try {
        const json j = json::parse(resp.body);
        const std::string status = j.value("status", "");
        if (status != "success") {
            out.error = "Upstox status='" + status + "'";
            return out;
        }
        auto it = j.find("data");
        if (it == j.end() || !it->is_array()) {
            out.error = "'data' field missing or not an array";
            return out;
        }
        out.positions.reserve(it->size());
        for (const auto& item : *it) {
            out.positions.push_back(parseOne(item));
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = std::string("JSON parse failure: ") + e.what();
    }
    return out;
}

PositionsResult UpstoxClient::getHoldings() const {
    PositionsResult out;
    const std::string url = baseUrl_ + "/v2/portfolio/long-term-holdings";

    const std::map<std::string, std::string> headers = {
        {"Accept",        "application/json"},
        {"Api-Version",   "2.0"},
        {"Authorization", "Bearer " + accessToken_},
    };

    const HttpResponse resp = http_.get(url, headers);
    out.httpStatus = resp.statusCode;
    out.rawBody    = resp.body;

    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox holdings";
        return out;
    }

    try {
        const json j = json::parse(resp.body);
        if (j.value("status", "") != "success") {
            out.error = "Upstox holdings status != success";
            return out;
        }
        auto it = j.find("data");
        if (it == j.end() || !it->is_array()) {
            out.error = "'data' field missing or not an array";
            return out;
        }
        out.positions.reserve(it->size());
        for (const auto& item : *it) {
            Position holding = parseOne(item);
            if (std::fabs(holding.currentValue) <= 0.0 &&
                std::fabs(holding.lastPrice) > 0.0) {
                holding.currentValue = holding.lastPrice * holding.quantity * holding.multiplier;
            }
            holding.unrealised = holding.pnl;
            out.positions.push_back(std::move(holding));
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = std::string("Holdings JSON parse failure: ") + e.what();
    }
    return out;
}

NewsResult UpstoxClient::getNews(const std::string& category) const {
    NewsResult out;
    const std::string url = baseUrl_ + "/v2/news?category=" + category;

    const std::map<std::string, std::string> headers = {
        {"Accept",        "application/json"},
        {"Authorization", "Bearer " + accessToken_},
    };

    const HttpResponse resp = http_.get(url, headers);
    out.httpStatus = resp.statusCode;
    out.rawBody    = resp.body;

    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox news";
        return out;
    }

    try {
        const json j = json::parse(resp.body);
        if (j.value("status", "") != "success") {
            out.error = "Upstox news status != success";
            return out;
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = std::string("News JSON parse failure: ") + e.what();
    }
    return out;
}

FundamentalsResult UpstoxClient::getFundamentals(const std::string& isin) const {
    FundamentalsResult out;
    if (isin.empty() || isin.size() > 32 ||
        isin.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos) {
        out.error = "invalid ISIN";
        return out;
    }
    const std::map<std::string, std::string> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + accessToken_},
    };
    const auto profile = http_.get(baseUrl_ + "/v2/fundamentals/" + isin + "/profile", headers);
    out.httpStatus = profile.statusCode;
    if (!profile.error.empty()) {
        out.error = "network error: " + profile.error;
        return out;
    }
    if (!profile.ok()) {
        out.error = "HTTP " + std::to_string(profile.statusCode) + " from Upstox fundamentals profile";
        return out;
    }
    const auto fetch = [&](const std::string& endpoint, std::string& output) {
        const auto response = http_.get(baseUrl_ + "/v2/fundamentals/" + isin + endpoint, headers);
        if (!response.error.empty()) {
            out.error = "network error: " + response.error;
            return false;
        }
        if (!response.ok()) {
            out.error = "HTTP " + std::to_string(response.statusCode) + " from Upstox fundamentals";
            return false;
        }
        try {
            const json payload = json::parse(response.body);
            if (payload.value("status", "") != "success") {
                out.error = "Upstox fundamentals response was not successful";
                return false;
            }
        } catch (const std::exception& error) {
            out.error = std::string("fundamentals JSON parse failure: ") + error.what();
            return false;
        }
        output = response.body;
        return true;
    };
    if (!fetch("/key-ratios", out.ratiosBody) ||
        !fetch("/balance-sheet?type=consolidated&fs=true", out.balanceSheetBody) ||
        !fetch("/income-statement?type=consolidated&time_period=yearly&fs=true", out.incomeStatementBody) ||
        !fetch("/cash-flow?type=consolidated&fs=true", out.cashFlowBody) ||
        !fetch("/corporate-actions", out.corporateActionsBody)) return out;
    try {
        const json profileJson = json::parse(profile.body);
        if (profileJson.value("status", "") != "success") {
            out.error = "Upstox fundamentals response was not successful";
            return out;
        }
        out.profileBody = profile.body;
        out.ok = true;
    } catch (const std::exception& error) {
        out.error = std::string("fundamentals JSON parse failure: ") + error.what();
    }
    return out;
}

namespace {

// Upstox requires the '|' inside instrument keys to be URL-encoded as %7C.
std::string urlEncodeKey(const std::string& k) {
    std::string out;
    out.reserve(k.size() + 4);
    for (char c : k) {
        if (c == '|') out += "%7C";
        else          out.push_back(c);
    }
    return out;
}

MarketQuoteResult fetchQuote(const HttpClient& http,
                             const std::string& url,
                             const std::map<std::string, std::string>& headers,
                             const std::string& label) {
    MarketQuoteResult out;
    const HttpResponse resp = http.get(url, headers);
    out.httpStatus = resp.statusCode;
    out.rawBody = resp.body;
    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox " + label;
        return out;
    }
    try {
        const json payload = json::parse(resp.body);
        if (payload.value("status", "") != "success") {
            out.error = "Upstox " + label + " status != success";
            return out;
        }
        if (!payload.contains("data") || !payload["data"].is_object()) {
            out.error = "'data' missing or not an object in Upstox " + label + " response";
            return out;
        }
        out.ok = true;
    } catch (const std::exception& error) {
        out.error = label + " JSON parse failure: " + error.what();
    }
    return out;
}

} // namespace

LtpResult UpstoxClient::fetchLtp(const std::vector<std::string>& keys) const {
    LtpResult out;
    if (keys.empty()) { out.ok = true; return out; }

    std::string joined;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) joined += ',';
        joined += urlEncodeKey(keys[i]);
    }
    const std::string url =
        baseUrl_ + "/v2/market-quote/ltp?instrument_key=" + joined;

    const std::map<std::string, std::string> headers = {
        {"Accept",        "application/json"},
        {"Api-Version",   "2.0"},
        {"Authorization", "Bearer " + accessToken_},
    };

    const HttpResponse resp = http_.get(url, headers);
    out.httpStatus = resp.statusCode;
    out.rawBody    = resp.body;

    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox LTP";
        return out;
    }
    try {
        const json j = json::parse(resp.body);
        if (j.value("status", "") != "success") {
            out.error = "Upstox LTP status != success";
            return out;
        }
        auto data = j.find("data");
        if (data == j.end() || !data->is_object()) {
            out.error = "'data' missing or not an object in LTP response";
            return out;
        }
        for (auto it = data->begin(); it != data->end(); ++it) {
            // The value carries both `instrument_token` (the canonical key)
            // and `last_price`. Prefer the token; fall back to the JSON key.
            const json& v = it.value();
            const std::string key = v.value("instrument_token", it.key());
            const double price = asDouble(v, "last_price", 0.0);
            if (!key.empty() && price > 0.0) out.ltp[key] = price;
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = std::string("LTP parse failure: ") + e.what();
    }
    return out;
}

MarketQuoteResult UpstoxClient::fetchMarketQuotes(const std::vector<std::string>& keys) const {
    if (keys.empty()) return {true, 0, {}, R"({"status":"success","data":{}})"};
    std::string joined;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) joined += ',';
        joined += urlEncodeKey(keys[i]);
    }
    const std::map<std::string, std::string> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + accessToken_},
    };
    return fetchQuote(http_, baseUrl_ + "/v2/market-quote/quotes?instrument_key=" + joined,
                      headers, "full market quotes");
}

WebSocketAuthorizationResult UpstoxClient::authorizeMarketDataFeed() const {
    WebSocketAuthorizationResult out;
    const std::map<std::string, std::string> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + accessToken_},
    };
    const HttpResponse resp = http_.get(
        baseUrl_ + "/v3/feed/market-data-feed/authorize", headers);
    out.httpStatus = resp.statusCode;
    out.rawBody = resp.body;
    if (!resp.error.empty()) {
        out.error = "network error: " + resp.error;
        return out;
    }
    if (!resp.ok()) {
        out.error = "HTTP " + std::to_string(resp.statusCode) + " from Upstox market feed authorization";
        return out;
    }
    try {
        const json payload = json::parse(resp.body);
        if (payload.value("status", "") != "success") {
            out.error = "Upstox market feed authorization status != success";
            return out;
        }
        out.authorizedRedirectUri = payload.value("data", json::object())
            .value("authorized_redirect_uri", "");
        if (out.authorizedRedirectUri.rfind("wss://", 0) != 0) {
            out.error = "Upstox authorization response did not contain a wss:// URL";
            return out;
        }
        out.ok = true;
    } catch (const std::exception& error) {
        out.error = std::string("market feed authorization JSON parse failure: ") + error.what();
    }
    return out;
}

MarketQuoteResult UpstoxClient::fetchOhlcQuotes(const std::vector<std::string>& keys,
                                                const std::string& interval) const {
    static const std::set<std::string> intervals = {"1d", "I1", "I30"};
    if (!intervals.count(interval)) {
        return {false, 0, "interval must be one of: 1d, I1, I30", {}};
    }
    if (keys.empty()) return {true, 0, {}, R"({"status":"success","data":{}})"};
    std::string joined;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) joined += ',';
        joined += urlEncodeKey(keys[i]);
    }
    const std::map<std::string, std::string> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + accessToken_},
    };
    return fetchQuote(http_, baseUrl_ + "/v3/market-quote/ohlc?instrument_key=" + joined +
                          "&interval=" + interval, headers, "OHLC quotes");
}

} // namespace folio
