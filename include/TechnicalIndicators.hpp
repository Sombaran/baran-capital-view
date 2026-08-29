#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace folio {

/**
 * @brief Wilder's RSI (Relative Strength Index).
 * Momentum indicator measuring overbought (>70) / oversold (<30) conditions.
 * v2.0.0+: Part of expanded indicator suite.
 */
struct RsiResult {
    bool ok = false;
    double value = 0.0;
    std::string interpretation;
    std::string reason;
};

// Calculates Wilder's RSI from chronological closing prices.
// Requires at least period + 1 prices; the latest RSI is returned.
RsiResult calculateRsi(const std::vector<double>& closes,
                       std::size_t period = 14);

/**
 * @brief MACD (Moving Average Convergence Divergence).
 * Trend-following momentum indicator.
 * v2.0.0+: New indicator in expanded suite.
 */
struct MacdResult {
    bool ok = false;
    double macdLine = 0.0;      // 12-period EMA - 26-period EMA
    double signalLine = 0.0;    // 9-period EMA of MACD
    double histogram = 0.0;     // MACD - Signal Line
    std::string interpretation; // "bullish", "bearish", "neutral"
};

// Calculates MACD from chronological closing prices.
// Requires at least 26 + 9 - 1 = 34 prices for accurate calculation.
MacdResult calculateMacd(const std::vector<double>& closes);

/**
 * @brief Bollinger Bands.
 * Volatility indicator using SMA(20) ± 2 std dev.
 * v2.0.0+: New indicator in expanded suite.
 */
struct BollingerBandsResult {
    bool ok = false;
    double upperBand = 0.0;
    double middleBand = 0.0;    // SMA(20)
    double lowerBand = 0.0;
    double currentPrice = 0.0;
    std::string position;        // "overbought", "neutral", "oversold"
};

// Calculates Bollinger Bands from OHLC data.
// Requires at least 20 prices for SMA calculation.
BollingerBandsResult calculateBollingerBands(
    const std::vector<double>& closes,
    std::size_t period = 20,
    double stdDevMultiplier = 2.0);

/**
 * @brief Stochastic Oscillator.
 * Momentum indicator measuring position relative to high-low range.
 * v2.0.0+: New indicator in expanded suite.
 */
struct StochasticResult {
    bool ok = false;
    double kPercent = 0.0;       // %K line
    double dPercent = 0.0;       // %D line (3-period EMA of %K)
    std::string interpretation;  // "overbought", "oversold", "neutral"
};

// Calculates Stochastic Oscillator.
// Requires at least 14 + 3 - 1 = 16 prices for accurate calculation.
StochasticResult calculateStochastic(
    const std::vector<double>& closes,
    const std::vector<double>& highs,
    const std::vector<double>& lows,
    std::size_t period = 14);

/**
 * @brief ATR (Average True Range).
 * Volatility measurement indicator.
 * v2.0.0+: New indicator in expanded suite.
 */
struct AtrResult {
    bool ok = false;
    double atr = 0.0;
    double trueHigh = 0.0;
    double trueLow = 0.0;
    std::string volatilityLevel;  // "low", "medium", "high"
};

// Calculates ATR from OHLC data.
// Requires at least 14 prices for standard calculation.
AtrResult calculateAtr(
    const std::vector<double>& highs,
    const std::vector<double>& lows,
    const std::vector<double>& closes,
    std::size_t period = 14);

/**
 * @brief EMA (Exponential Moving Average).
 * Trend line smoothing with recent prices weighted more heavily.
 * v2.0.0+: New utility for technical analysis.
 */
struct EmaResult {
    bool ok = false;
    double ema = 0.0;
    std::string trend;  // "uptrend", "downtrend", "neutral"
};

// Calculates EMA from chronological closing prices.
EmaResult calculateEma(const std::vector<double>& closes,
                       std::size_t period = 20);

} // namespace folio
