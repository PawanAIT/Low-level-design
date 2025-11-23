# LLD Solution for S1.4 Restaurant Management System

## 1. Problem Analysis
### Requirements
- **Functional**:
    - **Menu Management**: Add, update, remove items, manage categories/sections.
    - **Table Management**: Reserve tables, assign tables to customers, update table status (Free, Occupied, Reserved).
    - **Order Processing**: Create orders, modify orders, cancel orders.
    - **Kitchen Workflow**: Route orders to the kitchen, update cooking status, notify waiters.
    - **Payment Processing**: Generate bills, handle different payment methods (Cash, Card, Digital), split bills.
    - **Staff Management**: Manage employee roles (Waiter, Chef, Manager), shift management.
- **Non-Functional**:
    - **Scalability**: Support multiple branches.
    - **Reliability**: No lost orders; accurate billing.
    - **Concurrency**: Prevent double booking of tables.
    - ** Responsiveness**: Real-time updates for kitchen and waiters.
- **Constraints**:
    - "Show classes and their relationship don't implement internals".
- **Assumptions**:
    - A restaurant can have multiple branches.
    - Customers can walk in or reserve.
    - Kitchen has display units to see orders.

## 2. High-Level Design
### System Components
- **Restaurant Service**: Central management for branches and global settings.
- **Menu Service**: Manages catalog of food and beverages.
- **Order Service**: Handles order lifecycle from creation to completion.
- **Table Service**: Manages physical layout and availability.
- **Kitchen Service**: Queue management for food preparation.
- **Payment Service**: Billing and transaction handling.
- **Notification Service**: Real-time updates between Kitchen, Waiters, and Customers.

### Component Interaction
- **Waiter** takes order -> **Order Service**
- **Order Service** -> Validates with **Menu Service**
- **Order Service** -> Updates **Kitchen Service**
- **Kitchen** prepares food -> Updates **Order Service** -> Notifies **Waiter** via **Notification Service**
- Customer requests bill -> **Order Service** -> **Payment Service** generates Bill
- **Payment Service** -> Updates **Table Service** (releases table)

## 3. Detailed Design

### Class Diagram & Structure

#### A. Core Entities & Enums

```cpp
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <queue>

enum class AccountStatus {
    ACTIVE, CLOSED, CANCELED, BLACKLISTED, NONE
};

enum class OrderStatus {
    RECEIVED, PREPARING, READY, COMPLETED, CANCELED, NONE
};

enum class TableStatus {
    FREE, RESERVED, OCCUPIED, OTHER
};

enum class PaymentStatus {
    UNPAID, PENDING, COMPLETED, FAILED, DECLINED, SETTLED, REFUNDED
};

struct Address {
    std::string streetAddress;
    std::string city;
    std::string state;
    std::string zipCode;
    std::string country;
};

class Person {
protected:
    std::string name;
    std::string email;
    std::string phone;
public:
    virtual ~Person() = default;
};

class Customer : public Person {
    // Customer specific details
};
```

#### B. Actors

```cpp
class Employee : public Person {
protected:
    std::string employeeId;
    std::chrono::system_clock::time_point dateJoined;
    AccountStatus status;

public:
    virtual bool registerEmployee() = 0;
};

class Table; // Forward declaration

class Receptionist : public Employee {
public:
    bool registerEmployee() override;
    bool createReservation(std::chrono::system_clock::time_point time, int partySize, std::shared_ptr<Customer> customer);
    std::vector<std::shared_ptr<Table>> searchTables(std::chrono::system_clock::time_point time, int partySize);
};

class Menu; // Forward declaration

class Manager : public Employee {
public:
    bool registerEmployee() override;
    bool addEmployee(std::shared_ptr<Employee> employee);
    bool modifyMenu(std::shared_ptr<Menu> menu);
    void generateReport();
};

class Order; // Forward declaration

class Chef : public Employee {
public:
    bool registerEmployee() override;
    void viewKitchenOrders();
    bool updateCookingStatus(std::shared_ptr<Order> order, OrderStatus status);
};

class OrderItem; // Forward declaration

class Waiter : public Employee {
public:
    bool registerEmployee() override;
    bool createOrder(std::shared_ptr<Order> order);
    bool addOrderItem(std::string orderId, std::shared_ptr<OrderItem> item);
    bool serveOrder(std::string orderId);
};
```

