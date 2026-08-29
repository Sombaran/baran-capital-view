#include "PortfolioLoader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace folio {

using nlohmann::json;

namespace {

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    // Strip a single pair of surrounding quotes ("INFY" -> INFY).
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string extLower(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    return lower(path.substr(dot + 1));
}

std::vector<std::string> splitCsv(const std::string& line) {
    // Deliberately minimal - no embedded-comma support. Users with such
    // symbols can quote the field (e.g. "FOO, BAR"). Good enough for a
    // portfolio ticker list.
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    for (char c : line) {
        if (c == '"') { inQuotes = !inQuotes; cur.push_back(c); continue; }
        if (c == ',' && !inQuotes) { out.push_back(trim(cur)); cur.clear(); continue; }
        cur.push_back(c);
    }
    out.push_back(trim(cur));
    return out;
}

bool isBlankOrComment(const std::string& line) {
    for (char c : line) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        return c == '#';
    }
    return true;
}

// Fill derived fields (unrealised, pnl, exposure sign) after the raw
// columns have been populated so the analytics don't need to special-
// case user-supplied rows.
void finalise(Position& p) {
    if (p.multiplier <= 0) p.multiplier = 1;
    if (p.exchange.empty()) p.exchange = "NSE";
    if (p.product.empty())  p.product  = "D"; // delivery / CNC
    if (p.lastPrice == 0.0) p.lastPrice = p.averagePrice;
    if (p.closePrice == 0.0) p.closePrice = p.lastPrice;

    const double derived = p.lastPrice * p.quantity * p.multiplier; // signed
    p.value      = (std::fabs(p.value) > 0.0) ? p.value : derived;
    p.unrealised = (p.lastPrice - p.averagePrice) * p.quantity * p.multiplier;
    p.pnl        = p.unrealised + p.realised;
    // buy_value / sell_value approximated from the net leg - only used
    // by --raw dumps and callers that want to reason about the input.
    if (p.quantity >= 0) {
        p.buyPrice = p.averagePrice;
        p.buyValue = p.averagePrice * p.quantity * p.multiplier;
    } else {
        p.sellPrice = p.averagePrice;
        p.sellValue = p.averagePrice * (-p.quantity) * p.multiplier;
    }
}

// ---- CSV ----------------------------------------------------------------

PortfolioLoadResult loadCsv(std::istream& in) {
    PortfolioLoadResult out;

    // 1. Read the header.
    std::string line;
    std::unordered_map<std::string, std::size_t> col;
    while (std::getline(in, line)) {
        if (isBlankOrComment(line)) continue;
        const auto cols = splitCsv(line);
        for (std::size_t i = 0; i < cols.size(); ++i) col[lower(cols[i])] = i;
        break;
    }
    if (col.empty()) {
        out.error = "CSV: no header row found";
        return out;
    }

    auto require = [&](const char* name) -> long {
        auto it = col.find(name);
        return it == col.end() ? -1 : static_cast<long>(it->second);
    };
    const long cSym  = require("symbol");
    const long cQty  = require("quantity");
    const long cAvg  = require("average_price");
    if (cSym < 0 || cQty < 0 || cAvg < 0) {
        out.error = "CSV: required columns missing "
                    "(need at least symbol, quantity, average_price)";
        return out;
    }
    const long cLast = require("last_price");
    const long cClose = require("close_price");
    const long cExch = require("exchange");
    const long cProd = require("product");
    const long cMult = require("multiplier");
    const long cKey  = require("instrument_key");

    // 2. Read the data rows.
    std::size_t rowNo = 1;
    while (std::getline(in, line)) {
        ++rowNo;
        if (isBlankOrComment(line)) continue;
        const auto cols = splitCsv(line);
        auto at = [&](long c) -> std::string {
            return (c >= 0 && static_cast<std::size_t>(c) < cols.size()) ? cols[c] : std::string{};
        };

        Position p;
        try {
            p.tradingSymbol = at(cSym);
            p.quantity      = std::stol(at(cQty));
            p.averagePrice  = std::stod(at(cAvg));
            if (cLast  >= 0 && !at(cLast).empty())  p.lastPrice  = std::stod(at(cLast));
            if (cClose >= 0 && !at(cClose).empty()) p.closePrice = std::stod(at(cClose));
            if (cExch  >= 0) p.exchange       = at(cExch);
            if (cProd  >= 0) p.product        = at(cProd);
            if (cMult  >= 0 && !at(cMult).empty())  p.multiplier = std::stoi(at(cMult));
            if (cKey   >= 0) p.instrumentToken = at(cKey);
        } catch (const std::exception& e) {
            out.error = "CSV row " + std::to_string(rowNo) + ": " + e.what();
            return out;
        }

        if (p.tradingSymbol.empty()) {
            out.warnings.push_back("row " + std::to_string(rowNo) + ": empty symbol - skipped");
            continue;
        }
        if (cLast < 0 || at(cLast).empty()) {
            out.warnings.push_back("row " + std::to_string(rowNo) +
                                   " (" + p.tradingSymbol + "): last_price missing, "
                                   "using average_price (P&L will read as 0). "
                                   "Pass --fetch-ltp to fill from Upstox.");
        }

        finalise(p);
        out.positions.push_back(std::move(p));
    }

    if (out.positions.empty()) {
        out.error = "CSV: no data rows";
        return out;
    }
    out.ok = true;
    return out;
}

