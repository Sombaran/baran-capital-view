// Portfolio Health CLI ---------------------------------------------------
//
// Three modes of operation, mutually exclusive:
//
//   1. Live Upstox account (default)
//        export UPSTOX_ACCESS_TOKEN=...
//        ./portfolio_health
//      Calls GET /v2/portfolio/short-term-positions and reports on the
//      open positions in the authenticated account.
//
//   2. Raw broker payload (offline)
//        ./portfolio_health --file dump.json
//      Consumes a saved response body from the same endpoint. Useful for
//      replaying issues or running in an air-gapped environment.
//
//   3. USER PORTFOLIO (this project's headline feature)
//        ./portfolio_health --portfolio config/my_portfolio.csv
//        ./portfolio_health --portfolio config/my_portfolio.json
//      Analyses a portfolio you describe yourself (list of companies,
//      quantities, avg price, optional last price). No broker account
//      needed. If --fetch-ltp is passed AND UPSTOX_ACCESS_TOKEN is set
//      AND the input carries `instrument_key`, missing last prices are
//      filled from the Upstox market-quote LTP endpoint.
//
// Reference: https://upstox.com/developer/api-documentation/get-positions

#include "PortfolioHealth.hpp"
#include "PortfolioLoader.hpp"
#include "UpstoxClient.hpp"
#include "WebServer.hpp"

#include <nlohmann/json.hpp>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef PORTFOLIO_HEALTH_VERSION
#define PORTFOLIO_HEALTH_VERSION "unknown"
#endif

using folio::LtpResult;
using folio::PortfolioHealth;
using folio::PortfolioLoadResult;
using folio::Position;
using folio::PositionsResult;
using folio::UpstoxClient;
using folio::WebServer;
using nlohmann::json;

namespace {

struct Options {
    bool raw = false;
    bool asJson = false;
    bool fetchLtp = false;
    bool holdings = false;
    bool news = false;
    bool web = false;
    bool version = false;
    std::string updateHolding;
    std::string file;       // --file:      raw broker payload
    std::string portfolio;  // --portfolio: user-supplied CSV/JSON
    std::string config;     // --config:    JSON credentials file
    std::string baseUrl = "https://api.upstox.com";
};

int usage() {
    std::cerr <<
      "Usage: portfolio_health [options]\n"
      "\n"
      "Input (choose one; live account by default):\n"
      "  --portfolio <path>   Your own portfolio (CSV or JSON). See\n"
      "                       docs/API_Reference.md#portfolio-file-format.\n"
      "  --file <path>        Raw Upstox positions payload (JSON).\n"
      "\n"
      "Options:\n"
      "  --fetch-ltp          Fill missing last_price values from the\n"
      "                       Upstox LTP endpoint (needs a token AND\n"
      "                       instrument_key on every input row).\n"
      "  --raw                Print the raw broker payload and exit.\n"
    "  --holdings           Fetch long-term holdings instead of positions.\n"
        "  --news               Fetch holdings news and save config/portfolio_news.json.\n"
    "  --update-holding <path> Fetch holdings and atomically refresh the CSV values.\n"
    "  --web                Start the local browser UI on port 8080.\n"
      "  --json               Emit analytics as JSON (stable schema).\n"
    "  --config <path>      Reserved for compatibility; tokens use the environment.\n"
    "  --version            Print the application version.\n"
      "  --base <url>         Override the API base (default\n"
      "                       https://api.upstox.com).\n"
      "  -h, --help           This help.\n"
      "\n"
            "Token source (live and --fetch-ltp modes):\n"
            "  $UPSTOX_ACCESS_TOKEN\n"
            "Web credential sources:\n"
            "  $UPSTOX_API_KEY, $UPSTOX_API_SECRET, $UPSTOX_ACCESS_TOKEN\n";
    return 2;
}

bool trustedBaseUrl(const std::string& baseUrl);

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if      (a == "--raw")       out.raw = true;
        else if (a == "--json")      out.asJson = true;
        else if (a == "--fetch-ltp") out.fetchLtp = true;
        else if (a == "--holdings")  out.holdings = true;
        else if (a == "--news")      out.news = true;
        else if (a == "--web")       out.web = true;
        else if (a == "--version")  out.version = true;
        else if (a == "--update-holding" && i + 1 < argc) out.updateHolding = argv[++i];
        else if (a == "--file"      && i + 1 < argc) out.file      = argv[++i];
        else if (a == "--portfolio" && i + 1 < argc) out.portfolio = argv[++i];
        else if (a == "--config"    && i + 1 < argc) out.config    = argv[++i];
        else if (a == "--base"      && i + 1 < argc) out.baseUrl   = argv[++i];
        else if (a == "-h" || a == "--help") return false;
        else {
            std::cerr << "Unknown argument: " << a << "\n";
            return false;
        }
    }
    if (!out.file.empty() && !out.portfolio.empty()) {
        std::cerr << "--file and --portfolio are mutually exclusive.\n";
        return false;
    }
    if (!out.updateHolding.empty() && (!out.file.empty() || !out.portfolio.empty())) {
        std::cerr << "--update-holding requires a live Upstox account.\n";
        return false;
    }
    if (!trustedBaseUrl(out.baseUrl)) {
        std::cerr << "Unsupported stock API base URL. Only official Upstox endpoints are allowed.\n";
        return false;
    }
    return true;
}

