# Advanced C++ Thread Pool

A high-performance, feature-rich thread pool implementation in C++ with work-stealing, task prioritization, timeouts, and comprehensive statistics tracking. This library provides an elegant solution for multi-threaded applications requiring optimal CPU utilization and advanced task management.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C++11](https://img.shields.io/badge/Language-C%2B%2B11-orange.svg)](https://isocpp.org/wiki/faq/cpp11)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/yourusername/cpp-threadpool)

## Features

- **Work-stealing algorithm** for balanced workload distribution across threads
- **Task prioritization** with four priority levels (Low, Normal, High, Critical)
- **Dynamic priority updates** for queued tasks to adapt to changing conditions
- **Task cancellation** support for unexecuted tasks
- **Task timeout** capabilities to prevent stale tasks from executing
- **Execution statistics** tracking for performance monitoring and optimization
- **Future-based** task results for easy synchronization
- **Thread-safe** operations with proper mutex protection
- **Header-only** design for simple integration

## Table of Contents

- [Workflow Overview](#workflow-overview)
- [Requirements](#requirements)
- [Installation](#installation)
- [Basic Usage](#basic-usage)
- [Advanced Usage](#advanced-usage)
  - [Task Priorities](#task-priorities)
  - [Task Cancellation](#task-cancellation)
  - [Task Timeouts](#task-timeouts)
  - [Priority Updates](#priority-updates)
  - [Work Stealing Mechanism](#work-stealing-mechanism)
  - [Collecting Statistics](#collecting-statistics)
- [Architecture and Components](#architecture-and-components)
  - [ThreadPool Class](#threadpool-class)
  - [Task Structure](#task-structure)
  - [WorkStealingQueue Class](#workstealingqueue-class)
  - [ThreadStatistics Structure](#threadstatistics-structure)
- [API Reference](#api-reference)
- [Examples](#examples)
  - [Basic Example](#basic-example)
  - [Fibonacci Calculation Example](#fibonacci-calculation-example)
  - [Priority and Task Cancellation Example](#priority-and-task-cancellation-example)
  - [Stress Test Example](#stress-test-example)
- [Performance Considerations](#performance-considerations)
- [Thread Safety](#thread-safety)
- [Best Practices](#best-practices)
- [License](#license)

## Workflow Overview

Below is a high-level workflow diagram of how the thread pool operates:

```
┌─────────────────────┐     ┌──────────────────────────────────────────┐
│                     │     │                                          │
│   Client/User Code  │     │           Thread Pool Internals          │
│                     │     │                                          │
└──────────┬──────────┘     └──────────────────────────────────────────┘
           │                                    │
           │                                    │
           │    ┌─────────────────────┐         │
           │    │                     │         │
           ├───►│  Submit Task with   │         │
           │    │  Priority & Timeout │         │
           │    │                     │         │
           │    └──────────┬──────────┘         │
           │               │                    │
           │               ▼                    │
           │    ┌─────────────────────┐         │
           │    │                     │         │
           │    │  Task added to      │         │
           │    │  Global Task Queue  │         │
           │    │                     │         │
           │    └──────────┬──────────┘         │
           │               │                    │
           │               ▼                    │
┌──────────┴──────────┐   ┌────────────────────────────────────────────┐
│                     │   │                                            │
│  Return std::future │   │            Worker Threads                  │
│  for task result    │   │                                            │
│                     │   │  ┌───────────────┐   ┌───────────────┐     │
└─────────────────────┘   │  │ Worker        │   │ Worker        │     │
                          │  │ Thread 1      │   │ Thread 2      │ ... │
                          │  │               │   │               │     │
                          │  └───────┬───────┘   └───────┬───────┘     │
                          │          │                   │             │
                          │          ▼                   ▼             │
                          │  ┌─────────────────────────────────────┐   │
                          │  │                                     │   │
                          │  │         Task Acquisition            │   │
                          │  │                                     │   │
                          │  │  1. Try local queue                 │   │
                          │  │  2. Try global queue                │   │
                          │  │  3. Try stealing from other threads │   │
                          │  │                                     │   │
                          │  └─────────────────┬───────────────────┘   │
                          │                    │                       │
                          │                    ▼                       │
                          │  ┌─────────────────────────────────────┐   │
                          │  │                                     │   │
                          │  │         Task Execution              │   │
                          │  │                                     │   │
                          │  │  1. Check if task is canceled       │   │
                          │  │  2. Check if task has timed out     │   │
                          │  │  3. Execute task function           │   │
                          │  │  4. Update execution statistics     │   │
                          │  │  5. Set future result               │   │
                          │  │                                     │   │
                          │  └─────────────────────────────────────┘   │
                          │                                            │
                          └────────────────────────────────────────────┘
```

## Requirements

- C++11 compatible compiler
- Standard C++ libraries (thread, mutex, condition_variable, future, etc.)
- No external dependencies

## Installation

This is a header-only library. Simply copy the `ThreadPool.h` and its implementation file into your project and include them.

```cpp
#include "ThreadPool.h"
```

If you're using CMake, you can integrate it as follows:

```cmake
# Add the include directory
include_directories(path/to/threadpool)

# Add the source files
add_executable(your_app main.cpp path/to/threadpool/ThreadPool.cpp)
```

## Basic Usage

Here's a simple example of how to use the thread pool:

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <vector>

int main() {
    // Create a thread pool with default number of threads (hardware concurrency)
    ThreadPool pool;
    std::cout << "Thread pool created with " << pool.get_thread_count() << " threads" << std::endl;
    
    // Submit a simple task without return value
    pool.submit([]() {
        std::cout << "Hello from thread " << std::this_thread::get_id() << std::endl;
    });
    
    // Submit tasks with return values
    std::vector<std::future<int>> results;
    for (int i = 0; i < 10; ++i) {
        results.push_back(pool.submit([i]() {
            return i * i;
        }));
    }
    
    // Retrieve and print the results
    for (int i = 0; i < 10; ++i) {
        std::cout << "Result " << i << ": " << results[i].get() << std::endl;
    }
    
    return 0;
}
```

## Advanced Usage

### Task Priorities

The thread pool supports four levels of task priority:

```cpp
enum class TaskPriority {
    Low,
    Normal,
    High,
    Critical
};
```

You can submit tasks with specific priorities:

```cpp
// Submit tasks with different priorities
pool.submit(TaskPriority::Low, std::chrono::milliseconds(0), []() {
    std::cout << "Low priority task executed" << std::endl;
});

pool.submit(TaskPriority::Normal, std::chrono::milliseconds(0), []() {
    std::cout << "Normal priority task executed" << std::endl;
});

pool.submit(TaskPriority::High, std::chrono::milliseconds(0), []() {
    std::cout << "High priority task executed" << std::endl;
});

pool.submit(TaskPriority::Critical, std::chrono::milliseconds(0), []() {
    std::cout << "Critical priority task executed" << std::endl;
});
```

The thread pool will execute higher priority tasks before lower priority ones.

### Task Cancellation

You can cancel tasks that haven't started executing yet using their unique task ID:

```cpp
// Submit a task and get its unique ID
uint64_t task_id = 10; // In practice, you would track this from the pool
auto future = pool.submit(TaskPriority::Normal, std::chrono::milliseconds(0), []() {
    std::cout << "This task might not execute if canceled" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
});

// Cancel the task
bool canceled = pool.cancel_task(task_id);
if (canceled) {
    std::cout << "Task was successfully canceled" << std::endl;
} else {
    std::cout << "Task could not be canceled (may have already started)" << std::endl;
}
```

### Task Timeouts

Tasks can have timeout durations. If a task isn't started within its timeout period, it will be discarded:

```cpp
// Submit a task with a 500ms timeout
pool.submit(TaskPriority::Normal, std::chrono::milliseconds(500), []() {
    // If this task doesn't start within 500ms, it will be discarded
    std::cout << "Task with timeout executed" << std::endl;
});

// Submit many high-priority tasks to potentially cause the timeout
for (int i = 0; i < 100; ++i) {
    pool.submit(TaskPriority::High, std::chrono::milliseconds(0), [i]() {
        std::cout << "High priority task " << i << " executed" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
}
```

### Priority Updates

You can dynamically update the priority of tasks that are still queued:

```cpp
// Submit a low priority task
uint64_t task_id = 20; // In practice, you would track this from the pool
pool.submit(TaskPriority::Low, std::chrono::milliseconds(0), []() {
    std::cout << "This task was initially low priority" << std::endl;
});

// Update its priority to critical
bool updated = pool.update_task_priority(task_id, TaskPriority::Critical);
if (updated) {
    std::cout << "Task priority updated to Critical" << std::endl;
} else {
    std::cout << "Task priority could not be updated (task may have already started)" << std::endl;
}
```

### Work Stealing Mechanism

The thread pool automatically implements work stealing between threads. Each worker thread follows this procedure:

1. Try to get a task from its local queue
2. If local queue is empty, try to get a task from the global queue
3. If global queue is empty, try to steal a task from another thread's queue
4. If all of the above fail, wait for new tasks to be submitted

This ensures optimal load balancing and minimizes thread idle time.

```cpp
// Example demonstrating work stealing (submit tasks with varying execution times)
for (int i = 0; i < 20; ++i) {
    pool.submit([i]() {
        // Variable work duration to create imbalance and trigger work stealing
        int sleep_time = (i % 5) * 100;
        std::cout << "Task " << i << " starting on thread " 
                  << std::this_thread::get_id() << ", sleeping for " 
                  << sleep_time << "ms" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        
        std::cout << "Task " << i << " completed on thread " 
                  << std::this_thread::get_id() << std::endl;
    });
}
```

### Collecting Statistics

The thread pool tracks execution statistics for each thread:

```cpp
// Run some tasks
for (int i = 0; i < 100; ++i) {
    pool.submit([i]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(i % 10));
    });
}

// Wait for tasks to complete
std::this_thread::sleep_for(std::chrono::seconds(1));

// Get and display statistics
const auto& stats = pool.get_statistics();
for (size_t i = 0; i < stats.size(); ++i) {
    std::cout << "Thread " << i << " statistics:\n"
              << "  Tasks executed: " << stats[i].tasks_executed << "\n"
              << "  Total execution time: " << stats[i].total_execution_time << " μs\n"
              << "  Average execution time: " 
              << (stats[i].tasks_executed > 0 ? 
                  stats[i].total_execution_time / stats[i].tasks_executed : 0)
              << " μs per task\n";
}
```

## Architecture and Components

### ThreadPool Class

The main class that manages worker threads and task distribution:

- Maintains a pool of worker threads
- Manages the global task queue (prioritized)
- Provides the task submission interface
- Handles task cancellation and priority updates
- Collects execution statistics

### Task Structure

Represents a unit of work with metadata:

```cpp
struct Task {
    std::function<void()> function;     // The actual work to be done
    TaskPriority priority;              // Priority level
    uint64_t id;                        // Unique task ID for cancellation
    std::chrono::steady_clock::time_point submission_time; // When the task was submitted
    std::chrono::milliseconds timeout;  // Timeout duration
    std::atomic<bool> canceled;         // Flag for cancellation
    std::chrono::steady_clock::time_point start_time; // For execution time tracking
    std::chrono::steady_clock::time_point end_time;   // For execution time tracking
};
```

### WorkStealingQueue Class

A specialized queue used by each worker thread:

- Maintains a thread-local task queue
- Provides thread-safe push, pop, and steal operations
- Supports task cancellation and priority updates

### ThreadStatistics Structure

Tracks performance metrics for each worker thread:

```cpp
struct ThreadStatistics {
    std::atomic<size_t> tasks_executed;
    std::atomic<std::chrono::microseconds::rep> total_execution_time;
};
```

## API Reference

### ThreadPool Class

```cpp
// Constructor
ThreadPool(size_t num_threads = std::thread::hardware_concurrency());

// Destructor - stops all threads and cleans up resources
~ThreadPool();

// Submit a task with specific priority and timeout
template<class F, class... Args>
auto submit(TaskPriority priority, std::chrono::milliseconds timeout, F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;

// Submit a task with default priority (Normal) and no timeout
template<class F, class... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;

// Cancel a task by ID
bool cancel_task(uint64_t task_id);

// Update task priority
bool update_task_priority(uint64_t task_id, TaskPriority new_priority);

// Get current task count across all queues
size_t get_task_count() const;

// Get thread count
size_t get_thread_count() const;

// Get statistics for all threads
const std::vector<ThreadStatistics>& get_statistics() const;
```

## Examples

### Basic Example

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <string>

int main() {
    ThreadPool pool(4); // Create a pool with 4 threads
    
    // Submit a task with a return value
    auto result1 = pool.submit([]() {
        return std::string("Hello from thread pool!");
    });
    
    // Submit a task without a return value
    pool.submit([]() {
        std::cout << "Task executed on thread " << std::this_thread::get_id() << std::endl;
    });
    
    // Wait for and print the result
    std::cout << result1.get() << std::endl;
    
    return 0;
}
```

### Fibonacci Calculation Example

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <iomanip>

// Recursive Fibonacci calculation (inefficient on purpose to demonstrate scheduling)
int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    ThreadPool pool;
    
    // Submit several Fibonacci calculations
    std::vector<std::future<int>> results;
    for (int i = 20; i < 35; i += 2) {
        results.push_back(pool.submit([i]() {
            std::cout << "Computing fibonacci(" << i << ") on thread " 
                      << std::this_thread::get_id() << "\n";
            auto start = std::chrono::high_resolution_clock::now();
            int result = fibonacci(i);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Fibonacci(" << i << ") completed in " << duration.count() << "ms\n";
            return result;
        }));
    }
    
    // Retrieve and print the results
    std::cout << "\nResults:\n";
    std::cout << std::setw(12) << "n" << std::setw(20) << "fibonacci(n)" << "\n";
    std::cout << std::string(32, '-') << "\n";
    
    int i = 20;
    for (auto& future : results) {
        std::cout << std::setw(12) << i << std::setw(20) << future.get() << "\n";
        i += 2;
    }
    
    return 0;
}
```

### Priority and Task Cancellation Example

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    ThreadPool pool;
    
    std::cout << "Starting priority and cancellation example...\n";
    
    // Submit low priority tasks
    std::vector<uint64_t> task_ids;
    for (int i = 0; i < 5; ++i) {
        uint64_t id = 100 + i; // In a real scenario, you'd track this from submission
        task_ids.push_back(id);
        
        pool.submit(TaskPriority::Low, std::chrono::milliseconds(0), [i]() {
            std::cout << "Low priority task " << i << " executed on thread " 
                      << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    
    // Cancel the third task
    if (task_ids.size() >= 3) {
        bool canceled = pool.cancel_task(task_ids[2]);
        std::cout << "Cancellation of task ID " << task_ids[2] 
                  << (canceled ? " successful\n" : " failed\n");
    }
    
    // Update priority of the fourth task
    if (task_ids.size() >= 4) {
        bool updated = pool.update_task_priority(task_ids[3], TaskPriority::Critical);
        std::cout << "Priority update of task ID " << task_ids[3]
                  << (updated ? " successful\n" : " failed\n");
    }
    
    // Submit a high priority task that should execute before low priority tasks
    pool.submit(TaskPriority::High, std::chrono::milliseconds(0), []() {
        std::cout << "High priority task executed on thread "
                  << std::this_thread::get_id() << "\n";
    });
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    return 0;
}
```

### Stress Test Example

```cpp
#include "ThreadPool.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <iomanip>

int main() {
    ThreadPool pool;
    
    std::cout << "Starting stress test with " << pool.get_thread_count() << " threads\n";
    
    const int num_tasks = 10000;
    std::atomic<int> counter{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Submit a large number of quick tasks
    for (int i = 0; i < num_tasks; ++i) {
        pool.submit([&counter]() {
            // Simulate a small amount of work
            std::this_thread::sleep_for(std::chrono::microseconds(50));
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
    std::cout << "Average time per task: " 
              << std::fixed << std::setprecision(3)
              << static_cast<double>(duration.count()) / num_tasks << "ms\n\n";
    
    // Print statistics
    const auto& stats = pool.get_statistics();
    std::cout << "Thread statistics:\n";
    std::cout << std::setw(10) << "Thread" 
              << std::setw(15) << "Tasks" 
              << std::setw(20) << "Total Time (μs)" 
              << std::setw(20) << "Avg Time (μs)" << "\n";
    std::cout << std::string(65, '-') << "\n";
    
    for (size_t i = 0; i < stats.size(); ++i) {
        double avg_time = stats[i].tasks_executed > 0 
            ? static_cast<double>(stats[i].total_execution_time) / stats[i].tasks_executed 
            : 0.0;
        
        std::cout << std::setw(10) << i 
                  << std::setw(15) << stats[i].tasks_executed
                  << std::setw(20) << stats[i].total_execution_time
                  << std::setw(20) << std::fixed << std::setprecision(2) << avg_time << "\n";
    }
    
    return 0;
}
```

## Performance Considerations

The thread pool is designed with performance in mind:

- **Task Prioritization**: Critical tasks execute before less important ones
- **Work Stealing**: Ensures better load balancing across threads
- **Local Queues**: Reduces contention between threads for better scalability
- **Task Timeouts**: Prevents stale tasks from executing unnecessarily
- **Statistics Tracking**: Helps identify performance bottlenecks

For optimal performance:
- Match the number of threads to the number of available CPU cores
- Group related tasks with similar priorities
- Use appropriate timeouts for tasks that become irrelevant after a certain duration
- Monitor task execution statistics to identify performance issues

## Thread Safety

The thread pool is designed to be thread-safe:

- All queue operations are protected by mutexes
- Atomic operations are used for task cancellation flags and statistics counters
- The work-stealing algorithm is implemented with proper synchronization
- Task submission, cancellation, and priority updates can be safely called from any thread

## Best Practices

- **Choose the right number of threads**: Usually matching the number of CPU cores is optimal
- **Use appropriate task granularity**: Too small tasks create overhead, too large tasks reduce parallelism
- **Set proper task priorities**: Use higher priorities only for truly important tasks
- **Avoid long-running tasks**: Split them into smaller chunks when possible
- **Monitor statistics**: Check for uneven workload distribution
- **Handle exceptions**: Tasks throwing exceptions will set the exception in the future
- **Use timeouts wisely**: Set realistic timeout values based on your application's needs

## License

This project is licensed under the MIT License - see the LICENSE file for details.
