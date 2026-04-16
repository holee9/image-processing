add_library(xpe_common SHARED
    src/xpe_common.cpp
    src/xpe_memory.cpp
)

target_compile_definitions(xpe_common
    PRIVATE XPE_DLL_EXPORT
)

target_include_directories(xpe_common
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_common
    PRIVATE spdlog::spdlog
            nlohmann_json::nlohmann_json
            fmt::fmt
)

set_target_properties(xpe_common PROPERTIES
    OUTPUT_NAME "xpe_common"
    VERSION ${PROJECT_VERSION}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ==========================================================================
# Unit Tests (Google Test)
# ==========================================================================

set(_XPE_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_BUILD_TESTS ON)
endif()

if(_XPE_BUILD_TESTS)
    # Resolve GTest: prefer already-available target, fall back to FetchContent
    if(NOT TARGET GTest::gtest_main)
        find_package(GTest QUIET)
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

    include(GoogleTest)

    # --- test_xpe_common (18 API functions) ---
    add_executable(test_xpe_common
        tests/test_xpe_common.cpp
    )
    target_include_directories(test_xpe_common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_xpe_common PRIVATE
        xpe_common
        GTest::gtest_main
    )
    target_compile_features(test_xpe_common PRIVATE cxx_std_17)
    gtest_discover_tests(test_xpe_common
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
