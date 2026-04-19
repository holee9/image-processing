# XPE Enhance Advanced Module Tests
# Google Test suite for xpe_enhance_advanced.dll -- SPEC-XPE-P2-ADV
# Coverage target: >=85% statement, >=80% branch
#
# Test files correspond to modules/enhance_advanced/CMakeLists.txt list:
#   1. test_api_header.cpp      -- P/Invoke ABI compliance, header compilation
#   2. test_lifecycle.cpp       -- init/shutdown/version, not-initialized guard
#   3. test_mfp_scalar.cpp      -- SWU-2.5 Multiscale Frequency Processing
#   4. test_collimation_detect.cpp -- SWU-2.8 Collimation ROI Detection
#   5. test_edge_enhancement.cpp   -- SWU-2.6 Fractional-Order Edge Enhancement
#   6. test_exposure_index.cpp     -- SWU-2.10 Exposure Index Calculation
#   7. test_integration.cpp     -- Cross-SWU pipeline, thread safety, perf

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Single unified test executable (mirrors modules/enhance_advanced/CMakeLists.txt)
# ---------------------------------------------------------------------------
set(ENHANCE_ADV_TEST_SOURCES
    test_api_header.cpp
    test_lifecycle.cpp
    test_mfp_scalar.cpp
    test_collimation_detect.cpp
    test_edge_enhancement.cpp
    test_exposure_index.cpp
    test_integration.cpp
)

add_executable(test_enhance_advanced ${ENHANCE_ADV_TEST_SOURCES})

# ---------------------------------------------------------------------------
# Google Test discovery
# ---------------------------------------------------------------------------
if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

target_link_libraries(test_enhance_advanced
    PRIVATE
        xpe_enhance_advanced
        GTest::gtest
        GTest::gtest_main
)

# Include module internals for white-box config parser tests
target_include_directories(test_enhance_advanced
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/enhance_advanced/include
        ${CMAKE_SOURCE_DIR}/modules/enhance_advanced/src
        ${CMAKE_SOURCE_DIR}/modules/common/include
)

target_compile_features(test_enhance_advanced PRIVATE cxx_std_17)

# CTest discovery
include(GoogleTest)
gtest_discover_tests(test_enhance_advanced
    PROPERTIES
        LABELS "unit;enhance_advanced;SPEC-XPE-P2-ADV"
        TIMEOUT 300
)

# ---------------------------------------------------------------------------
# Coverage Configuration (Windows / OpenCPPCoverage)
# ---------------------------------------------------------------------------
if(WIN32 AND ENABLE_COVERAGE)
    find_program(OpenCPPCoverage OpenCPPCoverage)
    if(OpenCPPCoverage)
        add_custom_target(coverage_enhance_advanced
            COMMAND ${OpenCPPCoverage}
                --sources ${CMAKE_SOURCE_DIR}/modules/enhance_advanced/src
                --modules ${CMAKE_BINARY_DIR}/modules/enhance_advanced
                --export_type cobertura:coverage_enhance_advanced.xml
                -- $<TARGET_FILE:test_enhance_advanced>
            COMMENT "Running coverage analysis for xpe_enhance_advanced..."
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# AddressSanitizer Support
# ---------------------------------------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer for memory leak detection" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled for enhance_advanced tests")
    target_compile_options(test_enhance_advanced PRIVATE -fsanitize=address -g)
    target_link_options(test_enhance_advanced PRIVATE -fsanitize=address)
endif()
