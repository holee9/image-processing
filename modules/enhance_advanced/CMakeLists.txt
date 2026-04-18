# XPE Enhance Advanced Module
# Advanced image processing algorithms

cmake_minimum_required(VERSION 3.20)

# Module name
set(MODULE_NAME xpe_enhance_advanced)

# Shared library
add_library(${MODULE_NAME} SHARED
    src/xpe_enhance_advanced.cpp
    src/mfp_scalar.cpp
    src/fractional_derivative.cpp
    src/xpe_collimation_detect.cpp
    src/exposure_index.cpp
    src/detail/edge_detection.cpp
    src/detail/hough_transform.cpp
)

# C++ standard (inherited from root)
target_compile_features(${MODULE_NAME} PUBLIC cxx_std_17)

# Include directories
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link xpe_common dependency
target_link_libraries(${MODULE_NAME} PUBLIC
    xpe_common
)

# Link Eigen3 for matrix operations (REQ-ADV-001, REQ-ADV-010)
if(TARGET Eigen3::Eigen)
    target_link_libraries(${MODULE_NAME} PUBLIC Eigen3::Eigen)
elseif(TARGET eigen)
    # Fallback for FetchContent Eigen
    target_link_libraries(${MODULE_NAME} PUBLIC eigen)
endif()

# Export symbols + MSVC math macros (M_PI etc. require _USE_MATH_DEFINES)
target_compile_definitions(${MODULE_NAME} PRIVATE XPE_DLL_EXPORT _USE_MATH_DEFINES)

# Warnings-as-errors per-target (SPEC-P2-ADV S1 audit 6c)
# Applies to module only; avoids project-wide impact on other modules.
if(MSVC)
    target_compile_options(${MODULE_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${MODULE_NAME} PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter   # Eigen-heavy code triggers these spuriously
    )
endif()

# Note: xpe_enhance_advanced_version() is provided by src/xpe_enhance_advanced.cpp
# (real implementation returns "1.0.0"). The previous create_placeholder_version_function
# was removed to avoid LNK2005 (symbol multiply defined) — S1 build unblock.

# ============================================================================
# Tests (T-015: Google Test scaffolding)
# ============================================================================

if(BUILD_TESTS)
    # Google Test: reuse parent's target when available; otherwise
    # fall back to find_package, then FetchContent. Mirrors modules/common
    # pattern so the module works both in monorepo and standalone builds.
    if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
        find_package(GTest CONFIG QUIET)
    endif()

    if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
            GIT_SHALLOW TRUE
        )
        # Windows: prevent overriding parent project CRT settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    # Create test executable
    add_executable(test_${MODULE_NAME}
        tests/test_api_header.cpp
        tests/test_lifecycle.cpp
        tests/test_mfp_scalar.cpp
        tests/test_collimation_detect.cpp
        tests/test_edge_enhancement.cpp
        tests/test_exposure_index.cpp
        tests/test_integration.cpp
    )

    # Link against Google Test and the module
    target_link_libraries(test_${MODULE_NAME}
        PRIVATE
        GTest::gtest_main
        GTest::gtest
        ${MODULE_NAME}
    )

    # Include test directories
    target_include_directories(test_${MODULE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Enable C++17 for tests
    target_compile_features(test_${MODULE_NAME} PRIVATE cxx_std_17)

    # Discover tests automatically
    include(GoogleTest)
    gtest_discover_tests(test_${MODULE_NAME})

    message(STATUS "Enhance Advanced tests configured for ${MODULE_NAME}")
endif()
