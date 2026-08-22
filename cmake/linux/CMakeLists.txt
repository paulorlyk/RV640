cmake_minimum_required(VERSION 4.3)
project(RV640 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
#    "-g"
#    "-O0"
#    "-finline-functions"
#    "-finline-functions-called-once"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _GNU_SOURCE
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND dtc -o rv640.dtb ${CMAKE_CURRENT_SOURCE_DIR}/../../dt/rv640.dts
    VERBATIM
)
