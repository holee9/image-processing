# XPE Preprocess Unit Tests
# REQ-P1A-013: Runtime Detection Hampel 5-Sigma
# Google Test suite with >= 85% coverage requirement

cmake_minimum_required(VERSION 3.20)

# Include Google Test
include(FetchContent)

# Fetch Google Test if not already available
if(NOT TARGET GTest::gtest_main)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

# Test sources for runtime detection
set(XPE_PREPROCESS_TEST_SOURCES
    test_runtime_detection.cpp
)

# Test sources for gain correction
set(XPE_GAIN_CORRECTION_TEST_SOURCES
    test_gain_correction.cpp
)

# Create test executable
add_executable(test_runtime_detection
    ${XPE_PREPROCESS_TEST_SOURCES}
)

target_include_directories(test_runtime_detection
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/common/include
        ${CMAKE_SOURCE_DIR}/modules/preprocess/include
)

target_link_libraries(test_runtime_detection
    PRIVATE
        xpe_common
        xpe_preprocess
        GTest::gtest_main
)

target_compile_features(test_runtime_detection PRIVATE cxx_std_17)

# Discover and run tests
gtest_discover_tests(test_runtime_detection
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create gain correction test executable
add_executable(test_gain_correction
    ${XPE_GAIN_CORRECTION_TEST_SOURCES}
)

target_include_directories(test_gain_correction
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/common/include
        ${CMAKE_SOURCE_DIR}/modules/preprocess/include
)

target_link_libraries(test_gain_correction
    PRIVATE
        xpe_common
        xpe_preprocess
        GTest::gtest_main
)

target_compile_features(test_gain_correction PRIVATE cxx_std_17)

# Discover and run gain correction tests
gtest_discover_tests(test_gain_correction
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Additional preprocess tests can be added here following the same pattern
# Example:
# add_executable(test_offset_correct tests/test_offset_correct.cpp)
# target_link_libraries(test_offset_correct PRIVATE xpe_preprocess GTest::gtest_main)
# gtest_discover_tests(test_offset_correct)
