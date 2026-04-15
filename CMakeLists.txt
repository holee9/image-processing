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
option(BUILD_COVERAGE "Enable code coverage instrumentation (GCC/Clang only)" OFF)

# Platform detection
include(cmake/Platform.cmake)
include(cmake/CompilerWarnings.cmake)

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

# Find dependencies via vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Google Test (via vcpkg)
if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    enable_testing()
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
    xpe_add_optional_subdirectory(modules/gsvg)
endif()

# Tests
if(BUILD_TESTS)
    xpe_add_optional_subdirectory(tests)
endif()
