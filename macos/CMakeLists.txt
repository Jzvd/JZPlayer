cmake_minimum_required(VERSION 3.25)
project(macos)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE jzmpeg_c ${CMAKE_CURRENT_SOURCE_DIR}/../jzmpeg/*.c)
file(GLOB_RECURSE jzmpeg_h ${CMAKE_CURRENT_SOURCE_DIR}/../jzmpeg/*.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../jzmpeg/)

set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/6.0)
include_directories(${FFMPEG_DIR}/include/)
link_directories(${FFMPEG_DIR}/lib/)

set(FFMPEG_LIBS
        avfilter
        avdevice
        avformat
        avcodec
        avutil
        swresample
        swscale)

set(SDL2 /usr/local/Cellar/sdl2/2.26.5)
include_directories(${SDL2}/include/)
link_directories(${SDL2}/lib/)

add_executable(macos main.c ${jzmpeg_c} ${jzmpeg_h})

target_link_libraries(macos ${FFMPEG_LIBS} sdl2)