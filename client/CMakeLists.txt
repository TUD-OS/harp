# prevent that the add_library(tetrisclient) fail because Protobuf generated sources are not
# supposed to be generated yet.
set_source_files_properties(
        ${PROTO_OUTPUT_SOURCES} PROPERTIES GENERATED TRUE)

add_library(tetrisclient STATIC
        client.cc
        feature.cc
        concrete_client.cc
        push_message_listener.cc
        features/movable_threads.cc
        features/scalable_application.cc
        ${PROTO_OUTPUT_SOURCES}
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        ${CMAKE_SOURCE_DIR}/util/util.cc
        ${CMAKE_SOURCE_DIR}/util/debug_util.cc
        )
# ensure through a dependency that Protobuf message sources file are going to be generated
add_dependencies(tetrisclient generated_proto)
# install and export the tetrisclient library
install(TARGETS tetrisclient
        EXPORT tetrisclient-config
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        INCLUDES DESTINATION include
        )
install(FILES ${CMAKE_SOURCE_DIR}/client/client.h ${CMAKE_SOURCE_DIR}/client/feature.h DESTINATION include/tetris)
install(FILES ${CMAKE_BINARY_DIR}/proto/tetris.pb.h DESTINATION include/tetris/proto)
install(FILES ${CMAKE_SOURCE_DIR}/client/features/movable_threads.h DESTINATION include/tetris/features)
export(TARGETS tetrisclient FILE "${CMAKE_CURRENT_BINARY_DIR}/tetrisclient-config.cmake")
install(EXPORT tetrisclient-config DESTINATION lib/cmake/tetrisclient)
