#pragma once

#include "LogCommon.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <mutex>

// Helper to format timestamp
inline std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Helper to stringify log level
inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

class ConsoleLogStrategy : public ILogStrategy {
    std::mutex mutex_; // Prevent interleaved output on console
public:
    void log(const LogMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[" << formatTimestamp(msg.timestamp) << "] "
                  << "[" << logLevelToString(msg.level) << "] "
                  << "[" << msg.threadId << "] "
                  << "[" << msg.file << ":" << msg.line << "] "
                  << msg.message << std::endl;
    }
};

class FileLogStrategy : public ILogStrategy {
    std::string filePath_;
    std::ofstream fileStream_;
    size_t maxSizeBytes_;
    std::mutex mutex_;

    void rotate() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
        
        // Rename current file to log.timestamp.bak
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << filePath_ << "." << std::put_time(std::localtime(&t), "%Y%m%d%H%M%S") << ".bak";
        
        try {
            std::filesystem::rename(filePath_, ss.str());
        } catch (const std::exception& e) {
            std::cerr << "Log rotation failed: " << e.what() << std::endl;
        }

        fileStream_.open(filePath_, std::ios::out | std::ios::app);
    }

public:
    FileLogStrategy(const std::string& path, size_t maxSize = 10 * 1024 * 1024) 
        : filePath_(path), maxSizeBytes_(maxSize) {
        fileStream_.open(filePath_, std::ios::out | std::ios::app);
    }

    ~FileLogStrategy() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    void log(const LogMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!fileStream_.is_open()) return;

        std::stringstream ss;
        ss << "[" << formatTimestamp(msg.timestamp) << "] "
           << "[" << logLevelToString(msg.level) << "] "
           << "[" << msg.threadId << "] "
           << "[" << msg.file << ":" << msg.line << "] "
           << msg.message << "\n";
        
        std::string out = ss.str();

        // Check rotation
        std::streampos currentPos = fileStream_.tellp();
        if (currentPos != -1 && static_cast<size_t>(currentPos) + out.size() > maxSizeBytes_) {
            rotate();
        }

        fileStream_ << out;
        fileStream_.flush(); 
    }
};
