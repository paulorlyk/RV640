cmake_minimum_required(VERSION 3.31)
project(RV640 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
    "-static"
#    "-g"
#    "-O0"
#    "-finline-functions"
#    "-finline-functions-called-once"
)

target_link_options(${PROJECT_NAME} PRIVATE
    "-static"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _GNU_SOURCE
#    CONFIG_RV64=1
#    CONFIG_RV64_32BIT_ADDR=1
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND dtc -o rv640_rv32.dtb ${CMAKE_CURRENT_SOURCE_DIR}/../../dt/rv640_rv32.dts
    VERBATIM
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND dtc -o rv640_rv64.dtb ${CMAKE_CURRENT_SOURCE_DIR}/../../dt/rv640_rv64.dts
    VERBATIM
)
