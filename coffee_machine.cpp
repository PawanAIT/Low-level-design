#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <algorithm>
#include <sstream>

/**
 * ================================
 * COFFEE MACHINE SIMULATION
 * ================================
 * 
 * This program demonstrates several key Design Patterns and C++ features:
 * 1. Strategy Pattern: For Brewing methods and Payment processing.
 * 2. Observer Pattern: For notifying about maintenance events (like low stock).
 * 3. State Pattern: For managing the machine's operational state.
 * 4. Factory/Composition: Creating and organizing Coffee types.
 * 5. Thread Safety: Using mutexes and atomic variables for safe concurrent access.
 */

// ================================
// ENUMS AND DATA TYPES
// ================================

enum class CoffeeSize {
    SMALL,
    MEDIUM,
    LARGE
};

enum class IngredientType {
    COFFEE_BEANS,
    WATER,
    MILK,
    SUGAR,
    CHOCOLATE,
    FOAM
};

enum class PaymentMethod {
    CASH,
    CREDIT_CARD,
    MOBILE_WALLET
};

enum class MachineState {
    READY,
    BREWING,
    MAINTENANCE,
    PAYMENT_PROCESSING,
    OUT_OF_SERVICE
};

enum class MaintenanceType {
    LOW_INGREDIENTS,
    CLEANING_REQUIRED,
    SERVICE_NEEDED,
    CLEANING_COMPLETED
};

// ================================
// CUSTOM EXCEPTIONS
// ================================

