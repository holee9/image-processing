cmake_minimum_required(VERSION 3.20)
project(xpe_preprocess VERSION 0.1.0 LANGUAGES CXX)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTING "Build tests" ON)
option(XPE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)  # Disabled for testing

# Dependencies: xpe_common (types, error codes, logging)
# Note: xpe_common is built from parent CMakeLists.txt, no need to find_package

# Build xpe_preprocess library
add_library(xpe_preprocess SHARED
    src/xpe_preprocess.cpp       # Phase 1: Lifecycle
    src/xpe_calibration.cpp      # Phase 2: Calibration loading
    src/xpe_offset.cpp           # Phase 3: Offset correction
    src/xpe_gain.cpp             # Phase 3: Gain correction
    src/xpe_defect.cpp           # Phase 3: Defect correction
)

target_include_directories(xpe_preprocess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xpe_preprocess
    PUBLIC
        xpe_common
)

# Set DLL export for Windows
if(WIN32)
    target_compile_definitions(xpe_preprocess PUBLIC XPE_DLL_EXPORT)
endif()

# Warnings
if(MSVC)
    target_compile_options(xpe_preprocess PRIVATE /W4 /WX)
else()
    target_compile_options(xpe_preprocess PRIVATE -Wall -Wextra -Werror)
endif()

# Testing
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    enable_testing()

    # Find Google Test
    find_package(GTest REQUIRED)

    add_executable(test_xpe_preprocess
        tests/test_xpe_preprocess.cpp
    )

    target_link_libraries(test_xpe_preprocess
        PRIVATE
            xpe_preprocess
            GTest::gtest
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(test_xpe_preprocess)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS xpe_preprocess
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
