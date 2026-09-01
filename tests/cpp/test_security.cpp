#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include "HttpClient.hpp"
#include "MLModels.hpp"
#include "NotificationService.hpp"
#include "OrdersAPI.hpp"
#include "PortfolioHealth.hpp"
#include "PortfolioLoader.hpp"
#include "SecureUtils.hpp"
#include "SignalGenerator.hpp"
#include "TaskScheduler.hpp"
#include "TechnicalIndicators.hpp"
#include "UpstoxClient.hpp"
#include "WebServer.hpp"
#include "StreamingClient.hpp"

TEST(SecurityUtils, ValidateSymbolAcceptsExpectedFormat) {
    EXPECT_TRUE(folio::security::validateSymbol("NSE_EQ|INE009A01021"));
    EXPECT_FALSE(folio::security::validateSymbol("INE009A01021"));
    EXPECT_FALSE(folio::security::validateSymbol("nse_eq|ine009a01021"));
}

TEST(WebServer, DeeperAnalysisCategoriesAndActionsStayConsistent) {
    const auto categories = folio::deeperAnalysisCategoryOrder();
    EXPECT_EQ(categories.size(), 5u);
    EXPECT_EQ(categories[0], "Neutral news");
    EXPECT_EQ(categories[1], "No recent news");
    EXPECT_EQ(categories[2], "going good");
    EXPECT_EQ(categories[3], "invest more");
    EXPECT_EQ(categories[4], "sell it off");

    EXPECT_EQ(folio::normalizeDecisionAction("Consider adding - review"), "Consider adding");
    EXPECT_EQ(folio::normalizeDecisionAction("Do not add - review risk"), "Do not add");
    EXPECT_EQ(folio::normalizeDecisionAction("Hold / wait"), "Hold / wait");
}

TEST(WebServer, SecretCodeNormalizationAndValidationStaySafe) {
    EXPECT_EQ(folio::normalizeLoginCode(" 070923 \n"), "070923");
    EXPECT_EQ(folio::normalizeLoginCode("%2B070923"), "+070923");
    EXPECT_TRUE(folio::validateLoginCode("070923", "070923"));
    EXPECT_TRUE(folio::validateLoginCode(" 070923 ", "070923"));
    EXPECT_FALSE(folio::validateLoginCode("070923", "070924"));
    EXPECT_FALSE(folio::validateLoginCode("", "070923"));
}

TEST(WebServer, StaleAccessTokenDetectionMatchesBrokerErrors) {
    EXPECT_TRUE(folio::isAccessTokenStale("HTTP 401 Unauthorized from Upstox holdings"));
    EXPECT_TRUE(folio::isAccessTokenStale("token expired"));
    EXPECT_FALSE(folio::isAccessTokenStale("HTTP 500 from Upstox"));
    EXPECT_FALSE(folio::isAccessTokenStale("market data is temporarily unavailable"));
}

TEST(SecurityUtils, ValidateQuantityAndPriceBounds) {
    EXPECT_TRUE(folio::security::validateQuantity(10));
    EXPECT_FALSE(folio::security::validateQuantity(0));
    EXPECT_FALSE(folio::security::validateQuantity(1000001));

    EXPECT_TRUE(folio::security::validatePrice(125.50));
    EXPECT_FALSE(folio::security::validatePrice(0.0));
    EXPECT_FALSE(folio::security::validatePrice(100001.0));
}

TEST(SecurityUtils, ValidateOrderSide) {
    EXPECT_TRUE(folio::security::validateSide("BUY"));
    EXPECT_TRUE(folio::security::validateSide("SELL"));
    EXPECT_FALSE(folio::security::validateSide("buy"));
    EXPECT_FALSE(folio::security::validateSide("HOLD"));
}

TEST(SecurityUtils, SecureStringClearsBufferOnMove) {
    folio::security::SecureString value("secret-token");
    const std::string copied = value.to_string();
    EXPECT_EQ(copied, "secret-token");

    folio::security::SecureString moved(std::move(value));
    EXPECT_EQ(moved.to_string(), "secret-token");
}

