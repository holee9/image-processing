add_library(gsvg
    src/gsvg.cpp
)

target_include_directories(gsvg
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(gsvg
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(gsvg PROPERTIES
    OUTPUT_NAME "gsvg"
    VERSION ${PROJECT_VERSION}
)

if(MSVC)
    target_compile_options(gsvg PRIVATE /W4 /WX-)
endif()

set(_XPE_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_BUILD_TESTS ON)
endif()

if(_XPE_BUILD_TESTS)
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

    add_executable(gsvg_tests
        tests/test_gsvg_benchmark.cpp
        tests/test_gsvg_degraded.cpp
    )

    target_include_directories(gsvg_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(gsvg_tests PRIVATE
        gsvg
        GTest::gtest_main
    )
    target_compile_features(gsvg_tests PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(gsvg_tests PRIVATE /W4 /WX-)
    endif()

    gtest_discover_tests(gsvg_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