#### C. Restaurant & Branch Structure

```cpp
class Branch; // Forward declaration

class Restaurant {
private:
    std::string name;
    std::vector<std::shared_ptr<Branch>> branches;

public:
    bool addBranch(std::shared_ptr<Branch> branch);
};

class Kitchen; // Forward declaration

class Branch {
private:
    std::string name;
    Address location;
    std::shared_ptr<Kitchen> kitchen;
    std::shared_ptr<Menu> menu;
    
    // Relationships
    std::vector<std::shared_ptr<Table>> tables;
    std::vector<std::shared_ptr<Employee>> employees;

public:
    Address getLocation();
    std::shared_ptr<Menu> getMenu();
};
```

#### D. Menu Management

```cpp
class MenuItem; // Forward declaration

class MenuSection {
private:
    std::string title;
    std::string description;
    std::vector<std::shared_ptr<MenuItem>> items;
public:
    void addItem(std::shared_ptr<MenuItem> item);
};

class Menu {
private:
    std::string menuId;
    std::string title;
    std::string description;
    std::vector<std::shared_ptr<MenuSection>> sections;

public:
    void addSection(std::shared_ptr<MenuSection> section);
    void print();
};

class MenuItem {
private:
    std::string id;
    std::string title;
    std::string description;
    double price;
    bool isAvailable;

public:
    void updatePrice(double price);
};
```

#### E. Table Management

```cpp
class Table {
private:
    int tableID;
    int capacity;
    TableStatus status;
    int locationIdentifier; // e.g., floor number

public:
    bool isFree();
    bool reserve();
    bool occupy();
    bool release();
};

class Reservation {
private:
    std::string reservationId;
    std::chrono::system_clock::time_point timeOfReservation;
    int peopleCount;
    std::shared_ptr<Customer> customer;
    std::shared_ptr<Table> table;
    std::string notes;
};
```

#### F. Order Processing

```cpp
class Order {
private:
    std::string orderID;
    std::chrono::system_clock::time_point creationTime;
    OrderStatus status;
    std::shared_ptr<Table> table;
    std::shared_ptr<Waiter> waiter;
    std::vector<std::shared_ptr<OrderItem>> items;

public:
    void addMeal(std::shared_ptr<MenuItem> meal);
    void removeMeal(std::shared_ptr<MenuItem> meal);
    OrderStatus getStatus();
    void setStatus(OrderStatus status);
};

class OrderItem {
private:
    std::string orderItemId;
    std::shared_ptr<MenuItem> menuItem;
    int quantity;
    std::string specifications; // e.g., "no onions"

public:
    void updateQuantity(int quantity);
};
```

#### G. Kitchen Workflow

```cpp
// Observer Pattern: Kitchen observes Orders
class Kitchen {
private:
    std::string name;
    std::vector<std::shared_ptr<Chef>> chefs;
    std::queue<std::shared_ptr<Order>> activeOrders;

public:
    void receiveNewOrder(std::shared_ptr<Order> order);
    void assignChef(std::shared_ptr<Order> order, std::shared_ptr<Chef> chef);
    void completeOrder(std::shared_ptr<Order> order);
};
```

#### H. Payment Processing

