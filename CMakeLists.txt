cmake_minimum_required(VERSION 3.13)

project(redis)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)

add_executable(redis ${SOURCE_FILES}
        src/resp_parser.h
        src/resp_parser.cpp
        src/utils.h
        src/utils.cpp
        src/storage.h
        src/storage.cpp
        src/timer.h
        src/timer.cpp
        src/commands.h
        src/commands.cpp)

target_link_libraries(redis PRIVATE asio asio::asio)
target_link_libraries(redis PRIVATE Threads::Threads)