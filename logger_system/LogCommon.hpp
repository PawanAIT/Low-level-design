#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <iostream>

// Enum for Log Levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// Data Object
struct LogMessage {
    LogLevel level;
    std::string message;
    std::string file;
    int line;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;
};

// Interface for Output Strategies
class ILogStrategy {
public:
    virtual ~ILogStrategy() = default;
    virtual void log(const LogMessage& msg) = 0;
};
