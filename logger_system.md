# LLD Solution for Logger System (Strategy Pattern)

## 1. Problem Analysis
### Requirements
- **Functional**:
  - Support different log levels (DEBUG, INFO, WARN, ERROR, FATAL).
  - Support multiple output destinations (Console, File, Database) simultaneously.
  - Allow dynamic configuration of log levels and output destinations.
  - Support log rotation (based on size or time) and archival for file outputs.
  - Include timestamps, thread IDs, and source location (file/line) in logs.
- **Non-Functional**:
  - **Low Latency**: Logging should have minimal impact on the main application's performance.
  - **Thread Safety**: Must support concurrent logging from multiple threads.
  - **Reliability**: No log loss during normal operation; handle write failures gracefully.
  - **Extensibility**: Easy to add new log destinations (e.g., Network, Cloud CloudWatch) or formats (JSON, Plain Text).
- **Constraints**:
  - C++17 or later standard.
  - Disk I/O should not block the main execution path.
- **Assumptions**:
  - The system has filesystem access for file logging.
  - Database connectivity is handled by a separate connector/library if used.

## 2. High-Level Design
### System Components
1.  **Logger (Singleton)**: The central entry point for the client application to submit log messages. It manages configuration and dispatches messages.
2.  **LogMessage**: A data object encapsulating all log details (level, message, timestamp, metadata).
3.  **LogStrategy (Interface)**: Defines the contract for output destinations.
4.  **Concrete Strategies**: `ConsoleLogStrategy`, `FileLogStrategy`, `DatabaseLogStrategy`.
5.  **AsyncProcessor**: A background worker that decouples log submission from log writing to ensure performance.
6.  **Formatter**: Responsible for formatting the `LogMessage` into a string (e.g., PatternLayout, JsonLayout).

### Component Interaction
```
[Client App] --> [Logger :: log()] --> [Async Queue]
                                          |
                                    [Background Worker Thread]
                                          |
                                          v
                                    [LogManager / Dispatcher]
                                          |
                            +-------------+-------------+
                            |             |             |
                            v             v             v
                    [ConsoleStrategy] [FileStrategy] [DbStrategy]
```

### Data Flow
1.  Client calls `Logger::info("message")`.
2.  Logger captures timestamp, thread ID, captures the message, checks if `INFO` level is enabled.
3.  If enabled, wraps data into a `LogMessage` object.
4.  Pushes `LogMessage` into a thread-safe `RingBuffer` or `BlockingQueue`.
5.  Control returns immediately to the client (Non-blocking).
6.  Background thread wakes up, pops `LogMessage`.
7.  Iterates through registered `LogStrategy` instances.
8.  Each Strategy uses a `Formatter` to convert the object to a string/structure and writes to its destination.

## 3. Detailed Design

### Component 1: Logger Core
#### Classes/Interfaces

```cpp
// Enum for Log Levels
enum class LogLevel {
    DEBUG, INFO, WARN, ERROR, FATAL
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

// Logger Class (Singleton)
class Logger {
private:
    static Logger* instance;
    static std::mutex mutex_;
    LogLevel currentLevel;
    std::vector<std::shared_ptr<ILogStrategy>> strategies;
    AsyncLogProcessor asyncProcessor; // Handles background writing

    Logger(); // Private constructor

public:
    static Logger* getInstance();
    
    void setLevel(LogLevel level);
    void addStrategy(std::shared_ptr<ILogStrategy> strategy);
    
    void log(LogLevel level, const std::string& message, const char* file, int line);
    
    // Convenience methods
    void debug(const std::string& msg, const char* file = __builtin_FILE(), int line = __builtin_LINE());
    void info(const std::string& msg);
    void error(const std::string& msg);
};
```

### Component 2: Async Processing (Performance)
#### Algorithms & Data Structures
-   **Data Structure**: A thread-safe bounded blocking queue (e.g., `std::queue` with `std::mutex` and `std::condition_variable`, or a lock-free ring buffer for ultra-low latency).
-   **Algorithm**: Producer-Consumer pattern.
    -   **Producer (Logger::log)**: Pushes message to queue. If queue is full, depending on policy, either drop message (non-blocking) or block (guarantee delivery). For this design, we'll use a blocking strategy with a timeout or drop-on-full for critical performance.
-   **Concurrency**:
    -   Uses `std::mutex` for queue protection.
    -   `std::condition_variable` to notify the worker thread when new logs arrive.

#### C++ Implementation Sketch (AsyncProcessor)

```cpp
class AsyncLogProcessor {
    std::queue<LogMessage> queue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread workerThread_;
    bool stop_ = false;
    std::vector<std::shared_ptr<ILogStrategy>>& strategies_;

    void process() {
        while (true) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this]{ return !queue_.empty() || stop_; });

            if (stop_ && queue_.empty()) return;

            LogMessage msg = queue_.front();
            queue_.pop();
            lock.unlock();

            // Dispatch to all strategies
            for (auto& strategy : strategies_) {
                strategy->log(msg);
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
            queue_.push(msg);
        }
        queueCv_.notify_one();
    }

    ~AsyncLogProcessor() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        queueCv_.notify_one();
        if (workerThread_.joinable()) workerThread_.join();
    }
};
```

