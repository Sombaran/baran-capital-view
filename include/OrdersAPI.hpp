#ifndef FOLIO_ORDERS_API_HPP
#define FOLIO_ORDERS_API_HPP

#include <string>
#include <vector>
#include <map>
#include <ctime>

namespace folio {

/**
 * @brief Upstox Orders API integration with automated execution support.
 * 
 * Executes buy/sell orders on Upstox platform when signals meet configured
 * criteria. All orders are logged for audit trail. Supports automatic
 * cancellation on adverse price moves.
 * 
 * Security: Orders require explicit user authorization with 2FA.
 * Risk management: Portfolio limits are enforced before execution.
 * 
 * v2.0.0+: Automates trade execution for qualified signals.
 */
class OrdersAPI {
public:
    enum class OrderType {
        LIMIT,           // Limit order
        MARKET,          // Market order
        STOP_LOSS,       // Stop-loss order
        STOP_LIMIT       // Stop-limit order
    };

    enum class OrderStatus {
        PENDING,         // Awaiting execution
        OPEN,            // Partial fill or waiting
        COMPLETED,       // Fully executed
        CANCELLED,       // User cancelled
        REJECTED,        // Exchange rejected
        EXPIRED          // Order expired
    };

    struct Order {
        std::string orderId;
        std::string symbol;
        OrderType type = OrderType::LIMIT;
        std::string side;              // "BUY" or "SELL"
        long quantity = 0;
        double price = 0.0;
        double triggerPrice = 0.0;    // For stop orders
        
        OrderStatus status = OrderStatus::PENDING;
        long filledQuantity = 0;
        double filledPrice = 0.0;
        double totalValue = 0.0;      // Quantity × Average filled price
        
        time_t createdTime = 0;
        time_t executedTime = 0;
        time_t modifiedTime = 0;
        
        std::string remarks;
        std::string exchangeOrderId;
    };

    struct OrderSimulation {
        bool valid = false;
        std::string reason;
        
        // Impact projections
        double projectedPortfolioValue = 0.0;
        double projectedHealthScore = 0.0;
        double riskAfterOrder = 0.0;   // Risk score 0–100
        
        double commissionEstimate = 0.0;  // Brokerage fees
        double slippageEstimate = 0.0;    // Expected slippage
        
        struct RiskCheck {
            bool positionRiskOk = true;
            bool sectorRiskOk = true;
            bool totalRiskOk = true;
            std::string violatedLimits;
        } riskChecks;
    };

    struct ExecutionConfig {
        // Authorization
        bool requiresManualApproval = true;
        bool require2FA = true;
        
        // Risk limits
        double maxOrderSize = 100000.0;    // Max INR per order
        double maxDailyExposure = 500000.0; // Max INR daily
        double maxPositionRisk = 2.0;      // Max % of portfolio
        
        // Execution strategy
        bool useMarketOrdersInGap = false;  // Use market orders on gaps
        double slippageThreshold = 0.5;    // Max acceptable slippage %
        
        // Automatic cancellation
        bool autoCancelOnAdverseMove = true;
        double adverseMoveThreshold = 2.0;  // Cancel if price moves 2% adverse
        int cancelAfterMinutes = 30;       // Auto-cancel limit orders after N min
        
        // Logging
        bool logAllOrders = true;
        std::string auditLogPath = "config/orders_audit.log";
    };

    /**
     * @brief Constructor.
     * @param accessToken Upstox access token.
     * @param baseUrl Upstox API base URL.
     * @param config Execution configuration.
     */
    explicit OrdersAPI(const std::string& accessToken,
                       const std::string& baseUrl,
                       const ExecutionConfig& config);

    /**
     * @brief Simulate order impact on portfolio.
     * @param symbol Stock symbol.
     * @param quantity Units to buy/sell.
     * @param side "BUY" or "SELL".
     * @param type Limit/market/stop orders.
     * @param price Price for limit orders.
     * @param currentPortfolioValue Current portfolio total value.
     * @param currentHealthScore Current health score.
     * @return Simulation result showing projected impact.
     */
    OrderSimulation simulateOrder(const std::string& symbol,
                                   long quantity,
                                   const std::string& side,
                                   OrderType type,
                                   double price,
                                   double currentPortfolioValue,
                                   double currentHealthScore);

    /**
     * @brief Place an order (requires pre-approval or manual authorization).
     * @param symbol Stock symbol.
     * @param quantity Units to buy/sell.
     * @param side "BUY" or "SELL".
     * @param type Limit/market/stop orders.
     * @param price Price for limit orders.
     * @param triggerPrice Trigger price for stop orders.
     * @param reason Signal reason (for audit logging).
     * @return Order object with ID if successful.
     */
    Order placeOrder(const std::string& symbol,
                     long quantity,
                     const std::string& side,
                     OrderType type,
                     double price,
                     double triggerPrice = 0.0,
                     const std::string& reason = "");

    /**
     * @brief Place market order (convenience method).
     */
    Order placeMarketOrder(const std::string& symbol,
                          long quantity,
                          const std::string& side,
                          const std::string& reason = "");

    /**
     * @brief Cancel an open order.
     * @param orderId Order ID from placeOrder() result.
     * @return true if cancelled successfully.
     */
    bool cancelOrder(const std::string& orderId);

    /**
     * @brief Get order status.
     */
    OrderStatus getOrderStatus(const std::string& orderId);

    /**
     * @brief Get order details.
     */
    Order getOrderDetails(const std::string& orderId);

    /**
     * @brief List all orders from today.
     */
    std::vector<Order> getTodayOrders();

    /**
     * @brief List open orders (unfilled/partial).
     */
    std::vector<Order> getOpenOrders();

    /**
     * @brief Authorize 2FA for order placement.
     * @param token 2FA token from user's authenticator app.
     * @return true if authorization successful.
     */
    bool authorize2FA(const std::string& token);

    /**
     * @brief Revoke 2FA authorization.
     */
    void revoke2FA();

    /**
     * @brief Check if currently authorized for order placement.
     */
    bool isAuthorized() const;

    /**
     * @brief Update execution configuration.
     */
    void setExecutionConfig(const ExecutionConfig& config);

    /**
     * @brief Get current configuration.
     */
    ExecutionConfig getExecutionConfig() const;

    /**
     * @brief Enable/disable automatic order execution.
     */
    void setAutoExecutionEnabled(bool enabled);

    /**
     * @brief Check if automatic execution is enabled.
     */
    bool isAutoExecutionEnabled() const;

private:
    std::string accessToken_;
    std::string baseUrl_;
    ExecutionConfig config_;
    bool autoExecutionEnabled_ = false;
    bool authorized2FA_ = false;
    time_t auth2FAExpiryTime_ = 0;

    // Helper methods
    bool validateOrderRisks(const Order& order,
                            double portfolioValue) const;
    
    void logOrder(const Order& order, const std::string& action);
    
    double estimateCommission(const Order& order) const;
    double estimateSlippage(const std::string& symbol,
                            long quantity,
                            const std::string& side) const;

    bool checkConcentrationRisk(const std::string& symbol,
                                long quantity) const;
};

}  // namespace folio

#endif  // FOLIO_ORDERS_API_HPP
