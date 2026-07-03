cmake_minimum_required(VERSION 3.21)

project(OptionalPtr
    VERSION 1.0.0
    DESCRIPTION "A safe, std::optional-like wrapper around raw C++ pointers"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(optional_ptr INTERFACE)
add_library(OptionalPtr::optional_ptr ALIAS optional_ptr)

target_include_directories(optional_ptr INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_features(optional_ptr INTERFACE cxx_std_20)

# Everything below (generator enforcement, compile_commands.json, tests) only
# applies when this repo is built directly, not when it is pulled in via
# FetchContent by another project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        message(FATAL_ERROR
            "OptionalPtr must be configured with the Ninja generator, e.g.:\n"
            "  cmake -S . -B build -G Ninja\n"
            "Got generator: '${CMAKE_GENERATOR}'."
        )
    endif()

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # CMAKE_BINARY_DIR is build/Debug or build/Release depending on which
    # config was configured, so compile_commands.json ends up next to it.
    # clangd needs one stable path regardless of config, and it must survive
    # a `rm -rf build`, so copy the freshly generated file up to the source
    # root (not into build/, which is disposable) on every build.
    add_custom_target(propagate_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
        COMMENT "Propagating compile_commands.json to project root"
        VERBATIM
    )

    option(OPTIONAL_PTR_BUILD_TESTS "Build optional_ptr tests" ON)

    if(OPTIONAL_PTR_BUILD_TESTS)
        enable_testing()
        add_subdirectory(deps)
        add_subdirectory(tests)
    endif()
endif()
