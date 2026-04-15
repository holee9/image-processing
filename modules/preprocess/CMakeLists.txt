cmake_minimum_required(VERSION 3.20)

# ============================================================================
# xpe_preprocess — SPEC-XPE-P1A  IEC 62304 Class B
# ============================================================================

include(FetchContent)

# ---------------------------------------------------------------------------
# Google Test (for unit/integration tests)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Library sources (9 SWUs + helpers)
# ---------------------------------------------------------------------------
set(XPE_PREPROCESS_SOURCES
    src/preprocess.cpp           # version + legacy entry point
    src/offset_correct.cpp       # SWU-1.1 REQ-P1A-009..011
    src/gain_correct.cpp         # SWU-1.2 REQ-P1A-016..019
    src/readout_validate.cpp     # SWU-1.9 REQ-P1A-001..004
    src/temp_compensate.cpp      # SWU-1.6 REQ-P1A-005..008
    src/nonlinearity_correct.cpp # SWU-1.7 REQ-P1A-012..015
    src/binning_correct.cpp      # SWU-1.8 REQ-P1A-020..023
    src/defect_correct.cpp       # SWU-1.3 REQ-P1A-024..028
    src/ghost_correct.cpp        # SWU-1.4 REQ-P1A-029..034
    src/calibration_manager.cpp  # SWU-1.5 REQ-P1A-035..040
    src/helpers.cpp              # Shared: interpolation, JSON, CRC-32
)

add_library(xpe_preprocess SHARED
    ${XPE_PREPROCESS_SOURCES}
)

target_include_directories(xpe_preprocess
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include  # for internal header
)

target_link_libraries(xpe_preprocess
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

target_compile_features(xpe_preprocess PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(xpe_preprocess PRIVATE /W4 /WX)
else()
    target_compile_options(xpe_preprocess PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

set_target_properties(xpe_preprocess PROPERTIES
    OUTPUT_NAME "xpe_preprocess"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ---------------------------------------------------------------------------
# Unit and Integration Tests (Google Test)
# ---------------------------------------------------------------------------
if (BUILD_TESTING)
    enable_testing()
    include(GoogleTest)

    set(XPE_TEST_SOURCES
        tests/test_offset_correct.cpp
        tests/test_gain_correct.cpp
        tests/test_readout_validate.cpp
        tests/test_temp_nonlinearity_binning.cpp
        tests/test_defect_correct.cpp
        tests/test_ghost_correct.cpp
        tests/test_calibration_manager.cpp
        tests/test_integration.cpp
        tests/test_boundary.cpp
    )

    add_executable(xpe_preprocess_tests
        ${XPE_TEST_SOURCES}
    )

    target_include_directories(xpe_preprocess_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(xpe_preprocess_tests
        PRIVATE xpe_preprocess
                GTest::gtest_main
    )

    target_compile_features(xpe_preprocess_tests PRIVATE cxx_std_17)

    gtest_discover_tests(xpe_preprocess_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
