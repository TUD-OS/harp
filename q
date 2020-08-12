[1mdiff --git a/CMakeLists.txt b/CMakeLists.txt[m
[1mindex d10063d..83b06f9 100644[m
[1m--- a/CMakeLists.txt[m
[1m+++ b/CMakeLists.txt[m
[36m@@ -24,7 +24,9 @@[m [madd_library(tetrisclientlegacy SHARED tetris_client_legacy.cc debug_util.cc path[m
 target_link_libraries(tetrisclientlegacy Threads::Threads ${CMAKE_DL_LIBS})[m
 [m
 # libtetris client new version[m
[31m-add_library(tetrisclient STATIC tetris_client.cc proto/Tetris.pb.cc feature.cc push_message_listener.cc path_util.cc util.cc debug_util.cc concrete_client.cc)[m
[32m+[m[32madd_library(tetrisclient STATIC[m
[32m+[m[32m        tetris_client.cc proto/Tetris.pb.cc feature.cc push_message_listener.cc path_util.cc util.cc debug_util.cc[m
[32m+[m[32m        concrete_client.cc)[m
 target_link_libraries(tetrisclient ${Protobuf_LIBRARIES})[m
 install(TARGETS tetrisclient)[m
 install(FILES tetris_client.h DESTINATION include)[m
