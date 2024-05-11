set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

# Add the mock server library target
add_library(tetristdlib STATIC
    ${PROTO_OUTPUT_SOURCES}
    ${CMAKE_SOURCE_DIR}/util/path_util.cc
    ${CMAKE_SOURCE_DIR}/util/util.cc
    ${CMAKE_SOURCE_DIR}/util/debug_util.cc
)
target_link_libraries(tetristdlib ${Protobuf_LIBRARIES})
add_dependencies(tetristdlib generated_proto)

# Add the executable target
add_executable(tetristd td_server.cc)
target_link_libraries(tetristd tetristdlib)

