cmake_minimum_required(VERSION 3.25)
project(macos)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE jzmpeg_c ${CMAKE_CURRENT_SOURCE_DIR}/../jzmpeg/*.c)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../jzmpeg/)

add_executable(macos main.c ${jzmpeg_c})
