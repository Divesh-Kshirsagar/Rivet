cmake_minimum_required(VERSION 3.16)
project(RivetCompiler VERSION 1.0 LANGUAGES CXX)

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# LLVM Integration 
# -----------------------------------------------------------------------------
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
    Core       # The IR Builder and Context
    Support    # Error handling and raw output streams
    Target     # Generic target machine generation
    ARM        # The specific ARM Cortex backend
    MC         # Machine Code emission (object files)
)

# -----------------------------------------------------------------------------
# The Rivet Executable
# -----------------------------------------------------------------------------
include_directories(include)

set(SOURCES
    src/main.cpp
    src/Lexer.cpp
    src/AST.cpp
    src/Sema.cpp
    src/Parser.cpp
    src/CodeGen.cpp
)

add_executable(rivet ${SOURCES})

target_link_libraries(rivet PRIVATE ${llvm_libs})

target_compile_options(rivet PRIVATE -Wall -Wextra -pedantic)