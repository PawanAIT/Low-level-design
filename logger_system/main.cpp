#include "Logger.hpp"
#include "Strategies.hpp"
#include <thread>
#include <vector>

void worker(int id) {
    for (int i = 0; i < 5; ++i) {
        Logger::getInstance()->info("Message from worker " + std::to_string(id) + " iteration " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    auto logger = Logger::getInstance();
    
    // Configure Strategies
    logger->addStrategy(std::make_shared<ConsoleLogStrategy>());
    
    // File strategy with small rotation limit (e.g., 1KB) to demonstrate rotation
    logger->addStrategy(std::make_shared<FileLogStrategy>("app.log", 1024));

    logger->setLevel(LogLevel::DEBUG);

    logger->info("Application started");
    logger->debug("This is a debug message");
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    logger->warn("Application shutting down");
    
    // Allow some time for async logs to flush (in production, use proper shutdown handling)
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
