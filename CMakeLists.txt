cmake_minimum_required(VERSION 3.10)
project(DMA-PE-Dumper LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/memory)

set(SOURCES
    ./main.cpp
    ./memory/memory.cpp
)

add_executable(DMA-PE-Dumper ${SOURCES})

target_include_directories(DMA-PE-Dumper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
)

target_compile_definitions(DMA-PE-Dumper PRIVATE
    LINUX
    _FILE_OFFSET_BITS=64
    _FORTIFY_SOURCE=2
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vmm.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIB_DIR}/vmm.so
        ${CMAKE_CURRENT_BINARY_DIR}/vmm.so
    COMMENT "Copying vmm.so to build directory"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/leechcore.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIB_DIR}/leechcore.so
        ${CMAKE_CURRENT_BINARY_DIR}/leechcore.so
    COMMENT
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/leechcore_device_qemu.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIB_DIR}/leechcore_device_qemu.so
        ${CMAKE_CURRENT_BINARY_DIR}/leechcore_device_qemu.so
    COMMENT
)

add_custom_target(copied_libs ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/vmm.so
        ${CMAKE_CURRENT_BINARY_DIR}/leechcore.so
        ${CMAKE_CURRENT_BINARY_DIR}/leechcore_device_qemu.so
)

add_dependencies(DMA-PE-Dumper copied_libs)

target_link_libraries(DMA-PE-Dumper
    -L${CMAKE_CURRENT_BINARY_DIR}
    -l:vmm.so
    -l:leechcore.so
    -l:leechcore_device_qemu.so

    pthread
    dl
    fuse
)

target_link_options(DMA-PE-Dumper PRIVATE
    -Wl,-z,noexecstack
    -Wl,-rpath,'$$ORIGIN'
)

set_target_properties(DMA-PE-Dumper PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)

#target_compile_options(DMA-PE-Dumper PRIVATE
#    -fPIE
#    -fPIC
#    -pie
#    -fstack-protector-strong
#    -O2
#    -Wall
#    -Wextra
#)
