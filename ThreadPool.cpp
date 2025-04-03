#include "ThreadPool.h"

// Work Stealing Queue implementation
void WorkStealingQueue::push(Task task) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_front(std::move(task));
}

bool WorkStealingQueue::pop(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
        return false;
    }
    
    task = std::move(queue.front());
    queue.pop_front();
    return true;
}

bool WorkStealingQueue::steal(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
        return false;
    }
    
    task = std::move(queue.back());
    queue.pop_back();
    return true;
}

bool WorkStealingQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads)
    : stop(false) {
    
    // Create a local task queue for each thread
    local_queues.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        local_queues[i] = std::make_unique<WorkStealingQueue>();
    }
    
    // Create worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&ThreadPool::worker_thread, this, i);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_thread(size_t id) {
    while (true) {
        Task task;
        bool has_task = false;
        
        // Try to get task from local queue first
        has_task = local_queues[id]->pop(task);
        
        // If no task in local queue, try the global queue
        if (!has_task) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // Wait for task or stop signal
            condition.wait(lock, [this]() {
                return stop || !global_tasks.empty();
            });
            
            // Check if we're shutting down
            if (stop && global_tasks.empty()) {
                return;
            }
            
            // Get task from global queue
            if (!global_tasks.empty()) {
                task = std::move(const_cast<Task&>(global_tasks.top()));
                global_tasks.pop();
                has_task = true;
            }
        }
        
        // If still no task, try to steal from other threads
        if (!has_task) {
            has_task = steal_task(task);
            
            // If still no task and the pool is stopping, exit
            if (!has_task && stop) {
                return;
            } else if (!has_task) {
                // No task found, but not stopping - go back to waiting
                continue;
            }
        }
        
        // Execute the task
        task.function();
    }
}

bool ThreadPool::steal_task(Task& task) {
    const size_t num_queues = local_queues.size();
    
    // Try to steal from another thread's queue
    for (size_t i = 0; i < num_queues; ++i) {
        if (local_queues[i]->steal(task)) {
            return true;
        }
    }
    
    return false;
}

size_t ThreadPool::get_task_count() const {
    std::unique_lock<std::mutex> lock(queue_mutex);
    size_t count = global_tasks.size();
    
    // Also count tasks in local queues
    for (const auto& queue : local_queues) {
        if (!queue->empty()) {
            count++;  // This is an approximation as we can't get actual counts without locking
        }
    }
    
    return count;
}

size_t ThreadPool::get_thread_count() const {
    return workers.size();
}