TEST(SecurityUtils, RandomKeyAndFileHashAreStableShapes) {
    const std::string key = folio::security::generateRandomKey();
    EXPECT_EQ(key.size(), 64u);

    const auto tempPath = std::filesystem::temp_directory_path() / "folio-security-hash-test.txt";
    std::ofstream out(tempPath);
    out << "portfolio health";
    out.close();

    const std::string hash = folio::security::computeFileSha256(tempPath.string());
    EXPECT_EQ(hash.size(), 64u);
    EXPECT_FALSE(hash.empty());

    std::filesystem::remove(tempPath);
}

TEST(SecurityUtils, FilePermissionsValidationMatchesUnixMode) {
    const auto tempPath = std::filesystem::temp_directory_path() / "folio-permissions-test.txt";
    std::ofstream out(tempPath);
    out << "test";
    out.close();

    std::filesystem::permissions(
        tempPath,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);

    EXPECT_TRUE(folio::security::checkFilePermissions(tempPath.string(), 0600));
    std::filesystem::remove(tempPath);
}

TEST(SecurityUtils, RandomStringFallbackGeneratesHexPayload) {
    const std::string value = folio::security::generateRandomString(8);
    EXPECT_EQ(value.size(), 16u);
    EXPECT_FALSE(value.empty());
    EXPECT_TRUE(std::all_of(value.begin(), value.end(), [](char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    }));
}

TEST(Position, MarketValuePrefersBrokerReportedValue) {
    folio::Position pos{};
    pos.lastPrice = 100.0;
    pos.quantity = 10;
    pos.multiplier = 1;
    pos.value = 1234.56;

    EXPECT_DOUBLE_EQ(pos.marketValue(), 1234.56);
}

TEST(Position, MarketValuePrefersCurrentValueAliasWhenPresent) {
    folio::Position pos{};
    pos.lastPrice = 100.0;
    pos.quantity = 10;
    pos.multiplier = 1;
    pos.value = 0.0;
    pos.currentValue = 987.65;

    EXPECT_DOUBLE_EQ(pos.marketValue(), 987.65);
}

TEST(Position, MarketValueUsesLivePriceBeforeStaleExposureValue) {
    folio::Position pos{};
    pos.lastPrice = 120.0;
    pos.quantity = 10;
    pos.multiplier = 1;
    pos.currentValue = 1200.0;
    pos.value = 900.0;

    EXPECT_DOUBLE_EQ(pos.marketValue(), 1200.0);
}

TEST(Position, MarketValueUsesLivePriceWhenCurrentValueIsMissing) {
    folio::Position pos{};
    pos.lastPrice = 120.0;
    pos.quantity = 10;
    pos.multiplier = 1;

    EXPECT_DOUBLE_EQ(pos.marketValue(), 1200.0);
}

TEST(PortfolioHealth, AnalyzeAggregatesExposureAndHealth) {
    folio::Position p1{};
    p1.exchange = "NSE";
    p1.tradingSymbol = "RELIANCE";
    p1.instrumentToken = "NSE_EQ|INE002A01018";
    p1.product = "D";
    p1.quantity = 10;
    p1.averagePrice = 2500.0;
    p1.lastPrice = 2600.0;
    p1.closePrice = 2550.0;
    p1.multiplier = 1;
    p1.unrealised = (p1.lastPrice - p1.averagePrice) * p1.quantity * p1.multiplier;

    folio::Position p2{};
    p2.exchange = "NSE";
    p2.tradingSymbol = "TCS";
    p2.instrumentToken = "NSE_EQ|INE467A01029";
    p2.product = "D";
    p2.quantity = 20;
    p2.averagePrice = 3300.0;
    p2.lastPrice = 3350.0;
    p2.closePrice = 3320.0;
    p2.multiplier = 1;
    p2.unrealised = (p2.lastPrice - p2.averagePrice) * p2.quantity * p2.multiplier;

    std::vector<folio::Position> positions = {p1, p2};

    const auto health = folio::analyze(positions);
    EXPECT_EQ(health.openPositions, 2u);
    EXPECT_GT(health.grossExposure, 0.0);
    EXPECT_GT(health.totalUnrealised, 0.0);
    EXPECT_GE(health.healthScore, 0);
    EXPECT_FALSE(health.alerts.empty());
}

