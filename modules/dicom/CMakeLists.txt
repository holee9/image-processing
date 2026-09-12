cmake_minimum_required(VERSION 3.20)

# ============================================================================
# xpe_dicom — SPEC-XPE-P1B-DICOM  IEC 62304 Class B
# ============================================================================

# When built as a standalone project (not via add_subdirectory from root),
# declare a project so PROJECT_VERSION is defined.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xpe_dicom VERSION 1.0.0 LANGUAGES C CXX)
    option(BUILD_TESTING "Build unit tests" ON)
endif()

# ---------------------------------------------------------------------------
# Library sources (4 SWUs + module entry)
# ---------------------------------------------------------------------------
set(XPE_DICOM_SOURCES
    src/dicom.cpp               # version entry point
    src/DicomReader.cpp         # SWU-4.1 REQ-DICOM-001..011
    src/DicomWriter.cpp         # SWU-4.2 REQ-DICOM-012..020
    src/DicomValidator.cpp      # SWU-4.3 REQ-DICOM-021..028
    src/DicomNetworkSCU.cpp     # SWU-4.4 REQ-DICOM-029..040
)

# Find DCMTK dependencies (required for DICOM I/O)
find_package(dcmtk CONFIG QUIET)
if(NOT dcmtk_FOUND)
    message(WARNING "DCMTK not found via CONFIG, trying MODULE mode")
    find_package(DCMTK REQUIRED)
endif()

# OpenJPEG is called directly by DicomReader/DicomWriter for JPEG 2000
# transfer syntaxes (opj_* API). DCMTK's own dcmjpls does not expose it, so
# it must be linked explicitly.
find_package(OpenJPEG CONFIG REQUIRED)

add_library(xpe_dicom SHARED
    ${XPE_DICOM_SOURCES}
)

target_include_directories(xpe_dicom
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xpe_dicom
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            # OpenJPEG exports a bare `openjp2` target (no namespace).
            openjp2
)

if(dcmtk_FOUND)
    # DCMTK CONFIG mode provides imported targets.
    # The package exports the DCMTK:: namespace; DCMTK::DCMTK is the INTERFACE
    # aggregate over every component (dcmdata, dcmnet, ofstd, ... ).
    target_link_libraries(xpe_dicom PRIVATE DCMTK::DCMTK)
else()
    # DCMTK MODULE mode provides legacy variables
    if(DCMTK_INCLUDE_DIRS)
        target_include_directories(xpe_dicom PRIVATE ${DCMTK_INCLUDE_DIRS})
    endif()
    if(DCMTK_LIBRARIES)
        target_link_libraries(xpe_dicom PRIVATE ${DCMTK_LIBRARIES})
    endif()
endif()

