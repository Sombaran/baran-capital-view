#include "PortfolioHealth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace folio {

namespace {

double absVal(double x) { return x < 0 ? -x : x; }

// Format an INR amount as "1,23,456.78" (Indian numbering). Kept dependency-
// free rather than relying on a locale that may not be installed.
std::string fmtInr(double v) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os << std::setprecision(2) << absVal(v);
    std::string s = os.str();
    const auto dot = s.find('.');
    std::string whole = s.substr(0, dot);
    std::string frac  = s.substr(dot);

    // Indian grouping: last 3 digits, then groups of 2.
    std::string grouped;
    const int n = static_cast<int>(whole.size());
    for (int i = 0; i < n; ++i) {
        const int fromRight = n - i;
        if (i > 0 && ((fromRight == 3) || (fromRight > 3 && (fromRight - 3) % 2 == 0))) {
            grouped.push_back(',');
        }
        grouped.push_back(whole[i]);
    }
    return (v < 0 ? "-" : "") + grouped + frac;
}

std::string fmtPct(double v) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os << std::setprecision(2) << v << "%";
    return os.str();
}

std::string grade(int score) {
    if (score >= 85) return "A";
    if (score >= 70) return "B";
    if (score >= 55) return "C";
    if (score >= 40) return "D";
    return "F";
}

} // namespace

PortfolioHealth analyze(const std::vector<Position>& positions) {
    PortfolioHealth h;
    h.openPositions = 0;

    for (const auto& p : positions) {
        if (p.quantity == 0) continue; // closed intraday leg; ignore for exposure

        ++h.openPositions;
        const double mv        = p.marketValue();          // signed
        const double invested  = absVal(p.averagePrice * p.quantity * p.multiplier);
        const double dayLegPnl = (p.lastPrice - p.closePrice) *
                                 p.quantity * p.multiplier;

        h.grossExposure   += absVal(mv);
        h.netExposure     += mv;
        h.investedCapital += invested;
        h.totalUnrealised += p.unrealised;
        h.dayPnl          += dayLegPnl;

        if (mv >= 0) { ++h.longCount;  h.longExposure  += mv;  }
        else         { ++h.shortCount; h.shortExposure += -mv; }

        if (p.unrealised > 0)      ++h.winningCount;
        else if (p.unrealised < 0) ++h.losingCount;

        h.topGainers.push_back({p.tradingSymbol, p.unrealised});
        h.topLosers.push_back({p.tradingSymbol, p.unrealised});
        h.topExposures.push_back({p.tradingSymbol, absVal(mv)});
    }

    // Realised P&L is a whole-day figure and is reported per broker row even
    // for legs that have closed out (qty == 0), so sum it separately.
    for (const auto& p : positions) h.totalRealised += p.realised;
    h.totalPnl = h.totalUnrealised + h.totalRealised;

    if (h.investedCapital > 0.0) {
        h.pnlPercent = h.totalPnl / h.investedCapital * 100.0;
    }

    // Concentration is measured by symbol so multiple rows for one name are
    // not incorrectly treated as independent diversification.
    if (h.grossExposure > 0.0) {
        std::unordered_map<std::string, double> exposureBySymbol;
        for (const auto& p : positions) {
            if (p.quantity != 0) {
                exposureBySymbol[p.tradingSymbol] += absVal(p.marketValue());
            }
        }
        double topShare = 0.0;
        for (const auto& [symbol, exposure] : exposureBySymbol) {
            (void)symbol;
            const double w = exposure / h.grossExposure;
            h.herfindahlIndex += w * w;
            if (exposure > topShare) topShare = exposure;
        }
        h.concentrationPct = topShare / h.grossExposure * 100.0;
    }

    // Order the "top N" tables (keep only the informative rows).
    std::sort(h.topGainers.begin(), h.topGainers.end(),
              [](const Contributor& a, const Contributor& b) { return a.value > b.value; });
    std::sort(h.topLosers.begin(), h.topLosers.end(),
              [](const Contributor& a, const Contributor& b) { return a.value < b.value; });
    std::sort(h.topExposures.begin(), h.topExposures.end(),
              [](const Contributor& a, const Contributor& b) { return a.value > b.value; });

    auto trim = [](std::vector<Contributor>& v, std::size_t n) {
        if (v.size() > n) v.resize(n);
    };
    trim(h.topGainers,   5);
    trim(h.topLosers,    5);
    trim(h.topExposures, 5);

    // Heuristic health score in [0, 100]. Weights are intentionally simple:
    //   50 pts  - P&L percentage (clipped at +/- 25%)
    //   30 pts  - Win rate on open legs
    //   20 pts  - Diversification (1 - Herfindahl)
    const double pnlComponent = std::max(-25.0, std::min(25.0, h.pnlPercent));
    const double pnlScore = 25.0 + pnlComponent; // -> 0..50
    const double winRate  = h.openPositions == 0 ? 0.5
                          : static_cast<double>(h.winningCount) / h.openPositions;
    const double winScore = winRate * 30.0;
    const double diversityScore = (1.0 - h.herfindahlIndex) * 20.0;

    double raw = pnlScore + winScore + diversityScore;
    if (raw < 0.0)   raw = 0.0;
    if (raw > 100.0) raw = 100.0;
    h.healthScore = static_cast<int>(raw + 0.5);
    h.healthGrade = grade(h.healthScore);

    // Alerts -------------------------------------------------------------
    if (h.openPositions == 0) {
        h.alerts.push_back("No open positions.");
    }
    if (h.concentrationPct > 40.0) {
        h.alerts.push_back("High single-name concentration (" +
                           fmtPct(h.concentrationPct) + ").");
    }
    if (h.herfindahlIndex > 0.35) {
        h.alerts.push_back("Portfolio is poorly diversified (HHI=" +
                           fmtPct(h.herfindahlIndex * 100.0) + ").");
    }
    if (h.pnlPercent < -10.0) {
        h.alerts.push_back("Drawdown exceeds 10% of invested capital.");
    }
    if (h.openPositions > 0 &&
        h.losingCount * 4 > h.openPositions * 3) {
        // (losers > 75% of positions)
        h.alerts.push_back("Majority of open positions are underwater.");
    }
    return h;
}

