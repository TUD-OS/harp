set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)
add_executable(tetrisctl
        tetris_ctl.cc
        ${PROTO_OUTPUT_SOURCES}
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        )
target_link_libraries(tetrisctl ${Protobuf_LIBRARIES})
add_dependencies(tetrisctl generated_proto)