bool trustedBaseUrl(const std::string& baseUrl) {
    return baseUrl == "https://api.upstox.com" ||
           baseUrl == "https://api.upstox.com/" ||
           baseUrl == "https://api-sandbox.upstox.com" ||
           baseUrl == "https://api-sandbox.upstox.com/";
}

std::string readAll(std::istream& in) {
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}

std::unordered_set<std::string> readHoldingSymbols(const std::string& path) {
    std::unordered_set<std::string> symbols;
    std::ifstream in(path);
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (header) { header = false; continue; }
        const auto comma = line.find(',');
        const std::string symbol = line.substr(0, comma);
        if (!symbol.empty()) symbols.insert(symbol);
    }
    return symbols;
}

std::string filterNewsToSymbols(const std::string& body,
                                const std::unordered_set<std::string>& symbols,
                                const std::vector<Position>& accountHoldings) {
    const json source = json::parse(body);
    json filtered = source;
    filtered["data"] = json::object();
    auto data = source.find("data");
    if (data == source.end() || !data->is_object()) return filtered.dump(2);

    std::unordered_set<std::string> accountKeys;
    for (const auto& holding : accountHoldings) {
        if (symbols.count(holding.tradingSymbol) && !holding.instrumentToken.empty()) {
            accountKeys.insert(holding.instrumentToken);
        }
    }
    int articleCount = 0;
    for (auto it = data->begin(); it != data->end(); ++it) {
        if (accountKeys.count(it.key())) {
            filtered["data"][it.key()] = it.value();
            articleCount += static_cast<int>(it.value().size());
        }
    }
    if (filtered.contains("metadata") && filtered["metadata"].contains("page")) {
        filtered["metadata"]["page"]["total_records"] = articleCount;
        filtered["metadata"]["page"]["total_pages"] = articleCount ? 1 : 0;
    }
    return filtered.dump(2);
}

// Resolve the bearer token from the environment.
std::string resolveToken() {
    if (const char* env = std::getenv("UPSTOX_ACCESS_TOKEN"); env && *env)
        return env;
    return {};
}

