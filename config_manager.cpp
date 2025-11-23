#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>

// --- Data Types ---
using ConfigValue = std::variant<int, double, bool, std::string>;

// Helper to print ConfigValue
struct ValuePrinter {
    void operator()(int v) const { std::cout << v; }
    void operator()(double v) const { std::cout << v; }
    void operator()(bool v) const { std::cout << (v ? "true" : "false"); }
    void operator()(const std::string& v) const { std::cout << v; }
};

void printValue(const ConfigValue& v) {
    std::visit(ValuePrinter{}, v);
}

// --- Interfaces ---

// Validator Interface
class IValidator {
public:
    virtual bool validate(const std::string& key, const ConfigValue& value) = 0;
    virtual ~IValidator() = default;
};

// Source Interface
class IConfigSource {
public:
    virtual ~IConfigSource() = default;
    virtual std::unordered_map<std::string, ConfigValue> load() = 0;
    virtual void watch(std::function<void()> onChangeCallback) = 0;
};

// Observer Interface
class IConfigObserver {
public:
    virtual void onConfigChanged(const std::string& key) = 0;
    virtual ~IConfigObserver() = default;
};

// --- Implementations ---

// Range Validator
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
        return true; // Pass if type doesn't match (simplification)
    }
};

// Mock Source (Simulates a file or DB)
class MockSource : public IConfigSource {
private:
    std::unordered_map<std::string, ConfigValue> data;
    std::function<void()> callback;
public:
    MockSource() {
        data["max_connections"] = 100;
        data["app_name"] = std::string("MyApp");
        data["debug_mode"] = true;
    }

    std::unordered_map<std::string, ConfigValue> load() override {
        return data;
    }

    void watch(std::function<void()> onChangeCallback) override {
        callback = onChangeCallback;
    }

    // Method to simulate external change
    void updateData(const std::string& key, ConfigValue value) {
        data[key] = value;
        if (callback) {
            std::cout << "[Source] Detected change. Triggering reload...\n";
            callback();
        }
    }
};

// Configuration Manager
class ConfigurationManager {
private:
    std::unordered_map<std::string, ConfigValue> configStore;
    mutable std::shared_mutex mutex;
    std::vector<std::shared_ptr<IConfigSource>> sources;
    std::unordered_map<std::string, std::shared_ptr<IValidator>> validators;
    std::vector<std::weak_ptr<IConfigObserver>> observers;

    ConfigurationManager() = default;

    void notifyObservers() {
        std::cout << "[Manager] Notifying observers of update.\n";
        for (auto it = observers.begin(); it != observers.end(); ) {
            if (auto obs = it->lock()) {
                obs->onConfigChanged("ALL"); // Simplified: notify all changed
                ++it;
            } else {
                it = observers.erase(it);
            }
        }
    }

public:
    static ConfigurationManager& getInstance() {
        static ConfigurationManager instance;
        return instance;
    }

    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;

    void addSource(std::shared_ptr<IConfigSource> source) {
        sources.push_back(source);
        source->watch([this]() {
            this->reload();
        });
        reload();
    }

    void addValidator(const std::string& key, std::shared_ptr<IValidator> validator) {
        validators[key] = validator;
    }

    void registerObserver(std::shared_ptr<IConfigObserver> observer) {
        observers.push_back(observer);
    }

    void reload() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::unordered_map<std::string, ConfigValue> newStore;

        for (const auto& source : sources) {
            auto data = source->load();
            for (const auto& [key, value] : data) {
                newStore[key] = value;
            }
        }

        // Validation
        for (const auto& [key, value] : newStore) {
            if (validators.find(key) != validators.end()) {
                if (!validators[key]->validate(key, value)) {
                    std::cout << "[Manager] Validation Failed for key: " << key << ". Keeping old values.\n";
                    return; // Abort reload on validation failure
                }
            }
        }

        configStore = std::move(newStore);
        std::cout << "[Manager] Configuration reloaded successfully.\n";
        
        // Notify outside lock to avoid potential deadlocks if observer calls back
        lock.unlock(); 
        notifyObservers();
    }

    template <typename T>
    T getValue(const std::string& key, T defaultValue) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (configStore.find(key) != configStore.end()) {
            if (std::holds_alternative<T>(configStore.at(key))) {
                return std::get<T>(configStore.at(key));
            }
        }
        return defaultValue;
    }
};

// Example Observer
class LoggerService : public IConfigObserver {
public:
    void onConfigChanged(const std::string& key) override {
        bool debug = ConfigurationManager::getInstance().getValue<bool>("debug_mode", false);
        std::cout << "[LoggerService] Config changed. Debug mode is now: " << (debug ? "ON" : "OFF") << "\n";
    }
};

int main() {
    auto& config = ConfigurationManager::getInstance();

    // 1. Setup Validators
    config.addValidator("max_connections", std::make_shared<RangeValidator>(1, 1000));

    // 2. Add Source
    auto mockSource = std::make_shared<MockSource>();
    config.addSource(mockSource);

    // 3. Register Observer
    auto logger = std::make_shared<LoggerService>();
    config.registerObserver(logger);

    // 4. Initial Read
    std::cout << "\n--- Initial State ---\n";
    std::cout << "App Name: " << config.getValue<std::string>("app_name", "DefaultApp") << "\n";
    std::cout << "Max Conn: " << config.getValue<int>("max_connections", 0) << "\n";

    // 5. Simulate Hot Reload (Valid Update)
    std::cout << "\n--- Updating Config (Valid) ---\n";
    mockSource->updateData("max_connections", 500);
    mockSource->updateData("debug_mode", false);
    
    std::cout << "Max Conn (Updated): " << config.getValue<int>("max_connections", 0) << "\n";

    // 6. Simulate Hot Reload (Invalid Update - Validation Fail)
    std::cout << "\n--- Updating Config (Invalid) ---\n";
    mockSource->updateData("max_connections", 2000); // Max is 1000
    
    // Value should remain 500
    std::cout << "Max Conn (After Invalid): " << config.getValue<int>("max_connections", 0) << "\n";

    return 0;
}
