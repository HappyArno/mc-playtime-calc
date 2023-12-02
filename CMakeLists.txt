cmake_minimum_required(VERSION 3.12)
project(mc-playtime-calc
        VERSION 0.1
        LANGUAGES C)
add_executable(mc-playtime-calc mc-playtime-calc.c)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(mc-playtime-calc ${ZLIB_LIBRARIES})