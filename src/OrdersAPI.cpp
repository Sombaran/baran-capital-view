#include "OrdersAPI.hpp"
#include "SecureUtils.hpp"
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace folio {

OrdersAPI::OrdersAPI(const std::string& accessToken,
                     const std::string& baseUrl,
                     const ExecutionConfig& config)
    : accessToken_(accessToken),
      baseUrl_(baseUrl),
      config_(config) {
}

OrdersAPI::OrderSimulation OrdersAPI::simulateOrder(
    const std::string& symbol,
    long quantity,
    const std::string& side,
    OrderType type,
    double price,
    double currentPortfolioValue,
    double currentHealthScore) {
    
    OrderSimulation sim;
    
    // ✅ FIX 3: Input validation for simulation
    if (!security::validateSymbol(symbol)) {
        sim.reason = "Invalid symbol format";
        return sim;
    }
    
    if (!security::validateQuantity(quantity)) {
        sim.reason = "Invalid quantity";
        return sim;
    }
    
    if (!security::validatePrice(price)) {
        sim.reason = "Invalid price";
        return sim;
    }
    
    if (!security::validateSide(side)) {
        sim.reason = "Invalid side (must be BUY or SELL)";
        return sim;
    }

    // Estimate commission (typically 0.05% for NSE equity)
    sim.commissionEstimate = (quantity * price) * 0.0005;

    // Estimate slippage
    sim.slippageEstimate = price * (config_.slippageThreshold / 100.0);

    // Calculate projected portfolio value
    double orderValue = (quantity * price) + sim.commissionEstimate + sim.slippageEstimate;
    if (side == "BUY") {
        sim.projectedPortfolioValue = currentPortfolioValue + (quantity * price);
    } else {
        sim.projectedPortfolioValue = currentPortfolioValue - (quantity * price);
    }

    // Project health score impact (simplified)
    if (side == "BUY") {
        sim.projectedHealthScore = currentHealthScore - (orderValue / currentPortfolioValue) * 2.0;
    } else {
        sim.projectedHealthScore = currentHealthScore + (orderValue / currentPortfolioValue) * 2.0;
    }

    // Check risk limits
    double positionPercent = (quantity * price) / currentPortfolioValue * 100.0;
    sim.riskChecks.positionRiskOk = positionPercent <= config_.maxPositionRisk;

    // Check daily exposure
    sim.riskChecks.totalRiskOk = (quantity * price) <= config_.maxDailyExposure;

    // Overall validation
    sim.valid = sim.riskChecks.positionRiskOk &&
                sim.riskChecks.sectorRiskOk &&
                sim.riskChecks.totalRiskOk;

    if (!sim.valid) {
        sim.reason = "Risk limits violated";
        if (!sim.riskChecks.positionRiskOk) {
            sim.riskChecks.violatedLimits += "position_risk ";
        }
        if (!sim.riskChecks.sectorRiskOk) {
            sim.riskChecks.violatedLimits += "sector_risk ";
        }
        if (!sim.riskChecks.totalRiskOk) {
            sim.riskChecks.violatedLimits += "daily_exposure ";
        }
    } else {
        sim.reason = "Order simulation valid";
    }

    return sim;
}

OrdersAPI::Order OrdersAPI::placeOrder(
    const std::string& symbol,
    long quantity,
    const std::string& side,
    OrderType type,
    double price,
    double triggerPrice,
    const std::string& reason) {
    
    Order order;
    
    // ✅ FIX 3: Comprehensive input validation
    if (!security::validateSymbol(symbol)) {
        order.status = OrderStatus::REJECTED;
        order.remarks = "Invalid symbol format. Expected NSE_EQ|ISIN";
        return order;
    }
    
    if (!security::validateQuantity(quantity)) {
        order.status = OrderStatus::REJECTED;
        order.remarks = "Invalid quantity. Must be between 1 and 1,000,000";
        return order;
    }
    
    if (!security::validatePrice(price)) {
        order.status = OrderStatus::REJECTED;
        order.remarks = "Invalid price. Must be between ₹0.01 and ₹100,000";
        return order;
    }
    
    if (!security::validateSide(side)) {
        order.status = OrderStatus::REJECTED;
        order.remarks = "Invalid side. Must be BUY or SELL";
        return order;
    }
    
    order.symbol = symbol;
    order.quantity = (side == "SELL") ? -quantity : quantity;
    order.side = side;
    order.type = type;
    order.price = price;
    order.triggerPrice = triggerPrice;
    order.status = OrderStatus::PENDING;
    order.createdTime = time(nullptr);

    if (config_.logAllOrders) {
        logOrder(order, "PLACED");
    }

    // TODO: Call Upstox Orders API to place actual order
    // order.orderId = ...
    // order.exchangeOrderId = ...

    return order;
}

OrdersAPI::Order OrdersAPI::placeMarketOrder(
    const std::string& symbol,
    long quantity,
    const std::string& side,
    const std::string& reason) {
    
    // Market orders use current LTP (typically 0 or filled on execute)
    return placeOrder(symbol, quantity, side, OrderType::MARKET, 0.0, 0.0, reason);
}

bool OrdersAPI::cancelOrder(const std::string& orderId) {
    // TODO: Call Upstox Orders API to cancel
    if (config_.logAllOrders) {
        // logOrder(order, "CANCELLED");
    }
    return true;
}

OrdersAPI::OrderStatus OrdersAPI::getOrderStatus(const std::string& orderId) {
    // TODO: Call Upstox Orders API to get status
    return OrderStatus::PENDING;
}

OrdersAPI::Order OrdersAPI::getOrderDetails(const std::string& orderId) {
    Order order;
    // TODO: Call Upstox Orders API to get details
    return order;
}

std::vector<OrdersAPI::Order> OrdersAPI::getTodayOrders() {
    std::vector<Order> orders;
    // TODO: Call Upstox Orders API to get today's orders
    return orders;
}

std::vector<OrdersAPI::Order> OrdersAPI::getOpenOrders() {
    std::vector<Order> orders;
    // TODO: Call Upstox Orders API to get open orders
    return orders;
}

bool OrdersAPI::authorize2FA(const std::string& token) {
    // Validate 2FA token (typically 6-digit code)
    if (token.length() < 4) {
        return false;
    }

    authorized2FA_ = true;
    auth2FAExpiryTime_ = time(nullptr) + 3600;  // Valid for 1 hour
    return true;
}

void OrdersAPI::revoke2FA() {
    authorized2FA_ = false;
    auth2FAExpiryTime_ = 0;
}

bool OrdersAPI::isAuthorized() const {
    if (!authorized2FA_) {
        return false;
    }

    return time(nullptr) < auth2FAExpiryTime_;
}

void OrdersAPI::setExecutionConfig(const ExecutionConfig& config) {
    config_ = config;
}

OrdersAPI::ExecutionConfig OrdersAPI::getExecutionConfig() const {
    return config_;
}

void OrdersAPI::setAutoExecutionEnabled(bool enabled) {
    autoExecutionEnabled_ = enabled;
}

bool OrdersAPI::isAutoExecutionEnabled() const {
    return autoExecutionEnabled_;
}

bool OrdersAPI::validateOrderRisks(const Order& order,
                                    double portfolioValue) const {
    double orderValue = std::abs(order.quantity) * order.price;
    double riskPercent = (orderValue / portfolioValue) * 100.0;

    return riskPercent <= config_.maxPositionRisk &&
           orderValue <= config_.maxOrderSize;
}

void OrdersAPI::logOrder(const Order& order, const std::string& action) {
    std::ofstream logFile(config_.auditLogPath, std::ios::app);
    if (!logFile.is_open()) {
        return;
    }

    auto now = time(nullptr);
    auto tm = *std::localtime(&now);

    logFile << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " | "
            << action << " | "
            << order.symbol << " | "
            << order.side << " | "
            << order.quantity << " | "
            << order.price << " | "
            << order.orderId << "\n";

    logFile.close();
}

double OrdersAPI::estimateCommission(const Order& order) const {
    // Typically 0.05% for NSE equity
    return (std::abs(order.quantity) * order.price) * 0.0005;
}

double OrdersAPI::estimateSlippage(const std::string& symbol,
                                    long quantity,
                                    const std::string& side) const {
    // Slippage estimate based on order size and symbol liquidity
    // Typically 0.01-0.05% for liquid stocks
    return quantity > 10000 ? 0.0005 : 0.0001;
}

bool OrdersAPI::checkConcentrationRisk(const std::string& symbol,
                                        long quantity) const {
    // Check if this order would violate sector concentration limits
    // TODO: Implement actual concentration checking
    return true;
}

}  // namespace folio
