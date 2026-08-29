#pragma once

#include "Position.hpp"

#include <string>
#include <vector>

namespace folio {

// Result of loading a user-supplied portfolio file (CSV or JSON). On
// success `positions` is ready to be fed straight into
// PortfolioHealth::analyze(). On failure `error` is populated.
struct PortfolioLoadResult {
    bool ok = false;
    std::string error;
    std::vector<Position> positions;
    std::vector<std::string> warnings; // e.g. "row 3 missing last_price"
};

// Load a portfolio from disk. File format is inferred from the extension
// (.csv / .json / .jsn). For any other extension the contents are sniffed:
// leading '{' or '[' is treated as JSON, everything else as CSV.
//
// Supported CSV columns (header row required, order-independent, case-
// insensitive). The only mandatory columns are `symbol`, `quantity` and
// `average_price`. `last_price` is filled from `average_price` when
// missing so the analytics still run, though P&L will be zero for that
// row until an LTP is filled in.
//
//   symbol,quantity,average_price
//   symbol,quantity,average_price,last_price
//   symbol,quantity,average_price,last_price,close_price
//   symbol,quantity,average_price,last_price,exchange,product,multiplier,instrument_key
//
// Supported JSON shapes:
//   { "portfolio": [ { ... }, { ... } ] }
//   [ { ... }, { ... } ]
// Field names inside each object match the CSV column names above.
PortfolioLoadResult loadPortfolio(const std::string& path);

} // namespace folio
