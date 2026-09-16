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
option(BUILD_TESTS "Build unit and integration tests" OFF)
option(BUILD_GSVG "Build GSVG module (independent)" ON)
option(BUILD_PREPROCESS "Build preprocess module" OFF)
option(BUILD_ENHANCE_BASIC "Build basic enhancement module" OFF)
option(BUILD_ENHANCE_ADVANCED "Build advanced enhancement module (SPEC-P2-ADV)" ON)
option(BUILD_AI "Build AI module" ON)
option(BUILD_DISPLAY "Build display module" ON)
option(BUILD_DICOM "Build DICOM module" OFF)
option(XPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BUILD_COVERAGE "Enable code coverage instrumentation (GCC/Clang only)" OFF)

# Platform detection
include(cmake/Platform.cmake)
include(cmake/CompilerWarnings.cmake)
include(FetchContent)

# Coverage instrumentation (GCC/Clang). The `coverage` / `coverage_check`
# targets for every platform live in cmake/XpeCoverage.cmake, included at the
# end of this file once all module test targets exist (REQ-P0-006, #113).
if(BUILD_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Coverage: adding --coverage flags (GCC/Clang)")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
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
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()
    # Eigen is header-only. We create an INTERFACE target manually to avoid
    # running Eigen's own CMakeLists.txt, which requires cmake_minimum_required
    # < 3.5 and is rejected as a hard error by CMake 4.0 (windows-2025 runner).
    if(NOT TARGET Eigen3::Eigen)
        add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
        target_include_directories(Eigen3::Eigen INTERFACE "${eigen_SOURCE_DIR}")
    endif()
endif()

# Google Test (via vcpkg or FetchContent in tests/CMakeLists.txt)
if(BUILD_TESTS)
    find_package(GTest CONFIG)
    if(GTest_FOUND)
        include(GoogleTest)
    endif()
    enable_testing()
endif()

# --------------------------------------------------------------------------
# Orphan-source check (QA-A-77/A-78, #160)
#
# A .cpp under modules/<m>/src/ that nobody listed in that module's
# CMakeLists.txt compiles nowhere and fails nothing. Two were found this way:
# simd_dispatch.cpp, 340 lines carrying seven XPE_EXPORT functions, which had
# never been compiled in any commit in the repository's history and which
# declared "405/405 parity checks passing" in the same commit that added it;
# and mode_selector.cpp, whose PUBLIC header shipped four declarations with no
# implementation behind them. Neither showed up as a build failure, because a
# file outside the source list is not built rather than built wrong.
#
# This runs at CONFIGURE time on purpose, not in CI. A CI red light is a place
# a reader may never go - this repository had a workflow red for two days with
# nobody reading it - while a configure warning is printed in front of whoever
# is about to build. It starts as a WARNING; once the count has held at zero,
# promote it to FATAL_ERROR.
#
# Known blind spot, stated rather than discovered later: this compares SOURCES
# only. A header with no implementation - the sharper signal in the
# mode_selector case - is invisible here and needs its own check.
set(XPE_ORPHAN_SOURCES "")
file(GLOB XPE_MODULE_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/modules/*")
foreach(_mod_rel IN LISTS XPE_MODULE_DIRS)
    set(_mod "${CMAKE_CURRENT_SOURCE_DIR}/${_mod_rel}")
    if(NOT IS_DIRECTORY "${_mod}" OR NOT EXISTS "${_mod}/CMakeLists.txt" OR NOT IS_DIRECTORY "${_mod}/src")
        continue()
    endif()
    # QA-A-79: match against CODE only. string(FIND) is a substring test, so a
    # filename appearing anywhere in the file counts as "listed" - including inside
    # a comment. That is not hypothetical here: QA-A-76 corrected two stale TODOs in
    # modules/preprocess/CMakeLists.txt and deliberately quoted the old text, which
    # named files. A check that a comment can silence is a check that goes quiet
    # exactly when someone is writing about the thing it watches.
    file(STRINGS "${_mod}/CMakeLists.txt" _cml_lines)
    set(_cml "")
    # QA-A-80: strip from '#' to end of line, not just whole-comment lines. The
    # first fix only dropped lines that BEGIN with '#', so a trailing note -
    # `target_sources(...)  # mode_selector.cpp was deleted` - still silenced the
    # check on that filename. Same hazard, different spelling.
    foreach(_line IN LISTS _cml_lines)
        string(REGEX REPLACE "#.*$" "" _line "${_line}")
        string(APPEND _cml "${_line} ")
    endforeach()
    file(GLOB _srcs "${_mod}/src/*.cpp")
    foreach(_src IN LISTS _srcs)
        get_filename_component(_base "${_src}" NAME)
        string(FIND "${_cml}" "${_base}" _found)
        if(_found EQUAL -1)
            list(APPEND XPE_ORPHAN_SOURCES "${_mod_rel}/src/${_base}")
        endif()
    endforeach()
endforeach()
list(LENGTH XPE_ORPHAN_SOURCES XPE_ORPHAN_COUNT)
if(XPE_ORPHAN_COUNT GREATER 0)
    message(WARNING
        "${XPE_ORPHAN_COUNT} source file(s) under modules/*/src/ are named in no module CMakeLists.txt. "
        "They are not built, so nothing they contain is checked by any build or test:
"
        "  ${XPE_ORPHAN_SOURCES}
"
        "Either add each to its module's source list or delete it. Do not leave it here - "
        "a reader takes an uncompiled file for working code, and its declarations for an API.")
else()
    message(STATUS "Orphan-source check: 0 unlisted sources under modules/*/src/")
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

# Tests are registered by each module (modules/*/CMakeLists.txt); there is no
# tests/ aggregator. The former tests/CMakeLists.txt was retired in #113: it was
# never reachable and duplicated module targets (xpe_ai_tests, test_enhance_advanced).
# tests/e2e_post_pipeline is the one directory the root adds directly (below).

# E2E integration tests — requires full post-processing stack (ci-post preset).
# Guard prevents link failure on ci-common/ci-preprocess where post modules are OFF.
if(BUILD_TESTS AND BUILD_ENHANCE_BASIC AND BUILD_ENHANCE_ADVANCED AND BUILD_GSVG AND BUILD_DISPLAY)
    add_subdirectory(tests/e2e_post_pipeline)
endif()

# Coverage targets — after every module (and its tests) is registered.
include(cmake/XpeCoverage.cmake)
