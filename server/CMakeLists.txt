add_executable(tetrisserver tetris_server.cc algorithm.cc equivalence.cc)
target_link_libraries(tetrisserver util proto ${Protobuf_LIBRARIES})