cmake_minimum_required(VERSION 3.20)

# ============================================================================
# xpe_display — SPEC-XPE-P1B-DISP  IEC 62304 Class B
# ============================================================================

# When built as a standalone project (not via add_subdirectory from root),
# declare a project so PROJECT_VERSION is defined.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xpe_display VERSION 1.0.0 LANGUAGES C CXX)
    option(BUILD_TESTING "Build unit tests" ON)
endif()

# ---------------------------------------------------------------------------
# Library sources (SWU-3.1, SWU-3.2, SWU-3.3)
# ---------------------------------------------------------------------------
set(XPE_DISPLAY_SOURCES
    src/display.cpp           # version entry point
    src/display_helpers.cpp   # shared: format validation
    src/modality_lut.cpp      # SWU-3.1 REQ-DISP-001..008
    src/voi_lut.cpp           # SWU-3.2 REQ-DISP-009..018
    src/presentation_lut.cpp  # SWU-3.3 REQ-DISP-019..028
)

add_library(xpe_display SHARED
    ${XPE_DISPLAY_SOURCES}
)

target_include_directories(xpe_display
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xpe_display
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

target_compile_features(xpe_display PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(xpe_display PRIVATE /W4)
else()
    target_compile_options(xpe_display PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()
xpe_target_warnings_as_errors(xpe_display)

if(PROJECT_VERSION)
    set_target_properties(xpe_display PROPERTIES
        OUTPUT_NAME "xpe_display"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   1
    )
else()
    set_target_properties(xpe_display PROPERTIES
        OUTPUT_NAME "xpe_display"
    )
endif()

# ---------------------------------------------------------------------------
# Unit and Integration Tests (Google Test)
# ---------------------------------------------------------------------------
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

    # --- test_modality_lut (SWU-3.1: REQ-DISP-001..008) ---
    add_executable(test_display_modality_lut
        tests/test_modality_lut.cpp
    )
    target_include_directories(test_display_modality_lut PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_modality_lut PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_modality_lut PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_modality_lut PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_modality_lut)
    gtest_discover_tests(test_display_modality_lut
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_voi_lut (SWU-3.2: REQ-DISP-009..018) ---
    add_executable(test_display_voi_lut
        tests/test_voi_lut.cpp
    )
    target_include_directories(test_display_voi_lut PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_voi_lut PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_voi_lut PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_voi_lut PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_voi_lut)
    gtest_discover_tests(test_display_voi_lut
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_presentation_lut (SWU-3.3: REQ-DISP-019..028) ---
    add_executable(test_display_presentation_lut
        tests/test_presentation_lut.cpp
    )
    target_include_directories(test_display_presentation_lut PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_presentation_lut PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_presentation_lut PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_presentation_lut PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_presentation_lut)
    gtest_discover_tests(test_display_presentation_lut
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_display_parameter_dependency (#155 / QA-B-58) ---
    # Vary one parameter, hold the rest, require the output to move. This is the
    # assertion shape neither #154 nor #155 would have survived.
    add_executable(test_display_parameter_dependency
        tests/test_parameter_dependency.cpp
    )
    target_include_directories(test_display_parameter_dependency PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_parameter_dependency PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_parameter_dependency PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_parameter_dependency PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_parameter_dependency)
    gtest_discover_tests(test_display_parameter_dependency
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_display_gsdf_characterization (#142 / QA-B-57) ---
    # The existing GSDF case asserts monotonicity only; a linear ramp passes it.
    # These characterise the curve actually produced.
    add_executable(test_display_gsdf_characterization
        tests/test_gsdf_characterization.cpp
    )
    target_include_directories(test_display_gsdf_characterization PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_gsdf_characterization PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_gsdf_characterization PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_gsdf_characterization PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_gsdf_characterization)
    gtest_discover_tests(test_display_gsdf_characterization
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_display_datasize_overread (#142 / QA-B-52) ---
    # Short-dataSize probe with a guard page: every LUT entry point is called
    # with a buffer that faults past dataSize, so an over-read is observed
    # rather than reasoned about.
    add_executable(test_display_datasize_overread
        tests/test_datasize_overread.cpp
    )
    target_include_directories(test_display_datasize_overread PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_datasize_overread PRIVATE
        xpe_display
        GTest::gtest_main
    )
    target_compile_features(test_display_datasize_overread PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_datasize_overread PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_datasize_overread)
    gtest_discover_tests(test_display_datasize_overread
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_display_integration (REQ-DISP-029..035) ---
    add_executable(test_display_integration
        tests/test_display_integration.cpp
    )
    target_include_directories(test_display_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_display_integration PRIVATE
        xpe_display
        GTest::gtest_main
    )
    # Windows: psapi.lib is required for GetProcessMemoryInfo
    # (heap-growth endurance measurement, #105 G3)
    if(WIN32)
        target_link_libraries(test_display_integration PRIVATE psapi)
    endif()

    target_compile_features(test_display_integration PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_display_integration PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_display_integration)
    gtest_discover_tests(test_display_integration
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
