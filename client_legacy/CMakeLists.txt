add_library(tetrisclientlegacy SHARED
        tetris_client_legacy.cc
        ${CMAKE_SOURCE_DIR}/proto/tetris.pb.cc)