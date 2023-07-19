set_source_files_properties(
    ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

# Add the library target
add_library(tetrisserverlib STATIC
    algorithm.cc
    client.cc
    equivalence.cc
    manager.cc
    mapping_reader.cc
    ${PROTO_OUTPUT_SOURCES}
    ${CMAKE_SOURCE_DIR}/util/path_util.cc
    ${CMAKE_SOURCE_DIR}/util/util.cc
    ${CMAKE_SOURCE_DIR}/util/debug_util.cc
)
target_link_libraries(tetrisserverlib ${Protobuf_LIBRARIES} yaml-cpp)
add_dependencies(tetrisserverlib generated_proto)

# Add the executable target
add_executable(tetrisserver tetris_server.cc)
target_link_libraries(tetrisserver tetrisserverlib)

