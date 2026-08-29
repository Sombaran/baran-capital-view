#include "SignalGenerator.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace folio {

SignalGenerator::SignalGenerator(const SignalConfig& config)
    : config_(config) {
}

SignalGenerator::SignalResult SignalGenerator::generateSignal(
    const std::string& symbol,
    double currentPrice,
    const RsiResult* rsiResult,
    const MacdResult* macdResult,
    const BollingerBandsResult* bollingerResult,
    const StochasticResult* stochasticResult,
    const AtrResult* atrResult,
    const MLModels::TrendPrediction* mlTrendResult,
    const MLModels::PricePrediction* mlPriceResult,
    double sentimentScore,
    double sectorStrength) {
    
    SignalResult result;
    result.contributingFactors.clear();

    std::vector<double> scores;
    std::vector<double> weights;

    // Technical indicator scores
    double technicalScore = 0.0;
    int indicatorCount = 0;

    if (rsiResult && rsiResult->ok) {
        double rsiScore = scoreRsi(*rsiResult);
        technicalScore += rsiScore;
        indicatorCount++;
        
        SignalResult::Factor factor;
        factor.name = "RSI";
        factor.contribution = rsiScore;
        factor.reason = rsiResult->interpretation;
        result.contributingFactors.push_back(factor);
    }

    if (macdResult && macdResult->ok) {
        double macdScore = scoreMacd(*macdResult);
        technicalScore += macdScore;
        indicatorCount++;
        
        SignalResult::Factor factor;
        factor.name = "MACD";
        factor.contribution = macdScore;
        factor.reason = macdResult->interpretation;
        result.contributingFactors.push_back(factor);
    }

    if (bollingerResult && bollingerResult->ok) {
        double bbScore = scoreBollingerBands(*bollingerResult, currentPrice);
        technicalScore += bbScore;
        indicatorCount++;
        
        SignalResult::Factor factor;
        factor.name = "Bollinger Bands";
        factor.contribution = bbScore;
        factor.reason = bollingerResult->position;
        result.contributingFactors.push_back(factor);
    }

    if (stochasticResult && stochasticResult->ok) {
        double stochScore = scoreStochastic(*stochasticResult);
        technicalScore += stochScore;
        indicatorCount++;
        
        SignalResult::Factor factor;
        factor.name = "Stochastic";
        factor.contribution = stochScore;
        factor.reason = stochasticResult->interpretation;
        result.contributingFactors.push_back(factor);
    }

    if (atrResult && atrResult->ok) {
        double atrScore = scoreAtr(*atrResult);
        technicalScore += atrScore;
        indicatorCount++;
        
        SignalResult::Factor factor;
        factor.name = "ATR";
        factor.contribution = atrScore;
        factor.reason = atrResult->volatilityLevel;
        result.contributingFactors.push_back(factor);
    }

    if (indicatorCount > 0) {
        technicalScore /= indicatorCount;
    }

    scores.push_back(technicalScore);
    weights.push_back(config_.technicalWeight);

    // ML predictions
    double mlScore = 0.0;
    int mlCount = 0;

    if (mlTrendResult && mlTrendResult->ok) {
        mlScore += (mlTrendResult->upProbability - mlTrendResult->downProbability) / 100.0 * 100.0;
        mlCount++;
    }

    if (mlPriceResult && mlPriceResult->ok) {
        if (mlPriceResult->predictedClose > currentPrice) {
            mlScore += mlPriceResult->confidence;
        } else {
            mlScore -= mlPriceResult->confidence;
        }
        mlCount++;
    }

    if (mlCount > 0) {
        mlScore /= mlCount;
    }

    scores.push_back(mlScore);
    weights.push_back(config_.mlWeight);

    // Sentiment score
    scores.push_back(sentimentScore);
    weights.push_back(config_.sentimentWeight);

    // Risk score (sector strength)
    scores.push_back(sectorStrength);
    weights.push_back(config_.riskWeight);

    // Calculate consensus
    result.confidence = calculateConsensus(scores, weights);

    // Generate signal based on confidence
    if (result.confidence > config_.buyConfidenceThreshold) {
        result.action = Signal::BUY;
        result.riskScore = std::max(0.0, 100.0 - result.confidence);
    } else if (result.confidence < -config_.sellConfidenceThreshold) {
        result.action = Signal::SELL;
        result.riskScore = std::max(0.0, 100.0 + result.confidence);
    } else if (std::abs(result.confidence) < 30.0) {
        result.action = Signal::WAIT;
        result.riskScore = 50.0;
    } else {
        result.action = Signal::HOLD;
        result.riskScore = std::abs(result.confidence) / 2.0;
    }

    result.confidence = std::abs(result.confidence);
    result.targetPrice = currentPrice * (1.0 + (result.confidence / 100.0) * 0.05);
    result.daysToTarget = estimateHoldingPeriod(result, currentPrice);
    result.summary = generateSummary(result);

    return result;
}