```cpp
class Bill {
private:
    std::string billId;
    std::shared_ptr<Order> order;
    double totalAmount;
    double tax;
    double tip;
    bool isPaid;

public:
    double calculateTotal();
};

// Strategy Pattern for Payments
class Payment {
protected:
    std::string transactionId;
    double amount;
    std::chrono::system_clock::time_point timestamp;
    PaymentStatus status;

public:
    virtual boolean processPayment() = 0;
    virtual ~Payment() = default;
};

class CashPayment : public Payment {
private:
    double cashTendered;
public:
    bool processPayment() override; // Logic for change calculation
};

class CardPayment : public Payment {
private:
    std::string cardNumber; // Tokenized
    std::string cardHolderName;
public:
    bool processPayment() override; // External Gateway call
};
```

## 4. Design Patterns Applied

### 1. Singleton Pattern
- **Usage**: `Restaurant` class (if assuming single brand management).
- **Reason**: Ensures a single point of access for global configurations and branch management.

### 2. Strategy Pattern
- **Usage**: `Payment` mechanism.
- **Reason**: Allows interchangeable algorithms for processing payments (Credit Card, Cash, Apple Pay) without modifying the core payment logic.

### 3. Observer Pattern
- **Usage**: Order Status notifications.
- **Reason**: When an `Order` status changes (e.g., to READY), the `Waiter` and `Bill` components need to be notified automatically. `Kitchen` acts as the subject updates the observers.

### 4. Command Pattern
- **Usage**: Order operations.
- **Reason**: Encapsulate requests as objects (e.g., `AddOrderItem`, `CancelOrder`), allowing for queuing orders for the kitchen and supporting undo operations if an item was added by mistake.

### 5. State Pattern
- **Usage**: `Table` and `Order` status.
- **Reason**: Encapsulates the varying behavior associated with the state of an object. For example, a `Table` cannot be reserved if it is already `OCCUPIED`.

## 5. Performance & Scalability

### Database & Concurrency
- **Optimistic Locking**: Use versioning on `Table` rows to prevent double booking.
    - `UPDATE tables SET reserved_by = 'user1', version = version + 1 WHERE id = 1 AND version = 5`
- **Indexing**:
    - Index on `Table.status` and `Table.capacity` for fast search.
    - Index on `Order.status` and `Order.createdAt` for kitchen display systems.

### Scalability Strategy
- **Horizontal Scaling**: The system handles multiple branches. The database can be sharded by `BranchID`.
- **Caching**: Menu items are read-heavy and rarely change. Cache `Menu` data in Redis to reduce database load.

## 6. Data Management

### Database Schema (Relational)

**Tables:**
1.  **Restaurant**: `id, name, owner_id`
2.  **Branch**: `id, restaurant_id, name, address_id`
3.  **Employee**: `id, branch_id, role, name, contact_info`
4.  **Menu**: `id, branch_id, title`
5.  **MenuItem**: `id, menu_id, name, price, description`
6.  **Table**: `id, branch_id, capacity, status`
7.  **Order**: `id, table_id, waiter_id, status, created_at`
8.  **Order_Item**: `id, order_id, menu_item_id, quantity, notes`
9.  **Bill**: `id, order_id, amount, tax, tip, is_paid`
10. **Payment**: `id, bill_id, amount, method, status`

## 7. Security Considerations
- **Role-Based Access Control (RBAC)**:
    - **Waiters** can create orders but cannot modify Menu prices.
    - **Managers** can void transactions and modify Menus.
    - **Chefs** can only view and update Order status.
- **PCI-DSS Compliance**: Do not store raw credit card numbers. Use tokenization via payment gateways.
- **Audit Logs**: Track sensitive actions like voiding a bill or applying a discount.

## 8. Trade-offs & Decisions
- **Relational vs NoSQL**:
    - **Decision**: Use Relational Database (PostgreSQL/MySQL).
    - **Reason**: Structured data with strict relationships (Order -> Items -> Menu) and ACID compliance requirements for Payments and Inventory/Reservations are critical.
- **Real-time Updates**:
    - **Decision**: Use WebSockets for Kitchen Display System.
    - **Reason**: Polling is inefficient for a high-paced kitchen environment where seconds matter.