void printReport(const std::vector<Position>& positions,
                 const PortfolioHealth& h) {
    std::ostream& os = std::cout;
    os << "\n===================== Portfolio Health =====================\n";
    os << "Score            : " << h.healthScore << " / 100  (grade "
       << h.healthGrade << ")\n";
    os << "Open positions   : " << h.openPositions
       << "  (long " << h.longCount
       << ", short " << h.shortCount << ")\n";
    os << "Winners / losers : " << h.winningCount << " / " << h.losingCount << "\n";
    os << "\n-- Exposure ------------------------------------------------\n";
    os << "Invested capital : INR " << fmtInr(h.investedCapital) << "\n";
    os << "Gross exposure   : INR " << fmtInr(h.grossExposure)   << "\n";
    os << "Net exposure     : INR " << fmtInr(h.netExposure)     << "\n";
    os << "Long exposure    : INR " << fmtInr(h.longExposure)    << "\n";
    os << "Short exposure   : INR " << fmtInr(h.shortExposure)   << "\n";
    os << "Top-1 concentr.  : " << fmtPct(h.concentrationPct)    << "\n";
    os << "Herfindahl index : " << fmtPct(h.herfindahlIndex * 100.0)
       << "  (lower = better diversified)\n";

    os << "\n-- P&L -----------------------------------------------------\n";
    os << "Unrealised P&L   : INR " << fmtInr(h.totalUnrealised) << "\n";
    os << "Realised   P&L   : INR " << fmtInr(h.totalRealised)   << "\n";
    os << "Total      P&L   : INR " << fmtInr(h.totalPnl)
       << "   (" << fmtPct(h.pnlPercent) << " of invested)\n";
    os << "Day        P&L   : INR " << fmtInr(h.dayPnl) << "\n";

    auto printTable = [&](const char* title,
                          const std::vector<Contributor>& rows,
                          bool asPct) {
        if (rows.empty()) return;
        os << "\n-- " << title << " --\n";
        for (const auto& r : rows) {
            os << "  " << std::left << std::setw(16) << r.label
               << "  " << (asPct ? fmtPct(r.value / (h.grossExposure > 0 ? h.grossExposure : 1) * 100.0)
                                 : std::string("INR ") + fmtInr(r.value))
               << "\n";
        }
    };
    printTable("Top gainers (unrealised P&L)", h.topGainers,   false);
    printTable("Top losers  (unrealised P&L)", h.topLosers,    false);
    printTable("Top exposures (|market value|)", h.topExposures, false);

    os << "\n-- Positions -----------------------------------------------\n";
    os << std::left
       << std::setw(14) << "Symbol"
       << std::right
       << std::setw(6)  << "Qty"
       << std::setw(12) << "Avg"
       << std::setw(12) << "LTP"
       << std::setw(16) << "Unrealised"
       << std::setw(16) << "Realised"
       << "\n";
    os << std::string(76, '-') << "\n";
    for (const auto& p : positions) {
        if (p.quantity == 0 && p.realised == 0.0) continue;
        os << std::left  << std::setw(14) << p.tradingSymbol
           << std::right << std::setw(6)  << p.quantity
           << std::setw(12) << fmtInr(p.averagePrice)
           << std::setw(12) << fmtInr(p.lastPrice)
           << std::setw(16) << fmtInr(p.unrealised)
           << std::setw(16) << fmtInr(p.realised)
           << "\n";
    }

    if (!h.alerts.empty()) {
        os << "\n-- Alerts --------------------------------------------------\n";
        for (const auto& a : h.alerts) os << "  * " << a << "\n";
    }
    os << "============================================================\n\n";
}

} // namespace folio
