cmake_minimum_required(VERSION 3.31)
project(RV640 C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

set(CMAKE_C_COMPILER "ia16-elf-gcc")

set(EXECUTABLE_8086 "${PROJECT_NAME}.exe")

add_executable(${EXECUTABLE_8086}
    ${SOURCES}
)

target_compile_options(${EXECUTABLE_8086} PRIVATE
    "-Wall"

    "-flto"

    "-march=i8086"
    "-mmsdos"
    "-mcmodel=medium"
    "-Os"

    "-g0"
#    "-mno-segment-relocation-stuff"
#    "-mshort-jumps"
#    "-mrtd"
#    "-mregparmcall"

#    "-mnewlib-nano-stdio"

    "-masm=intel"
#    "-S"
)

target_link_libraries(${EXECUTABLE_8086} PRIVATE
    "i86"
)

target_link_options(${EXECUTABLE_8086} PRIVATE
    "-mcmodel=medium"
    "-flto"
#    "-mnewlib-nano-stdio"
)

target_compile_definitions(${EXECUTABLE_8086} PRIVATE
#    NDEBUG
    CONFIG_DOS=1
)

add_custom_command(TARGET ${EXECUTABLE_8086} POST_BUILD
    COMMAND dtc -o rv640.dtb ${CMAKE_CURRENT_SOURCE_DIR}/../../dt/rv640.dts
    VERBATIM
)
