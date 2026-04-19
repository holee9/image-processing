cmake_minimum_required(VERSION 3.20)
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
option(BUILD_PREPROCESS "Build preprocess module" OFF)
option(BUILD_ENHANCE_BASIC "Build basic enhancement module" OFF)
option(BUILD_ENHANCE_ADVANCED "Build advanced enhancement module (SPEC-P2-ADV)" ON)
option(BUILD_AI "Build AI module" ON)
option(BUILD_DISPLAY "Build display module" ON)
option(BUILD_DICOM "Build DICOM module" ON)
option(XPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_COVERAGE "Enable code coverage instrumentation (GCC/Clang only)" OFF)

# Platform detection
include(cmake/Platform.cmake)
include(cmake/CompilerWarnings.cmake)
include(FetchContent)

# Coverage instrumentation
# On Windows with MSVC, use OpenCppCoverage externally:
#   OpenCppCoverage --sources <src> -- ctest --preset default
if(BUILD_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Coverage: adding --coverage flags (GCC/Clang)")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage: BUILD_COVERAGE=ON is only supported on GCC/Clang. "
                        "On MSVC/Windows use OpenCppCoverage externally.")
    endif()
endif()

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

# Eigen 3.4.x for matrix operations (REQ-ADV-001, REQ-ADV-010)
set(XPE_NEEDS_EIGEN ${BUILD_ENHANCE_ADVANCED})
if(BUILD_GSVG)
    set(XPE_NEEDS_EIGEN TRUE)
endif()

if(XPE_NEEDS_EIGEN)
    find_package(Eigen3 CONFIG QUIET)
endif()

if(XPE_NEEDS_EIGEN AND NOT Eigen3_FOUND)
    FetchContent_Declare(
        eigen
        URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
    )

    # Eigen is header-only, so we just need to make it available
    FetchContent_MakeAvailable(eigen)
endif()

# Google Test (via vcpkg)
if(BUILD_TESTS)
    find_package(GTest CONFIG)
    include(GoogleTest)
    enable_testing()
endif()

# XPE modules (Layer 0 + Layer 1)
add_subdirectory(modules/common)

if(BUILD_PREPROCESS)
    xpe_add_optional_subdirectory(modules/preprocess)
endif()

if(BUILD_ENHANCE_BASIC)
    xpe_add_optional_subdirectory(modules/enhance_basic)
endif()

if(BUILD_ENHANCE_ADVANCED)
    xpe_add_optional_subdirectory(modules/enhance_advanced)
endif()

if(BUILD_AI)
    xpe_add_optional_subdirectory(modules/ai)
endif()

if(BUILD_DISPLAY)
    xpe_add_optional_subdirectory(modules/display)
endif()

if(BUILD_DICOM)
    xpe_add_optional_subdirectory(modules/dicom)
endif()

# GSVG module (Layer 1-G, independent)
if(BUILD_GSVG)
    xpe_add_optional_subdirectory(modules/gsvg)
endif()

# Tests
if(BUILD_TESTS)
    xpe_add_optional_subdirectory(tests)
endif()
