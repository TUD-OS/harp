set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)
add_executable(tetrisserver
        tetris_server.cc
        algorithm.cc
        equivalence.cc
        knob_description.cc
        ${PROTO_OUTPUT_SOURCES}
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        ${CMAKE_SOURCE_DIR}/util/util.cc
        ${CMAKE_SOURCE_DIR}/util/debug_util.cc
        )
target_link_libraries(tetrisserver ${Protobuf_LIBRARIES})
add_dependencies(tetrisserver generated_proto)