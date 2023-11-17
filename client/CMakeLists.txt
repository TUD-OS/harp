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
        features/application_progress.cc
        ${PROTO_OUTPUT_SOURCES}
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        ${CMAKE_SOURCE_DIR}/util/util.cc
        ${CMAKE_SOURCE_DIR}/util/debug_util.cc
        ${CMAKE_SOURCE_DIR}/util/pthread_direct.cc
        ${CMAKE_SOURCE_DIR}/util/platform/platform.cc
        ${CMAKE_SOURCE_DIR}/util/platform/equiv_res_alloc.cc
        ${CMAKE_SOURCE_DIR}/util/mapping_reader.cc
        ${CMAKE_SOURCE_DIR}/util/mapping.cc
        )
# ensure through a dependency that Protobuf message sources file are going to be generated
add_dependencies(tetrisclient generated_proto)

target_include_directories(tetrisclient
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util>
    $<INSTALL_INTERFACE:include/tetris>
)

# install and export the tetrisclient library
install(TARGETS tetrisclient
        EXPORT tetrisclient-config
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        INCLUDES DESTINATION include
        )
install(FILES
  ${CMAKE_SOURCE_DIR}/client/client.h
  ${CMAKE_SOURCE_DIR}/client/concrete_client.h
  ${CMAKE_SOURCE_DIR}/client/feature.h
  ${CMAKE_SOURCE_DIR}/client/mapping_feature.h
  ${CMAKE_SOURCE_DIR}/client/push_message_listener.h
  DESTINATION include/tetris
)
install(FILES
  ${CMAKE_SOURCE_DIR}/util/lock_util.h
  ${CMAKE_SOURCE_DIR}/util/mapping.h
  ${CMAKE_SOURCE_DIR}/util/path_util.h
  ${CMAKE_SOURCE_DIR}/util/socket.h
  ${CMAKE_SOURCE_DIR}/util/tetris.h
  ${CMAKE_SOURCE_DIR}/util/util.h
  DESTINATION include/tetris/util
)
install(FILES
  ${CMAKE_SOURCE_DIR}/util/platform/cpu_sets.h
  DESTINATION include/tetris/util/platform
)
install(FILES
  ${CMAKE_BINARY_DIR}/proto/tetris.pb.h
  DESTINATION include/tetris/proto
)
install(FILES
  ${CMAKE_SOURCE_DIR}/client/features/movable_threads.h
  DESTINATION include/tetris/features
)
export(TARGETS tetrisclient FILE "${CMAKE_CURRENT_BINARY_DIR}/tetrisclient-config.cmake")
install(EXPORT tetrisclient-config DESTINATION lib/cmake/tetrisclient)
