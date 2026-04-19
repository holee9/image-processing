cmake_minimum_required(VERSION 3.20)

# ============================================================================
# xpe_enhance_basic — SPEC-XPE-P1B-ENH  IEC 62304 Class B
# ============================================================================

# When built as a standalone project (not via add_subdirectory from root),
# declare a project so PROJECT_VERSION is defined.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xpe_enhance_basic VERSION 1.0.0 LANGUAGES C CXX)
    option(BUILD_TESTING "Build unit tests" ON)
endif()

# ---------------------------------------------------------------------------
# Library sources (5 SWUs + module entry)
# ---------------------------------------------------------------------------
set(XPE_ENHANCE_BASIC_SOURCES
    src/enhance_basic.cpp        # version entry point
    src/log_transform.cpp        # SWU-2.1  REQ-ENH-001..006
    src/noise_reduce.cpp         # SWU-2.2  REQ-ENH-007..012
    src/contrast_enhance.cpp     # SWU-2.3  REQ-ENH-013..017
    src/edge_enhance.cpp         # SWU-2.4  REQ-ENH-018..022
    src/exposure_index.cpp       # SWU-2.10 REQ-ENH-023..030
)

add_library(xpe_enhance_basic SHARED
    ${XPE_ENHANCE_BASIC_SOURCES}
)

target_include_directories(xpe_enhance_basic
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xpe_enhance_basic
    PUBLIC xpe_common
)

target_compile_features(xpe_enhance_basic PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(xpe_enhance_basic PRIVATE /W4)
    # /arch:AVX2 + /fp:fast together enable SVML auto-vectorization for
    # transcendental functions (exp, log) in MSVC, giving ~8x throughput for
    # log_transform and bilateral filter loops.  /arch:AVX2 alone is NOT
    # sufficient — MSVC requires /fp:fast to relax IEEE 754 semantics and
    # substitute scalar CRT calls with vectorized SVML intrinsics.
    # Haswell+ (2013) CPUs support AVX2; required as system prerequisite per
    # SPEC-XPE-P1B-ENH Section 3.1.
    target_compile_options(xpe_enhance_basic PRIVATE /arch:AVX2 /fp:fast)
else()
    target_compile_options(xpe_enhance_basic PRIVATE
        -Wall -Wextra -Wpedantic -mavx2
    )
endif()

target_compile_definitions(xpe_enhance_basic PRIVATE XPE_DLL_EXPORT)

# VERSION/SOVERSION only meaningful on Linux; guard for portability
if(PROJECT_VERSION)
    set_target_properties(xpe_enhance_basic PROPERTIES
        OUTPUT_NAME "xpe_enhance_basic"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   1
    )
else()
    set_target_properties(xpe_enhance_basic PROPERTIES
        OUTPUT_NAME "xpe_enhance_basic"
    )
endif()

# ---------------------------------------------------------------------------
# Unit and Integration Tests (Google Test)
# ---------------------------------------------------------------------------
set(_XPE_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_BUILD_TESTS ON)
endif()

if(_XPE_BUILD_TESTS)
    # Resolve GTest: prefer already-available target (vcpkg / parent project),
    # then fall back to FetchContent for standalone builds.
    if(NOT TARGET GTest::gtest_main)
        find_package(GTest QUIET)
    endif()
    if(NOT TARGET GTest::gtest_main)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    include(GoogleTest)

    set(XPE_TEST_SOURCES
        tests/test_log_transform.cpp
        tests/test_noise_reduce.cpp
        tests/test_contrast_enhance.cpp
        tests/test_edge_enhance.cpp
        tests/test_exposure_index.cpp
        tests/test_enhance_integration.cpp
        tests/test_enh01_log_noise.cpp   # SPRINT-P1B-ENH-01 integration tests
    )

    add_executable(xpe_enhance_basic_tests
        ${XPE_TEST_SOURCES}
    )

    target_include_directories(xpe_enhance_basic_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(xpe_enhance_basic_tests
        PRIVATE xpe_enhance_basic
                GTest::gtest_main
    )

    target_compile_features(xpe_enhance_basic_tests PRIVATE cxx_std_17)

    gtest_discover_tests(xpe_enhance_basic_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
