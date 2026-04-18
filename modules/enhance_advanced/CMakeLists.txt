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

# Placeholder version function
function(create_placeholder_version_function MODULE_NAME)
    set(VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
    file(WRITE "${VERSION_SOURCE}"
"#include <xpe/common/xpe_common_api.h>

extern \"C\" XPE_API const char* ${MODULE_NAME}_version() {
    return \"0.1.0-placeholder\";
}
")
    target_sources(${MODULE_NAME} PRIVATE "${VERSION_SOURCE}")
endfunction()

create_placeholder_version_function(${MODULE_NAME})

# ============================================================================
# Tests (T-015: Google Test scaffolding)
# ============================================================================

if(BUILD_TESTS)
    # Find Google Test
    find_package(GTest REQUIRED)

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
