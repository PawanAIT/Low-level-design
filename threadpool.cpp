#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>

using namespace std;

// Custom ThreadPool implementation (C++ has no standard thread pool yet)
class ThreadPool {
public:
    // Constructor: creates worker threads
    ThreadPool(size_t numThreads) : stop(false) {
        for(size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while(true) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(queueMutex);
                        // Wait until there's a task or stop signal
                        condition.wait(lock, [this] { 
                            return stop || !tasks.empty(); 
                        });
                        
                        if(stop && tasks.empty()) {
                            return; // Exit thread
                        }
                        
                        task = move(tasks.front());
                        tasks.pop();
                    }
                    task(); // Execute task outside the lock
                }
            });
        }
    }
    
    // Submit a task to the pool and get a future for the result
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> future<typename result_of<F(Args...)>::type> 
    {
        using return_type = typename result_of<F(Args...)>::type;
        
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );
        
        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(queueMutex);
            
            if(stop) {
                throw runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    
    // Destructor: waits for all tasks to complete
    ~ThreadPool() {
        {
            unique_lock<mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for(thread &worker: workers) {
            worker.join();
        }
    }
    
private:
    vector<thread> workers;              // Worker threads
    queue<function<void()>> tasks;       // Task queue
    
    mutex queueMutex;                    // Protects task queue
    condition_variable condition;        // For thread synchronization
    bool stop;                           // Stop flag
};

// Example usage
int main() {
    // Create a thread pool with 4 worker threads
    ThreadPool pool(4);
    
    // Example 1: Simple tasks without return values
    cout << "=== Example 1: Simple Tasks ===" << endl;
    vector<future<void>> results1;
    for(int i = 0; i < 8; ++i) {
        results1.emplace_back(
            pool.enqueue([i] {
                cout << "Task " << i << " executing on thread " 
                     << this_thread::get_id() << endl;
                this_thread::sleep_for(chrono::milliseconds(100));
            })
        );
    }
    // Wait for all tasks
    for(auto &result : results1) {
        result.get();
    }
    
    cout << "\n=== Example 2: Tasks with Return Values ===" << endl;
    // Example 2: Tasks that return values
    vector<future<int>> results2;
    for(int i = 0; i < 5; ++i) {
        results2.emplace_back(
            pool.enqueue([i] {
                int result = i * i;
                cout << "Computing " << i << "^2 = " << result << endl;
                return result;
            })
        );
    }
    // Get results
    for(auto &result : results2) {
        cout << "Result: " << result.get() << endl;
    }
    
    cout << "\n=== Example 3: Tasks with Parameters ===" << endl;
    // Example 3: Functions with parameters
    auto multiply = [](int a, int b) {
        cout << a << " * " << b << " = " << (a*b) << endl;
        return a * b;
    };
    
    vector<future<int>> results3;
    for(int i = 1; i <= 5; ++i) {
        results3.emplace_back(pool.enqueue(multiply, i, 10));
    }
    for(auto &result : results3) {
        result.get();
    }
    
    return 0;
}

/*
STANDARD C++ THREAD POOL STATUS:
================================

❌ C++11/14/17: No standard thread pool
❌ C++20: Still no standard thread pool (added std::jthread but not a pool)
❌ C++23: No standard thread pool yet
⏳ C++26: Proposed but not yet standardized

ALTERNATIVES:
=============

1. Custom Implementation (like above) ✅
   - Full control
   - Lightweight
   - No dependencies

2. Boost.Asio thread pool:
   #include <boost/asio/thread_pool.hpp>
   boost::asio::thread_pool pool(4);
   boost::asio::post(pool, task);

3. Intel TBB (Threading Building Blocks):
   #include <tbb/task_group.h>
   tbb::task_group tg;
   tg.run(task);

4. std::async (C++11) - Not a pool but simpler:
   auto future = std::async(std::launch::async, function);

KEY CONCEPTS:
============

1. Worker Threads: Fixed number of threads waiting for work
2. Task Queue: Thread-safe queue of tasks to execute
3. Mutex: Protects the task queue from race conditions
4. Condition Variable: Efficiently wakes threads when work arrives
5. Future/Promise: Allows returning values from async tasks

BENEFITS:
=========
✓ Avoid thread creation overhead
✓ Control resource usage (limit thread count)
✓ Better than creating threads on-demand
✓ Efficient task distribution
*/
