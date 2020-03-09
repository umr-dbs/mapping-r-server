# Project
project(mapping_r_server
        LANGUAGES CXX
        )

# CMake Settings
cmake_minimum_required(VERSION 3.5)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

## Debug
set(CMAKE_VERBOSE_MAKEFILE off)

# C++ Standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/lib)

# Include mapping-core as dependency
find_file(
        MAPPING_CORE_PATH
        mapping-core
        HINTS ..
        DOC "The `mapping-core`path"
)
list(APPEND CMAKE_MODULE_PATH ${MAPPING_CORE_PATH}/cmake)

add_subdirectory(src)
