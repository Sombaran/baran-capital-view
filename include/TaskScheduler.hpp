#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace folio {

class TaskScheduler {
public:
    using TaskFn = std::function<void()>;

    struct Task {
        std::string id;
        std::string type;
        TaskFn fn;
        int priority = 0;
        std::chrono::steady_clock::time_point scheduledAt;
    };

    explicit TaskScheduler(std::size_t workerCount = 2, std::size_t maxQueueSize = 256);
    ~TaskScheduler();

    std::string schedule(const std::string& type, TaskFn fn, int priority = 0);
    void shutdown();
    bool running() const;

private:
    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<Task> tasks_;
    std::vector<std::thread> workers_;
    bool running_ = false;
    std::size_t maxQueueSize_ = 0;
    std::size_t nextId_ = 0;
};

} // namespace folio
