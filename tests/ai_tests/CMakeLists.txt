# XPE AI Module Tests
# Google Test suite for xpe_ai.dll -- SPEC-XPE-P3-AI
# Coverage target: >=85% statement
#
# Test files:
#   1. test_ai_abi.cpp              -- C ABI smoke tests (version, init, shutdown)
#   2. test_ai_fallback.cpp         -- Fallback routing and stub behavior
#   3. test_ai_model_card.cpp       -- Model Card JSON API
#   4. test_ai_worker_isolation.cpp -- Process isolation and crash handling (stub)
#
# REQ-AI-001 through REQ-AI-012 (SPEC-XPE-P3-AI v1.1)

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
set(AI_TEST_SOURCES
    test_ai_abi.cpp
    test_ai_fallback.cpp
    test_ai_model_card.cpp
    test_ai_worker_isolation.cpp
    test_ai_ipc_bridge.cpp  # T-001: Named pipe IPC bridge
    test_ai_model_versioning.cpp  # T-004: Model versioning and metadata
)

# Use xpe_ai_tests to match module-level CMakeLists.txt expectation
add_executable(xpe_ai_tests ${AI_TEST_SOURCES})

# ---------------------------------------------------------------------------
# Google Test discovery
# ---------------------------------------------------------------------------
if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

target_link_libraries(xpe_ai_tests
    PRIVATE
        xpe_ai
        xpe_common
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(xpe_ai_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/ai/include
        ${CMAKE_SOURCE_DIR}/modules/common/include
)

target_compile_features(xpe_ai_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(xpe_ai_tests PRIVATE /W4 /WX- /wd4996)
endif()

# CTest discovery
include(GoogleTest)
gtest_discover_tests(xpe_ai_tests
    PROPERTIES
        LABELS "unit;ai;SPEC-XPE-P3-AI"
        TIMEOUT 30
)

# ---------------------------------------------------------------------------
# Coverage Configuration (Windows / OpenCPPCoverage)
# ---------------------------------------------------------------------------
if(WIN32 AND ENABLE_COVERAGE)
    find_program(OpenCPPCoverage OpenCPPCoverage)
    if(OpenCPPCoverage)
        add_custom_target(coverage_ai
            COMMAND ${OpenCPPCoverage}
                --sources ${CMAKE_SOURCE_DIR}/modules/ai/src
                --modules ${CMAKE_BINARY_DIR}/modules/ai
                --export_type cobertura:coverage_ai.xml
                -- $<TARGET_FILE:xpe_ai_tests>
            COMMENT "Running coverage analysis for xpe_ai..."
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
        )
    endif()
endif()
