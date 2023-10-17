cmake_minimum_required(VERSION 3.12)

project(tetris
        VERSION 1.0)

# general setup
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

# use c++17
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "-Wno-format-security")

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

# Find the YAML-CPP library
find_package(yaml-cpp REQUIRED)

# Add every component of the TETRiS system.
add_subdirectory(client)
add_subdirectory(client_legacy)
add_subdirectory(server)
#add_subdirectory(controller)

enable_testing()
set(TEST_SOURCE_DIRECTORY test)
set(TEST_TARGET_NAME runTests)
add_subdirectory(${TEST_SOURCE_DIRECTORY})

# Copy mappigns directory
add_custom_target(copy_mappings ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/examples ${CMAKE_BINARY_DIR}/examples
    COMMENT "Copying mappings to build directory"
)
add_dependencies(${TEST_TARGET_NAME} copy_mappings)
