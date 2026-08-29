#ifndef FOLIO_SIGNAL_GENERATOR_HPP
#define FOLIO_SIGNAL_GENERATOR_HPP

#include "TechnicalIndicators.hpp"
#include "MLModels.hpp"
#include <string>
#include <vector>
#include <map>

namespace folio {

/**
 * @brief Trading signal generator combining multiple analysis sources.
 * 
 * Generates consensus Buy/Sell/Hold signals by combining:
 * - Technical indicator analysis (RSI, MACD, Bollinger Bands, etc.)
 * - ML model predictions
 * - News sentiment scores
 * - Portfolio risk metrics
 * - Sector relative strength
 * 
 * All signals are advisory and require explicit user approval for execution.
 * 
 * v2.0.0+: Core intelligence for automated portfolio management.
 */
class SignalGenerator {
public:
    enum class Signal {
        BUY,      // Strong bullish setup
        SELL,     // Strong bearish setup
        HOLD,     // Neutral or conflicting signals
        WAIT      // Insufficient data or unclear setup
    };

    struct SignalResult {
        Signal action = Signal::HOLD;
        double confidence = 0.0;          // 0–100
        double riskScore = 0.0;           // 0–100 (portfolio impact)
        int daysToTarget = 0;             // Estimated holding period
        double targetPrice = 0.0;         // Expected exit price
        
        struct Factor {
            std::string name;
            double contribution = 0.0;    // -100 to +100
            std::string reason;
        };
        std::vector<Factor> contributingFactors;
        std::string summary;
    };

    struct SignalConfig {
        // Signal generation thresholds
        double buyConfidenceThreshold = 70.0;
        double sellConfidenceThreshold = 70.0;
        
        // Risk limits
        double maxPositionRisk = 2.0;     // Max % of portfolio
        double maxSectorConcentration = 20.0;  // Max % in one sector
        
        // Indicator weights
        double technicalWeight = 0.4;
        double mlWeight = 0.3;
        double sentimentWeight = 0.2;
        double riskWeight = 0.1;
        
        // Time parameters
        int minHoldingDays = 3;
        int maxHoldingDays = 30;
    };

    /**
     * @brief Constructor with configuration.
     */
    explicit SignalGenerator(const SignalConfig& config);

    /**
     * @brief Generate trading signal for a symbol.
     * @param symbol Stock symbol.
     * @param currentPrice Latest market price.
     * @param rsiResult RSI indicator result (optional).
     * @param macdResult MACD indicator result (optional).
     * @param bollingerResult Bollinger Bands result (optional).
     * @param stochasticResult Stochastic result (optional).
     * @param atrResult ATR result (optional).
     * @param mlTrendResult ML trend prediction (optional).
     * @param mlPriceResult ML price prediction (optional).
     * @param sentimentScore News sentiment -100 (bearish) to +100 (bullish).
     * @param sectorStrength Sector relative strength -100 to +100.
     * @return Signal with confidence and contributing factors.
     */
    SignalResult generateSignal(
        const std::string& symbol,
        double currentPrice,
        const RsiResult* rsiResult = nullptr,
        const MacdResult* macdResult = nullptr,
        const BollingerBandsResult* bollingerResult = nullptr,
        const StochasticResult* stochasticResult = nullptr,
        const AtrResult* atrResult = nullptr,
        const MLModels::TrendPrediction* mlTrendResult = nullptr,
        const MLModels::PricePrediction* mlPriceResult = nullptr,
        double sentimentScore = 0.0,
        double sectorStrength = 0.0);

    /**
     * @brief Batch signal generation for multiple symbols.
     * @param symbols List of stock symbols.
     * @param pricesAndData Map of symbol to (price, all indicators).
     * @return Map of symbol to SignalResult.
     */
    std::map<std::string, SignalResult> generateSignalsBatch(
        const std::vector<std::string>& symbols,
        const std::map<std::string, std::pair<double, std::map<std::string, void*>>>& pricesAndData);

    /**
     * @brief Check if signal is within acceptable risk parameters.
     * @param signal The signal result to validate.
     * @param portfolioTotalValue Total portfolio value in INR.
     * @param positionValue Current position value in INR.
     * @param sectorExposure Current sector exposure in INR.
     * @return true if signal passes risk checks.
     */
    bool validateSignalRisk(const SignalResult& signal,
                            double portfolioTotalValue,
                            double positionValue,
                            double sectorExposure) const;

    /**
     * @brief Update signal configuration.
     */
    void setConfig(const SignalConfig& config);

    /**
     * @brief Get current configuration.
     */
    SignalConfig getConfig() const;

    /**
     * @brief Score individual technical indicator.
     * Returns -100 (bearish) to +100 (bullish).
     */
    double scoreRsi(const RsiResult& result) const;
    double scoreMacd(const MacdResult& result) const;
    double scoreBollingerBands(const BollingerBandsResult& result,
                                double currentPrice) const;
    double scoreStochastic(const StochasticResult& result) const;
    double scoreAtr(const AtrResult& result) const;

private:
    SignalConfig config_;

    // Helper methods
    double calculateConsensus(const std::vector<double>& scores,
                              const std::vector<double>& weights) const;
    
    std::string generateSummary(const SignalResult& result) const;
    
    int estimateHoldingPeriod(const SignalResult& signal,
                               double currentPrice) const;
};

}  // namespace folio

#endif  // FOLIO_SIGNAL_GENERATOR_HPP
