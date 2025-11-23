# LLD Solution for S2.1 Configuration Manager System

## 1. Problem Analysis
### Requirements
- **Functional**:
    - **Multi-Source Loading**: Support loading configurations from files (JSON, YAML), environment variables, command-line arguments, and remote databases/services (e.g., Consul, Etcd).
    - **Hot Reloading**: Detect changes in configuration sources and update the application state in real-time without restarting.
    - **Default Values**: Provide fallback values if a configuration key is missing.
    - **Validation**: Ensure configuration values meet specific criteria (types, ranges, formats) before being applied.
    - **Type Safety**: Retrieve values in their appropriate data types (int, string, bool, etc.).

- **Non-Functional**:
    - **Thread Safety**: Multiple threads reads, single thread write (updates).
    - **Performance**: Fast read access (configurations are read frequently).
    - **Consistency**: Ensure partial updates don't leave the system in an inconsistent state.

- **Constraints**:
    - Use C++ for implementation examples.
    - Minimize external dependencies for the core logic.

## 2. High-Level Design
### System Components
- **ConfigurationManager**: The central entry point (Singleton). Coordinates loading, merging, and accessing configurations.
- **ConfigSource**: Abstract interface for different configuration backends (File, Env, DB).
- **ConfigParser**: Parses raw data (JSON, YAML, etc.) into an internal representation.
- **Validator**: Validates configuration values against rules.
- **Watcher**: Monitors sources for changes and triggers reloads.

### Data Flow
1.  **Startup**: `ConfigurationManager` registers multiple `ConfigSource`s.
2.  **Load**: Iterates through sources in priority order (e.g., Env > File > Default).
3.  **Merge**: Flattens data from all sources into a central Key-Value store.
4.  **Validate**: Runs validation rules. If failed, rollback or log error.
5.  **Access**: Application queries values via `getValue<T>(key)`.
6.  **Update**: `Watcher` detects file change -> Triggers Load -> Merge -> Validate -> Notify Observers.

## 3. Detailed Design

### Class Diagram & Structure

#### A. Core Components

```cpp
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <shared_mutex>
#include <functional>
#include <any>

// Using std::any or std::variant for storing values of different types
using ConfigValue = std::variant<int, double, bool, std::string>;

// Observer Interface for Hot Reloading
class IConfigObserver {
public:
    virtual void onConfigChanged(const std::string& key) = 0;
    virtual ~IConfigObserver() = default;
};

// Abstract Base for Configuration Sources
class IConfigSource {
public:
    virtual ~IConfigSource() = default;
    
    // Returns a map of Key-Value pairs loaded from the source
    virtual std::unordered_map<std::string, ConfigValue> load() = 0;
    
    // Optional: Setup a watcher for this source
    virtual void watch(std::function<void()> onChangeCallback) = 0;
};
```

#### B. Concrete Sources

```cpp
class EnvVarSource : public IConfigSource {
public:
    std::unordered_map<std::string, ConfigValue> load() override {
        // Implementation to read environment variables
        // e.g., getenv("APP_PORT")
        return {}; 
    }
    
    void watch(std::function<void()> onChangeCallback) override {
        // Environment variables usually don't change at runtime, no-op
    }
};

class FileSource : public IConfigSource {
private:
    std::string filePath;
public:
    FileSource(const std::string& path) : filePath(path) {}
    
    std::unordered_map<std::string, ConfigValue> load() override {
        // Read file, parse JSON/YAML, return map
        return {};
    }
    
    void watch(std::function<void()> onChangeCallback) override {
        // Use file system events (e.g., inotify on Linux) to detect changes
        // When change detected, call onChangeCallback()
    }
};
```

#### C. Validation Strategy

```cpp
class IValidator {
public:
    virtual bool validate(const std::string& key, const ConfigValue& value) = 0;
    virtual ~IValidator() = default;
};

class RangeValidator : public IValidator {
private:
    int min, max;
public:
    RangeValidator(int minVal, int maxVal) : min(minVal), max(maxVal) {}
    bool validate(const std::string& key, const ConfigValue& value) override {
        if (std::holds_alternative<int>(value)) {
            int v = std::get<int>(value);
            return v >= min && v <= max;
        }
        return false; 
    }
};
```

#### D. Configuration Manager (Singleton)

