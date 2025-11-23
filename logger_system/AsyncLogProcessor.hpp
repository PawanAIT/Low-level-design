#pragma once

#include "LogCommon.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>

class AsyncLogProcessor {
    std::queue<LogMessage> queue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread workerThread_;
    std::atomic<bool> stop_{false};
    std::vector<std::shared_ptr<ILogStrategy>>& strategies_;

    void process() {
        while (true) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this]{ return !queue_.empty() || stop_; });

            if (stop_ && queue_.empty()) return;

            // Process all messages in the queue (batching could be added here)
            // For now, process one by one but loop until empty to reduce locking overhead if we used pop separately
            // However, typical implementation is one pop per loop iteration.
            
            // To ensure we process all items before exiting if stop_ is true:
            // The condition (stop_ && queue_.empty()) handles the exit.
            // If !queue_.empty(), we proceed.

            LogMessage msg = queue_.front();
            queue_.pop();
            lock.unlock();

            // Dispatch to all strategies
            for (auto& strategy : strategies_) {
                if (strategy) {
                    strategy->log(msg);
                }
            }
        }
    }

public:
    AsyncLogProcessor(std::vector<std::shared_ptr<ILogStrategy>>& strats) 
        : strategies_(strats) {
        workerThread_ = std::thread(&AsyncLogProcessor::process, this);
    }
    
    void enqueue(LogMessage msg) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            queue_.push(std::move(msg));
        }
        queueCv_.notify_one();
    }

    ~AsyncLogProcessor() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        queueCv_.notify_one();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }
};
