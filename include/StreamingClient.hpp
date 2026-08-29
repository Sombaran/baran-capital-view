#ifndef FOLIO_STREAMING_CLIENT_HPP
#define FOLIO_STREAMING_CLIENT_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>

namespace folio {

/**
 * @brief Real-time streaming client for Upstox Market Data Feed V3 (WebSocket).
 * 
 * Provides live quote streaming and holdings updates without polling.
 * Thread-safe quote cache serves the latest market data.
 * Handles automatic reconnection on network failure.
 * 
 * v2.0.0+: Replaces polling-based quote fetching with real-time WebSocket feeds.
 */
class StreamingClient {
public:
    struct Quote {
        std::string instrumentKey;
        double lastPrice = 0.0;
        double bidPrice = 0.0;
        double bidQty = 0.0;
        double askPrice = 0.0;
        double askQty = 0.0;
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
        long volume = 0;
        long oiValue = 0;
        long timestamp = 0;
    };

    struct DepthData {
        std::vector<std::pair<double, long>> bids;  // (price, qty) pairs
        std::vector<std::pair<double, long>> asks;  // (price, qty) pairs
    };

    explicit StreamingClient(std::string accessToken,
                             std::string baseUrl = "https://api.upstox.com");
    
    ~StreamingClient();

    /**
     * @brief Get authorization URL for WebSocket feed.
     * @return Authorized WebSocket URL or error string.
     */
    std::string getAuthorizedWsUrl() const;

    /**
     * @brief Connect to WebSocket and start streaming.
     * @param instrumentKeys Symbols to subscribe to.
     * @return true on success, false otherwise.
     */
    bool connect(const std::vector<std::string>& instrumentKeys);

    /**
     * @brief Disconnect from WebSocket stream.
     */
    void disconnect();

    /**
     * @brief Check if currently connected.
     */
    bool isConnected() const;

    /**
     * @brief Subscribe to additional instruments.
     * @param instrumentKeys Symbols to add to subscription.
     */
    void subscribe(const std::vector<std::string>& instrumentKeys);

    /**
     * @brief Unsubscribe from instruments.
     * @param instrumentKeys Symbols to remove from subscription.
     */
    void unsubscribe(const std::vector<std::string>& instrumentKeys);

    /**
     * @brief Get latest quote for an instrument.
     * @param instrumentKey Symbol identifier.
     * @return Quote struct if available, empty Quote if not.
     */
    Quote getQuote(const std::string& instrumentKey) const;

    /**
     * @brief Get depth data for an instrument.
     * @param instrumentKey Symbol identifier.
     * @return Depth with bid/ask levels, empty if not available.
     */
    DepthData getDepth(const std::string& instrumentKey) const;

    /**
     * @brief Set callback for quote updates.
     * Called whenever a new quote arrives from the stream.
     */
    using QuoteCallback = std::function<void(const Quote&)>;
    void setQuoteCallback(QuoteCallback callback);

    /**
     * @brief Set callback for depth updates.
     */
    using DepthCallback = std::function<void(const std::string&, const DepthData&)>;
    void setDepthCallback(DepthCallback callback);

    /**
     * @brief Enable/disable automatic reconnection.
     * @param enable true to auto-reconnect on disconnect.
     * @param delayMs delay between reconnection attempts.
     */
    void setAutoReconnect(bool enable, long delayMs = 5000);

private:
    std::string accessToken_;
    std::string baseUrl_;
    
    mutable std::mutex quoteMutex_;
    std::map<std::string, Quote> quoteCache_;
    std::map<std::string, DepthData> depthCache_;
    
    bool connected_ = false;
    bool autoReconnect_ = true;
    long reconnectDelayMs_ = 5000;

    QuoteCallback quoteCallback_;
    DepthCallback depthCallback_;

    // WebSocket handling (implementation-specific)
    class Impl;
    std::unique_ptr<Impl> impl_;

    void onQuoteUpdate(const Quote& quote);
    void onDepthUpdate(const std::string& instrumentKey, const DepthData& depth);
};

}  // namespace folio

#endif  // FOLIO_STREAMING_CLIENT_HPP
