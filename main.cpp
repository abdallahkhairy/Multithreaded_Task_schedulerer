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
    std::cout << "Task " << id << " starting on thread " << std::this_thread::get_id() << ", will sleep for " << sleep_time << "ms\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

    std::cout << "Task " << id << " completed on thread " << std::this_thread::get_id() << " after " << sleep_time << "ms\n";
}

int main() {
    // Create thread pool with default number of threads (hardware concurrency)
    ThreadPool pool;
    std::cout << "Creating thread pool with " << pool.get_thread_count() << " threads (hardware concurrency)\n";

    // Example 1: Basic task submission with timeout
    std::cout << "\n--- Example 1: Basic task submission with timeout ---\n";
    for (int i = 0; i < 10; ++i) {
        pool.submit(TaskPriority::Normal, std::chrono::milliseconds(500), [i]() {
            std::cout << "Task " << i << " executed by thread " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
    }

    // Sleep to allow some tasks to complete or timeout
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Example 2: Task priorities and priority updates
    std::cout << "\n--- Example 2: Task priorities and priority updates ---\n";

    // Submit low priority tasks
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        auto future = pool.submit(TaskPriority::Low, std::chrono::milliseconds(0), [i]() {
            std::cout << "Low priority task " << i << " executed by thread " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
        futures.push_back(std::move(future));
    }

    // Submit a high priority task
    pool.submit(TaskPriority::High, std::chrono::milliseconds(0), []() {
        std::cout << "High priority task executed by thread " << std::this_thread::get_id() << "\n";
        });

    // Update priority of task 2 to Critical
    pool.update_task_priority(2, TaskPriority::Critical);
    std::cout << "Updated task 2 priority to Critical\n";

    // Submit a critical priority task
    pool.submit(TaskPriority::Critical, std::chrono::milliseconds(0), []() {
        std::cout << "Critical priority task executed by thread " << std::this_thread::get_id() << "\n";
        });

    // Sleep to allow tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Example 3: Task cancellation
    std::cout << "\n--- Example 3: Task cancellation ---\n";

    // Submit a task and immediately try to cancel it
    // Since we can't easily get the task ID, we'll use a hard-coded ID that's likely to be current
    uint64_t task_to_cancel = 15; // Estimate based on previous task submissions

    auto future_to_cancel = pool.submit(TaskPriority::Normal, std::chrono::milliseconds(0), []() {
        std::cout << "This task should not execute (if canceled successfully)\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        });

    // Cancel task
    bool canceled = pool.cancel_task(task_to_cancel);
    std::cout << "Cancellation of task ID " << task_to_cancel << " " << (canceled ? "successful" : "failed") << "\n";

    // Submit another task to ensure the pool is still working
    pool.submit([]() {
        std::cout << "This task should execute\n";
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Example 4: Tasks with return values
    std::cout << "\n--- Example 4: Tasks with return values ---\n";

    // Submit several Fibonacci calculations
    std::vector<std::future<int>> results;
    for (int i = 20; i < 30; i += 2) {
        results.push_back(pool.submit([i]() {
            std::cout << "Computing fibonacci(" << i << ") on thread " << std::this_thread::get_id() << "\n";
            return fibonacci(i);
            }));
    }

    // Retrieve and print the results
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Fibonacci result " << i << ": " << results[i].get() << "\n";
    }

    // Example 5: Work stealing demonstration
    std::cout << "\n--- Example 5: Work stealing demonstration ---\n";

    // Submit tasks with varying durations
    for (int i = 0; i < 20; ++i) {
        pool.submit([i]() {
            work_with_random_duration(i, 500);
            });
    }

    // Sleep to allow tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Example 6: Stress test and statistics
    std::cout << "\n--- Example 6: Stress test and statistics ---\n";

    const int num_tasks = 1000;
    std::atomic<int> counter{ 0 };

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

    // Print statistics
    const auto& stats = pool.get_statistics();
    for (size_t i = 0; i < stats.size(); ++i) {
        std::cout << "Thread " << i << ": " << stats[i].tasks_executed << " tasks executed, "
            << "total execution time: " << stats[i].total_execution_time << "us, "
            << "average execution time: "
            << (stats[i].tasks_executed > 0 ? stats[i].total_execution_time / stats[i].tasks_executed : 0)
            << "us per task\n";
    }

    std::cout << "\nAll examples completed successfully!\n";

    return 0;
}