// Reuse the same parser code path as the live client for the --file mode.
PositionsResult parseBrokerBody(const std::string& body) {
    PositionsResult out;
    out.rawBody = body;
    try {
        const json j = json::parse(body);
        if (j.value("status", "") != "success") {
            out.error = "status != success in payload";
            return out;
        }
        auto it = j.find("data");
        if (it == j.end() || !it->is_array()) {
            out.error = "'data' missing or not an array";
            return out;
        }
        for (const auto& row : *it) {
            Position p;
            p.exchange        = row.value("exchange", "");
            p.tradingSymbol   = row.value("trading_symbol",
                                row.value("tradingsymbol", std::string{}));
            p.instrumentToken = row.value("instrument_token", "");
            p.product         = row.value("product", "");
            p.quantity          = row.value("quantity", 0L);
            p.overnightQuantity = row.value("overnight_quantity", 0L);
            p.dayBuyQuantity    = row.value("day_buy_quantity", 0L);
            p.daySellQuantity   = row.value("day_sell_quantity", 0L);
            p.averagePrice = row.value("average_price", 0.0);
            p.lastPrice    = row.value("last_price", 0.0);
            p.closePrice   = row.value("close_price", 0.0);
            p.buyPrice     = row.value("buy_price", 0.0);
            p.sellPrice    = row.value("sell_price", 0.0);
            p.buyValue     = row.value("buy_value", 0.0);
            p.sellValue    = row.value("sell_value", 0.0);
            p.unrealised = row.value("unrealised", 0.0);
            p.realised   = row.value("realised", 0.0);
            p.pnl        = row.value("pnl", 0.0);
            p.currentValue = row.value("current_value", 0.0);
            if (std::fabs(p.currentValue) <= 0.0) p.currentValue = row.value("market_value", 0.0);
            p.value = row.value("value", 0.0);
            if (std::fabs(p.value) <= 0.0 && std::fabs(p.currentValue) > 0.0) p.value = p.currentValue;
            p.multiplier = row.value("multiplier", 1);
            if (p.multiplier == 0) p.multiplier = 1;
            out.positions.push_back(std::move(p));
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = std::string("parse error: ") + e.what();
    }
    return out;
}

// Fill last_price on the caller-supplied portfolio from Upstox. Only rows
// whose instrument_token (== instrument_key in the input file) is present
// and whose current last_price equals average_price (i.e. it was defaulted
// during loading) are updated. Returns the number of rows filled.
std::size_t fillLtp(std::vector<Position>& positions,
                    const std::string& baseUrl,
                    const std::string& token,
                    std::string& errorOut) {
    std::vector<std::string> keys;
    for (const auto& p : positions) {
        if (!p.instrumentToken.empty()) keys.push_back(p.instrumentToken);
    }
    if (keys.empty()) {
        errorOut = "no instrument_key present on any row";
        return 0;
    }
    UpstoxClient client(token, baseUrl);
    const LtpResult r = client.fetchLtp(keys);
    if (!r.ok) { errorOut = r.error; return 0; }

    std::size_t filled = 0;
    for (auto& p : positions) {
        auto it = r.ltp.find(p.instrumentToken);
        if (it == r.ltp.end()) continue;
        p.lastPrice = it->second;
        if (p.closePrice == 0.0) p.closePrice = p.lastPrice;
        if (std::fabs(p.value) <= 0.0) p.value = p.lastPrice * p.quantity * p.multiplier;
        p.unrealised = (p.lastPrice - p.averagePrice) *
                        p.quantity * p.multiplier;
        p.pnl        = p.unrealised + p.realised;
        ++filled;
    }
    return filled;
}

void emitJson(const PortfolioHealth& h) {
    json j;
    j["score"] = h.healthScore;
    j["grade"] = h.healthGrade;
    j["open_positions"] = h.openPositions;
    j["long_count"]  = h.longCount;
    j["short_count"] = h.shortCount;
    j["winning_count"] = h.winningCount;
    j["losing_count"]  = h.losingCount;
    j["invested_capital"] = h.investedCapital;
    j["gross_exposure"]   = h.grossExposure;
    j["net_exposure"]     = h.netExposure;
    j["long_exposure"]    = h.longExposure;
    j["short_exposure"]   = h.shortExposure;
    j["total_unrealised"] = h.totalUnrealised;
    j["total_realised"]   = h.totalRealised;
    j["total_pnl"]        = h.totalPnl;
    j["day_pnl"]          = h.dayPnl;
    j["pnl_percent"]      = h.pnlPercent;
    j["top_concentration_pct"] = h.concentrationPct;
    j["herfindahl_index"]      = h.herfindahlIndex;
    j["alerts"] = h.alerts;
    auto dumpRows = [](const std::vector<folio::Contributor>& rows) {
        json a = json::array();
        for (const auto& r : rows) a.push_back({{"symbol", r.label}, {"value", r.value}});
        return a;
    };
    j["top_gainers"]   = dumpRows(h.topGainers);
    j["top_losers"]    = dumpRows(h.topLosers);
    j["top_exposures"] = dumpRows(h.topExposures);
    std::cout << j.dump(2) << "\n";
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(field);
    if (!line.empty() && line.back() == ',') fields.emplace_back();
    return fields;
}

void updateHoldingCsv(const std::string& path,
                      const std::vector<Position>& holdings) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open holdings CSV: " + path);
    std::string headerLine;
    if (!std::getline(input, headerLine)) throw std::runtime_error("Holdings CSV is empty");
    const auto headers = splitCsvLine(headerLine);
    std::unordered_map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < headers.size(); ++i) columns[headers[i]] = i;
    for (const auto& name : {"Symbol", "Net Qty", "Avg. Price", "LTP", "Current Value", "Overall P&L"}) {
        if (!columns.count(name)) throw std::runtime_error("Holdings CSV missing column: " + std::string(name));
    }

    std::unordered_map<std::string, const Position*> latest;
    for (const auto& holding : holdings) latest[holding.tradingSymbol] = &holding;
    const std::string temporary = path + ".tmp." + std::to_string(static_cast<long long>(getpid()));
    std::ofstream output(temporary);
    if (!output) throw std::runtime_error("Cannot create temporary holdings CSV");
    output << headerLine << '\n' << std::fixed << std::setprecision(2);
    std::string line;
    while (std::getline(input, line)) {
        auto fields = splitCsvLine(line);
        if (!fields.empty() && fields.size() > columns.at("Symbol")) {
            const auto it = latest.find(fields[columns.at("Symbol")]);
            if (it != latest.end()) {
                const Position& holding = *it->second;
                fields.resize(headers.size());
                fields[columns.at("Net Qty")] = std::to_string(holding.quantity);
                fields[columns.at("Avg. Price")] = std::to_string(holding.averagePrice);
                fields[columns.at("LTP")] = std::to_string(holding.lastPrice);
                fields[columns.at("Current Value")] = std::to_string(
                    holding.value != 0.0 ? holding.value : holding.marketValue());
                fields[columns.at("Overall P&L")] = std::to_string(holding.pnl);
            }
        }
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i) output << ',';
            output << fields[i];
        }
        output << '\n';
    }
    output.close();
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("Cannot replace holdings CSV: " + path);
    }
}

} // namespace

