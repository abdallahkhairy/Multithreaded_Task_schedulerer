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

bool WorkStealingQueue::cancel_task(uint64_t task_id) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (it->id == task_id) {
            Task task = *it;
            task.canceled = true;
            queue.erase(it);
            queue.push_back(task);
            return true;
        }
    }
    return false;
}

bool WorkStealingQueue::update_task_priority(uint64_t task_id, TaskPriority new_priority) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (it->id == task_id) {
            Task task = *it;
            queue.erase(it);
            task.priority = new_priority;
            queue.push_back(task);
            return true;
        }
    }
    return false;
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads)
    : stop(false), task_id_counter(0), steal_rng(std::random_device{}()) {

    // Ensure num_threads is at least 1
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) { // In case hardware_concurrency returns 0
            num_threads = 1;
        }
    }

    // Create a local task queue and statistics for each thread
    local_queues.resize(num_threads);
    statistics.resize(num_threads);
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

            // Get task from global queue, checking for timeouts and cancellation
            auto now = std::chrono::steady_clock::now();
            for (auto it = global_tasks.begin(); it != global_tasks.end();) {
                if (it->canceled) {
                    it = global_tasks.erase(it);
                    continue;
                }

                if (it->timeout.count() > 0) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->submission_time);
                    if (elapsed > it->timeout) {
                        it = global_tasks.erase(it);
                        continue;
                    }
                }

                task = *it;
                global_tasks.erase(it);
                has_task = true;
                break;
            }
        }

        // If still no task, try to steal from other threads
        if (!has_task) {
            has_task = steal_task(task);

            // If still no task and the pool is stopping, exit
            if (!has_task && stop) {
                return;
            }
            else if (!has_task) {
                // No task found, but not stopping - go back to waiting
                continue;
            }
        }

        // Execute the task and track statistics
        if (!task.canceled) {
            task.start_time = std::chrono::steady_clock::now();
            task.function();
            task.end_time = std::chrono::steady_clock::now();

            // Update statistics
            auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(task.end_time - task.start_time);
            statistics[id].tasks_executed++;
            statistics[id].total_execution_time += execution_time;
        }
    }
}

bool ThreadPool::steal_task(Task& task) {
    const size_t num_queues = local_queues.size();
    if (num_queues <= 1) return false;

    // Randomize the order of queues to steal from to reduce contention
    std::vector<size_t> indices(num_queues);
    for (size_t i = 0; i < num_queues; ++i) {
        indices[i] = i;
    }
    std::shuffle(indices.begin(), indices.end(), steal_rng);

    // Try to steal from another thread's queue
    for (size_t i : indices) {
        if (local_queues[i]->steal(task)) {
            return true;
        }
    }

    return false;
}

bool ThreadPool::cancel_task(uint64_t task_id) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    for (auto it = global_tasks.begin(); it != global_tasks.end(); ++it) {
        if (it->id == task_id) {
            Task task = *it;
            task.canceled = true;
            global_tasks.erase(it);
            global_tasks.insert(task);
            return true;
        }
    }

    // Check local queues
    for (const auto& queue : local_queues) {
        if (queue->cancel_task(task_id)) {
            return true;
        }
    }

    return false;
}

bool ThreadPool::update_task_priority(uint64_t task_id, TaskPriority new_priority) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    for (auto it = global_tasks.begin(); it != global_tasks.end(); ++it) {
        if (it->id == task_id) {
            Task task = *it;
            global_tasks.erase(it);
            task.priority = new_priority;
            global_tasks.insert(task);
            return true;
        }
    }

    // Check local queues
    for (const auto& queue : local_queues) {
        if (queue->update_task_priority(task_id, new_priority)) {
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
            count++;
        }
    }

    return count;
}

size_t ThreadPool::get_thread_count() const {
    return workers.size();
}

const std::vector<ThreadStatistics>& ThreadPool::get_statistics() const {
    return statistics;
}