cmake_minimum_required(VERSION 3.25)
project(xpe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared library output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# DLL export macro
add_compile_definitions($<$<BOOL:${BUILD_SHARED_LIBS}>:XPE_DLL_EXPORT>)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)" ON)
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_GSVG "Build GSVG module (independent)" ON)
option(XPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Platform detection
include(cmake/Platform.cmake)
include(cmake/CompilerWarnings.cmake)
include(FetchContent)

function(xpe_add_optional_subdirectory relative_dir)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${relative_dir}/CMakeLists.txt")
        add_subdirectory(${relative_dir})
    else()
        message(STATUS "Skipping missing subdirectory: ${relative_dir}")
    endif()
endfunction()

# Find dependencies via vcpkg when available; otherwise use pinned FetchContent
# sources so CI is not coupled to vcpkg's tool acquisition path.
find_package(spdlog CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)
find_package(fmt CONFIG QUIET)

if(NOT spdlog_FOUND OR NOT nlohmann_json_FOUND OR NOT fmt_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(spdlog nlohmann_json fmt)
endif()

# XPE modules (Layer 0 + Layer 1)
add_subdirectory(modules/common)
xpe_add_optional_subdirectory(modules/preprocess)
xpe_add_optional_subdirectory(modules/enhance_basic)
xpe_add_optional_subdirectory(modules/enhance_advanced)
xpe_add_optional_subdirectory(modules/ai)
xpe_add_optional_subdirectory(modules/display)
xpe_add_optional_subdirectory(modules/dicom)

# GSVG module (Layer 1-G, independent)
if(BUILD_GSVG)
    xpe_add_optional_subdirectory(gsvg)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    xpe_add_optional_subdirectory(tests)
endif()