```cpp
class ConfigurationManager {
private:
    // Thread-safe storage
    std::unordered_map<std::string, ConfigValue> configStore;
    mutable std::shared_mutex mutex;

    std::vector<std::shared_ptr<IConfigSource>> sources;
    std::vector<std::weak_ptr<IConfigObserver>> observers;
    std::unordered_map<std::string, std::shared_ptr<IValidator>> validators;

    // Private Constructor
    ConfigurationManager() = default;

    void reload() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::unordered_map<std::string, ConfigValue> newStore;

        // Load from all sources in priority order
        // Later sources overwrite earlier ones (Priority: Env > File > Defaults)
        for (const auto& source : sources) {
            auto data = source->load();
            for (const auto& [key, value] : data) {
                newStore[key] = value;
            }
        }

        // Validate
        for (const auto& [key, value] : newStore) {
            if (validators.find(key) != validators.end()) {
                if (!validators[key]->validate(key, value)) {
                    // Handle Validation Error (Log, fallback, or reject update)
                    return; 
                }
            }
        }

        // Commit changes
        configStore = std::move(newStore);
        notifyObservers();
    }

    void notifyObservers() {
        // Notify subscribers about changes
    }

public:
    static ConfigurationManager& getInstance() {
        static ConfigurationManager instance;
        return instance;
    }

    // Delete copy/move
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;

    void addSource(std::shared_ptr<IConfigSource> source) {
        sources.push_back(source);
        // Setup watcher
        source->watch([this]() {
            this->reload();
        });
        reload(); // Initial load
    }

    void addValidator(const std::string& key, std::shared_ptr<IValidator> validator) {
        validators[key] = validator;
    }

    template <typename T>
    T getValue(const std::string& key, T defaultValue) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (configStore.find(key) != configStore.end()) {
            try {
                // Simplified extraction logic
                if (std::holds_alternative<T>(configStore[key])) {
                    return std::get<T>(configStore[key]);
                }
            } catch (...) {}
        }
        return defaultValue;
    }
};
```

## 4. Design Patterns Applied

### 1. Singleton Pattern
- **Usage**: `ConfigurationManager`.
- **Reason**: Ensures global access to configurations and creates a centralized point for managing updates and synchronization.

### 2. Strategy Pattern
- **Usage**: `IConfigSource` and `IValidator`.
- **Reason**: Allows algorithms (loading from file vs DB, validating range vs regex) to be selected and swapped at runtime.

### 3. Observer Pattern
- **Usage**: `IConfigObserver` and Hot Reloading mechanism.
- **Reason**: Allows parts of the application to react dynamically when configuration changes (e.g., logging level changes from INFO to DEBUG).

### 4. Template Method / Builder (Implicit)
- **Usage**: Often used for configuring the Manager itself (e.g., `.addSource().addValidator().build()`).

## 5. Performance & Scalability

### Bottlenecks
- **Lock Contention**: Frequent reads vs infrequent writes.
    - **Solution**: Used `std::shared_mutex` (Readers-Writer Lock). Multiple threads can read concurrently; write locks are only taken during reload.
- **Parsing Overhead**: Re-parsing large JSON/YAML files on every change.
    - **Solution**: Keep file sizes manageable or split into multiple source files.

### Thread Safety
- Access to the underlying map is protected by `shared_mutex`.
- `reload()` is atomic in terms of swapping the map pointer or contents, ensuring readers don't see a partially updated state.

## 6. Trade-offs & Decisions
- **`std::variant` vs `std::string`**:
    - **Decision**: Use `std::variant` or a custom `Value` class.
    - **Reason**: Storing everything as strings requires parsing on every get. Variant stores typed data, improving read performance and type safety.
- **Push vs Pull for Updates**:
    - **Decision**: Hybrid. The Manager updates internal state (Push from Source), and App components pull values when needed, or subscribe if they need instant notification.

## 7. Testing Strategy
- **Unit Tests**:
    - Mock `IConfigSource` to return specific data sets.
    - Test priority merging (Env overriding File).
    - Test validation failures.
- **Integration Tests**:
    - Test with actual files on disk. Modify file and verify `getValue` returns new value.
- **Concurrency Tests**:
    - Multiple threads reading while one thread triggers a reload.
