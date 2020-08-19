cmake_minimum_required(VERSION 3.1)

project(tetris
        VERSION 1.0)

# general setup
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

# use c++17
set(CMAKE_CXX_STANDARD "17")

# dependent libraries
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

# Add Protobuf
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# Add every component of the TETRiS system.
add_subdirectory(client)
add_subdirectory(client_legacy)
add_subdirectory(server)
add_subdirectory(controller)

enable_testing()
set(TEST_SOURCE_DIRECTORY test)
set(TEST_TARGET_NAME runTests)
add_subdirectory(${TEST_SOURCE_DIRECTORY})