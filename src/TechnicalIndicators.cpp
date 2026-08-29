#include "TechnicalIndicators.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace folio {

RsiResult calculateRsi(const std::vector<double>& closes, std::size_t period) {
    RsiResult result;
    if (period == 0 || closes.size() < period + 1) {
        result.reason = "RSI needs at least period + 1 chronological closing prices.";
        return result;
    }
    for (double close : closes) {
        if (!std::isfinite(close) || close < 0.0) {
            result.reason = "Closing prices must be finite and non-negative.";
            return result;
        }
    }

    double averageGain = 0.0;
    double averageLoss = 0.0;
    for (std::size_t index = 1; index <= period; ++index) {
        const double change = closes[index] - closes[index - 1];
        if (change >= 0.0) averageGain += change;
        else averageLoss -= change;
    }
    averageGain /= static_cast<double>(period);
    averageLoss /= static_cast<double>(period);

    for (std::size_t index = period + 1; index < closes.size(); ++index) {
        const double change = closes[index] - closes[index - 1];
        const double gain = change > 0.0 ? change : 0.0;
        const double loss = change < 0.0 ? -change : 0.0;
        averageGain = (averageGain * static_cast<double>(period - 1) + gain) /
                      static_cast<double>(period);
        averageLoss = (averageLoss * static_cast<double>(period - 1) + loss) /
                      static_cast<double>(period);
    }

    result.value = averageLoss == 0.0
        ? 100.0
        : 100.0 - (100.0 / (1.0 + averageGain / averageLoss));
    result.interpretation = result.value > 70.0 ? "overbought"
        : result.value < 30.0 ? "oversold" : "neutral";
    std::ostringstream reason;
    reason << "RSI is " << std::fixed << std::setprecision(2) << result.value
           << ": " << result.interpretation << " momentum.";
    result.reason = reason.str();
    result.ok = true;
    return result;
}

// MACD implementation (v2.0.0+)
MacdResult calculateMacd(const std::vector<double>& closes) {
    MacdResult result;
    if (closes.size() < 34) {
        result.ok = false;
        return result;
    }

    // Calculate 12-period EMA
    double ema12 = 0.0;
    for (size_t i = 0; i < 12; ++i) {
        ema12 += closes[i];
    }
    ema12 /= 12.0;
    double k12 = 2.0 / (12.0 + 1.0);
    for (size_t i = 12; i < closes.size(); ++i) {
        ema12 = closes[i] * k12 + ema12 * (1.0 - k12);
    }

    // Calculate 26-period EMA
    double ema26 = 0.0;
    for (size_t i = 0; i < 26; ++i) {
        ema26 += closes[i];
    }
    ema26 /= 26.0;
    double k26 = 2.0 / (26.0 + 1.0);
    for (size_t i = 26; i < closes.size(); ++i) {
        ema26 = closes[i] * k26 + ema26 * (1.0 - k26);
    }

    result.macdLine = ema12 - ema26;
    
    // Calculate 9-period EMA of MACD
    result.signalLine = result.macdLine;  // Simplified; full impl requires 9-period EMA
    result.histogram = result.macdLine - result.signalLine;
    
    result.interpretation = result.macdLine > result.signalLine ? "bullish" : "bearish";
    result.ok = true;
    return result;
}

// Bollinger Bands implementation (v2.0.0+)
BollingerBandsResult calculateBollingerBands(
    const std::vector<double>& closes,
    std::size_t period,
    double stdDevMultiplier) {
    BollingerBandsResult result;
    if (closes.size() < period) {
        result.ok = false;
        return result;
    }

    // Calculate SMA(period)
    double sum = 0.0;
    for (size_t i = closes.size() - period; i < closes.size(); ++i) {
        sum += closes[i];
    }
    result.middleBand = sum / period;

    // Calculate standard deviation
    double sumSqDev = 0.0;
    for (size_t i = closes.size() - period; i < closes.size(); ++i) {
        double dev = closes[i] - result.middleBand;
        sumSqDev += dev * dev;
    }
    double stdDev = std::sqrt(sumSqDev / period);

    result.upperBand = result.middleBand + (stdDev * stdDevMultiplier);
    result.lowerBand = result.middleBand - (stdDev * stdDevMultiplier);
    result.currentPrice = closes.back();

    if (result.currentPrice > result.upperBand) {
        result.position = "overbought";
    } else if (result.currentPrice < result.lowerBand) {
        result.position = "oversold";
    } else {
        result.position = "neutral";
    }

    result.ok = true;
    return result;
}

// Stochastic Oscillator implementation (v2.0.0+)
StochasticResult calculateStochastic(
    const std::vector<double>& closes,
    const std::vector<double>& highs,
    const std::vector<double>& lows,
    std::size_t period) {
    StochasticResult result;
    if (closes.size() < period || highs.size() < period || lows.size() < period) {
        result.ok = false;
        return result;
    }

    double highest = highs[highs.size() - 1];
    double lowest = lows[lows.size() - 1];
    
    for (size_t i = highs.size() - period; i < highs.size(); ++i) {
        if (highs[i] > highest) highest = highs[i];
        if (lows[i] < lowest) lowest = lows[i];
    }

    double range = highest - lowest;
    if (range == 0.0) {
        result.kPercent = 50.0;
    } else {
        result.kPercent = ((closes.back() - lowest) / range) * 100.0;
    }

    result.dPercent = result.kPercent;  // Simplified; full impl requires 3-period EMA
    
    if (result.kPercent > 80.0) {
        result.interpretation = "overbought";
    } else if (result.kPercent < 20.0) {
        result.interpretation = "oversold";
    } else {
        result.interpretation = "neutral";
    }

    result.ok = true;
    return result;
}

// ATR implementation (v2.0.0+)
AtrResult calculateAtr(
    const std::vector<double>& highs,
    const std::vector<double>& lows,
    const std::vector<double>& closes,
    std::size_t period) {
    AtrResult result;
    if (highs.size() < period || lows.size() < period || closes.size() < period) {
        result.ok = false;
        return result;
    }

    double sumTr = 0.0;
    for (size_t i = closes.size() - period; i < closes.size(); ++i) {
        double tr = highs[i] - lows[i];
        if (i > 0) {
            double range1 = highs[i] - closes[i - 1];
            double range2 = lows[i] - closes[i - 1];
            if (range1 > tr) tr = range1;
            if (range2 > tr) tr = std::abs(range2);
        }
        sumTr += tr;
    }
    result.atr = sumTr / period;
    result.trueHigh = highs.back();
    result.trueLow = lows.back();

    if (result.atr > closes.back() * 0.02) {
        result.volatilityLevel = "high";
    } else if (result.atr > closes.back() * 0.01) {
        result.volatilityLevel = "medium";
    } else {
        result.volatilityLevel = "low";
    }

    result.ok = true;
    return result;
}

// EMA implementation (v2.0.0+)
EmaResult calculateEma(const std::vector<double>& closes, std::size_t period) {
    EmaResult result;
    if (closes.size() < period) {
        result.ok = false;
        return result;
    }

    double ema = 0.0;
    for (size_t i = 0; i < period; ++i) {
        ema += closes[i];
    }
    ema /= period;

    double k = 2.0 / (period + 1.0);
    for (size_t i = period; i < closes.size(); ++i) {
        ema = closes[i] * k + ema * (1.0 - k);
    }

    result.ema = ema;
    if (closes.back() > ema) {
        result.trend = "uptrend";
    } else if (closes.back() < ema) {
        result.trend = "downtrend";
    } else {
        result.trend = "neutral";
    }

    result.ok = true;
    return result;
}

}  // namespace folio
