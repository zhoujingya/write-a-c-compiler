cmake_minimum_required(VERSION 3.22)
project(tinycc C CXX)

find_program(MOLD_PATH NAMES mold)
if(MOLD_PATH)
    message(STATUS "Mold is installed, using mold as linker")
    set(CMAKE_LINKER ${MOLD_PATH})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(SYSTEM include ${CMAKE_CURRENT_SOURCE_DIR}/gtest/include)

find_package(LLVM REQUIRED)
include(AddLLVM)
message(STATUS "Found LLVM: " ${LLVM_FOUND})
message(STATUS "LLVM version: " ${LLVM_VERSION})
message(STATUS "LLVM include directory: " ${LLVM_INCLUDE_DIRS})
message(STATUS "LLVM library directory: " ${LLVM_LIBRARY_DIRS})
message(STATUS "LLVM install path: " ${LLVM_INSTALL_PREFIX})
set(LLVM_DIR ${LLVM_INSTALL_PREFIX})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(ENABLE_TESTING)
    add_subdirectory(test)
endif()

add_subdirectory(src)

