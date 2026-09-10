cmake_minimum_required(VERSION 3.20)

# ============================================================================
# xpe_preprocess — SPEC-XPE-P1A  IEC 62304 Class B
# ============================================================================

# When built as a standalone project (not via add_subdirectory from root),
# declare a project so PROJECT_VERSION is defined.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xpe_preprocess VERSION 1.0.0 LANGUAGES C CXX)
    option(BUILD_TESTING "Build unit tests" ON)
endif()

# ---------------------------------------------------------------------------
# Library sources (9 SWUs + helpers)
# ---------------------------------------------------------------------------
set(XPE_PREPROCESS_SOURCES
    src/preprocess.cpp           # version + legacy entry point
    src/offset_correct.cpp       # SWU-1.1 REQ-P1A-009..011 (new 3-arg g_calib API)
    src/gain_correct.cpp         # SWU-1.2 REQ-P1A-016..019 (new 3-arg g_calib API)
    src/readout_validate.cpp     # SWU-1.9 REQ-P1A-001..004
    src/temp_compensate.cpp      # SWU-1.6 REQ-P1A-005..008
    src/nonlinearity_correct.cpp # SWU-1.7 REQ-P1A-012..015
    src/binning_correct.cpp      # SWU-1.8 REQ-P1A-020..023
    src/defect_correct.cpp       # SWU-1.3 REQ-P1A-024..028 (new 3-arg g_calib API)
    src/ghost_correct.cpp        # SWU-1.4 REQ-P1A-029..034
    src/calibration_manager.cpp  # SWU-1.5 CRC-32 only (legacy helpers removed)
    src/calibration_cache.cpp    # SWU-1.10 LRU calibration cache
    src/runtime_detection.cpp    # REQ-P1A-013 Hampel 5-sigma outlier detection
    src/helpers.cpp              # Shared: interpolation, JSON, CRC-32
    src/pipeline.cpp             # Full pipeline integration (REQ-P1A-041..047)
    src/rle_codec.cpp            # RLE compression for defect maps
    src/xpe_calib_check_expiry.cpp  # REQ-P1A-018 XCal v1 header-only expiry check (3-arg API)
    src/xpe_defect_gen.cpp       # SWU-1.11 BPM Generation (FUNC-022~025)
    src/xpe_calib_mode.cpp       # SWU-1.12 Calibration Mode Selection (FUNC-031~033)
    # XCal v1 calibration API (T-006..T-009)
    src/xpe_calibration.cpp         # global g_calib + g_calib_mutex definitions
    src/xpe_calib_load_offset.cpp   # T-006: 1-arg load offset (XCal v1)
    src/xpe_calib_load_gain.cpp     # T-006: 1-arg load gain (XCal v1)
    src/xpe_calib_load_defect_map.cpp # T-006: 1-arg load defect map (XCal v1)
    src/xpe_calib_save.cpp          # T-007: 2-arg save (XCal v1)
    src/xpe_calib_generate_offset.cpp # T-008: 5-arg generate offset (XCal v1)
    src/xpe_calib_generate_offset_methods.cpp # shared offset generation methods
    src/xpe_calib_generate_gain.cpp  # SWU-1.12: FUNC-026/027 gain generation
    # XCal v1 file format helpers (required by load/save/generate functions above)
    src/xcal_reader.cpp
    src/xcal_writer.cpp
    src/xcal_validator.cpp
    # SWU-1.12: Verification Metrics API (PRE-11)
    src/xpe_verify_metrics.cpp  # Quality assessment metrics for calibration verification
)

add_library(xpe_preprocess SHARED
    ${XPE_PREPROCESS_SOURCES}
)

target_include_directories(xpe_preprocess
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include  # for internal header
            ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party  # picosha2 (xcal_reader/writer)
)

target_link_libraries(xpe_preprocess
    PUBLIC xpe_common
)