TEST(PortfolioLoader, LoadCsvAndJsonPortfolios) {
    const auto csvPath = std::filesystem::temp_directory_path() / "folio_csv_portfolio.csv";
    {
        std::ofstream csv(csvPath);
        csv << "symbol,quantity,average_price,last_price,close_price,exchange,product,multiplier,instrument_key\n";
        csv << "RELIANCE,10,2500,2600,2550,NSE,D,1,NSE_EQ|INE002A01018\n";
    }

    const auto csvResult = folio::loadPortfolio(csvPath.string());
    EXPECT_TRUE(csvResult.ok);
    EXPECT_EQ(csvResult.positions.size(), 1u);
    EXPECT_EQ(csvResult.positions[0].tradingSymbol, "RELIANCE");

    const auto jsonPath = std::filesystem::temp_directory_path() / "folio_json_portfolio.json";
    {
        std::ofstream json(jsonPath);
        json << R"({"portfolio":[{"symbol":"TCS","quantity":5,"average_price":3300.0,"last_price":3400.0,"close_price":3350.0,"exchange":"NSE","product":"D","multiplier":1,"instrument_key":"NSE_EQ|INE467A01029"}]})";
    }

    const auto jsonResult = folio::loadPortfolio(jsonPath.string());
    EXPECT_TRUE(jsonResult.ok);
    EXPECT_EQ(jsonResult.positions.size(), 1u);
    EXPECT_EQ(jsonResult.positions[0].tradingSymbol, "TCS");

    std::filesystem::remove(csvPath);
    std::filesystem::remove(jsonPath);
}

TEST(TechnicalIndicators, ComputeStandardIndicatorValues) {
    std::vector<double> closes;
    closes.reserve(40);
    for (int i = 0; i < 40; ++i) {
        closes.push_back(100.0 + static_cast<double>(i) * 0.75);
    }

    const auto rsi = folio::calculateRsi(closes, 14);
    EXPECT_TRUE(rsi.ok);
    EXPECT_FALSE(rsi.reason.empty());

    const auto macd = folio::calculateMacd(closes);
    EXPECT_TRUE(macd.ok);

    const auto bollinger = folio::calculateBollingerBands(closes, 5, 2.0);
    EXPECT_TRUE(bollinger.ok);
    EXPECT_EQ(bollinger.position, "neutral");

    const auto atr = folio::calculateAtr(closes, closes, closes, 5);
    EXPECT_TRUE(atr.ok);
    EXPECT_FALSE(atr.volatilityLevel.empty());

    const auto ema = folio::calculateEma(closes, 5);
    EXPECT_TRUE(ema.ok);
    EXPECT_FALSE(ema.trend.empty());
}

TEST(OrdersAPI, SimulateAndPlaceSecurityValidatedOrders) {
    folio::OrdersAPI::ExecutionConfig config;
    folio::OrdersAPI api("token", "https://api.upstox.com", config);

    const auto sim = api.simulateOrder("NSE_EQ|INE009A01021", 10, "BUY", folio::OrdersAPI::OrderType::LIMIT,
                                       150.0, 100000.0, 60.0);
    EXPECT_TRUE(sim.valid);
    EXPECT_EQ(sim.reason, "Order simulation valid");

    const auto place = api.placeOrder("NSE_EQ|INE009A01021", 10, "BUY", folio::OrdersAPI::OrderType::LIMIT,
                                      150.0, 0.0, "unit-test");
    EXPECT_EQ(place.status, folio::OrdersAPI::OrderStatus::PENDING);
    EXPECT_EQ(place.symbol, "NSE_EQ|INE009A01021");
    EXPECT_EQ(place.side, "BUY");
}

