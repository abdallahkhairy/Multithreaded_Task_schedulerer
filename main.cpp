#include "ThreadPool.h"
#include <iostream>
#include <chrono>
#include <random>

// Example function to compute Fibonacci numbers (inefficient on purpose to demonstrate scheduling)
int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Function that sleeps for a random duration
void work_with_random_duration(int id, int max_ms) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(10, max_ms);
    
    int sleep_time = dist(gen);
    std::cout << "Task " << id << " starting, will sleep for " << sleep_time << "ms\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    
    std::cout << "Task " << id << " completed after " << sleep_time << "ms\n";
}

int main() {
    std::cout << "Creating thread pool with " << std::thread::hardware_concurrency() << " threads\n";
    ThreadPool pool;
    
    // Example 1: Basic task submission
    std::cout << "\n--- Example 1: Basic task submission ---\n";
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            std::cout << "Task " << i << " executed by thread " << std::this_thread::get_id() << "\n";
        });
    }
    
    // Sleep to allow tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Example 2: Task priorities
    std::cout << "\n--- Example 2: Task priorities ---\n";
    
    // Submit low priority tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit(TaskPriority::Low, [i]() {
            std::cout << "Low priority task " << i << " executed\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    
    // Submit a high priority task
    pool.submit(TaskPriority::High, []() {
        std::cout << "High priority task executed\n";
    });
    
    // Submit more normal priority tasks
    for (int i = 0; i < 3; ++i) {
        pool.submit(TaskPriority::Normal, [i]() {
            std::cout << "Normal priority task " << i << " executed\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    
    // Submit a critical priority task
    pool.submit(TaskPriority::Critical, []() {
        std::cout << "Critical priority task executed\n";
    });
    
    // Sleep to allow tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Example 3: Tasks with return values
    std::cout << "\n--- Example 3: Tasks with return values ---\n";
    
    // Submit several Fibonacci calculations
    std::vector<std::future<int>> results;
    for (int i = 20; i < 30; i += 2) {
        results.push_back(pool.submit([i]() {
            std::cout << "Computing fibonacci(" << i << ")\n";
            return fibonacci(i);
        }));
    }
    
    // Retrieve and print the results
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Fibonacci result " << i << ": " << results[i].get() << "\n";
    }
    
    // Example 4: Work stealing demonstration
    std::cout << "\n--- Example 4: Work stealing demonstration ---\n";
    
    // Submit tasks with varying durations
    for (int i = 0; i < 20; ++i) {
        pool.submit([i]() {
            work_with_random_duration(i, 500);
        });
    }
    
    // Sleep to allow tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Example 5: Stress test
    std::cout << "\n--- Example 5: Stress test ---\n";
    
    const int num_tasks = 1000;
    std::atomic<int> counter{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Submit a large number of quick tasks
    for (int i = 0; i < num_tasks; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    // Wait for all tasks to complete
    while (counter.load() < num_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed " << num_tasks << " tasks in " << duration.count() << "ms\n";
    std::cout << "Average time per task: " << static_cast<double>(duration.count()) / num_tasks << "ms\n";
    
    std::cout << "\nAll examples completed successfully!\n";
    
    return 0;
}
