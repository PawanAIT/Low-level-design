#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <cassert>
#include <iomanip>

// ==========================================
// 1. Lock-Free MPMC Queue (From previous step)
// ==========================================
template<typename T>
class MPMCQueue {
private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };

    static const size_t CACHELINE_SIZE = 64;
    std::vector<Cell> buffer_;
    size_t buffer_mask_;
    alignas(CACHELINE_SIZE) std::atomic<size_t> enqueue_pos_;
    alignas(CACHELINE_SIZE) std::atomic<size_t> dequeue_pos_;

public:
    MPMCQueue(size_t buffer_size) 
        : buffer_(buffer_size), buffer_mask_(buffer_size - 1) {
        assert((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i < buffer_size; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    bool enqueue(const T& data) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while (true) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell->data = data;
                    cell->sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (dif < 0) return false;
            else pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
    }

    bool dequeue(T& data) {
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        while (true) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    data = cell->data;
                    cell->sequence.store(pos + buffer_mask_ + 1, std::memory_order_release);
                    return true;
                }
            } else if (dif < 0) return false;
            else pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
    }
};

// ==========================================
// 2. Standard Blocking Queue (Mutex + CV)
// ==========================================
template<typename T>
class BlockingQueue {
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t capacity_;

public:
    BlockingQueue(size_t capacity) : capacity_(capacity) {}

    bool enqueue(const T& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this]{ return queue_.size() < capacity_; });
        queue_.push(data);
        not_empty_.notify_one();
        return true; 
    }

    bool dequeue(T& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait with timeout to avoid deadlock at the end of benchmark
        if (!not_empty_.wait_for(lock, std::chrono::milliseconds(10), [this]{ return !queue_.empty(); })) {
            return false;
        }
        data = queue_.front();
        queue_.pop();
        not_full_.notify_one();
        return true;
    }
};

// ==========================================
// 3. Benchmark Harness
// ==========================================

const int NUM_PRODUCERS = 16;
const int NUM_CONSUMERS = 16;
const int ITEMS_PER_PRODUCER = 250000; // Total 4 Million items
const size_t QUEUE_SIZE = 65536; // Larger queue for high throughput

template<typename Q>
void producer(Q& q, int id) {
    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        int val = id * ITEMS_PER_PRODUCER + i;
        // Lock-Free MPMC returns false if full, so we loop.
        // BlockingQueue returns true always (blocks internally).
        // This handles both (if enqueue returns bool).
        while (!q.enqueue(val)) {
            std::this_thread::yield();
        }
    }
}

template<typename Q>
void consumer(Q& q, std::atomic<int>& total_consumed) {
    int val;
    while (total_consumed.load(std::memory_order_relaxed) < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
        // MPMC returns false if empty. BlockingQueue blocks.
        // We need a way to stop BlockingQueue consumers if using wait.
        // But for MPMC we spin.
        // To support BlockingQueue properly in this generic harness is tricky 
        // without a "Shutdown" signal.
        // However, standard Blocking Queue benchmark usually compares "Wait/Notify" vs "Spinning".
        
        // For this specific benchmark, we will use a version of consumer 
        // that handles the specific queue type behavior or just rely on the fact 
        // that we know how many items to consume.
        
        if (q.dequeue(val)) {
             total_consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
             std::this_thread::yield();
        }
    }
}

template<typename QueueType>
void run_benchmark(const std::string& name) {
    QueueType queue(QUEUE_SIZE);
    std::atomic<int> total_consumed{0};

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "Benchmarking: " << name << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_PRODUCERS; ++i) 
        producers.emplace_back(producer<QueueType>, std::ref(queue), i);

    for (int i = 0; i < NUM_CONSUMERS; ++i) 
        consumers.emplace_back(consumer<QueueType>, std::ref(queue), std::ref(total_consumed));

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    double throughput = (NUM_PRODUCERS * ITEMS_PER_PRODUCER) / diff.count() / 1000000.0;

    std::cout << "Time:       " << std::fixed << std::setprecision(4) << diff.count() << " s" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(4) << throughput << " M ops/sec" << std::endl;
}

int main() {
    std::cout << "Benchmark Configuration:" << std::endl;
    std::cout << "Producers: " << NUM_PRODUCERS << ", Consumers: " << NUM_CONSUMERS << std::endl;
    std::cout << "Total Items: " << (NUM_PRODUCERS * ITEMS_PER_PRODUCER) << std::endl;

    run_benchmark<MPMCQueue<int>>("Lock-Free MPMC Queue");
    run_benchmark<BlockingQueue<int>>("Standard Mutex Queue");
    
    return 0;
}
