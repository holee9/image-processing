add_library(gsvg
    src/gsvg.cpp
    src/grid_dwt.cpp
    # #180 (QA-B-91): virtual grid
    src/virtual_grid.cpp
    # #179 (QA-B-105): thread-count request storage
    src/thread_config.cpp
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
    target_compile_options(gsvg PRIVATE /W4)
endif()
xpe_target_warnings_as_errors(gsvg)

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
        tests/test_gsvg_coverage.cpp
        tests/test_gsvg_edge_cases.cpp
        tests/test_gsvg_extended_coverage.cpp
        tests/test_gsvg_abi_smoke.cpp
        # #142 (QA-B-52): guard-page probe for short src / gainMap buffers.
        tests/test_datasize_overread.cpp
        # #156 (QA-B-59): does each config flag reach the output
        tests/test_parameter_dependency.cpp
        # #145 (QA-B-60): unread config keys are named, valid configs stay quiet
        tests/test_config_warning.cpp
        # #180 (QA-B-89): synthetic grid generator, residual-energy and edge-MTF tools
        tests/test_grid_tools.cpp
        # #180 (QA-B-90): DWT grid suppression, measured with the tools above
        tests/test_grid_suppression.cpp
        # #180 (QA-B-91): virtual grid on self-consistent synthetic images
        tests/test_virtual_grid.cpp
        # #180 (QA-B-95): virtual grid against the MC-GPU phantoms (tests/data/mc)
        tests/test_virtual_grid_mc.cpp
        # #180 (QA-B-101): xpe_gsvg_process_ex result and reason codes
        tests/test_gsvg_result.cpp
        # #180 (QA-B-101): product virtual-grid table (data/) and the [grid] loader extension
        tests/test_vg_product_table.cpp
        # #179 (QA-B-105): output does not depend on the thread count
        tests/test_thread_determinism.cpp
        tests/test_mask_outside.cpp
        tests/test_thickness_range.cpp
        # grid_dwt.cpp is compiled into the test binary as well: its functions are
        # internal to gsvg.dll (not exported), and the tests call them directly.
        src/grid_dwt.cpp
        src/virtual_grid.cpp
        src/thread_config.cpp
    )

    target_include_directories(gsvg_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # #180 (QA-B-90): the grid suppression internals, driven directly by
        # test_grid_suppression.cpp for its falsification switches
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(gsvg_tests PRIVATE
        gsvg
        GTest::gtest_main
    )
    # Windows: psapi.lib is required for GetProcessMemoryInfo
    # (heap-growth endurance measurement, #105 G3)
    if(WIN32)
        target_link_libraries(gsvg_tests PRIVATE psapi)
    endif()

    target_compile_features(gsvg_tests PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(gsvg_tests PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(gsvg_tests)

    gtest_discover_tests(gsvg_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
