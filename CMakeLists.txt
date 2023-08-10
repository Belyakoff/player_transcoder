
if (CMAKE_VERSION VERSION_GREATER 3.12)
 
project(teleweb)			
set(CMAKE_CXX_STANDARD 11)

message(STATUS "CMAKE SOURCE DIR = ${CMAKE_SOURCE_DIR}")


set(SOURCE_EXE )
set(SOURCE_LIB_GLFW ${CMAKE_SOURCE_DIR}/player_transcoder/lib/glfw3/glfw3.lib)

include_directories(${CMAKE_SOURCE_DIR}/player_transcoder/include)

add_executable(main ${SOURCE_EXE}  "main.cpp" "video_reader.cpp") 

list(APPEND SOURCE_AV
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/avcodec.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/avdevice.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/avfilter.lib 
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/avformat.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/avutil.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/postproc.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/swresample.lib
    ${CMAKE_SOURCE_DIR}/player_transcoder/lib/swscale.lib
)



target_link_libraries(main ${SOURCE_LIB_GLFW} opengl32.dll ${SOURCE_AV})


endif()

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
