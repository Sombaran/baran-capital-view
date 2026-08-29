#include "TaskScheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace folio {

TaskScheduler::TaskScheduler(std::size_t workerCount, std::size_t maxQueueSize)
    : maxQueueSize_(maxQueueSize), running_(true) {
    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back(&TaskScheduler::workerLoop, this);
    }
}

TaskScheduler::~TaskScheduler() {
    shutdown();
}

std::string TaskScheduler::schedule(const std::string& type, TaskFn fn, int priority) {
    if (!running_) {
        throw std::runtime_error("task scheduler is shut down");
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (tasks_.size() >= maxQueueSize_) {
        throw std::runtime_error("task queue is full");
    }
    const std::string id = type + "-" + std::to_string(++nextId_);
    Task task{ id, type, std::move(fn), priority, std::chrono::steady_clock::now() };
    tasks_.push(std::move(task));
    condition_.notify_one();
    return id;
}

void TaskScheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

bool TaskScheduler::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void TaskScheduler::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return !running_ || !tasks_.empty();
            });
            if (!running_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        if (task.fn) {
            try {
                task.fn();
            } catch (...) {
                // Intentionally swallow to preserve scheduler uptime.
            }
        }
    }
}

} // namespace folio
