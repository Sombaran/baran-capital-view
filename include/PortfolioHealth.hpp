#pragma once

#include "Position.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace folio {

// A single row shown in the "Top movers" tables of the health report.
struct Contributor {
    std::string label;
    double      value = 0.0;
};

// Aggregate portfolio-health metrics derived from a snapshot of open
// positions. All monetary values are in the account's base currency (INR).
struct PortfolioHealth {
    std::size_t openPositions       = 0;
    std::size_t longCount           = 0;
    std::size_t shortCount          = 0;
    std::size_t losingCount         = 0;
    std::size_t winningCount        = 0;

    double grossExposure     = 0.0;   // Sum |market value|
    double netExposure       = 0.0;   // Sum signed market value
    double longExposure      = 0.0;
    double shortExposure     = 0.0;

    double investedCapital   = 0.0;   // Sum |avg * qty * multiplier|
    double totalUnrealised   = 0.0;
    double totalRealised     = 0.0;
    double totalPnl          = 0.0;   // unrealised + realised
    double dayPnl            = 0.0;   // (last - close) * qty * multiplier

    double pnlPercent        = 0.0;   // totalPnl / investedCapital * 100
    double concentrationPct  = 0.0;   // top-1 |exposure| / grossExposure * 100
    double herfindahlIndex   = 0.0;   // sum(w_i^2), 0..1 (1 = single-name)

    // Health score in [0, 100] combining P&L %, win-rate and concentration.
    // Higher is better. Intended as a quick heuristic, not a risk model.
    int    healthScore       = 0;
    std::string healthGrade;          // "A" .. "F"
    std::vector<std::string> alerts;  // e.g. "concentration > 40%"

    std::vector<Contributor> topGainers;   // by unrealised, desc
    std::vector<Contributor> topLosers;    // by unrealised, asc
    std::vector<Contributor> topExposures; // by |market value|, desc
};

PortfolioHealth analyze(const std::vector<Position>& positions);

// Emit a human-readable, fixed-width report to stdout.
void printReport(const std::vector<Position>& positions,
                 const PortfolioHealth& health);

} // namespace folio
