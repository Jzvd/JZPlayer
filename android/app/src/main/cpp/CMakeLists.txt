cmake_minimum_required(VERSION 3.22.1)

project("jzplayer")

file(GLOB_RECURSE jzmpeg_c ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../jzmpeg/*.c)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../jzmpeg/)

include_directories(${CMAKE_SOURCE_DIR}/includes)
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavcodec.so)
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavformat.so)
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavutil.so)
add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswresample.so)
add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswscale.so)


add_library( # Sets the name of the library.
        jzplayer
        SHARED
        native-lib.cpp ${jzmpeg_c})
find_library( # Sets the name of the path variable.
        log-lib
        log)

set(FFMPEG_LIBS
#        avfilter
#        avdevice
        avformat
        avcodec
        avutil
        swresample
        swscale)

target_link_libraries( # Specifies the target library.
        jzplayer
        aaudio
        android
        ${FFMPEG_LIBS}
        ${log-lib})