#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <iostream>

// Forward declaration
class WorkStealingQueue;

// Task priority levels
enum class TaskPriority {
    Low,
    Normal,
    High,
    Critical
};

// Task structure that holds the function and its priority
struct Task {
    std::function<void()> function;
    TaskPriority priority;
    
    Task(std::function<void()> func, TaskPriority prio = TaskPriority::Normal)
        : function(std::move(func)), priority(prio) {}
    
    // Comparison operator for priority queue
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

// Custom comparator for priority queue
struct TaskComparator {
    bool operator()(const Task& t1, const Task& t2) const {
        return t1.priority < t2.priority;
    }
};

class ThreadPool {
public:
    // Constructor with number of threads (default: number of hardware threads)
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    
    // Destructor
    ~ThreadPool();
    
    // Submit a task with a specific priority
    template<class F, class... Args>
    auto submit(TaskPriority priority, F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Submit a task with default priority
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Get current statistics
    size_t get_task_count() const;
    size_t get_thread_count() const;
    
private:
    // Thread workers
    std::vector<std::thread> workers;
    
    // Task queues
    std::priority_queue<Task, std::vector<Task>, TaskComparator> global_tasks;
    std::vector<std::unique_ptr<WorkStealingQueue>> local_queues;
    
    // Synchronization
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    
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
    
private:
    std::deque<Task> queue;
    mutable std::mutex mutex;
};

// Implementation of the template methods

template<class F, class... Args>
auto ThreadPool::submit(TaskPriority priority, F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        // Don't allow enqueueing after stopping the pool
        if(stop) {
            throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
        }
        
        global_tasks.emplace([task](){ (*task)(); }, priority);
    }
    
    condition.notify_one();
    return result;
}

template<class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    // Use the version with priority, defaulting to Normal
    return submit(TaskPriority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
}

#endif // THREAD_POOL_H
