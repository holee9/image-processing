# XpeCoverage.cmake — REQ-P0-006 coverage tooling (SPEC-XPE-P0).
#
# History (#113): the `coverage` target used to live in tests/CMakeLists.txt, a
# file the root never added, so no configuration could ever reach it. This file
# is the reachable home. It is included from the root CMakeLists.txt after all
# modules are registered and is a no-op unless BUILD_COVERAGE=ON.
#
# Targets (BUILD_COVERAGE=ON only):
#   coverage        run the whole ctest suite under the platform coverage tool and
#                   write ${CMAKE_BINARY_DIR}/coverage/coverage.xml (Cobertura)
#   coverage_check  fail unless line-rate in coverage.xml >= XPE_COVERAGE_MIN
#
# MSVC   : OpenCppCoverage (choco install opencppcoverage). Instrumentation-free,
#          so the regular build is used; only the run is wrapped.
# GCC/Clang: --coverage flags are added by the root; the `coverage` target here
#          runs ctest and, when lcov is present, produces coverage/lcov.info.

if(NOT BUILD_COVERAGE)
    return()
endif()

set(XPE_COVERAGE_MIN "0.85" CACHE STRING
    "Minimum statement coverage (line-rate, 0..1) required by coverage_check (REQ-P0-006: 85%)")

# Performance-budget tests assert wall-clock limits that a Debug (unoptimized)
# coverage build cannot meet; they cover no code path the functional tests do
# not, and wall-clock regression is owned by the benchmark workflow. Measured
# 2026-09-09: 13/389 ci-post tests fail under coverage-post for this reason only.
set(XPE_COVERAGE_EXCLUDE_TESTS "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance" CACHE STRING
    "ctest -E regex of tests skipped by the coverage target (timing-budget tests)")

set(_xpe_cov_dir "${CMAKE_BINARY_DIR}/coverage")
file(MAKE_DIRECTORY "${_xpe_cov_dir}")

if(MSVC)
    find_program(XPE_OPENCPPCOVERAGE OpenCppCoverage
        PATHS "$ENV{ProgramFiles}/OpenCppCoverage" "$ENV{ProgramFiles\(x86\)}/OpenCppCoverage")

    if(XPE_OPENCPPCOVERAGE)
        message(STATUS "Coverage: OpenCppCoverage found at ${XPE_OPENCPPCOVERAGE}")
        # The x86 build of OpenCppCoverage cannot debug x64 targets ("Cannot run
        # process, check if it is a valid executable"). Measured 2026-09-09 (#113).
        if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND XPE_OPENCPPCOVERAGE MATCHES "\(x86\)|/\(x86\)")
            message(WARNING "Coverage: ${XPE_OPENCPPCOVERAGE} looks like the x86 build; "
                            "it cannot cover this x64 configuration. Install the x64 build "
                            "(choco install opencppcoverage) into Program Files.")
        endif()
        # OpenCppCoverage rejects forward-slash paths ("Please use Windows path
        # separator"), so every path it receives is converted to native form.
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/modules"        _cov_src)
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/modules/*/tests" _cov_excl_mod_tests)
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/tests"          _cov_excl_tests)
        file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/bin"            _cov_bin)
        file(TO_NATIVE_PATH "${_xpe_cov_dir}/coverage.xml"       _cov_xml)
        file(TO_NATIVE_PATH "${_xpe_cov_dir}/html"               _cov_html)
        file(TO_NATIVE_PATH "${CMAKE_CTEST_COMMAND}"             _cov_ctest)
        file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}"                _cov_bindir)
        # --cover_children follows the ctest → test executable process tree.
        # Sources are restricted to modules/ minus test code so tests do not inflate the rate.
        add_custom_target(coverage
            COMMAND "${XPE_OPENCPPCOVERAGE}"
                --quiet
                --cover_children
                --sources "${_cov_src}"
                --excluded_sources "${_cov_excl_mod_tests}"
                --excluded_sources "${_cov_excl_tests}"
                --modules "${_cov_bin}"
                --export_type "cobertura:${_cov_xml}"
                --export_type "html:${_cov_html}"
                -- "${_cov_ctest}" --test-dir "${_cov_bindir}" -C $<CONFIG> --output-on-failure
                   -E "${XPE_COVERAGE_EXCLUDE_TESTS}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Coverage: running ctest under OpenCppCoverage -> ${_xpe_cov_dir}/coverage.xml"
            VERBATIM)
    else()
        # Fail-open at configure time (the regular build must not depend on the
        # tool), fail-closed when the target is actually requested.
        message(WARNING "Coverage: BUILD_COVERAGE=ON but OpenCppCoverage was not found. "
                        "Install with: choco install opencppcoverage. "
                        "The `coverage` target will fail until it is available.")
        add_custom_target(coverage
            COMMAND "${CMAKE_COMMAND}" -E false
            COMMENT "Coverage: OpenCppCoverage not found at configure time (REQ-P0-006)"
            VERBATIM)
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(XPE_LCOV lcov)
    if(XPE_LCOV)
        add_custom_target(coverage
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
                -E "${XPE_COVERAGE_EXCLUDE_TESTS}"
            COMMAND "${XPE_LCOV}" --capture --directory "${CMAKE_BINARY_DIR}"
                --output-file "${_xpe_cov_dir}/lcov.info" --quiet
            COMMAND "${XPE_LCOV}" --remove "${_xpe_cov_dir}/lcov.info" "*/tests/*" "*/_deps/*" "/usr/*"
                --output-file "${_xpe_cov_dir}/lcov.info" --quiet
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Coverage: running ctest with --coverage instrumentation -> ${_xpe_cov_dir}/lcov.info"
            VERBATIM)
    else()
        message(WARNING "Coverage: lcov not found; `coverage` target runs ctest only (instrumentation still on).")
        add_custom_target(coverage
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            VERBATIM)
    endif()
else()
    message(WARNING "Coverage: no coverage tool mapping for compiler ${CMAKE_CXX_COMPILER_ID}")
    return()
endif()

# coverage_check — threshold gate for REQ-P0-006. Cobertura only (MSVC path);
# on lcov builds the check is a no-op until an lcov summary parser is added.
add_custom_target(coverage_check
    COMMAND "${CMAKE_COMMAND}"
        -DXPE_COVERAGE_XML=${_xpe_cov_dir}/coverage.xml
        -DXPE_COVERAGE_MIN=${XPE_COVERAGE_MIN}
        -P "${CMAKE_SOURCE_DIR}/cmake/XpeCoverageCheck.cmake"
    DEPENDS coverage
    COMMENT "Coverage: checking line-rate >= ${XPE_COVERAGE_MIN}"
    VERBATIM)
