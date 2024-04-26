set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

add_library(tetrismockclient SHARED
        mock_client.cc
        mock_message_listener.cc
        mock_library.cc
    )
target_link_libraries(tetrismockclient client_internal ${Protobuf_LIBRARIES} yaml-cpp)
add_dependencies(tetrismockclient generated_proto)
add_dependencies(tetrismockclient tetrisclient)

# Add the mock server library target
add_library(tetrismockserverlib STATIC
    ${PROTO_OUTPUT_SOURCES}
    ${CMAKE_SOURCE_DIR}/util/path_util.cc
    ${CMAKE_SOURCE_DIR}/util/util.cc
    ${CMAKE_SOURCE_DIR}/util/debug_util.cc
    ${CMAKE_SOURCE_DIR}/util/platform/platform.cc
    ${CMAKE_SOURCE_DIR}/util/platform/equiv_res_alloc.cc
    ${CMAKE_SOURCE_DIR}/util/mapping_reader.cc
    ${CMAKE_SOURCE_DIR}/util/mapping.cc
)
target_link_libraries(tetrismockserverlib ${Protobuf_LIBRARIES} yaml-cpp)
add_dependencies(tetrismockserverlib generated_proto)

# Add the executable target
add_executable(tetrismockserver mock_server.cc)
target_link_libraries(tetrismockserver tetrismockserverlib)

