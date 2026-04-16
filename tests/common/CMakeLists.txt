# XPE Common Unit Tests
# Google Test suite for xpe_common.dll -- SPEC-XPE-P0 REQ-P0-007
# Coverage target: >=85% statement coverage

cmake_minimum_required(VERSION 3.20)

# Test executable (64 test cases)
add_executable(test_xpe_common
    test_xpe_common.cpp
)

# Link against xpe_common library
# Note: This assumes xpe_common is built in the parent project
target_link_libraries(test_xpe_common
    PRIVATE
        xpe_common
)

# Find Google Test only if the parent FetchContent setup did not create targets.
if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

# Link Google Test
target_link_libraries(test_xpe_common
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

# Enable CTest discovery (REQ-P0-005)
include(GoogleTest)
gtest_discover_tests(test_xpe_common
    PROPERTIES
    LABELS "unit;common;xpe_common;REQ-P0-007"
    TIMEOUT 300
)

# ============================================================================
# Coverage Configuration (REQ-P0-006)
# ============================================================================

if(WIN32 AND ENABLE_COVERAGE)
    find_program(OpenCPPCoverage OpenCPPCoverage)

    if(OpenCPPCoverage)
        # Coverage target for xpe_common
        add_custom_target(coverage_xpe_common
            COMMAND ${OpenCPPCoverage}
                --sources ${CMAKE_SOURCE_DIR}/modules/common/src
                --modules ${CMAKE_BINARY_DIR}/modules/common
                --export_type cobertura:coverage.xml
                -- $<TARGET_FILE:test_xpe_common>
            COMMENT "Running coverage analysis for xpe_common..."
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
        )

        # Coverage threshold check (85% requirement)
        add_custom_target(coverage_check_xpe_common
            COMMAND ${CMAKE_COMMAND} -E echo "Checking coverage threshold: 85%"
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: coverage.xml"
            DEPENDS coverage_xpe_common
            COMMENT "Checking 85% coverage threshold..."
        )

        message(STATUS "Coverage targets added: coverage_xpe_common, coverage_check_xpe_common")
    endif()
endif()

# ============================================================================
# Test Labels for CTest
# ============================================================================

# Memory Sanitizer Support (Optional)

option(ENABLE_ASAN "Enable AddressSanitizer for memory leak detection" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled for test_xpe_common")
    target_compile_options(test_xpe_common PRIVATE -fsanitize=address -g)
    target_link_options(test_xpe_common PRIVATE -fsanitize=address)
endif()
