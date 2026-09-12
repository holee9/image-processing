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
# =============================================================================
# QA-A-36 (#141): deterministic XCal fixture generator
#
# The repository ships no XCal fixtures and binaries are not committed, so GUI,
# E2E and CI build a set at run time with this tool. It links the module it
# ships with and adds no export.
# =============================================================================
add_executable(xpe_calib_fixture_gen tools/xpe_calib_fixture_gen.cpp)

target_include_directories(xpe_calib_fixture_gen
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party  # picosha2, via xpe_sha256.hpp
)

target_link_libraries(xpe_calib_fixture_gen PRIVATE xpe_preprocess xpe_common)
target_compile_features(xpe_calib_fixture_gen PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(xpe_calib_fixture_gen PRIVATE /W4)
else()
    target_compile_options(xpe_calib_fixture_gen PRIVATE -Wall -Wextra)
endif()
xpe_target_warnings_as_errors(xpe_calib_fixture_gen)

# Same output directory as the test binaries so the CI artifact picks it up.
set_target_properties(xpe_calib_fixture_gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# =============================================================================
# QA-A-41 (#143 #144): candidate detector experiment (EXPERIMENT ONLY)
#
# Not registered with ctest: it runs six full-frame variants over three seeds
# and takes minutes. It ships no production code -- the candidates live in the
# tool's own TU and reuse the shipped detection primitives.
# =============================================================================
add_executable(xpe_detect_experiment tools/xpe_detect_experiment.cpp)

target_include_directories(xpe_detect_experiment
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(xpe_detect_experiment PRIVATE xpe_preprocess xpe_common)
target_compile_features(xpe_detect_experiment PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(xpe_detect_experiment PRIVATE /W4)
else()
    target_compile_options(xpe_detect_experiment PRIVATE -Wall -Wextra)
endif()
xpe_target_warnings_as_errors(xpe_detect_experiment)
set_target_properties(xpe_detect_experiment PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# =============================================================================
# QA-A-47 (#143): structured-frame probe (EXPERIMENT ONLY)
#
# Not registered with ctest. Builds simulated frames whose local noise varies by
# region and measures what the global sigma floor and ceiling do there.
# =============================================================================
add_executable(xpe_struct_frames tools/xpe_struct_frames.cpp)

target_include_directories(xpe_struct_frames
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(xpe_struct_frames PRIVATE xpe_preprocess xpe_common)
target_compile_features(xpe_struct_frames PRIVATE cxx_std_17)
# fopen/sscanf are the right tools for a throwaway probe; MSVC C4996 + /WX would
# otherwise force _s variants that add nothing here.
target_compile_definitions(xpe_struct_frames PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(xpe_struct_frames PRIVATE /W4)
else()
    target_compile_options(xpe_struct_frames PRIVATE -Wall -Wextra)
endif()
xpe_target_warnings_as_errors(xpe_struct_frames)
set_target_properties(xpe_struct_frames PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# =============================================================================
# QA-A-51 (#151): real-frame verification harness (EXPERIMENT ONLY)
#
# Not registered with ctest. Reads real exposures (raw, and DICOM via a RUNTIME
# LoadLibrary of xpe_dicom.dll -- never a link dependency, because the
# ci-preprocess preset configures BUILD_DICOM=OFF) and measures only what a
# frame without defect ground truth can support. Decision thresholds are fixed
# in the source and printed on every run.
# =============================================================================
add_executable(xpe_real_frames tools/xpe_real_frames.cpp)

target_include_directories(xpe_real_frames
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            # QA-A-52: headers ONLY, for the compile-time ABI drift guard. No
            # link dependency on xpe_dicom is added and none may be -- the
            # ci-preprocess preset configures BUILD_DICOM=OFF, and the DLL is
            # resolved at runtime via LoadLibrary.
            ${CMAKE_CURRENT_SOURCE_DIR}/../dicom/include
)
target_link_libraries(xpe_real_frames PRIVATE xpe_preprocess xpe_common)
target_compile_features(xpe_real_frames PRIVATE cxx_std_17)

# QA-A-53: bake the commit this tool was CONFIGURED from, so a staged
# xpe_dicom.dll carrying a different headSha in its provenance.json is visible at
# run time. Configure-time, not build-time: CMake has no portable per-build hook,
# and the window between configure and build is the residual (named in the report).
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE XPE_A53_HEAD_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT XPE_A53_HEAD_SHA)
    execute_process(COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE XPE_A53_HEAD_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
target_compile_definitions(xpe_real_frames
    PRIVATE XPE_A53_BUILD_HEAD_SHA="${XPE_A53_HEAD_SHA}")
# fopen/sscanf are the right tools for a throwaway probe; MSVC C4996 + /WX would
# otherwise force _s variants that add nothing here.
target_compile_definitions(xpe_real_frames PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(xpe_real_frames PRIVATE /W4)
else()
    target_compile_options(xpe_real_frames PRIVATE -Wall -Wextra)
endif()
xpe_target_warnings_as_errors(xpe_real_frames)
set_target_properties(xpe_real_frames PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

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
        # QA-A-32 (#120): ghost Tier2/Tier3 coverage
        tests/test_ghost_tiers.cpp
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
        # QA-A-32 (#120): pipeline stage bodies + failure propagation
        tests/test_pipeline_stages.cpp
        tests/test_calibration_cache.cpp
        # QA-A-22 (#127): cached-loader buffer ownership (cache-owned view)
        tests/test_calib_cache_ownership.cpp
        # QA-A-31 (#127): put/get window under concurrent clear and eviction
        tests/test_calib_cache_concurrency.cpp
        # QA-A-14 (#119): error-code precedence rule 1 probe
        tests/test_error_precedence.cpp
        tests/test_xpe_calib_generate_offset.cpp
        tests/test_calib_generate_offset_multi.cpp
        # QA-A-26 (#97): SigmaClip conformance to XPE-ALG-001 9.8.2.1
        tests/test_sigma_clip_conformance.cpp
        # QA-A-27 (#112): ported from the deleted tests/preprocess_smoke orphan
        tests/test_param_range_names.cpp
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
        # QA-A-42 (#144): renamed from test_runtime_detection_avx2_parity.cpp --
        # it asserts determinism, not AVX2 parity; there is no AVX2 path here.
        tests/test_runtime_detection_determinism.cpp
        # QA-A-33 (#120 #112): correctness half deleted by QA-A-27
        tests/test_runtime_detection_functional.cpp
        tests/test_runtime_detection_errors.cpp
        # TODO SPEC-SIMD-001: test_offset_correct_avx512_parity.cpp — blocked by xpe_simd_force_scalar API
        # TODO SPEC-SIMD-001: test_offset_correct_neon_parity.cpp — blocked by NEON dispatch API (ARM only)
        # SWU-1.12: Calibration generation and verification tests (FUNC-022~027)
        tests/test_calib_generate_gain.cpp
        tests/test_defect_gen.cpp
        tests/test_verify_metrics.cpp
        # QA-A-32 (#120): verify_* edge paths (format/fallback/median branches)
        tests/test_verify_metrics_edges.cpp
        # SWU-1.12: Calibration Mode Selection tests (FUNC-031~033)
        tests/test_calib_mode.cpp
        # QA-A-35 (#140): FUNC-033 quality metadata, wired to production
        tests/test_calib_quality_meta.cpp
        # REQ-P1A-066: OOM guard / IO failure / SHA-256 mismatch / Ghost multi-handle race
        tests/test_req_p1a_066.cpp
        # QA-A-36 (#141): fixture generator determinism + pipeline acceptance
        tests/test_calib_fixture_gen.cpp
        # QA-A-37 (#140): XCAL_TYPE_GAIN_POLY read path
        tests/test_calib_gain_poly_load.cpp
        # QA-A-38 (#138 #97): XPE-ALG-001 9.8.2.1 N_min static-defect marking
        tests/test_sigma_clip_nmin.cpp
        # QA-A-39 (#138 #97): public config_json_or_null on generate_offset
        tests/test_calib_generate_offset_config.cpp
        # QA-A-40 (#120 #112): REQ-P1A-013 TPR/FPR quantitative harness
        tests/test_runtime_detection_rates.cpp
        # QA-A-42 (#144): buffer-reuse parity for the detection loop
        tests/test_runtime_detection_buffer_reuse.cpp
        tests/test_runtime_detection_median8_parity.cpp
        tests/test_runtime_detection_radix_select_parity.cpp
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

    # QA-A-36 (#141): test_calib_fixture_gen.cpp runs the tool as a subprocess.
    # The path is handed over through a generated header rather than a compile
    # definition -- on Windows the path contains backslashes, which a plain
    # -D"..." string turns into invalid escapes. A raw string literal does not.
    add_dependencies(xpe_preprocess_tests xpe_calib_fixture_gen)
    file(GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xpe_fixture_gen_path.h
        CONTENT "#pragma once
#define XPE_FIXTURE_GEN_EXE R\"($<TARGET_FILE:xpe_calib_fixture_gen>)\"
"
    )
    target_include_directories(xpe_preprocess_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    gtest_discover_tests(xpe_preprocess_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
