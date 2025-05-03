#include "ThreadPool.h"

// ThreadStatistics implementation of copy constructor and assignment operator
// Already defined inline in the header file, but we'll implement them here for consistency

// Task struct implementation of copy constructor and assignment operator
// These are needed because std::atomic members aren't copyable by default
Task::Task(const Task& other)
    : function(other.function),
    priority(other.priority),
    id(other.id),
    submission_time(other.submission_time),
    timeout(other.timeout),
    canceled(other.canceled.load()),
    start_time(other.start_time),
    end_time(other.end_time) {
}

Task& Task::operator=(const Task& other) {
    if (this != &other) {
        function = other.function;
        priority = other.priority;
        id = other.id;
        submission_time = other.submission_time;
        timeout = other.timeout;
        canceled.store(other.canceled.load());
        start_time = other.start_time;
        end_time = other.end_time;
    }
    return *this;
}

// WorkStealingQueue implementations
void WorkStealingQueue::push(Task task) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_back(std::move(task));
}

bool WorkStealingQueue::pop(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
        return false;
    }

    task = std::move(queue.back());
    queue.pop_back();
    return true;
}

bool WorkStealingQueue::steal(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
        return false;
    }

    task = std::move(queue.front());
    queue.pop_front();
    return true;
}

bool WorkStealingQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
}

size_t WorkStealingQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
}

bool WorkStealingQueue::cancel_task(uint64_t task_id) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& task : queue) {
        if (task.id == task_id) {
            task.canceled = true;
            return true;
        }
    }
    return false;
}

bool WorkStealingQueue::update_task_priority(uint64_t task_id, TaskPriority new_priority) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& task : queue) {
        if (task.id == task_id) {
            task.priority = new_priority;
            return true;
        }
    }
    return false;
}

// ThreadPool implementations
ThreadPool::ThreadPool(size_t num_threads)
    : stop(false), task_id_counter(0), steal_rng(std::random_device{}()) {

    // Initialize statistics vector
    statistics.resize(num_threads);

    // Initialize local queues
    local_queues.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        local_queues[i] = std::make_unique<WorkStealingQueue>();
    }

    // Start worker threads
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
        bool got_task = false;

        // Try to get a task from the thread's local queue first
        if (local_queues[id]->pop(task)) {
            got_task = true;
        }
        // If no task in local queue, try to get one from the global queue
        else {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait for a task or stop signal
            if (global_tasks.empty() && !stop) {
                condition.wait(lock, [this] { return !global_tasks.empty() || stop; });
            }

            // Check stop condition after waiting
            if (stop && global_tasks.empty()) {
                return;
            }

            // Get a task from the global queue if available
            if (!global_tasks.empty()) {
                auto it = global_tasks.begin();
                task = std::move(const_cast<Task&>(*it));
                global_tasks.erase(it);
                got_task = true;
            }
        }

        // If still no task, try to steal from other threads
        if (!got_task && !stop) {
            got_task = steal_task(id, task);
        }

        // If we got a task, execute it
        if (got_task) {
            // Check if task is canceled or timed out
            if (!task.canceled) {
                // Check timeout
                auto now = std::chrono::steady_clock::now();
                if (task.timeout.count() > 0 &&
                    now - task.submission_time > task.timeout) {
                    // Task timed out, don't execute
                    continue;
                }

                // Record start time
                task.start_time = std::chrono::steady_clock::now();

                // Execute the task
                task.function();

                // Record end time
                task.end_time = std::chrono::steady_clock::now();

                // Update statistics
                statistics[id].tasks_executed++;
                statistics[id].total_execution_time += std::chrono::duration_cast<std::chrono::microseconds>(
                    task.end_time - task.start_time).count();
            }
        }

        // Check stop condition
        if (stop && global_tasks.empty()) {
            return;
        }
    }
}

bool ThreadPool::steal_task(size_t thread_id, Task& task) {
    // Generate a random starting index to avoid always stealing from the same thread
    std::uniform_int_distribution<size_t> dist(0, workers.size() - 1);
    size_t start_idx = dist(steal_rng);

    // Try to steal from each thread in sequence
    for (size_t i = 0; i < workers.size(); ++i) {
        size_t idx = (start_idx + i) % workers.size();

        // Don't steal from self
        if (idx == thread_id) {
            continue;
        }

        if (local_queues[idx]->steal(task)) {
            return true;
        }
    }

    return false;
}

bool ThreadPool::cancel_task(uint64_t task_id) {
    // First, check global queue
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        for (auto it = global_tasks.begin(); it != global_tasks.end(); ++it) {
            if (const_cast<Task&>(*it).id == task_id) {
                const_cast<Task&>(*it).canceled = true;
                return true;
            }
        }
    }

    // Then, check all local queues
    for (auto& queue : local_queues) {
        if (queue->cancel_task(task_id)) {
            return true;
        }
    }

    return false;
}

bool ThreadPool::update_task_priority(uint64_t task_id, TaskPriority new_priority) {
    // First, check global queue
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        for (auto it = global_tasks.begin(); it != global_tasks.end(); ++it) {
            if (const_cast<Task&>(*it).id == task_id) {
                // Since we can't directly modify an element in a set,
                // we need to remove and reinsert
                Task task = *it;
                global_tasks.erase(it);
                task.priority = new_priority;
                global_tasks.insert(std::move(task));
                return true;
            }
        }
    }

    // Then, check all local queues
    for (auto& queue : local_queues) {
        if (queue->update_task_priority(task_id, new_priority)) {
            return true;
        }
    }

    return false;
}

size_t ThreadPool::get_task_count() const {
    std::unique_lock<std::mutex> lock(queue_mutex);
    size_t count = global_tasks.size();

    for (const auto& queue : local_queues) {
        count += queue->size();
    }

    return count;
}

size_t ThreadPool::get_thread_count() const {
    return workers.size();
}

const std::vector<ThreadStatistics>& ThreadPool::get_statistics() const {
    return statistics;
}