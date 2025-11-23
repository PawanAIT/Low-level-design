#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <optional>
#include <cassert>

/**
 * @brief A fixed-size Lock-Free Multiple Producer Multiple Consumer (MPMC) Queue.
 * 
 * Implementation based on Dmitry Vyukov's bounded MPMC queue.
 * It uses a ring buffer with sequence numbers to coordinate producers and consumers
 * without locks.
 */
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
    
    // Padding to prevent false sharing between head and tail
    alignas(CACHELINE_SIZE) std::atomic<size_t> enqueue_pos_;
    alignas(CACHELINE_SIZE) std::atomic<size_t> dequeue_pos_;

public:
    MPMCQueue(size_t buffer_size) 
        : buffer_(buffer_size), buffer_mask_(buffer_size - 1) {
        // Buffer size must be a power of 2
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
                // The cell is free for writing (sequence == pos)
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    // Success: we claimed this spot
                    cell->data = data;
                    // Increment sequence to allow reading (pos + 1)
                    cell->sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (dif < 0) {
                // Queue is full
                return false;
            } else {
                // Sequence > pos: another producer moved enqueue_pos_ forward but hasn't updated the sequence yet?
                // Or we loaded a stale 'pos'. Reload 'pos'.
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
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
                // The cell is ready for reading (sequence == pos + 1)
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    // Success: we claimed this spot
                    data = cell->data;
                    // Set sequence to pos + mask + 1 to indicate it's free for the NEXT lap
                    cell->sequence.store(pos + buffer_mask_ + 1, std::memory_order_release);
                    return true;
                }
            } else if (dif < 0) {
                // Queue is empty
                return false;
            } else {
                // Catch up
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};

// --- Test Harness ---

const int NUM_PRODUCERS = 4;
const int NUM_CONSUMERS = 4;
const int ITEMS_PER_PRODUCER = 100000;
const size_t QUEUE_SIZE = 1024; // Power of 2

void producer(MPMCQueue<int>& q, int id) {
    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        int val = id * ITEMS_PER_PRODUCER + i;
        while (!q.enqueue(val)) {
            std::this_thread::yield(); // Backoff strategy
        }
    }
}

void consumer(MPMCQueue<int>& q, std::atomic<int>& total_consumed) {
    int val;
    int count = 0;
    while (total_consumed.load(std::memory_order_relaxed) < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
        if (q.dequeue(val)) {
            total_consumed.fetch_add(1, std::memory_order_relaxed);
            count++;
        } else {
            std::this_thread::yield();
        }
    }
}

int main() {
    MPMCQueue<int> queue(QUEUE_SIZE);
    std::atomic<int> total_consumed{0};

    std::cout << "Starting Lock-Free MPMC Queue Test..." << std::endl;
    std::cout << "Producers: " << NUM_PRODUCERS << ", Consumers: " << NUM_CONSUMERS << std::endl;
    std::cout << "Items per producer: " << ITEMS_PER_PRODUCER << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer, std::ref(queue), i);
    }

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer, std::ref(queue), std::ref(total_consumed));
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Test Finished." << std::endl;
    std::cout << "Total Consumed: " << total_consumed << " / " << (NUM_PRODUCERS * ITEMS_PER_PRODUCER) << std::endl;
    std::cout << "Time: " << diff.count() << " seconds" << std::endl;
    std::cout << "Throughput: " << (NUM_PRODUCERS * ITEMS_PER_PRODUCER) / diff.count() / 1000000.0 << " M ops/sec" << std::endl;

    if (total_consumed == NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
        std::cout << "SUCCESS!" << std::endl;
    } else {
        std::cout << "FAILURE!" << std::endl;
    }

    return 0;
}
