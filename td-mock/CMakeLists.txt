set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

add_library(tetristdclient SHARED
    td_client.cc
)
target_link_libraries(tetristdclient client_internal ${Protobuf_LIBRARIES} yaml-cpp)
add_dependencies(tetristdclient generated_proto)
add_dependencies(tetristdclient tetrisclient)

# Add the TD server library target
add_library(tetristdlib STATIC
    ${PROTO_OUTPUT_SOURCES}
    ${CMAKE_SOURCE_DIR}/util/path_util.cc
    ${CMAKE_SOURCE_DIR}/util/util.cc
    ${CMAKE_SOURCE_DIR}/util/debug_util.cc
)
target_link_libraries(tetristdlib ${Protobuf_LIBRARIES})
add_dependencies(tetristdlib generated_proto)

# Add the TD executable target
add_executable(tetristd td_server.cc)
target_link_libraries(tetristd tetristdlib)