### Component 3: Strategies & Log Rotation
#### FileLogStrategy
-   **Responsibility**: Writes formatted strings to a file. Handles rotation.
-   **Rotation Logic**: Check file size before writing. If `current_size + msg_size > max_size`, rename current file to `log.timestamp.bak` and create new `log`.

```cpp
class FileLogStrategy : public ILogStrategy {
    std::ofstream fileStream;
    std::string filePath;
    size_t maxSizeBytes;
    
    void rotate() {
        fileStream.close();
        std::string newName = filePath + "." + std::to_string(std::time(nullptr));
        std::rename(filePath.c_str(), newName.c_str());
        fileStream.open(filePath, std::ios::out | std::ios::app);
    }

public:
    FileLogStrategy(const std::string& path) : filePath(path) {
        fileStream.open(filePath, std::ios::out | std::ios::app);
    }

    void log(const LogMessage& msg) override {
        // Simple formatting
        std::stringstream ss;
        ss << "[" << msg.level << "] " << msg.message << "\n";
        std::string out = ss.str();

        // Check rotation
        if (fileStream.tellp() > maxSizeBytes) {
            rotate();
        }

        fileStream << out;
        fileStream.flush(); // Or rely on system buffering for speed
    }
};
```

## 4. Design Patterns Applied
1.  **Singleton Pattern**: Used for the `Logger` class to ensure a global point of access and a single configuration state across the application.
2.  **Strategy Pattern**: `ILogStrategy` allows swapping output destinations (Console, File, DB) without modifying the Logger's core logic.
3.  **Observer Pattern (Variation)**: The Logger holds a list of strategies. When a log event occurs, it notifies all "observers" (strategies).
4.  **Producer-Consumer Pattern**: Implemented via `AsyncLogProcessor` to decouple the high-speed application threads from the slower I/O operations of logging.

## 5. Performance Considerations
### Bottlenecks
-   **Disk I/O**: Writing to file is slow. **Solution**: Async logging moves I/O to a background thread.
-   **String Formatting**: Constructing log strings is expensive. **Solution**: Defer formatting to the background thread or use `fmt` library / `std::format` (C++20) for efficiency.
-   **Mutex Contention**: If many threads log simultaneously, the queue mutex becomes a hotspot. **Optimization**: Use a lock-free queue (like MPMC queue) or thread-local buffers that batch logs before pushing to the main queue.

### Scalability Plan
-   The design scales with the number of strategies. Adding 10 output destinations increases the work of the background thread but doesn't slow down the main application thread.
-   For extremely high throughput (e.g., HFT), we might drop logs (lossy logging) if the queue fills up to prevent blocking the application.

## 6. Data Management
### Log Format & Schema
-   **Console/File**: Text-based (structured). `[TIMESTAMP] [LEVEL] [THREAD_ID] [FILE:LINE] Message`
-   **Database**:
    ```sql
    CREATE TABLE app_logs (
        id BIGINT PRIMARY KEY AUTO_INCREMENT,
        timestamp DATETIME NOT NULL,
        level VARCHAR(10),
        message TEXT,
        module VARCHAR(100),
        thread_id VARCHAR(50)
    );
    ```

## 7. Error Handling & Resilience
-   **Strategy Failures**: If writing to a file fails (e.g., disk full), the `FileLogStrategy` should catch the exception. It should attempt to write to a fallback (e.g., `std::cerr`) or fail silently to avoid crashing the application.
-   **Rotation Failure**: If rotation fails (permissions), append to the existing file or log an error to internal diagnostic output.
-   **Destruction**: Ensure `Logger` flushes all pending messages in the queue before the application exits (`AsyncProcessor` destructor handles this).

## 8. Security Implementation
-   **Sensitive Data**: The Logger should ideally support a "Masking" feature (Decorator pattern on Strategy or Message) to scrub PII (Personally Identifiable Information) like passwords or credit card numbers before writing.
-   **File Permissions**: Log files should be created with restricted permissions (e.g., `600` on Unix) to prevent unauthorized reading.

## 9. Testing Strategy
### Test Cases
-   **Unit Tests**:
    -   Mock `ILogStrategy` to verify `Logger` passes messages correctly.
    -   Test `FileLogStrategy` rotation logic by simulating small file limits.
-   **Concurrency Tests**:
    -   Spawn 100 threads, each logging 1000 messages. Verify total count in output matches 100,000.
-   **Performance Tests**:
    -   Measure average latency of `Logger::info()` call (time to enqueue).
    -   Measure throughput (logs/sec) the background thread can handle.

## 10. Trade-offs & Decisions
-   **Async vs Sync**: chose Async to satisfy the "Performance impact minimization" requirement. Trade-off is complexity in handling application crash (logs in queue might be lost). Solution: Install signal handlers to flush logs on crash.
-   **Singleton**: Chose Singleton for ease of use (`Logger::getInstance()->info()`). Trade-off is global state, which can make unit testing dependent components harder. Solution: Use dependency injection where possible, passing `Logger` instance.

## 11. C++ Specific Implementation Details
-   **Smart Pointers**: Used `std::shared_ptr` for strategies to manage lifetime automatically.
-   **Move Semantics**: `LogMessage` should be moved (`std::move`) into the queue to avoid unnecessary string copies.
-   **Condition Variables**: Essential for the battery-efficient waiting mechanism of the background thread.
-   **`std::filesystem`**: Use C++17 `<filesystem>` for robust path handling and file renaming during rotation.