// ---- JSON --------------------------------------------------------------

PortfolioLoadResult loadJson(std::istream& in) {
    PortfolioLoadResult out;
    std::ostringstream ss; ss << in.rdbuf();
    try {
        const json doc = json::parse(ss.str());
        const json* arr = nullptr;
        if (doc.is_array()) arr = &doc;
        else if (doc.is_object() && doc.contains("portfolio") && doc["portfolio"].is_array()) {
            arr = &doc["portfolio"];
        } else {
            out.error = "JSON: expected an array or an object with a 'portfolio' array";
            return out;
        }

        std::size_t idx = 0;
        for (const auto& row : *arr) {
            ++idx;
            if (!row.is_object()) {
                out.error = "JSON entry " + std::to_string(idx) + " is not an object";
                return out;
            }
            Position p;
            p.tradingSymbol   = row.value("symbol", row.value("trading_symbol", std::string{}));
            p.quantity        = row.value("quantity", 0L);
            p.averagePrice    = row.value("average_price", 0.0);
            if (row.contains("last_price")   && !row["last_price"].is_null())
                p.lastPrice   = row.value("last_price", 0.0);
            else
                out.warnings.push_back("entry " + std::to_string(idx) +
                                       " (" + p.tradingSymbol + "): last_price missing, "
                                       "using average_price. Pass --fetch-ltp to fill.");
            p.closePrice      = row.value("close_price", 0.0);
            p.exchange        = row.value("exchange", std::string{});
            p.product         = row.value("product", std::string{});
            p.multiplier      = row.value("multiplier", 1);
            p.instrumentToken = row.value("instrument_key",
                                row.value("instrument_token", std::string{}));

            if (p.tradingSymbol.empty()) {
                out.warnings.push_back("entry " + std::to_string(idx) + ": empty symbol - skipped");
                continue;
            }
            finalise(p);
            out.positions.push_back(std::move(p));
        }
    } catch (const std::exception& e) {
        out.error = std::string("JSON parse error: ") + e.what();
        return out;
    }

    if (out.positions.empty()) {
        out.error = "JSON: no positions found";
        return out;
    }
    out.ok = true;
    return out;
}

} // namespace

PortfolioLoadResult loadPortfolio(const std::string& path) {
    PortfolioLoadResult out;
    std::ifstream in(path);
    if (!in) {
        out.error = "cannot open portfolio file: " + path;
        return out;
    }

    const std::string ext = extLower(path);
    if (ext == "json" || ext == "jsn") return loadJson(in);
    if (ext == "csv"  || ext == "tsv") return loadCsv(in);

    // Unknown extension - sniff.
    char c;
    do { in.get(c); } while (in && std::isspace(static_cast<unsigned char>(c)));
    in.unget();
    if (c == '{' || c == '[') return loadJson(in);
    return loadCsv(in);
}

} // namespace folio
