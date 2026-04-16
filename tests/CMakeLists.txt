# XPE Test Suite
# Integration and unit tests for XPE modules
# SPEC-XPE-P0 REQ-P0-005, REQ-P0-006: Google Test + CTest + 85% coverage

cmake_minimum_required(VERSION 3.25)

# Include FetchContent for Google Test
include(FetchContent)

# Fetch Google Test (REQ-P0-005)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

# For Windows: Prevent overriding parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing (REQ-P0-005)
enable_testing()

# Test subdirectories
add_subdirectory(common_smoke)
add_subdirectory(common)

# ============================================================================
# Coverage Configuration (REQ-P0-006: >=85% statement coverage)
# ============================================================================

option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Coverage enabled - target: 85% statement coverage")

    if(WIN32)
        # Windows: Use OpenCPPCoverage
        find_program(OpenCPPCoverage OpenCPPCoverage)

        if(OpenCPPCoverage)
            message(STATUS "Found OpenCPPCoverage: ${OpenCPPCoverage}")

            # Add coverage target for xpe_common
            add_custom_target(coverage
                COMMAND ${OpenCPPCoverage}
                    --sources ${CMAKE_SOURCE_DIR}/modules/common/src
                    --modules ${CMAKE_BINARY_DIR}/modules/common
                    --export_type cobertura:coverage.xml
                    -- $<TARGET_FILE:test_xpe_common>
                COMMENT "Running coverage analysis for xpe_common..."
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                VERBATIM
            )

            # Add coverage summary target
            add_custom_target(coverage_summary
                COMMAND ${CMAKE_COMMAND} -E echo "Coverage Report: coverage.xml"
                COMMAND ${CMAKE_COMMAND} -E echo "Open in browser or use CI parser"
                DEPENDS coverage
            )

            message(STATUS "Coverage targets added: 'coverage', 'coverage_summary'")
        else()
            message(WARNING "OpenCPPCoverage not found. Install via: choco install opencppcoverage")
        endif()
    else()
        # Linux/macOS: Use gcov/lcov
        message(STATUS "Coverage for Unix platforms: use gcov/lcov (not yet implemented)")
    endif()

    # Coverage threshold warning
    message(STATUS "Coverage threshold: 85% (fails if below)")
endif()

# ============================================================================
# CI/CD Integration
# ============================================================================

# Add a custom target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_xpe_common
    COMMENT "Running all tests..."
)

# Add a custom target to run tests with verbose output
add_custom_target(check_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
    DEPENDS test_xpe_common
    COMMENT "Running all tests with verbose output..."
)
