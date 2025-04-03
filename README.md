# Multithreaded Task Scheduler

A high-performance thread pool implementation for efficient task scheduling in C++.

## Features

* Thread pool implementation for efficient task scheduling
* Modern C++ concurrency features (std::thread, std::mutex, std::condition_variable)
* Work-stealing algorithm for balanced load distribution
* Priority queue system for task management
* Clean API for submitting and managing asynchronous tasks
* Support for tasks with return values via std::future

## Requirements

* C++14 or higher
* CMake 3.10 or higher
* A C++ compiler with support for C++14 features

## Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

Here's a simple example of how to use the thread pool:

```cpp
#include "ThreadPool.h"
#include <iostream>

int main() {
    // Create a thread pool with default number of threads (hardware concurrency)
    ThreadPool pool;
    
    // Submit a task without return value
    pool.submit([]() {
        std::cout << "Hello from task!" << std::endl;
    });
    
    // Submit a task with return value
    auto future = pool.submit([]() {
        return 42;
    });
    
    // Wait for the result
    std::cout << "The answer is: " << future.get() << std::endl;
    
    // Submit a high priority task
    pool.submit(TaskPriority::High, []() {
        std::cout << "High priority task" << std::endl;
    });
    
    // Tasks will be automatically completed before the pool is destroyed
    return 0;
}
```

## Task Priorities

Tasks can be submitted with different priority levels:

* `TaskPriority::Low`: Low priority tasks
* `TaskPriority::Normal`: Normal priority tasks (default)
* `TaskPriority::High`: High priority tasks
* `TaskPriority::Critical`: Critical priority tasks

Higher priority tasks are processed before lower priority tasks.

## Work Stealing

The thread pool implements a work-stealing algorithm to ensure balanced load distribution among threads. When a thread has no tasks in its local queue, it will:

1. First check the global queue for high-priority tasks
2. If the global queue is empty, attempt to steal tasks from other threads' queues

This approach minimizes contention on the global queue and ensures efficient processing of tasks.

## License

[MIT License](LICENSE)
