# XPE Post-Processing E2E Integration Tests
# Full pipeline performance: Pre → Post → Display → DICOM

cmake_minimum_required(VERSION 3.20)

# Google Test dependency
if(NOT TARGET GTest::gtest_main)
    find_package(GTest QUIET CONFIG)
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

# E2E Test executable
add_executable(test_e2e_post_pipeline
    test_e2e_full_pipeline.cpp
)

# Include directories for all required modules
target_include_directories(test_e2e_post_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/common/include
    ${CMAKE_SOURCE_DIR}/modules/enhance_basic/include
    ${CMAKE_SOURCE_DIR}/modules/enhance_advanced/include
    ${CMAKE_SOURCE_DIR}/modules/gsvg/include
    ${CMAKE_SOURCE_DIR}/modules/display/include
)

# Link against all required post-processing modules
target_link_libraries(test_e2e_post_pipeline PRIVATE
    GTest::gtest_main
    xpe_common
    xpe_enhance_basic
    xpe_enhance_advanced
    gsvg
    xpe_display
)

# Enable C++17
target_compile_features(test_e2e_post_pipeline PRIVATE cxx_std_17)

# MSVC warnings
if(MSVC)
    target_compile_options(test_e2e_post_pipeline PRIVATE /W4 /WX-)
endif()

# Discover tests
include(GoogleTest)
gtest_discover_tests(test_e2e_post_pipeline)

message(STATUS "E2E Post-Processing Pipeline test configured")
