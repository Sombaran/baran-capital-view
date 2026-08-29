#ifndef FOLIO_ML_MODELS_HPP
#define FOLIO_ML_MODELS_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>

namespace folio {

/**
 * @brief Machine Learning model predictions for portfolio analysis.
 * 
 * Runs inference on trained models to predict price movements,
 * trends, volatility, and mean reversion probabilities.
 * 
 * v2.0.0+: Integrates with signal generation for consensus predictions.
 */
class MLModels {
public:
    struct OhlcData {
        std::vector<double> opens;
        std::vector<double> highs;
        std::vector<double> lows;
        std::vector<double> closes;
        std::vector<long> volumes;
    };

    struct PricePrediction {
        bool ok = false;
        double predictedClose = 0.0;
        double confidenceInterval = 0.0;  // ±% range
        double confidence = 0.0;           // 0–100
        std::string reasoning;
    };

    struct TrendPrediction {
        bool ok = false;
        double upProbability = 0.0;        // 0–100
        double downProbability = 0.0;      // 0–100
        double neutralProbability = 0.0;   // 0–100
        std::string trend;                 // "uptrend", "downtrend", "neutral"
    };

    struct VolatilityForecast {
        bool ok = false;
        double predictedVolatility = 0.0;  // standard deviation forecast
        std::string level;                 // "low", "medium", "high"
        double confidence = 0.0;
    };

    struct MeanReversionProbability {
        bool ok = false;
        double probability = 0.0;          // 0–100
        double expectedTarget = 0.0;       // Price level
        std::string strength;              // "weak", "moderate", "strong"
    };

    /**
     * @brief Constructor.
     * @param modelsPath Directory containing pre-trained model weights.
     */
    explicit MLModels(const std::string& modelsPath = "config/ml_models/");

    /**
     * @brief Predict next-day close price.
     * @param symbol Stock symbol.
     * @param ohlcData Recent OHLC data (at least 30 bars).
     * @return Prediction with confidence interval.
     */
    PricePrediction predictPrice(const std::string& symbol,
                                  const OhlcData& ohlcData);

    /**
     * @brief Predict trend direction for next period.
     * @param symbol Stock symbol.
     * @param ohlcData Recent OHLC data.
     * @return Probability distribution over up/down/neutral.
     */
    TrendPrediction predictTrend(const std::string& symbol,
                                  const OhlcData& ohlcData);

    /**
     * @brief Forecast volatility for next period.
     * @param symbol Stock symbol.
     * @param ohlcData Recent OHLC data.
     * @return Volatility forecast with confidence.
     */
    VolatilityForecast forecastVolatility(const std::string& symbol,
                                           const OhlcData& ohlcData);

    /**
     * @brief Predict mean reversion probability.
     * Useful for identifying overbought/oversold setups.
     * @param symbol Stock symbol.
     * @param ohlcData Recent OHLC data.
     * @param currentPrice Latest market price.
     * @return Reversion probability and target price.
     */
    MeanReversionProbability predictMeanReversion(
        const std::string& symbol,
        const OhlcData& ohlcData,
        double currentPrice);

    /**
     * @brief Load model from disk.
     * @param modelName Identifier (e.g., "price_predictor_v1").
     * @return true if model loaded successfully.
     */
    bool loadModel(const std::string& modelName);

    /**
     * @brief Clear cached predictions for a symbol.
     * Useful when market conditions change significantly.
     */
    void clearCache(const std::string& symbol = "");

    /**
     * @brief Check if predictions are still valid (< 1 hour old).
     */
    bool isCacheValid(const std::string& symbol) const;

    /**
     * @brief Get last prediction timestamp for a symbol.
     */
    time_t getLastPredictionTime(const std::string& symbol) const;

private:
    std::string modelsPath_;
    
    // Model weights and configuration
    struct ModelWeights {
        std::string name;
        std::vector<double> weights;
        std::vector<double> biases;
        size_t inputSize = 0;
        size_t hiddenSize = 0;
        size_t outputSize = 0;
    };
    
    std::map<std::string, ModelWeights> loadedModels_;
    
    // Prediction cache (symbol -> result)
    struct CachedPrediction {
        PricePrediction priceResult;
        TrendPrediction trendResult;
        VolatilityForecast volatilityResult;
        time_t timestamp = 0;
    };
    
    std::map<std::string, CachedPrediction> predictionCache_;

    // Model inference
    std::vector<double> runInference(const std::string& modelName,
                                      const std::vector<double>& features);

    // Feature extraction from OHLC
    std::vector<double> extractFeatures(const OhlcData& ohlcData);
};

}  // namespace folio

#endif  // FOLIO_ML_MODELS_HPP
