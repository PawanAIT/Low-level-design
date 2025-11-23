#pragma once

#include "LogCommon.hpp"
#include "AsyncLogProcessor.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>

class Logger {
private:
    static Logger* instance;
    static std::mutex mutex_;
    LogLevel currentLevel = LogLevel::INFO;
    std::vector<std::shared_ptr<ILogStrategy>> strategies;
    AsyncLogProcessor asyncProcessor;

    Logger() : asyncProcessor(strategies) {}

public:
    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger* getInstance() {
        if (instance == nullptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance == nullptr) {
                instance = new Logger();
            }
        }
        return instance;
    }

    void setLevel(LogLevel level) {
        currentLevel = level;
    }

    void addStrategy(std::shared_ptr<ILogStrategy> strategy) {
        // Since strategies are accessed by the background thread, 
        // we should ideally protect this if we allow dynamic addition during runtime.
        // For simplicity in this LLD, we assume strategies are added at startup.
        // Or we could use a mutex in AsyncProcessor or here if shared.
        // Given AsyncProcessor holds a reference to 'strategies', we need to be careful.
        // A safer way is to copy-on-write or use a mutex in AsyncProcessor.
        // For this implementation, let's assume startup configuration.
        strategies.push_back(strategy);
    }

    void log(LogLevel level, const std::string& message, const char* file, int line) {
        if (level < currentLevel) return;

        LogMessage msg;
        msg.level = level;
        msg.message = message;
        msg.file = file;
        msg.line = line;
        msg.timestamp = std::chrono::system_clock::now();
        msg.threadId = std::this_thread::get_id();

        asyncProcessor.enqueue(std::move(msg));
    }

    // Convenience methods
    void debug(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
        log(LogLevel::DEBUG, msg, file, line);
    }
    
    void info(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
        log(LogLevel::INFO, msg, file, line);
    }
    
    void warn(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
        log(LogLevel::WARN, msg, file, line);
    }

    void error(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
        log(LogLevel::ERROR, msg, file, line);
    }
    
    void fatal(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
        log(LogLevel::FATAL, msg, file, line);
    }
};

// Initialize static members
Logger* Logger::instance = nullptr;
std::mutex Logger::mutex_;
