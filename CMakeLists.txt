cmake_minimum_required(VERSION 3.31)
project(RV640 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

file(GLOB GLOB_SOURCES
    src/*.h
    src/*.c
    src/dev/*.h
    src/dev/*.c
    src/cpu/*.h
    src/cpu/*.c

    third_party/dtc/libfdt/*.h
    third_party/dtc/libfdt/*.c
)

set(SOURCES "${GLOB_SOURCES}")

file(GLOB GLOB_INCLUDE_DIRS
    third_party/dtc/libfdt
)

set(INCLUDE_DIRS "${GLOB_INCLUDE_DIRS}")

add_subdirectory(cmake/linux)

add_subdirectory(cmake/dos)
