cmake_minimum_required(VERSION 3.10)
project(MultithreadedTaskScheduler)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threads package
find_package(Threads REQUIRED)

# Add executable
add_executable(task_scheduler 
    main.cpp
    ThreadPool.cpp
)

# Link against thread library
target_link_libraries(task_scheduler PRIVATE Threads::Threads)

# Add include directories
target_include_directories(task_scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