int runCli(int argc, char** argv) {
    Options opts;
    if (!parse(argc, argv, opts)) return usage();
    if (opts.version) {
        std::cout << PORTFOLIO_HEALTH_VERSION << "\n";
        return 0;
    }
    if (!trustedBaseUrl(opts.baseUrl)) {
        std::cerr << "Unsupported --base URL; only Upstox HTTPS endpoints are allowed.\n";
        return 1;
    }

    if (opts.web) {
        const std::string token = resolveToken();
        const char* apiKey = std::getenv("UPSTOX_API_KEY");
        const char* apiSecret = std::getenv("UPSTOX_API_SECRET");
        if (token.empty() || !apiKey || !*apiKey || !apiSecret || !*apiSecret) {
            std::cerr << "--web requires UPSTOX_API_KEY, UPSTOX_API_SECRET, "
                         "and UPSTOX_ACCESS_TOKEN\n";
            return 1;
        }
        UpstoxClient client(token, opts.baseUrl);
        WebServer server(client, "config/holding.csv");
        return server.run();
    }

    std::vector<Position> positions;
    std::string rawBody; // populated for --raw

    // -------- 1. Acquire the positions --------------------------------
    if (!opts.portfolio.empty()) {
        const PortfolioLoadResult r = folio::loadPortfolio(opts.portfolio);
        if (!r.ok) {
            std::cerr << "Cannot load portfolio: " << r.error << "\n";
            return 1;
        }
        positions = r.positions;
        for (const auto& w : r.warnings) std::cerr << "warning: " << w << "\n";

        if (opts.fetchLtp) {
            const std::string tok = resolveToken();
            if (tok.empty()) {
                std::cerr << "--fetch-ltp requires UPSTOX_ACCESS_TOKEN\n";
                return 1;
            }
            std::string ltpErr;
            const std::size_t n = fillLtp(positions, opts.baseUrl, tok, ltpErr);
            if (!ltpErr.empty()) {
                std::cerr << "warning: LTP fill failed: " << ltpErr << "\n";
            } else {
                std::cerr << "info: filled last_price for " << n
                          << " row(s) from Upstox LTP.\n";
            }
        }
    } else if (!opts.file.empty()) {
        std::ifstream in(opts.file);
        if (!in) { std::cerr << "Cannot open file: " << opts.file << "\n"; return 1; }
        const PositionsResult r = parseBrokerBody(readAll(in));
        if (!r.ok) { std::cerr << "Bad payload: " << r.error << "\n"; return 1; }
        positions = r.positions;
        rawBody   = r.rawBody;
    } else {
        const std::string tok = resolveToken();
        if (tok.empty()) {
            std::cerr <<
              "No access token found.\n"
              "Provide it in one of these ways:\n"
              "  1) export UPSTOX_ACCESS_TOKEN=..., or\n"
              "  2) --portfolio <file> to analyse your own list, or\n"
              "  3) --file <sample.json> for offline replay.\n";
            return 1;
        }
        UpstoxClient client(tok, opts.baseUrl);
        if (!opts.updateHolding.empty()) {
            const auto holdings = client.getHoldings();
            if (!holdings.ok) {
                std::cerr << "Failed to fetch holdings for CSV update: "
                          << holdings.error << "\n";
                return 1;
            }
            try {
                updateHoldingCsv(opts.updateHolding, holdings.positions);
                std::cout << "Updated " << opts.updateHolding << " from live holdings.\n";
            } catch (const std::exception& error) {
                std::cerr << "Failed to update holdings CSV: " << error.what() << "\n";
                return 1;
            }
            return 0;
        }
        if (opts.news) {
            const auto symbols = readHoldingSymbols("config/holding.csv");
            if (symbols.empty()) {
                std::cerr << "Failed to read holdings from config/holding.csv\n";
                return 1;
            }
            const auto holdings = client.getHoldings();
            if (!holdings.ok) {
                std::cerr << "Failed to fetch holdings for news: "
                          << holdings.error << "\n";
                return 1;
            }
            const auto news = client.getNews("holdings");
            if (!news.ok) {
                std::cerr << "Failed to fetch holdings news: " << news.error << "\n";
                return 1;
            }
            try {
                const std::string output = filterNewsToSymbols(
                    news.rawBody, symbols, holdings.positions);
                std::ofstream out("config/portfolio_news.json");
                if (!out) {
                    std::cerr << "Failed to create config/portfolio_news.json\n";
                    return 1;
                }
                out << output << "\n";
                std::cout << "Saved holdings news to config/portfolio_news.json\n";
            } catch (const std::exception& e) {
                std::cerr << "Failed to filter holdings news: " << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        const PositionsResult r = opts.holdings
            ? client.getHoldings() : client.getPositions();
        if (!r.ok) {
            std::cerr << "Failed to fetch "
                      << (opts.holdings ? "holdings" : "positions")
                      << ": " << r.error << "\n";
            return 1;
        }
        positions = r.positions;
        rawBody   = r.rawBody;
    }

    // -------- 2. Render ------------------------------------------------
    if (opts.raw) {
        if (rawBody.empty()) {
            std::cerr << "--raw is only available when the input is a broker payload "
                         "(default live mode or --file). Use --json for user portfolios.\n";
            return 2;
        }
        std::cout << rawBody << "\n";
        return 0;
    }

    const PortfolioHealth health = folio::analyze(positions);
    if (opts.asJson) emitJson(health);
    else             folio::printReport(positions, health);
    return 0;
}

int runInteractiveUi() {
    std::string configPath;
    std::cout << "\nPortfolio Health\n"
              << "================\n"
              << "Interactive tabs are ready. Select an action by number.\n";

    while (true) {
        std::cout << "\n"
                  << "[1] Fetch LTP   [2] Portfolio   [3] Raw\n"
                  << "[4] Holdings    [5] News        [6] JSON\n"
                  << "[7] Config      [0] Exit\n"
                  << "Select tab: ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return 0;
        if (choice == "0") return 0;

        if (choice == "7") {
            std::cout << "Config path (blank = auto-detect ./config/config.json): ";
            std::getline(std::cin, configPath);
            std::cout << (configPath.empty() ? "Using auto-detected config.\n"
                                             : "Config path saved for this session.\n");
            continue;
        }

        std::vector<std::string> arguments{"portfolio_health"};
        if (!configPath.empty()) {
            arguments.push_back("--config");
            arguments.push_back(configPath);
        }

        if (choice == "1" || choice == "2") {
            std::cout << "Portfolio file (default config/my_portfolio.csv): ";
            std::string path;
            std::getline(std::cin, path);
            if (path.empty()) path = "config/my_portfolio.csv";
            arguments.push_back("--portfolio");
            arguments.push_back(path);
            if (choice == "1") arguments.push_back("--fetch-ltp");
        } else if (choice == "3") {
            arguments.push_back("--raw");
        } else if (choice == "4") {
            arguments.push_back("--holdings");
        } else if (choice == "5") {
            arguments.push_back("--news");
        } else if (choice == "6") {
            arguments.push_back("--holdings");
            arguments.push_back("--json");
        } else {
            std::cout << "Unknown tab. Choose 0-7.\n";
            continue;
        }

        std::vector<char*> argv;
        argv.reserve(arguments.size());
        for (auto& argument : arguments) argv.push_back(argument.data());
        runCli(static_cast<int>(argv.size()), argv.data());
        std::cout << "\nPress Enter to return to tabs...";
        std::string pause;
        std::getline(std::cin, pause);
    }
}

int main(int argc, char** argv) {
    if (argc == 1) return runInteractiveUi();
    return runCli(argc, argv);
}
