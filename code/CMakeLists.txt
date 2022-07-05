cmake_minimum_required(VERSION 3.16)
project(Kaleidoscope)

set(CMAKE_CXX_STANDARD 14)

set(BUILD_ARCH "-m64")
set(TARGET_64 ON)

# import LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(Kaleidoscope "../code/main.cpp")

llvm_map_components_to_libnames(llvm_libs
    Analysis
    Core
    ExecutionEngine
    InstCombine
    Object
    OrcJIT
    RuntimeDyld
    ScalarOpts
    Support
    native
    )

target_link_libraries(Kaleidoscope ${llvm_libs})
