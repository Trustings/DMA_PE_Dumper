cmake_minimum_required(VERSION 3.5)

project(DMA-PE-Dumper LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD 99)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/memory)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/util)

set(SOURCES
    ./main.cpp
    ./memory/memory.cpp
)

add_executable(DMA-PE-Dumper ${SOURCES})

find_library(PTHREAD_LIBRARY pthread)
find_library(DL_LIBRARY dl)

add_library(vmm SHARED IMPORTED)
set_target_properties(vmm PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/vmm.so
)

add_library(leechcore SHARED IMPORTED)
set_target_properties(leechcore PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/leechcore.so
)

add_library(leechcore_device_devmem SHARED IMPORTED)
set_target_properties(leechcore_device_devmem PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/leechcore_device_devmem.so
)

add_library(leechcore_device_qemu SHARED IMPORTED)
set_target_properties(leechcore_device_qemu PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/leechcore_device_qemu.so
)

target_link_libraries(DMA-PE-Dumper
    vmm
    leechcore
    leechcore_device_devmem
    leechcore_device_qemu
    ${PTHREAD_LIBRARY}
    ${DL_LIBRARY}
)
