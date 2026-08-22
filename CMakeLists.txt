cmake_minimum_required(VERSION 4.3)
project(RV640 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

file(GLOB GLOB_SOURCES
    src/*.h
    src/*.c
    src/dev/*.h
    src/dev/*.c
)

set(SOURCES "${GLOB_SOURCES}")

add_subdirectory(cmake/linux)

add_subdirectory(cmake/dos)
