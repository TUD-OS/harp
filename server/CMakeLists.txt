add_executable(tetrisserver
        tetris_server.cc
        algorithm.cc
        equivalence.cc
        ${CMAKE_SOURCE_DIR}/proto/tetris.pb.cc
        ${CMAKE_SOURCE_DIR}/util/path_util.cc
        ${CMAKE_SOURCE_DIR}/util/util.cc
        ${CMAKE_SOURCE_DIR}/util/debug_util.cc
        )
target_link_libraries(tetrisserver ${Protobuf_LIBRARIES})