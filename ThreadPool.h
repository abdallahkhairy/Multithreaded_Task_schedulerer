#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <set>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <iostream>
#include <chrono>
#include <random>
#include <deque>

// Forward declaration
class WorkStealingQueue;

// Task priority levels
enum class TaskPriority {
    Low,
    Normal,
    High,
    Critical
};

// Task structure with ID, timeout, and execution time tracking
struct Task {
    std::function<void()> function;
    TaskPriority priority;
    uint64_t id; // Unique task ID for cancellation
    std::chrono::steady_clock::time_point submission_time; // For timeout
    std::chrono::milliseconds timeout; // Timeout duration
    bool canceled; // Flag to mark task as canceled
    std::chrono::steady_clock::time_point start_time; // For execution time tracking
    std::chrono::steady_clock::time_point end_time; // For execution time tracking

    Task(std::function<void()> func, TaskPriority prio = TaskPriority::Normal, uint64_t task_id = 0,
        std::chrono::milliseconds timeout_duration = std::chrono::milliseconds(0))
        : function(std::move(func)), priority(prio), id(task_id),
        submission_time(std::chrono::steady_clock::now()), timeout(timeout_duration),
        canceled(false) {
    }

    // Comparison operator for std::set
    bool operator<(const Task& other) const {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return id > other.id; // Ensure unique ordering for tasks with same priority
    }

    Task() = default;
};

// Statistics structure
struct ThreadStatistics {
    size_t tasks_executed;
    std::chrono::microseconds total_execution_time;
    ThreadStatistics() : tasks_executed(0), total_execution_time(0) {}
};

class ThreadPool {
public:
    // Constructor: Default number of threads is the hardware concurrency
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Submit a task with a specific priority and optional timeout
    template<class F, class... Args>
    auto submit(TaskPriority priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Submit a task with default priority and no timeout
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Cancel a task by ID
    bool cancel_task(uint64_t task_id);

    // Update task priority
    bool update_task_priority(uint64_t task_id, TaskPriority new_priority);

    // Get statistics
    size_t get_task_count() const;
    size_t get_thread_count() const;
    const std::vector<ThreadStatistics>& get_statistics() const;

private:
    // Thread workers
    std::vector<std::thread> workers;

    // Task queues
    std::set<Task> global_tasks; // Replaced priority_queue with set for dynamic updates
    std::vector<std::unique_ptr<WorkStealingQueue>> local_queues;

    // Synchronization
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;

    // Statistics
    std::vector<ThreadStatistics> statistics;

    // Task ID counter
    std::atomic<uint64_t> task_id_counter;

    // Random number generator for work stealing
    std::mt19937 steal_rng;

    // Worker thread function
    void worker_thread(size_t id);

    // Attempt to steal work from other threads
    bool steal_task(Task& task);
};

// Work-stealing queue implementation
class WorkStealingQueue {
public:
    WorkStealingQueue() = default;

    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    void push(Task task);
    bool pop(Task& task);
    bool steal(Task& task);
    bool empty() const;

    // New methods to support cancellation and priority updates
    bool cancel_task(uint64_t task_id);
    bool update_task_priority(uint64_t task_id, TaskPriority new_priority);

private:
    std::deque<Task> queue;
    mutable std::mutex mutex;
};

// Implementation of the template methods

template<class F, class... Args>
auto ThreadPool::submit(TaskPriority priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {

    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();
    uint64_t task_id = task_id_counter.fetch_add(1, std::memory_order_relaxed);

    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if (stop) {
            throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
        }

        global_tasks.emplace([task]() { (*task)(); }, priority, task_id, timeout);
    }

    condition.notify_one();
    return result;
}

template<class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {
    return submit(TaskPriority::Normal, std::chrono::milliseconds(0), std::forward<F>(f), std::forward<Args>(args)...);
}

#endif // THREAD_POOL_H