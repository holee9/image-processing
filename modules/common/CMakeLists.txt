cmake_minimum_required(VERSION 3.20)
project(xpe_common VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard (inherited from root, removed override to allow propagation)
# Note: Root CMakeLists.txt sets C++17, this module inherits that setting
# set(CMAKE_CXX_STANDARD 23)  # REMOVED: SPEC-XPE-P0 requires C++17

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTING "Build tests" ON)
option(XPE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

# Fetch dependencies only when this module is configured standalone.
if(NOT TARGET spdlog::spdlog OR NOT TARGET nlohmann_json::nlohmann_json OR NOT TARGET fmt::fmt)
    include(FetchContent)

    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(spdlog nlohmann_json fmt)
endif()

# Build xpe_common library
add_library(xpe_common
    src/xpe_common.cpp
    src/xpe_memory.cpp
    src/xpe_logging.cpp
    src/xpe_aed.cpp
)

target_include_directories(xpe_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_common
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        fmt::fmt
)

# Set DLL export for Windows
if(WIN32)
    target_compile_definitions(xpe_common PUBLIC XPE_DLL_EXPORT)
endif()

# Warnings
if(MSVC)
    target_compile_options(xpe_common PRIVATE /W4 /WX)
else()
    target_compile_options(xpe_common PRIVATE -Wall -Wextra -Werror)
endif()

# Testing
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    enable_testing()

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )

    # For Windows: Prevent overriding parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(test_xpe_common
        tests/test_xpe_common.cpp
    )

    target_link_libraries(test_xpe_common
        PRIVATE
        xpe_common
        GTest::gtest
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(test_xpe_common)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS xpe_common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
