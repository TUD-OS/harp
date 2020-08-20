add_library(tetrisclient STATIC
        client.cc
        feature.cc
        concrete_client.cc
        push_message_listener.cc
        ${CMAKE_SOURCE_DIR}/proto/tetris.pb.cc
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        ${CMAKE_SOURCE_DIR}/util/util.cc
        ${CMAKE_SOURCE_DIR}/util/debug_util.cc
        )
install(TARGETS tetrisclient
        EXPORT tetrisclient-config
        ARCHIVE DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/client/client.h ${CMAKE_SOURCE_DIR}/client/feature.h DESTINATION include/tetris)
install(FILES ${CMAKE_SOURCE_DIR}/proto/tetris.pb.h DESTINATION include/tetris/proto)
export(TARGETS tetrisclient FILE "${CMAKE_CURRENT_BINARY_DIR}/tetrisclient-config.cmake")
install(EXPORT tetrisclient-config DESTINATION lib/cmake/tetrisclient)