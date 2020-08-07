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
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# libtetris client legacy version
add_library(tetrisclientlegacy SHARED tetris_client_legacy.cc debug_util.cc path_util.cc util.cc)
target_link_libraries(tetrisclientlegacy Threads::Threads ${CMAKE_DL_LIBS})

# libtetris client new version
add_library(tetrisclient SHARED tetris_client.cc proto/Tetris.pb.cc tetris_feature.cc push_message_listener.cc path_util.cc util.cc debug_util.cc)
target_link_libraries(tetrisclient Threads::Threads ${CMAKE_DL_LIBS} ${Protobuf_LIBRARIES})

# tetris server binary
add_executable(tetrisserver tetris_server.cc algorithm.cc equivalence.cc debug_util.cc path_util.cc util.cc)

# tetris control binary
add_executable(tetrisctl tetris_ctl.cc path_util.cc util.cc)

enable_testing()
set(TEST_SOURCE_DIRECTORY test)
set(TEST_TARGET_NAME runTests)
add_subdirectory(${TEST_SOURCE_DIRECTORY})