std::map<std::string, SignalGenerator::SignalResult> SignalGenerator::generateSignalsBatch(
    const std::vector<std::string>& symbols,
    const std::map<std::string, std::pair<double, std::map<std::string, void*>>>& pricesAndData) {
    
    std::map<std::string, SignalResult> results;

    for (const auto& symbol : symbols) {
        auto it = pricesAndData.find(symbol);
        if (it != pricesAndData.end()) {
            double price = it->second.first;
            // TODO: Extract individual indicators from the data map
            SignalResult result = generateSignal(symbol, price);
            results[symbol] = result;
        }
    }

    return results;
}

bool SignalGenerator::validateSignalRisk(const SignalResult& signal,
                                          double portfolioTotalValue,
                                          double positionValue,
                                          double sectorExposure) const {
    // Check position risk
    double positionRiskPercent = (positionValue / portfolioTotalValue) * 100.0;
    if (positionRiskPercent > config_.maxPositionRisk) {
        return false;
    }

    // Check sector concentration
    if (sectorExposure > config_.maxSectorConcentration) {
        return false;
    }

    return true;
}

void SignalGenerator::setConfig(const SignalConfig& config) {
    config_ = config;
}

SignalGenerator::SignalConfig SignalGenerator::getConfig() const {
    return config_;
}

double SignalGenerator::scoreRsi(const RsiResult& result) const {
    // RSI > 70 = overbought (bearish), RSI < 30 = oversold (bullish)
    if (result.value > 70.0) {
        return -50.0;  // Bearish
    } else if (result.value < 30.0) {
        return 50.0;   // Bullish
    } else {
        return 0.0;    // Neutral
    }
}

double SignalGenerator::scoreMacd(const MacdResult& result) const {
    // MACD > Signal = bullish, MACD < Signal = bearish
    if (result.macdLine > result.signalLine && result.histogram > 0) {
        return 50.0;  // Bullish
    } else if (result.macdLine < result.signalLine && result.histogram < 0) {
        return -50.0; // Bearish
    } else {
        return 0.0;   // Neutral
    }
}

double SignalGenerator::scoreBollingerBands(const BollingerBandsResult& result,
                                             double currentPrice) const {
    if (result.position == "overbought") {
        return -50.0;  // Bearish
    } else if (result.position == "oversold") {
        return 50.0;   // Bullish
    } else {
        return 0.0;    // Neutral
    }
}

double SignalGenerator::scoreStochastic(const StochasticResult& result) const {
    if (result.kPercent > 80.0) {
        return -50.0;  // Overbought, bearish
    } else if (result.kPercent < 20.0) {
        return 50.0;   // Oversold, bullish
    } else {
        return 0.0;    // Neutral
    }
}

double SignalGenerator::scoreAtr(const AtrResult& result) const {
    // Higher ATR = higher volatility (increases risk)
    if (result.volatilityLevel == "high") {
        return -25.0;  // Slightly bearish due to risk
    } else if (result.volatilityLevel == "low") {
        return 25.0;   // Slightly bullish, better for trading
    } else {
        return 0.0;    // Neutral
    }
}

double SignalGenerator::calculateConsensus(const std::vector<double>& scores,
                                            const std::vector<double>& weights) const {
    if (scores.empty() || weights.empty()) {
        return 0.0;
    }

    double totalWeight = 0.0;
    double weightedSum = 0.0;

    for (size_t i = 0; i < scores.size() && i < weights.size(); ++i) {
        weightedSum += scores[i] * weights[i];
        totalWeight += weights[i];
    }

    if (totalWeight == 0.0) {
        return 0.0;
    }

    return weightedSum / totalWeight;
}

std::string SignalGenerator::generateSummary(const SignalResult& result) const {
    std::ostringstream ss;

    switch (result.action) {
        case Signal::BUY:
            ss << "BUY signal at " << std::fixed << std::setprecision(0)
               << result.confidence << "% confidence. ";
            break;
        case Signal::SELL:
            ss << "SELL signal at " << std::fixed << std::setprecision(0)
               << result.confidence << "% confidence. ";
            break;
        case Signal::HOLD:
            ss << "HOLD signal. Neutral confluence. ";
            break;
        case Signal::WAIT:
            ss << "WAIT for better setup. Insufficient signals. ";
            break;
    }

    if (result.targetPrice > 0) {
        ss << "Target: " << std::fixed << std::setprecision(2)
           << result.targetPrice << ". ";
    }

    if (result.daysToTarget > 0) {
        ss << "Estimated holding period: " << result.daysToTarget << " days.";
    }

    return ss.str();
}

int SignalGenerator::estimateHoldingPeriod(const SignalResult& signal,
                                            double currentPrice) const {
    if (signal.action == Signal::WAIT || signal.action == Signal::HOLD) {
        return 0;
    }

    // Estimate holding period based on target and volatility
    double targetMove = std::abs(signal.targetPrice - currentPrice) / currentPrice;
    
    // Assume ~1% daily move in normal conditions
    int estimatedDays = static_cast<int>(targetMove * 100.0);
    
    // Clamp within config limits
    if (estimatedDays < config_.minHoldingDays) {
        estimatedDays = config_.minHoldingDays;
    }
    if (estimatedDays > config_.maxHoldingDays) {
        estimatedDays = config_.maxHoldingDays;
    }

    return estimatedDays;
}

}  // namespace folio
