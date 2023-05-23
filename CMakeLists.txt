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
include_directories(${CMAKE_BINARY_DIR})

# Find the Protobuf library
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

# Generate .pb.cc and .pb.h files
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
file(GLOB PROTO_FILES "${PROTO_PATH}/*.proto")
foreach(PROTO_FILE ${PROTO_FILES})
    string(REGEX REPLACE "[.]proto$" ".pb.cc" TMP_OUTPUT_SOURCE ${PROTO_FILE})
    file(RELATIVE_PATH RELATIVE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR} ${TMP_OUTPUT_SOURCE})
    file(TO_CMAKE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${RELATIVE_SOURCE} PROTO_OUTPUT_SOURCE)
    list(APPEND PROTO_OUTPUT_SOURCES ${PROTO_OUTPUT_SOURCE})
endforeach()
add_custom_command(OUTPUT ${PROTO_OUTPUT_SOURCES}
        COMMAND protoc --proto_path=${PROTO_PATH} --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto ${PROTO_FILES}
        DEPENDS ${PROTO_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating Protobuf message sources")
add_custom_target(generated_proto DEPENDS ${PROTO_OUTPUT_SOURCES})

# Add every component of the TETRiS system.
add_subdirectory(client)
add_subdirectory(client_legacy)
add_subdirectory(server)
#add_subdirectory(controller)