// Base exception class for all Coffee Machine related errors
class CoffeeMachineException : public std::exception {
private:
    std::string message_;
public:
    explicit CoffeeMachineException(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

class InsufficientIngredientsException : public CoffeeMachineException {
public:
    explicit InsufficientIngredientsException(const std::string& msg) 
        : CoffeeMachineException("Insufficient ingredients: " + msg) {}
};

class PaymentFailedException : public CoffeeMachineException {
public:
    explicit PaymentFailedException(const std::string& msg) 
        : CoffeeMachineException("Payment failed: " + msg) {}
};

class InvalidStateException : public CoffeeMachineException {
public:
    explicit InvalidStateException(const std::string& msg) 
        : CoffeeMachineException("Invalid state: " + msg) {}
};

// ================================
// DATA STRUCTURES
// ================================

struct Recipe {
    std::map<IngredientType, double> ingredients;
    
    // Helper to adjust ingredient amounts based on cup size
    Recipe scaleForSize(CoffeeSize size) const {
        Recipe scaledRecipe;
        double scaleFactor = (size == CoffeeSize::SMALL) ? 0.8 : 
                           (size == CoffeeSize::MEDIUM) ? 1.0 : 1.2;
        
        for (const auto& [ingredient, amount] : ingredients) {
            scaledRecipe.ingredients[ingredient] = amount * scaleFactor;
        }
        return scaledRecipe;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Recipe {";
        for (const auto& [ingredient, amount] : ingredients) {
            oss << static_cast<int>(ingredient) << ":" << amount << " ";
        }
        oss << "}";
        return oss.str();
    }
};

struct Ingredient {
    IngredientType type;
    std::string name;
    double currentQuantity;
    double minimumThreshold;
    std::string unit;
    
    bool isLowStock() const { return currentQuantity <= minimumThreshold; }
    bool canDispense(double requiredAmount) const { return currentQuantity >= requiredAmount; }
};

struct PaymentInfo {
    PaymentMethod method;
    double amount;
    std::string cardNumber; // Last 4 digits only
    std::string walletId;
};

struct CoffeeResult {
    std::string coffeeName;
    CoffeeSize size;
    double temperature;
    double volume;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toString() const {
        std::ostringstream oss;
        oss << "Coffee: " << coffeeName << " (" << static_cast<int>(size) 
            << "), Temp: " << temperature << "°C, Volume: " << volume << "ml";
        return oss.str();
    }
};

// ================================
// OBSERVER PATTERN
// ================================
// Generic implementation of the Observer pattern to allow components to listen for events.

template<typename T>
class Observer {
public:
    virtual ~Observer() = default;
    virtual void update(const T& data) = 0;
};

template<typename T>
class Subject {
private:
    // weak_ptr prevents circular references and memory leaks
    std::vector<std::weak_ptr<Observer<T>>> observers_;
    mutable std::mutex mutex_;
    
public:
    void attach(std::weak_ptr<Observer<T>> observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }
    
    void notify(const T& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = observers_.begin();
        while (it != observers_.end()) {
            // Check if the observer still exists
            if (auto observer = it->lock()) {
                observer->update(data);
                ++it;
            } else {
                // Remove expired observers
                it = observers_.erase(it);
            }
        }
    }
};

// ================================
// STRATEGY PATTERN - BREWING STRATEGIES
// ================================
// Strategy interface for different brewing methods.
// This allows the Coffee Machine to support new brewing types without changing its core logic.

class BrewingStrategy {
public:
    virtual ~BrewingStrategy() = default;
    virtual void brew(const Recipe& recipe, CoffeeSize size) = 0;
    virtual bool validateRecipe(const Recipe& recipe) const = 0;
    virtual std::string getBrewingMethod() const = 0;
};

class EspressoStrategy : public BrewingStrategy {
public:
    void brew(const Recipe& recipe, CoffeeSize size) override {
        std::cout << "☕ Starting high-pressure espresso extraction..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        
        if (recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end()) {
            double coffeeAmount = recipe.ingredients.at(IngredientType::COFFEE_BEANS);
            std::cout << "⚡ High-pressure extraction (9 bars) with " << coffeeAmount << "g coffee" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "✅ Espresso extraction complete!" << std::endl;
    }
    
    bool validateRecipe(const Recipe& recipe) const override {
        return recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end();
    }
    
    std::string getBrewingMethod() const override { return "High-Pressure Espresso"; }
};

class FilterCoffeeStrategy : public BrewingStrategy {
public:
    void brew(const Recipe& recipe, CoffeeSize size) override {
        std::cout << "☕ Starting gravity-based filter brewing..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        
        if (recipe.ingredients.find(IngredientType::WATER) != recipe.ingredients.end()) {
            double waterAmount = recipe.ingredients.at(IngredientType::WATER);
            std::cout << "💧 Heating water to 85°C and dispensing " << waterAmount << "ml" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        std::cout << "✅ Filter coffee brewing complete!" << std::endl;
    }
    
    bool validateRecipe(const Recipe& recipe) const override {
        return recipe.ingredients.find(IngredientType::WATER) != recipe.ingredients.end() &&
               recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end();
    }
    
    std::string getBrewingMethod() const override { return "Gravity Filter Brewing"; }
};

// Composite Strategy: Uses EspressoStrategy internally
class CappuccinoStrategy : public BrewingStrategy {
private:
    std::unique_ptr<BrewingStrategy> espressoStrategy = std::make_unique<EspressoStrategy>();
    
public:
    void brew(const Recipe& recipe, CoffeeSize size) override {
        std::cout << "☕ Starting cappuccino preparation..." << std::endl;
        
        // 1. Brew espresso base
        Recipe espressoRecipe = extractEspressoComponent(recipe);
        espressoStrategy->brew(espressoRecipe, size);
        
        // 2. Steam milk
        if (recipe.ingredients.find(IngredientType::MILK) != recipe.ingredients.end()) {
            double milkAmount = recipe.ingredients.at(IngredientType::MILK);
            std::cout << "🥛 Steaming milk at 65°C for " << milkAmount << "ml" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        
        // 3. Create foam
        if (recipe.ingredients.find(IngredientType::FOAM) != recipe.ingredients.end()) {
            double foamAmount = recipe.ingredients.at(IngredientType::FOAM);
            std::cout << "🫧 Creating milk foam for " << foamAmount << "ml" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        std::cout << "✅ Cappuccino preparation complete!" << std::endl;
    }
    
    bool validateRecipe(const Recipe& recipe) const override {
        return recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end() &&
               recipe.ingredients.find(IngredientType::MILK) != recipe.ingredients.end();
    }
    
    std::string getBrewingMethod() const override { return "Espresso + Steamed Milk + Foam"; }
    
private:
    Recipe extractEspressoComponent(const Recipe& recipe) const {
        Recipe espressoRecipe;
        if (recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end()) {
            espressoRecipe.ingredients[IngredientType::COFFEE_BEANS] = recipe.ingredients.at(IngredientType::COFFEE_BEANS);
        }
        if (recipe.ingredients.find(IngredientType::WATER) != recipe.ingredients.end()) {
            espressoRecipe.ingredients[IngredientType::WATER] = recipe.ingredients.at(IngredientType::WATER) * 0.6; // Less water for espresso
        }
        return espressoRecipe;
    }
};

class LatteStrategy : public BrewingStrategy {
private:
    std::unique_ptr<BrewingStrategy> espressoStrategy = std::make_unique<EspressoStrategy>();
    
public:
    void brew(const Recipe& recipe, CoffeeSize size) override {
        std::cout << "☕ Starting latte preparation..." << std::endl;
        
        // 1. Brew espresso base
        Recipe espressoRecipe = extractEspressoComponent(recipe);
        espressoStrategy->brew(espressoRecipe, size);
        
        // 2. Steam milk (more than cappuccino)
        if (recipe.ingredients.find(IngredientType::MILK) != recipe.ingredients.end()) {
            double milkAmount = recipe.ingredients.at(IngredientType::MILK);
            std::cout << "🥛 Steaming milk at 60°C for " << milkAmount << "ml (smooth texture)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
        
        std::cout << "✅ Latte preparation complete!" << std::endl;
    }
    
    bool validateRecipe(const Recipe& recipe) const override {
        return recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end() &&
               recipe.ingredients.find(IngredientType::MILK) != recipe.ingredients.end();
    }
    
    std::string getBrewingMethod() const override { return "Espresso + Steamed Milk"; }
    
private:
    Recipe extractEspressoComponent(const Recipe& recipe) const {
        Recipe espressoRecipe;
        if (recipe.ingredients.find(IngredientType::COFFEE_BEANS) != recipe.ingredients.end()) {
            espressoRecipe.ingredients[IngredientType::COFFEE_BEANS] = recipe.ingredients.at(IngredientType::COFFEE_BEANS);
        }
        if (recipe.ingredients.find(IngredientType::WATER) != recipe.ingredients.end()) {
            espressoRecipe.ingredients[IngredientType::WATER] = recipe.ingredients.at(IngredientType::WATER) * 0.5;
        }
        return espressoRecipe;
    }
};

// ================================
// COFFEE TYPES
// ================================

class CoffeeType {
private:
    std::string id_;
    std::string name_;
    Recipe recipe_;
    double basePrice_;
    std::unique_ptr<BrewingStrategy> brewingStrategy_;
    std::string description_;
    
public:
    CoffeeType(const std::string& id, const std::string& name, Recipe recipe, 
               double basePrice, std::unique_ptr<BrewingStrategy> strategy,
               const std::string& description = "")
        : id_(id), name_(name), recipe_(std::move(recipe)), basePrice_(basePrice),
          brewingStrategy_(std::move(strategy)), description_(description) {}
    
    const std::string& getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    Recipe getScaledRecipe(CoffeeSize size) const {
        return recipe_.scaleForSize(size);
    }
    
    double calculatePrice(CoffeeSize size) const {
        double sizeMultiplier = (size == CoffeeSize::SMALL) ? 0.8 : 
                               (size == CoffeeSize::MEDIUM) ? 1.0 : 1.3;
        return basePrice_ * sizeMultiplier;
    }
    
    // Delegates brewing to the Strategy
    void brew(CoffeeSize size) const {
        Recipe scaledRecipe = getScaledRecipe(size);
        if (brewingStrategy_ && brewingStrategy_->validateRecipe(scaledRecipe)) {
            std::cout << "🎯 Brewing method: " << brewingStrategy_->getBrewingMethod() << std::endl;
            brewingStrategy_->brew(scaledRecipe, size);
        } else {
            throw CoffeeMachineException("Invalid brewing strategy or recipe validation failed");
        }
    }
    
    const Recipe& getRecipe() const { return recipe_; }
    double getBasePrice() const { return basePrice_; }
};

// ================================
// PAYMENT STRATEGIES
// ================================
// Another use of Strategy Pattern for handling different payment methods.

class PaymentStrategy {
public:
    virtual ~PaymentStrategy() = default;
    virtual bool processPayment(double amount, const PaymentInfo& info) = 0;
    virtual std::string getPaymentMethod() const = 0;
};

class CashPaymentStrategy : public PaymentStrategy {
public:
    bool processPayment(double amount, const PaymentInfo& info) override {
        std::cout << "💰 Processing cash payment of $" << amount << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return true; // Assume always successful for demo
    }
    
    std::string getPaymentMethod() const override { return "Cash"; }
};

class CreditCardPaymentStrategy : public PaymentStrategy {
public:
    bool processPayment(double amount, const PaymentInfo& info) override {
        std::cout << "💳 Processing credit card payment of $" << amount << std::endl;
        std::cout << "Card ending in: " << info.cardNumber << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true; // Assume always successful for demo
    }
    
    std::string getPaymentMethod() const override { return "Credit Card"; }
};

class MobileWalletPaymentStrategy : public PaymentStrategy {
public:
    bool processPayment(double amount, const PaymentInfo& info) override {
        std::cout << "📱 Processing mobile wallet payment of $" << amount << std::endl;
        std::cout << "Wallet ID: " << info.walletId << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return true; // Assume always successful for demo
    }
    
    std::string getPaymentMethod() const override { return "Mobile Wallet"; }
};

class PaymentProcessor {
private:
    std::map<PaymentMethod, std::unique_ptr<PaymentStrategy>> strategies_;
    
public:
    PaymentProcessor() {
        strategies_[PaymentMethod::CASH] = std::make_unique<CashPaymentStrategy>();
        strategies_[PaymentMethod::CREDIT_CARD] = std::make_unique<CreditCardPaymentStrategy>();
        strategies_[PaymentMethod::MOBILE_WALLET] = std::make_unique<MobileWalletPaymentStrategy>();
    }
    
    bool processPayment(PaymentMethod method, double amount, const PaymentInfo& info) {
        auto it = strategies_.find(method);
        if (it == strategies_.end()) {
            throw CoffeeMachineException("Unsupported payment method");
        }
        
        std::cout << "💳 Payment processing started..." << std::endl;
        bool result = it->second->processPayment(amount, info);
        
        if (result) {
            std::cout << "✅ Payment successful!" << std::endl;
        } else {
            std::cout << "❌ Payment failed!" << std::endl;
        }
        
        return result;
    }
};

// ================================
// INGREDIENT MANAGEMENT
// ================================

class IngredientManager {
private:
    std::map<IngredientType, Ingredient> ingredients_;
    mutable std::mutex mutex_;
    Subject<MaintenanceType> maintenanceSubject_; // Subject for Observer Pattern
    
public:
    void addIngredient(const Ingredient& ingredient) {
        std::lock_guard<std::mutex> lock(mutex_); // Thread safety
        ingredients_[ingredient.type] = ingredient;
    }
    
    bool hasIngredients(const Recipe& recipe) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [ingredientType, requiredAmount] : recipe.ingredients) {
            auto it = ingredients_.find(ingredientType);
            if (it == ingredients_.end() || !it->second.canDispense(requiredAmount)) {
                return false;
            }
        }
        return true;
    }
    
    void consumeIngredients(const Recipe& recipe) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [ingredientType, requiredAmount] : recipe.ingredients) {
            auto it = ingredients_.find(ingredientType);
            if (it != ingredients_.end()) {
                it->second.currentQuantity -= requiredAmount;
                
                // Check for low stock and notify observers if needed
                if (it->second.isLowStock()) {
                    std::cout << "⚠️  LOW STOCK: " << it->second.name << std::endl;
                    maintenanceSubject_.notify(MaintenanceType::LOW_INGREDIENTS);
                }
            }
        }
    }
    
    void restockIngredient(IngredientType type, double amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ingredients_.find(type);
        if (it != ingredients_.end()) {
            it->second.currentQuantity += amount;
            std::cout << "✅ Restocked " << it->second.name << " by " << amount << it->second.unit << std::endl;
        }
    }
    
    std::vector<Ingredient> getLowStockIngredients() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Ingredient> lowStock;
        
        for (const auto& [type, ingredient] : ingredients_) {
            if (ingredient.isLowStock()) {
                lowStock.push_back(ingredient);
            }
        }
        return lowStock;
    }
    
    void attachMaintenanceObserver(std::weak_ptr<Observer<MaintenanceType>> observer) {
        maintenanceSubject_.attach(observer);
    }
    
    void displayInventory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n📦 CURRENT INVENTORY:" << std::endl;
        for (const auto& [type, ingredient] : ingredients_) {
            std::cout << ingredient.name << ": " << ingredient.currentQuantity 
                      << ingredient.unit << " (Min: " << ingredient.minimumThreshold << ")" << std::endl;
        }
    }
};

// ================================
// STATE PATTERN
// ================================
// Defines the interface for different states of the coffee machine (Ready, Brewing, etc.)

class CoffeeMachineContext;

class CoffeeMachineState {
public:
    virtual ~CoffeeMachineState() = default;
    virtual void handle(CoffeeMachineContext& context) = 0;
    virtual MachineState getState() const = 0;
    virtual std::string getStateName() const = 0;
};

class ReadyState : public CoffeeMachineState {
public:
    void handle(CoffeeMachineContext& context) override;
    MachineState getState() const override { return MachineState::READY; }
    std::string getStateName() const override { return "READY"; }
};

class BrewingState : public CoffeeMachineState {
public:
    void handle(CoffeeMachineContext& context) override;
    MachineState getState() const override { return MachineState::BREWING; }
    std::string getStateName() const override { return "BREWING"; }
};

class PaymentState : public CoffeeMachineState {
public:
    void handle(CoffeeMachineContext& context) override;
    MachineState getState() const override { return MachineState::PAYMENT_PROCESSING; }
    std::string getStateName() const override { return "PAYMENT_PROCESSING"; }
};

class MaintenanceState : public CoffeeMachineState {
public:
    void handle(CoffeeMachineContext& context) override;
    MachineState getState() const override { return MachineState::MAINTENANCE; }
    std::string getStateName() const override { return "MAINTENANCE"; }
};

// ================================
// MAIN COFFEE MACHINE CONTEXT
// ================================
// The central class that manages the Coffee Machine's operation.
// It maintains the current state, ingredients, and coffee types.

class CoffeeMachineContext {
private:
    std::unique_ptr<CoffeeMachineState> currentState_;
    std::map<std::string, std::unique_ptr<CoffeeType>> coffeeTypes_;
    IngredientManager ingredientManager_;
    PaymentProcessor paymentProcessor_;
    std::atomic<MachineState> state_; // Atomic for thread-safe state checking
    std::mutex stateMutex_;
    
public:
    CoffeeMachineContext() : currentState_(std::make_unique<ReadyState>()), state_(MachineState::READY) {
        initializeCoffeeTypes();
        initializeIngredients();
    }
    
    void initializeCoffeeTypes() {
        // Espresso
        Recipe espressoRecipe;
        espressoRecipe.ingredients[IngredientType::COFFEE_BEANS] = 18.0;
        espressoRecipe.ingredients[IngredientType::WATER] = 30.0;
        coffeeTypes_["espresso"] = std::make_unique<CoffeeType>(
            "espresso", "Espresso", espressoRecipe, 3.50,
            std::make_unique<EspressoStrategy>(), "Rich, bold coffee shot"
        );
        
        // Cappuccino
        Recipe cappuccinoRecipe;
        cappuccinoRecipe.ingredients[IngredientType::COFFEE_BEANS] = 18.0;
        cappuccinoRecipe.ingredients[IngredientType::WATER] = 30.0;
        cappuccinoRecipe.ingredients[IngredientType::MILK] = 100.0;
        cappuccinoRecipe.ingredients[IngredientType::FOAM] = 50.0;
        coffeeTypes_["cappuccino"] = std::make_unique<CoffeeType>(
            "cappuccino", "Cappuccino", cappuccinoRecipe, 4.50,
            std::make_unique<CappuccinoStrategy>(), "Espresso with steamed milk and foam"
        );
        
        // Latte
        Recipe latteRecipe;
        latteRecipe.ingredients[IngredientType::COFFEE_BEANS] = 18.0;
        latteRecipe.ingredients[IngredientType::WATER] = 30.0;
        latteRecipe.ingredients[IngredientType::MILK] = 150.0;
        coffeeTypes_["latte"] = std::make_unique<CoffeeType>(
            "latte", "Latte", latteRecipe, 4.00,
            std::make_unique<LatteStrategy>(), "Espresso with steamed milk"
        );
        
        // Filter Coffee
        Recipe filterRecipe;
        filterRecipe.ingredients[IngredientType::COFFEE_BEANS] = 12.0;
        filterRecipe.ingredients[IngredientType::WATER] = 200.0;
        coffeeTypes_["filter"] = std::make_unique<CoffeeType>(
            "filter", "Filter Coffee", filterRecipe, 2.50,
            std::make_unique<FilterCoffeeStrategy>(), "Classic drip-brewed coffee"
        );
    }
    
    void initializeIngredients() {
        ingredientManager_.addIngredient({IngredientType::COFFEE_BEANS, "Coffee Beans", 500.0, 50.0, "g"});
        ingredientManager_.addIngredient({IngredientType::WATER, "Water", 2000.0, 200.0, "ml"});
        ingredientManager_.addIngredient({IngredientType::MILK, "Milk", 1000.0, 100.0, "ml"});
        ingredientManager_.addIngredient({IngredientType::SUGAR, "Sugar", 200.0, 20.0, "g"});
        ingredientManager_.addIngredient({IngredientType::FOAM, "Foam", 500.0, 50.0, "ml"});
    }
    
    // Main workflow for preparing coffee
    CoffeeResult prepareCoffee(const std::string& coffeeTypeId, CoffeeSize size, 
                              PaymentMethod paymentMethod, const PaymentInfo& paymentInfo) {
        validateState(MachineState::READY);
        
        auto coffeeTypeIt = coffeeTypes_.find(coffeeTypeId);
        if (coffeeTypeIt == coffeeTypes_.end()) {
            throw CoffeeMachineException("Coffee type not found: " + coffeeTypeId);
        }
        
        const CoffeeType& coffeeType = *coffeeTypeIt->second;
        Recipe requiredRecipe = coffeeType.getScaledRecipe(size);
        
        // Check ingredients
        if (!ingredientManager_.hasIngredients(requiredRecipe)) {
            throw InsufficientIngredientsException("Insufficient ingredients for " + coffeeType.getName());
        }
        
        // Calculate price
        double price = coffeeType.calculatePrice(size);
        
        // Process payment
        setState(MachineState::PAYMENT_PROCESSING);
        if (!paymentProcessor_.processPayment(paymentMethod, price, paymentInfo)) {
            throw PaymentFailedException("Payment processing failed");
        }
        setState(MachineState::READY);
        
        // Start brewing
        setState(MachineState::BREWING);
        std::cout << "\n🎯 Starting preparation: " << coffeeType.getName() << std::endl;
        
        try {
            coffeeType.brew(size);
            
            // Consume ingredients
            ingredientManager_.consumeIngredients(requiredRecipe);
            
            // Simulate coffee dispensing
            CoffeeResult result = {
                coffeeType.getName(),
                size,
                70.0 + (rand() % 20), // Random temperature between 70-90°C
                (size == CoffeeSize::SMALL) ? 150.0 : (size == CoffeeSize::MEDIUM) ? 200.0 : 250.0,
                std::chrono::system_clock::now()
            };
            
            std::cout << "☕ " << result.toString() << std::endl;
            std::cout << "✅ Coffee ready! Enjoy your " << coffeeType.getName() << "!" << std::endl;
            
            setState(MachineState::READY);
            return result;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Brewing failed: " << e.what() << std::endl;
            setState(MachineState::READY); // Ensure state is reset before re-throwing
            throw;
        }
    }
    
    void displayAvailableCoffeeTypes() const {
        std::cout << "\n☕ AVAILABLE COFFEE TYPES:" << std::endl;
        for (const auto& [id, coffeeType] : coffeeTypes_) {
            std::cout << "🔹 " << coffeeType->getName() << " (ID: " << id << ")" << std::endl;
            std::cout << "   💰 Small: $" << coffeeType->calculatePrice(CoffeeSize::SMALL)
                      << " | Medium: $" << coffeeType->calculatePrice(CoffeeSize::MEDIUM)
                      << " | Large: $" << coffeeType->calculatePrice(CoffeeSize::LARGE) << std::endl;
            std::cout << "   📝 " << coffeeType->getDescription() << std::endl;
            std::cout << std::endl;
        }
    }
    
    void displaySystemStatus() const {
        std::cout << "\n🤖 COFFEE MACHINE STATUS:" << std::endl;
        std::cout << "State: " << getCurrentStateName() << std::endl;
        ingredientManager_.displayInventory();
    }
    
    void triggerMaintenance(MaintenanceType type) {
        std::cout << "\n🔧 MAINTENANCE ALERT: " << static_cast<int>(type) << std::endl;
        setState(MachineState::MAINTENANCE);
        // Simulate maintenance action
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "✅ Maintenance completed" << std::endl;
        setState(MachineState::READY);
    }

    // Proxy method to access IngredientManager capabilities
    IngredientManager& getIngredientManager() {
        return ingredientManager_;
    }
    
    MachineState getCurrentState() const { return state_.load(); }
    std::string getCurrentStateName() const {
        switch (state_.load()) {
            case MachineState::READY: return "Ready";
            case MachineState::BREWING: return "Brewing";
            case MachineState::MAINTENANCE: return "Maintenance";
            case MachineState::PAYMENT_PROCESSING: return "Payment Processing";
            case MachineState::OUT_OF_SERVICE: return "Out of Service";
            default: return "Unknown";
        }
    }
    
    void setState(MachineState newState) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.store(newState);
    }
    
private:
    void validateState(MachineState expectedState) const {
        if (state_.load() != expectedState) {
            throw InvalidStateException("Expected state " + getCurrentStateName() + 
                                      " but machine is in " + getCurrentStateName());
        }
    }
};

// ================================
// STATE IMPLEMENTATIONS
// ================================

void ReadyState::handle(CoffeeMachineContext& context) {
    // Ready to accept new orders
    std::cout << "🤖 Coffee machine is READY for orders!" << std::endl;
}

void BrewingState::handle(CoffeeMachineContext& context) {
    std::cout << "☕ Coffee machine is BREWING your coffee..." << std::endl;
}

void PaymentState::handle(CoffeeMachineContext& context) {
    std::cout << "💳 Processing payment..." << std::endl;
}

void MaintenanceState::handle(CoffeeMachineContext& context) {
    std::cout << "🔧 Machine is under maintenance..." << std::endl;
}

// ================================
// MAINTENANCE OBSERVER
// ================================
// Concrete observer that reacts to maintenance notifications

class MaintenanceObserver : public Observer<MaintenanceType> {
public:
    void update(const MaintenanceType& data) override {
        std::cout << "🚨 MAINTENANCE NOTIFICATION: " << static_cast<int>(data) << std::endl;
        switch (data) {
            case MaintenanceType::LOW_INGREDIENTS:
                std::cout << "⚠️  Alert: Some ingredients are running low!" << std::endl;
                break;
            case MaintenanceType::CLEANING_REQUIRED:
                std::cout << "🧽 Alert: Machine cleaning is required!" << std::endl;
                break;
            case MaintenanceType::SERVICE_NEEDED:
                std::cout << "🔧 Alert: Professional service is needed!" << std::endl;
                break;
            case MaintenanceType::CLEANING_COMPLETED:
                std::cout << "✨ Maintenance completed successfully!" << std::endl;
                break;
        }
    }
};

// ================================
// MAIN DEMONSTRATION
// ================================

int main() {
    std::cout << "🤖 SMART COFFEE MACHINE SYSTEM" << std::endl;
    std::cout << "================================" << std::endl;
    
    try {
        // Initialize coffee machine
        auto coffeeMachine = std::make_unique<CoffeeMachineContext>();
        
        // Set up maintenance monitoring (Observer Pattern)
        auto maintenanceObserver = std::make_shared<MaintenanceObserver>();
        coffeeMachine->getIngredientManager().attachMaintenanceObserver(maintenanceObserver);
        
        // Display system information
        coffeeMachine->displayAvailableCoffeeTypes();
        coffeeMachine->displaySystemStatus();
        
        // Simulate customer orders
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "📝 SIMULATING CUSTOMER ORDERS" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        // Order 1: Customer orders Espresso with credit card
        try {
            std::cout << "\n👤 Customer 1: Ordering Espresso (Medium) - Credit Card" << std::endl;
            PaymentInfo payment1{PaymentMethod::CREDIT_CARD, 3.50, "1234", ""};
            auto result1 = coffeeMachine->prepareCoffee("espresso", CoffeeSize::MEDIUM, 
                                                      PaymentMethod::CREDIT_CARD, payment1);
            std::cout << "🎉 Order completed: " << result1.toString() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Order failed: " << e.what() << std::endl;
        }
        
        // Order 2: Customer orders Cappuccino with cash
        try {
            std::cout << "\n👤 Customer 2: Ordering Cappuccino (Large) - Cash" << std::endl;
            PaymentInfo payment2{PaymentMethod::CASH, 4.50, "", ""};
            auto result2 = coffeeMachine->prepareCoffee("cappuccino", CoffeeSize::LARGE, 
                                                      PaymentMethod::CASH, payment2);
            std::cout << "🎉 Order completed: " << result2.toString() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Order failed: " << e.what() << std::endl;
        }
        
        // Order 3: Customer orders Latte with mobile wallet
        try {
            std::cout << "\n👤 Customer 3: Ordering Latte (Small) - Mobile Wallet" << std::endl;
            PaymentInfo payment3{PaymentMethod::MOBILE_WALLET, 4.00, "", "user123"};
            auto result3 = coffeeMachine->prepareCoffee("latte", CoffeeSize::SMALL, 
                                                      PaymentMethod::MOBILE_WALLET, payment3);
            std::cout << "🎉 Order completed: " << result3.toString() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Order failed: " << e.what() << std::endl;
        }
        
        // Display updated inventory after orders
        std::cout << "\n📊 INVENTORY AFTER ORDERS:" << std::endl;
        coffeeMachine->displaySystemStatus();
        
        // Simulate low stock scenario to trigger Observer
        std::cout << "\n🔋 SIMULATING LOW STOCK SCENARIO:" << std::endl;
        coffeeMachine->getIngredientManager().restockIngredient(IngredientType::COFFEE_BEANS, -450.0); // Deplete beans
        
        // Show final system status
        std::cout << "\n📈 FINAL SYSTEM STATUS:" << std::endl;
        coffeeMachine->displaySystemStatus();
        
        std::cout << "\n✨ DEMONSTRATION COMPLETE!" << std::endl;
        std::cout << "\nKey Design Patterns Demonstrated:" << std::endl;
        std::cout << "🏗️  Strategy Pattern: Different brewing methods (Espresso, Filter, Cappuccino, Latte)" << std::endl;
        std::cout << "🏛️  State Pattern: Machine states (Ready, Brewing, Payment, Maintenance)" << std::endl;
        std::cout << "👁️  Observer Pattern: Maintenance notifications" << std::endl;
        std::cout << "💰 Strategy Pattern: Payment methods (Cash, Credit Card, Mobile Wallet)" << std::endl;
        std::cout << "📦 Composition: Coffee Machine composed of multiple specialized components" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ System error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
