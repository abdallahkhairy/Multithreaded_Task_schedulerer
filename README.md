# Multithreaded Task Scheduler

## Overview

I created this **Multithreaded Task Scheduler** as a C++ project to efficiently manage and execute tasks in a multithreaded environment. My goal was to build a thread pool that could handle tasks with different priorities, balance workloads across threads using work stealing, and provide an easy-to-use interface for submitting tasks and retrieving results. This project leverages modern C++ features like `std::future`, `std::thread`, and `std::set` to achieve these goals.

The thread pool supports task prioritization (Low, Normal, High, Critical), work stealing for load balancing, asynchronous task execution with return values, task cancellation, timeouts, dynamic priority updates, and detailed statistics. I also included a variety of examples to demonstrate its capabilities, such as handling tasks with different priorities, computing Fibonacci numbers, and performing a stress test with 1000 tasks.

## Features

- **Task Prioritization**: Tasks can be submitted with different priority levels (Low, Normal, High, Critical), ensuring that more important tasks are executed first.
- **Dynamic Priority Updates**: Update the priority of a task even after it has been submitted.
- **Work Stealing**: Threads can steal tasks from each other to balance the workload, with an optimized algorithm to reduce contention.
- **Asynchronous Execution**: Tasks can return values via `std::future`, allowing for asynchronous execution and result retrieval.
- **Task Cancellation**: Cancel tasks before they are executed using a unique task ID.
- **Timeouts**: Specify a timeout for tasks; if they aren’t executed within the timeout period, they are automatically canceled.
- **Detailed Statistics**: Track per-thread task counts, total execution time, and average execution time per task.
- **Thread Safety**: The implementation uses `std::mutex` and `std::condition_variable` to ensure thread safety and avoid race conditions.
- **Scalability**: The thread pool automatically scales to the number of hardware threads available (`std::thread::hardware_concurrency()`).

## Project Structure

The project is organized into three main files:

- **`ThreadPool.h`**: Header file containing the definitions of the `ThreadPool` class, `WorkStealingQueue` class, and related structures like `Task` and `TaskPriority`.
- **`ThreadPool.cpp`**: Implementation file for the `ThreadPool` and `WorkStealingQueue` classes.
- **`main.cpp`**: Test file with examples demonstrating the thread pool’s functionality.

## Requirements

- **C++17** or later (for features like `std::future`, `std::atomic`, and `std::thread`).
- A C++ compiler (e.g., MSVC, GCC, Clang).
- **CMake** (optional, but recommended for building the project).
- An IDE or editor for development (e.g., Visual Studio, CLion, VS Code).

## Building the Project

### Using CMake
I recommend using CMake to build the project, as it simplifies the build process across different platforms. Here’s how I set it up:

1. Create a `CMakeLists.txt` file in the project root with the following content:
   ```cmake
   cmake_minimum_required(VERSION 3.10)
   project(MultithreadedTaskScheduler)
   set(CMAKE_CXX_STANDARD 17)
   add_executable(TaskScheduler main.cpp ThreadPool.cpp)