target_compile_features(xpe_dicom PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(xpe_dicom PRIVATE /W4)
else()
    target_compile_options(xpe_dicom PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()
xpe_target_warnings_as_errors(xpe_dicom)

if(PROJECT_VERSION)
    set_target_properties(xpe_dicom PROPERTIES
        OUTPUT_NAME "xpe_dicom"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   1
    )
else()
    set_target_properties(xpe_dicom PROPERTIES
        OUTPUT_NAME "xpe_dicom"
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

    # --- test_dicom_parameter_dependency (#156 / QA-B-59) ---
    # Change one input, write, read back, require the difference to survive the
    # round trip. A metadata field that never reaches the file is invisible to a
    # return-code test.
    add_executable(test_dicom_parameter_dependency
        tests/test_parameter_dependency.cpp
    )
    target_include_directories(test_dicom_parameter_dependency PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_dicom_parameter_dependency PRIVATE
        xpe_dicom
        xpe_common
        GTest::gtest_main
    )
    target_compile_features(test_dicom_parameter_dependency PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_dicom_parameter_dependency PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_dicom_parameter_dependency)
    gtest_discover_tests(test_dicom_parameter_dependency
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_dicom_reader (SWU-4.1: REQ-DICOM-001..011) ---
    add_executable(test_dicom_reader
        tests/test_dicom_reader.cpp
    )
    target_include_directories(test_dicom_reader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # #146 (QA-B-45): the accepted transfer-syntax table lives in the
        # module-internal DicomReader.h. The test iterates it so the list and the
        # module's real decode capability cannot drift apart again.
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_dicom_reader PRIVATE
        xpe_dicom
        GTest::gtest_main
    )
    # #124 (QA-B-25): this suite derives its negative-path fixtures with DCMTK
    # (see the test's SetUpTestSuite), so it links DCMTK the same way xpe_dicom
    # does above.
    if(dcmtk_FOUND)
        target_link_libraries(test_dicom_reader PRIVATE DCMTK::DCMTK)
    else()
        if(DCMTK_INCLUDE_DIRS)
            target_include_directories(test_dicom_reader PRIVATE ${DCMTK_INCLUDE_DIRS})
        endif()
        if(DCMTK_LIBRARIES)
            target_link_libraries(test_dicom_reader PRIVATE ${DCMTK_LIBRARIES})
        endif()
    endif()

    target_compile_features(test_dicom_reader PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_dicom_reader PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_dicom_reader)
    gtest_discover_tests(test_dicom_reader
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_dicom_writer (SWU-4.2: REQ-DICOM-012..020) ---
    add_executable(test_dicom_writer
        tests/test_dicom_writer.cpp
    )
    target_include_directories(test_dicom_writer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_dicom_writer PRIVATE
        xpe_dicom
        GTest::gtest_main
    )
    # Windows: psapi.lib is required for GetProcessMemoryInfo
    # (heap-growth endurance measurement, #105 G3)
    if(WIN32)
        target_link_libraries(test_dicom_writer PRIVATE psapi)
    endif()

    target_compile_features(test_dicom_writer PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_dicom_writer PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_dicom_writer)
    gtest_discover_tests(test_dicom_writer
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_dicom_validator (SWU-4.3: REQ-DICOM-021..028) ---
    add_executable(test_dicom_validator
        tests/test_dicom_validator.cpp
    )
    target_include_directories(test_dicom_validator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_dicom_validator PRIVATE
        xpe_dicom
        GTest::gtest_main
    )
    # #124 (QA-B-25): this suite derives its negative-path fixtures with DCMTK
    # (see the test's SetUpTestSuite), so it links DCMTK the same way xpe_dicom
    # does above.
    if(dcmtk_FOUND)
        target_link_libraries(test_dicom_validator PRIVATE DCMTK::DCMTK)
    else()
        if(DCMTK_INCLUDE_DIRS)
            target_include_directories(test_dicom_validator PRIVATE ${DCMTK_INCLUDE_DIRS})
        endif()
        if(DCMTK_LIBRARIES)
            target_link_libraries(test_dicom_validator PRIVATE ${DCMTK_LIBRARIES})
        endif()
    endif()

    target_compile_features(test_dicom_validator PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_dicom_validator PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_dicom_validator)
    gtest_discover_tests(test_dicom_validator
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # --- test_dicom_network_scu (SWU-4.4: REQ-DICOM-029..040) ---
    add_executable(test_dicom_network_scu
        tests/test_dicom_network_scu.cpp
        tests/mock_scp.cpp
    )
    target_include_directories(test_dicom_network_scu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_dicom_network_scu PRIVATE
        xpe_dicom
        GTest::gtest_main
    )
    # #120 (QA-B-29): the suite runs an in-process SCP (tests/mock_scp.cpp), so
    # it links DCMTK directly -- dcmnet for DcmStorageSCP, the same way
    # xpe_dicom links it above.
    if(dcmtk_FOUND)
        target_link_libraries(test_dicom_network_scu PRIVATE DCMTK::DCMTK)
    else()
        if(DCMTK_INCLUDE_DIRS)
            target_include_directories(test_dicom_network_scu PRIVATE ${DCMTK_INCLUDE_DIRS})
        endif()
        if(DCMTK_LIBRARIES)
            target_link_libraries(test_dicom_network_scu PRIVATE ${DCMTK_LIBRARIES})
        endif()
    endif()
    if(WIN32)
        target_link_libraries(test_dicom_network_scu PRIVATE ws2_32)
    endif()

    target_compile_features(test_dicom_network_scu PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_dicom_network_scu PRIVATE /W4)
    endif()
    xpe_target_warnings_as_errors(test_dicom_network_scu)
    gtest_discover_tests(test_dicom_network_scu
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
