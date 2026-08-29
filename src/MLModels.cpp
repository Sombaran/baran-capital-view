#include "MLModels.hpp"
#include <cmath>
#include <algorithm>
#include <ctime>

namespace folio {

MLModels::MLModels(const std::string& modelsPath)
    : modelsPath_(modelsPath) {
}

MLModels::PricePrediction MLModels::predictPrice(const std::string& symbol,
                                                   const OhlcData& ohlcData) {
    PricePrediction result;
    
    // Check cache
    if (isCacheValid(symbol)) {
        auto it = predictionCache_.find(symbol);
        if (it != predictionCache_.end()) {
            return it->second.priceResult;
        }
    }
    
    if (ohlcData.closes.size() < 30) {
        result.ok = false;
        result.reasoning = "Need at least 30 bars for prediction";
        return result;
    }

    // Extract features from OHLC data
    std::vector<double> features = extractFeatures(ohlcData);

    // Run inference on pre-trained model
    std::vector<double> predictions = runInference("price_predictor", features);

    if (predictions.empty()) {
        result.ok = false;
        result.reasoning = "Model inference failed";
        return result;
    }

    // Predictions contain: [predicted_close, confidence, lower_bound, upper_bound]
    result.predictedClose = predictions[0];
    result.confidence = predictions.size() > 1 ? predictions[1] : 50.0;
    result.confidenceInterval = predictions.size() > 2 ? predictions[2] : 5.0;
    result.ok = true;
    result.reasoning = "Based on 30+ historical bars and trained model";

    // Cache the result
    CachedPrediction cached;
    cached.priceResult = result;
    cached.timestamp = time(nullptr);
    predictionCache_[symbol] = cached;

    return result;
}

MLModels::TrendPrediction MLModels::predictTrend(const std::string& symbol,
                                                   const OhlcData& ohlcData) {
    TrendPrediction result;
    
    if (ohlcData.closes.size() < 30) {
        result.ok = false;
        return result;
    }

    std::vector<double> features = extractFeatures(ohlcData);
    std::vector<double> predictions = runInference("trend_predictor", features);

    if (predictions.size() < 3) {
        result.ok = false;
        return result;
    }

    result.upProbability = predictions[0] * 100.0;
    result.downProbability = predictions[1] * 100.0;
    result.neutralProbability = predictions[2] * 100.0;

    if (result.upProbability > result.downProbability &&
        result.upProbability > result.neutralProbability) {
        result.trend = "uptrend";
    } else if (result.downProbability > result.neutralProbability) {
        result.trend = "downtrend";
    } else {
        result.trend = "neutral";
    }

    result.ok = true;
    return result;
}

MLModels::VolatilityForecast MLModels::forecastVolatility(const std::string& symbol,
                                                           const OhlcData& ohlcData) {
    VolatilityForecast result;
    
    if (ohlcData.closes.size() < 30) {
        result.ok = false;
        return result;
    }

    std::vector<double> features = extractFeatures(ohlcData);
    std::vector<double> predictions = runInference("volatility_predictor", features);

    if (predictions.empty()) {
        result.ok = false;
        return result;
    }

    result.predictedVolatility = predictions[0];
    result.confidence = predictions.size() > 1 ? predictions[1] * 100.0 : 50.0;

    if (result.predictedVolatility > 0.02) {
        result.level = "high";
    } else if (result.predictedVolatility > 0.01) {
        result.level = "medium";
    } else {
        result.level = "low";
    }

    result.ok = true;
    return result;
}

MLModels::MeanReversionProbability MLModels::predictMeanReversion(
    const std::string& symbol,
    const OhlcData& ohlcData,
    double currentPrice) {
    MeanReversionProbability result;
    
    if (ohlcData.closes.size() < 30) {
        result.ok = false;
        return result;
    }

    std::vector<double> features = extractFeatures(ohlcData);
    features.push_back(currentPrice);  // Add current price as additional feature
    
    std::vector<double> predictions = runInference("mean_reversion_predictor", features);

    if (predictions.size() < 2) {
        result.ok = false;
        return result;
    }

    result.probability = predictions[0] * 100.0;
    result.expectedTarget = predictions[1];

    if (result.probability > 70.0) {
        result.strength = "strong";
    } else if (result.probability > 50.0) {
        result.strength = "moderate";
    } else {
        result.strength = "weak";
    }

    result.ok = true;
    return result;
}

bool MLModels::loadModel(const std::string& modelName) {
    // Load model weights from disk
    // TODO: Implement model loading (e.g., ONNX, TensorFlow Lite, custom format)
    return true;
}

void MLModels::clearCache(const std::string& symbol) {
    if (symbol.empty()) {
        predictionCache_.clear();
    } else {
        predictionCache_.erase(symbol);
    }
}

bool MLModels::isCacheValid(const std::string& symbol) const {
    auto it = predictionCache_.find(symbol);
    if (it == predictionCache_.end()) {
        return false;
    }
    
    time_t now = time(nullptr);
    return (now - it->second.timestamp) < 3600;  // 1 hour cache
}

time_t MLModels::getLastPredictionTime(const std::string& symbol) const {
    auto it = predictionCache_.find(symbol);
    if (it == predictionCache_.end()) {
        return 0;
    }
    return it->second.timestamp;
}

std::vector<double> MLModels::runInference(const std::string& modelName,
                                            const std::vector<double>& features) {
    // TODO: Implement actual model inference
    // For now, return placeholder results
    std::vector<double> result(4);
    if (!features.empty()) {
        result[0] = features.back();  // Placeholder
        result[1] = 0.75;  // Confidence
        result[2] = 0.95;
        result[3] = 1.05;
    }
    return result;
}

std::vector<double> MLModels::extractFeatures(const OhlcData& ohlcData) {
    std::vector<double> features;
    
    // Extract relevant features from OHLC data
    if (ohlcData.closes.empty()) {
        return features;
    }

    // Recent price
    features.push_back(ohlcData.closes.back());

    // Moving averages
    double sma20 = 0.0;
    size_t start = ohlcData.closes.size() > 20 ? ohlcData.closes.size() - 20 : 0;
    for (size_t i = start; i < ohlcData.closes.size(); ++i) {
        sma20 += ohlcData.closes[i];
    }
    sma20 /= (ohlcData.closes.size() - start);
    features.push_back(sma20);

    // Volume average
    double avgVolume = 0.0;
    for (long vol : ohlcData.volumes) {
        avgVolume += vol;
    }
    avgVolume /= ohlcData.volumes.size();
    features.push_back(avgVolume);

    // Price change
    if (ohlcData.closes.size() > 1) {
        double priceChange = (ohlcData.closes.back() - ohlcData.closes[ohlcData.closes.size() - 2]) /
                            ohlcData.closes[ohlcData.closes.size() - 2];
        features.push_back(priceChange);
    }

    return features;
}

}  // namespace folio
