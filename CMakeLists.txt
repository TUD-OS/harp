project(tetris)

cmake_minimum_required(VERSION 3.1)

# general setup
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

# use c++17
set(CMAKE_CXX_STANDARD "17")

# dependent libraries
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# libtetris client legacy version
add_library(tetrisclientlegacy SHARED tetris_client_legacy.cc debug_util.cc)
target_link_libraries(tetrisclientlegacy Threads::Threads ${CMAKE_DL_LIBS})

# tetris server binary
add_executable(tetrisserver tetris_server.cc algorithm.cc equivalence.cc debug_util.cc)

# tetris control binary
add_executable(tetrisctl tetris_ctl.cc)
