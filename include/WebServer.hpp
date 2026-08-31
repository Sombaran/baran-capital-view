#pragma once

#include "UpstoxClient.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>

namespace folio {

std::string normalizeSymbol(std::string symbol);
std::vector<std::string> deeperAnalysisCategoryOrder();
std::string normalizeDecisionAction(const std::string& value);
std::string normalizeLoginCode(const std::string& value);
bool validateLoginCode(const std::string& submittedValue, const std::string& configuredValue);

class WebServer {
public:
    WebServer(const UpstoxClient& client, std::string holdingsFile,
              int port = 8080);

    int run();

private:
    using MutexGuard = std::lock_guard<std::mutex>;

    struct Snapshot {
        std::string holdings;
        std::string positions;
        std::string news;
        std::string metrics;
        std::chrono::steady_clock::time_point created;
    };

    std::shared_ptr<const Snapshot> snapshot() const;
    std::string runDeeperAnalysis() const;
    std::string stockAnalysis(const std::string& symbol) const;
    std::string fundamentalsAnalysis(const std::string& symbol) const;
    bool authenticated(const std::string& request) const;
    std::string newSession();
    void removeSession(const std::string& request);

    const UpstoxClient& client_;
    std::string holdingsFile_;
    int port_;
    mutable std::mutex snapshotMutex_;
    mutable std::mutex refreshMutex_;
    mutable std::shared_ptr<const Snapshot> snapshot_;
    mutable std::mutex sessionMutex_;
    std::unordered_set<std::string> sessions_;
    mutable std::mutex analysisMutex_;
    mutable bool analysisRunning_ = false;
    mutable std::string analysisResult_;
    mutable std::string analysisError_;
    mutable std::chrono::steady_clock::time_point analysisCreated_{};
    mutable std::unordered_set<std::string> stockAnalysisRunning_;
    mutable std::unordered_map<std::string, std::string> stockAnalysisResults_;
    mutable std::unordered_map<std::string, std::string> stockAnalysisErrors_;
    mutable std::unordered_set<std::string> fundamentalsRunning_;
    mutable std::unordered_map<std::string, std::string> fundamentalsResults_;
    mutable std::unordered_map<std::string, std::string> fundamentalsErrors_;
};

} // namespace folio
