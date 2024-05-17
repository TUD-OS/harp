set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

add_library(tetrisclientlegacy SHARED
        tetris_client_legacy.cc
        )
target_link_libraries(tetrisclientlegacy client_internal ${Protobuf_LIBRARIES} yaml-cpp)
add_dependencies(tetrisclientlegacy generated_proto)
add_dependencies(tetrisclientlegacy tetrisclient)

add_library(scaler_gomp SHARED
        scaler_gomp.cc
        )

add_library(scaler_tbb SHARED
        scaler_tbb.cc
        )
