set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)
add_library(tetrisclientlegacy SHARED
        tetris_client_legacy.cc
        ${PROTO_OUTPUT_SOURCES}
    )
target_link_libraries(tetrisclientlegacy ${Protobuf_LIBRARIES} tetrisclient)
add_dependencies(tetrisclientlegacy generated_proto)
add_dependencies(tetrisclientlegacy tetrisclient)