target_compile_features(xpe_preprocess PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(xpe_preprocess PRIVATE /W4 /arch:AVX2)
else()
    target_compile_options(xpe_preprocess PRIVATE
        -Wall -Wextra -Wpedantic
        -mavx2
    )
endif()

xpe_target_warnings_as_errors(xpe_preprocess)

# VERSION/SOVERSION only meaningful on Linux; guard for portability
if(PROJECT_VERSION)
    set_target_properties(xpe_preprocess PROPERTIES
        OUTPUT_NAME "xpe_preprocess"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   1
    )
else()
    set_target_properties(xpe_preprocess PROPERTIES
        OUTPUT_NAME "xpe_preprocess"
    )
endif()

# ---------------------------------------------------------------------------
# Unit and Integration Tests (Google Test)
# BUILD_TESTING: CMake standard variable (set by standalone project above or
#                by the root project via enable_testing())
# BUILD_TESTS:   XPE root project option
# ---------------------------------------------------------------------------
set(_XPE_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_BUILD_TESTS ON)
endif()

if(_XPE_BUILD_TESTS)
    # Resolve GTest: prefer already-available target (vcpkg / parent project),
    # then fall back to FetchContent for standalone builds.
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

    set(XPE_TEST_SOURCES
        tests/test_offset_correct.cpp
        # QA-A-15 (#120): still unregistered. Not an argument-count mismatch --
        # the file is written against the SPEC signature xpe_gain_correct(img,
        # gainMap) with in-place ownership transfer, while the shipped API is
        # xpe_gain_correct(input, output, metadata) over the global g_calib map.
        # Migrating it means re-authoring all 10 cases, and which side is
        # correct is what QA-A-12 (#117) judges. Register after that verdict.
        tests/test_gain_correct.cpp
        tests/test_readout_validate.cpp
        tests/test_temp_nonlinearity_binning.cpp
        # QA-A-15 (#120): compiles, but xpe_defect_correct(&img, &defectMap, nullptr)
        # passes the map where the shipped API expects the output buffer -- the
        # types match so the compiler is silent and 5 cases fail at runtime.
        # Same divergence as test_gain_correct; register after QA-A-12 (#117).
        tests/test_defect_correct.cpp
        tests/test_ghost_correct.cpp
        tests/test_calibration_manager.cpp
        tests/test_integration.cpp
        tests/test_boundary.cpp
        tests/test_xpe_preprocess_memleak.cpp
        tests/test_xcal_reader.cpp
        tests/test_xcal_writer.cpp
        tests/test_xcal_validator.cpp
        tests/test_xpe_sha256.cpp
        tests/test_xpe_calib_check_expiry.cpp
        tests/test_xpe_calib_endurance.cpp
        # QA-A-15 (#120): present on disk but never registered -- pipeline.cpp,
        # rle_codec.cpp and calibration_cache.cpp measured 0%/6% coverage only
        # because these suites were never compiled.
        tests/test_xcal_compression.cpp
        tests/test_json_helpers.cpp
        # QA-A-25 (#120): the three suites below were on disk but never named
        # here -- 54 cases that had never been compiled. All three are now
        # migrated to the shipped file/global-store calibration API (issue #117
        # decision B) and the two contract conflicts were resolved against the
        # implementation: gain_correct.cpp:296 divides by the stored map, and
        # xpe_validate_readout_artifact reports two booleans rather than a
        # 0..100 score. Findings: .moai/reports/lane-pre/QA-A-21/, QA-A-25/.
        tests/test_golden_reference.cpp
        tests/test_gain_correct_reciprocal_fma.cpp
        tests/test_calibration_roundtrip.cpp
        tests/test_pipeline_ex.cpp
        tests/test_calibration_cache.cpp
        # QA-A-22 (#127): cached-loader buffer ownership (cache-owned view)
        tests/test_calib_cache_ownership.cpp
        # QA-A-14 (#119): error-code precedence rule 1 probe
        tests/test_error_precedence.cpp
        tests/test_xpe_calib_generate_offset.cpp
        tests/test_calib_generate_offset_multi.cpp
        # QA-A-26 (#97): SigmaClip conformance to XPE-ALG-001 9.8.2.1
        tests/test_sigma_clip_conformance.cpp
        tests/test_xpe_calib_load.cpp
        tests/test_xpe_calib_save.cpp
        # QA-A-29 (#132): xpe_calib_save 3-arg embedded expiry, api-spec 6.14
        tests/test_calib_save_expiry.cpp
        # QA-A-21 (#120) judgement. The "legacy 3-arg API" half of the previous
        # note was stale: the shipped API *is* 3-arg and this file has zero
        # 2-argument correction calls. Registering it produces 14 errors, all
        # struct fields, but they are not a rename job:
        #   XpeImageBuffer::stride  -> removed (2 uses, derivable from width)
        #   kvp -> kVp, sid_mm -> SID_mm                    (rename)
        #   ma -> mAs                 (mA vs mA*s -- different quantity)
        #   acquisition_time_s -> acquisitionTime  (seconds vs epoch ms)
        #   temperature_c, integration_time_ms -> no field exists
        # The last three change what the affected cases assert, so registering
        # needs a contract decision, not an edit. Details in
        # .moai/reports/lane-pre/QA-A-21/.
        # tests/test_xpe_preprocess.cpp
        tests/test_xpe_preprocess_calibration.cpp
        tests/test_xpe_preprocess_correction.cpp
        tests/test_xpe_preprocess_init.cpp
        # BP-01~05 DegradedMode benchmark smoke tests (null/identity calibration)
        tests/test_preprocess_degraded.cpp
        # BP-01~05 DegradedMode — see test_preprocess_degraded.cpp above
        # SPEC-SIMD-001: AVX2 parity tests (3-arg API)
        tests/test_offset_correct_avx2_parity.cpp
        tests/test_gain_correct_avx2_parity.cpp
        tests/test_defect_correct_avx2_parity.cpp
        tests/test_runtime_detection_avx2_parity.cpp
        # TODO SPEC-SIMD-001: test_offset_correct_avx512_parity.cpp — blocked by xpe_simd_force_scalar API
        # TODO SPEC-SIMD-001: test_offset_correct_neon_parity.cpp — blocked by NEON dispatch API (ARM only)
        # SWU-1.12: Calibration generation and verification tests (FUNC-022~027)
        tests/test_calib_generate_gain.cpp
        tests/test_defect_gen.cpp
        tests/test_verify_metrics.cpp
        # SWU-1.12: Calibration Mode Selection tests (FUNC-031~033)
        tests/test_calib_mode.cpp
        # REQ-P1A-066: OOM guard / IO failure / SHA-256 mismatch / Ghost multi-handle race
        tests/test_req_p1a_066.cpp
    )

    add_executable(xpe_preprocess_tests
        ${XPE_TEST_SOURCES}
        src/rle_codec.cpp
        src/xpe_calib_generate_offset_methods.cpp
        src/xcal_writer.cpp
        src/xcal_reader.cpp
        src/xcal_validator.cpp
    )

    target_include_directories(xpe_preprocess_tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
    )

    target_link_libraries(xpe_preprocess_tests
        PRIVATE xpe_preprocess
                GTest::gtest_main
    )

    # Windows: psapi.lib is required for GetProcessMemoryInfo
    # (used by test_xpe_preprocess_memleak.cpp to measure heap growth).
    if(WIN32)
        target_link_libraries(xpe_preprocess_tests PRIVATE psapi)
    endif()

    target_compile_features(xpe_preprocess_tests PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(xpe_preprocess_tests PRIVATE /W4)
    else()
        target_compile_options(xpe_preprocess_tests PRIVATE -Wall -Wextra)
    endif()
    xpe_target_warnings_as_errors(xpe_preprocess_tests)

    gtest_discover_tests(xpe_preprocess_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