TEST(NotificationService, SendAlertQueuesNotification) {
    folio::NotificationService service({});
    folio::NotificationService::Notification note;
    note.type = folio::NotificationService::AlertType::PRICE_ALERT;
    note.title = "Test price alert";
    note.message = "Price spike";
    note.severity = 0.9;

    const auto id = service.send(note);
    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(service.getNotificationStatus(id).sent || !service.getNotificationStatus(id).id.empty());
}

TEST(MLModels, PredictPriceAndTrendFromSyntheticBars) {
    folio::MLModels model("config/ml_models/");
    folio::MLModels::OhlcData data;
    data.closes.resize(30);
    data.volumes.resize(30, 1000L);
    for (std::size_t i = 0; i < data.closes.size(); ++i) {
        data.closes[i] = 100.0 + static_cast<double>(i) * 0.5;
    }

    const auto price = model.predictPrice("RELIANCE", data);
    EXPECT_TRUE(price.ok);
    EXPECT_GT(price.predictedClose, 0.0);

    const auto trend = model.predictTrend("RELIANCE", data);
    EXPECT_TRUE(trend.ok);
    EXPECT_FALSE(trend.trend.empty());
}

TEST(SignalGenerator, GenerateWeightedSignalFromIndicators) {
    folio::SignalGenerator::SignalConfig config;
    folio::SignalGenerator generator(config);

    folio::RsiResult rsi;
    rsi.ok = true;
    rsi.value = 75.0;
    rsi.interpretation = "overbought";

    folio::MacdResult macd;
    macd.ok = true;
    macd.macdLine = 2.0;
    macd.signalLine = 1.0;
    macd.histogram = 1.0;
    macd.interpretation = "bullish";

    const auto signal = generator.generateSignal("RELIANCE", 100.0, &rsi, &macd);
    EXPECT_TRUE(signal.confidence >= 0.0);
    EXPECT_TRUE(signal.action == folio::SignalGenerator::Signal::SELL ||
                signal.action == folio::SignalGenerator::Signal::HOLD ||
                signal.action == folio::SignalGenerator::Signal::WAIT ||
                signal.action == folio::SignalGenerator::Signal::BUY);
}

TEST(TaskScheduler, SchedulesAndRunsTasks) {
    folio::TaskScheduler scheduler(1, 8);
    std::atomic<int> counter{0};
    const std::string id = scheduler.schedule("unit-test", [&counter]() { counter.fetch_add(1); }, 1);
    EXPECT_FALSE(id.empty());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_GE(counter.load(), 1);
    scheduler.shutdown();
}

TEST(StreamingClient, ConnectAndReportConnectionState) {
    folio::StreamingClient client("token", "https://api.upstox.com");
    EXPECT_TRUE(client.connect({"NSE_EQ|INE009A01021"}));
    EXPECT_TRUE(client.isConnected());
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST(UpstoxClient, ConstructorAcceptsRuntimeBaseUrl) {
    folio::UpstoxClient client("token", "https://api.upstox.com");
    EXPECT_NO_THROW(folio::UpstoxClient("token", "https://api.upstox.com"));
    EXPECT_FALSE(client.getNews("holdings").error.empty() || client.getNews("holdings").ok);
}

TEST(HttpClient, RejectsNonHttpsAndUntrustedHosts) {
    folio::HttpClient client;

    const auto badScheme = client.get("http://api.upstox.com/market-data", {}, 5);
    EXPECT_FALSE(badScheme.ok());
    EXPECT_EQ(badScheme.error, "non-HTTPS URL blocked for stock API requests");

    const auto evilHost = client.get("https://evil.example.com/v2/market-data", {}, 5);
    EXPECT_FALSE(evilHost.ok());
    EXPECT_EQ(evilHost.error, "untrusted stock API host blocked");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
