# ============================================================================
# xpe_ai -- SPEC-XPE-P3-AI  IEC 62304 Class B
# AI-based image processing: body part recognition, stitching, bone
# suppression, DL denoising. Inference runs in sandboxed worker process.
#
# REQ-AI-001: Layer 1 dependency (xpe_common only, no lateral DLL deps)
# REQ-AI-003: Worker isolation (xpe_ai_worker.exe over named pipe IPC)
# REQ-AI-006: ONNX Runtime 1.20+ integration with multi-EP support
#   -- PARTLY MET as of QA-B-160 (#130). A full build now links ONNX Runtime
#      and runs a model (verified against 1.30.0, so the "1.20+" half holds);
#      src/ai_onnx_session.cpp's "full" arm no longer falls through to the stub
#      and EP availability is a runtime query, not a hardcoded list.
#      The multi-EP half is NOT met: only the CPU provider is registered.
#      Until QA-B-160 neither half was -- no build linked ONNX at all.
#      (Cited by function name, not by line: QA-B-156's own header edit moved
#      that file by +25/-2 lines and the ":198" written here in QA-B-155 went
#      stale the same day.)
#
# Build modes (the two are NOT independent -- see the contradiction guard
# below, the `if(XPE_AI_USE_ONNXRUNTIME AND XPE_AI_STUB_BUILD)` FATAL_ERROR):
#   XPE_AI_USE_ONNXRUNTIME=ON  -- Ask for ONNX Runtime        (default OFF)
#   XPE_AI_STUB_BUILD=ON       -- Build stubs without ONNX    (default ON)
#   Turning only the first one ON is a FATAL_ERROR, because the documented
#   defaults contradict each other. An ONNX build needs BOTH:
#   -DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Standalone project support
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xpe_ai VERSION 0.1.0 LANGUAGES C CXX)
    option(BUILD_TESTING "Build unit tests" ON)
endif()

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(XPE_AI_USE_ONNXRUNTIME "Link ONNX Runtime for inference" OFF)
option(XPE_AI_STUB_BUILD      "Build stubs without ONNX Runtime" ON)

# ---------------------------------------------------------------------------
# #130 (QA-B-153): AN EXPLICITLY REQUESTED `ON` IS NEVER SILENTLY DOWNGRADED.
#
# Two silent paths were measured (QA-B-152/153). Both ended with a successful
# configure, a successful build, and no onnxruntime in the DLL's PE import
# table -- so "I turned the option on and it built" read as "ONNX is linked":
#
#   (1) USE_ONNXRUNTIME=ON with STUB_BUILD=OFF: the acquisition block below
#       could not find a runtime and reset BOTH cache entries with
#       set(... FORCE), leaving only a message(WARNING).
#   (2) USE_ONNXRUNTIME=ON with STUB_BUILD left at its default ON: the
#       acquisition block is not even entered (its condition is
#       `USE AND NOT STUB`), so there was NOT EVEN A WARNING -- and the cache
#       still reported USE_ONNXRUNTIME=ON. That one is worse: the variable a
#       reader would inspect said the opposite of what was built.
#
# `ON` can only come from the caller: option(XPE_AI_USE_ONNXRUNTIME) above
# defaults it to OFF, and nothing else in the tree sets it (grepped across
# CMakeLists/*.cmake/presets).
# So no default-vs-user-supplied detection is needed here -- the value itself
# is the request. The `if(DEFINED VAR)`-before-option() trick was considered
# and rejected: it reports TRUE on every RE-configure once the cache holds the
# entry, so it cannot tell a default apart from a caller's value on the second
# run. (That limitation is reasoned from CMake's cache semantics, not measured.)
#
# Auto-clearing STUB_BUILD when USE_ONNXRUNTIME is ON was also rejected: it is
# the same "quietly change what the caller asked for" move being removed here,
# only in the friendlier direction. An error that names the fix costs one line
# of friction and cannot be misread.
if(XPE_AI_USE_ONNXRUNTIME AND XPE_AI_STUB_BUILD)
    message(FATAL_ERROR
        "xpe_ai: XPE_AI_USE_ONNXRUNTIME=ON and XPE_AI_STUB_BUILD=ON contradict "
        "each other, and the stub wins -- the build would produce a stub while "
        "the cache reported ONNX as enabled (#130).\n"
        "  For an ONNX build:  -DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF\n"
        "  For a stub build:   -DXPE_AI_USE_ONNXRUNTIME=OFF  (the default)")
endif()

# ---------------------------------------------------------------------------
# ONNX Runtime acquisition -- CALLER-SUPPLIED ONLY. Nothing is downloaded.
# ---------------------------------------------------------------------------
# #130 (QA-B-155). WHAT THIS COMMENT USED TO SAY:
#
#   "ONNX Runtime via FetchContent (optional)"
#   "FetchContent downloads pre-built NuGet package on Windows or builds
#    from source on Linux."
#   "Set ONNXRUNTIME_ROOT ..., or let this script attempt FetchContent."
#
# There was no FetchContent here: `FetchContent_Declare` count in this block
# was 0. Control, so the count is not a blind search: the googletest declare in
# this same file's test block is real, and modules/common/CMakeLists.txt has
# four. The lead read those lines while deciding #130's procurement path and
# concluded acquisition was automated; counting the declares showed it was not.
# Measured: with ONNXRUNTIME_ROOT unset and -DXPE_AI_USE_ONNXRUNTIME=ON
# -DXPE_AI_STUB_BUILD=OFF, configure exits 1 with the FATAL_ERROR below --
# nothing is fetched.
#
# THE ACTUAL PATH, and the #130 decision it implements:
#   procurement    pre-built binaries, downloaded and placed by the caller
#                  https://github.com/microsoft/onnxruntime/releases
#   vcpkg          NOT used (no onnxruntime in vcpkg.json)
#   auto-download  NOT performed (no FetchContent, no ExternalProject)
#
#   1. ONNXRUNTIME_ROOT defined  -> IMPORTED target from <root>/include + /lib
#   2. otherwise                 -> find_package(onnxruntime QUIET CONFIG), once
#   3. neither resolves          -> FATAL_ERROR (QA-B-153; never a silent stub)
#
# ONNX Runtime ships no standard CMake config package, so path 2 succeeds only
# where something else installed one.
if(XPE_AI_USE_ONNXRUNTIME AND NOT XPE_AI_STUB_BUILD)

    if(DEFINED ONNXRUNTIME_ROOT)
        # User-provided ONNX Runtime installation
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
        set(ONNXRUNTIME_LIB_DIR     "${ONNXRUNTIME_ROOT}/lib")

        add_library(onnxruntime SHARED IMPORTED)
        if(WIN32)
            set_target_properties(onnxruntime PROPERTIES
                IMPORTED_IMPLIB "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib"
                IMPORTED_LOCATION "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
            )
        else()
            set_target_properties(onnxruntime PROPERTIES
                IMPORTED_LOCATION "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.so"
            )
        endif()
        target_include_directories(onnxruntime INTERFACE ${ONNXRUNTIME_INCLUDE_DIR})

        message(STATUS "ONNX Runtime: using pre-built from ${ONNXRUNTIME_ROOT}")
    else()
        # find_package only. (This said "find_package or pkg-config"; there is
        # no pkg-config call here -- pkg_check_modules count in this file: 0.)
        find_package(onnxruntime QUIET CONFIG)

        if(NOT onnxruntime_FOUND)
            # #130 (QA-B-153). WHAT WAS HERE BEFORE:
            #
            #   message(WARNING "ONNX Runtime not found. ... Falling back to stub build.")
            #   set(XPE_AI_STUB_BUILD      ON  CACHE BOOL "Fallback to stub" FORCE)
            #   set(XPE_AI_USE_ONNXRUNTIME OFF CACHE BOOL "ONNX unavailable"  FORCE)
            #
            # Configure and build both returned 0 and the produced xpe_ai.dll
            # imported no onnxruntime (PE import table: xpe_common, spdlog, CRT
            # only). A caller who asked for ONNX got a stub and a green build.
            #
            # The runtime is not optional once it has been asked for: the whole
            # inference path, REQ-AI-002's deterministic fallback contract and
            # SOUP-003's Class B record all describe a build that HAS it.
            # Choosing the stub on the caller's behalf decides that for them.
            message(FATAL_ERROR
                "xpe_ai: XPE_AI_USE_ONNXRUNTIME=ON was requested but no ONNX "
                "Runtime was found, so the build cannot honour it (#130). It is "
                "NOT downgraded to a stub -- that used to happen silently and a "
                "green build then meant nothing.\n"
                "  Point at a pre-built runtime:  -DONNXRUNTIME_ROOT=<dir containing include/ and lib/>\n"
                "  or install one find_package(onnxruntime CONFIG) can see\n"
                "  or build the stub on purpose:  -DXPE_AI_USE_ONNXRUNTIME=OFF")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# DLL: xpe_ai (in-process proxy)
# ---------------------------------------------------------------------------
set(MODULE_NAME xpe_ai)

set(XPE_AI_SOURCES
    src/ai.cpp
    src/ai_ipc_bridge.cpp  # T-001: Named pipe IPC bridge
    src/ai_worker_supervisor.cpp  # QA-B-171B: owns the worker process (launch, kill on fault, restart)
    src/ai_onnx_session.cpp  # T-003: ONNX Runtime session manager
)

add_library(${MODULE_NAME} SHARED
    ${XPE_AI_SOURCES}
)

target_include_directories(${MODULE_NAME}
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
target_compile_definitions(${MODULE_NAME} PRIVATE XPE_DLL_EXPORT)

if(XPE_AI_STUB_BUILD OR NOT XPE_AI_USE_ONNXRUNTIME)
    # Stub build: no ONNX Runtime dependency
    target_compile_definitions(${MODULE_NAME} PRIVATE XPE_AI_STUB_BUILD=1)
    message(STATUS "xpe_ai: STUB build (no ONNX Runtime)")
else()
    # Full build: link ONNX Runtime
    target_compile_definitions(${MODULE_NAME} PRIVATE XPE_AI_USE_ONNXRUNTIME=1)
    target_link_libraries(${MODULE_NAME} PRIVATE onnxruntime)
    message(STATUS "xpe_ai: FULL build with ONNX Runtime")
endif()

# Soft dependencies: spdlog and nlohmann_json (optional but recommended)
# These are detected at configure time; the source code uses #ifdef guards.
if(TARGET spdlog::spdlog)
    target_link_libraries(${MODULE_NAME} PRIVATE spdlog::spdlog)
    target_compile_definitions(${MODULE_NAME} PRIVATE XPE_AI_USE_SPDLOG=1)
endif()

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(${MODULE_NAME} PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(${MODULE_NAME} PRIVATE XPE_AI_USE_NLOHMANN_JSON=1)
endif()

# Mandatory dependency: xpe_common (Layer 1 -- REQ-AI-001)
target_link_libraries(${MODULE_NAME}
    PUBLIC xpe_common
)

target_compile_features(${MODULE_NAME} PUBLIC cxx_std_17)

if(MSVC)
    # #226 (QA-B-164): /wd4150 REMOVED -- it is not needed and it was hiding a
    # real defect. C4150 is "deletion of pointer to incomplete type; no
    # destructor called", i.e. a leak of everything the hidden type owns.
    # QA-B-161 found one behind this flag (OnnxSession's destructor sat ABOVE
    # struct Impl, so the ONNX session and env were never released in a full
    # build) and it only surfaced because the TEST target has no suppression.
    #
    # Measured before removing it: with /wd4150 gone, C4150 count is 0 in all
    # four combinations (xpe_ai and xpe_ai_worker, stub and full). A positive
    # control -- a deliberate `delete` of an incomplete type -- did produce
    # exactly one C4150 in both configs, so the 0 is a measurement and not a
    # build that quietly did nothing.
    #
    # /wd4996 stays: that is the deprecated-function warning, a different
    # question, and not this card's.
    target_compile_options(${MODULE_NAME} PRIVATE /W4 /wd4996)
else()
    target_compile_options(${MODULE_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# Set output properties
if(PROJECT_VERSION)
    set_target_properties(${MODULE_NAME} PROPERTIES
        OUTPUT_NAME "xpe_ai"
        VERSION     ${PROJECT_VERSION}
        SOVERSION   0
    )
else()
    set_target_properties(${MODULE_NAME} PROPERTIES
        OUTPUT_NAME "xpe_ai"
    )
endif()

# ---------------------------------------------------------------------------
# Worker executable: xpe_ai_worker.exe (sandboxed inference process)
#
# REQ-AI-003: Worker runs in isolated process. The DLL proxy communicates
# with it over named pipes. The worker hosts the actual ONNX Runtime session.
#
# T-002: Worker skeleton builds in both STUB and FULL modes for testing
# ---------------------------------------------------------------------------
if(TRUE)  # Worker build enabled
set(WORKER_NAME xpe_ai_worker)

add_executable(${WORKER_NAME}
    src/ai_worker_main.cpp
    # QA-B-170: the worker runs the model now. It compiles the session layer
    # itself for the same reason xpe_ai_tests does -- OnnxSession is a C++ class
    # and xpe_ai.dll exports a C ABI only.
    src/ai_onnx_session.cpp
)

target_include_directories(${WORKER_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            # Headers only (xpe_error.h for the XPE_ERR_* codes the worker
            # returns). The worker does not link xpe_common: that would drag a
            # DLL into a process that needs none of its functions.
            $<TARGET_PROPERTY:xpe_common,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_features(${WORKER_NAME} PRIVATE cxx_std_17)

target_compile_definitions(${WORKER_NAME} PRIVATE XPE_AI_WORKER_BUILD=1)

if(XPE_AI_STUB_BUILD OR NOT XPE_AI_USE_ONNXRUNTIME)
    # Stub build: no ONNX Runtime dependency
    target_compile_definitions(${WORKER_NAME} PRIVATE XPE_AI_STUB_BUILD=1)
    message(STATUS "xpe_ai_worker: STUB build (no ONNX Runtime)")
else()
    # Full build: link ONNX Runtime
    target_compile_definitions(${WORKER_NAME} PRIVATE XPE_AI_USE_ONNXRUNTIME=1)
    target_link_libraries(${WORKER_NAME} PRIVATE onnxruntime)
    message(STATUS "xpe_ai_worker: FULL build with ONNX Runtime")

    # QA-B-170: until now this link was decorative -- nothing in the worker
    # referenced ONNX Runtime, so the linker dropped the import and the exe
    # started without the DLL. It calls Ort:: now, so the loader needs
    # onnxruntime.dll beside it, and a worker that cannot start looks like a
    # protocol failure rather than a missing file.
    if(WIN32 AND DEFINED ONNXRUNTIME_ROOT)
        add_custom_command(TARGET ${WORKER_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
                    "$<TARGET_FILE_DIR:${WORKER_NAME}>"
            COMMENT "xpe_ai_worker: staging onnxruntime.dll next to the worker exe")
    endif()
endif()

# Optional spdlog for worker logging
if(TARGET spdlog::spdlog)
    target_link_libraries(${WORKER_NAME} PRIVATE spdlog::spdlog)
endif()

if(MSVC)
    # #226 (QA-B-164): /wd4150 removed here too -- 0 occurrences, measured.
    # This target compiles one file (src/ai_worker_main.cpp) and has no PIMPL,
    # which the issue flagged as unverified; checked, and it has none.
    target_compile_options(${WORKER_NAME} PRIVATE /W4 /wd4996)
else()
    target_compile_options(${WORKER_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

endif()  # if(TRUE) above. This said "End of temporarily disabled worker
         # build" while the guard it closes is `if(TRUE)` -- the worker HAS
         # been built all along (xpe_ai_worker.exe is produced; #205 found the
         # single-process step tripping over it). QA-B-155.

# ---------------------------------------------------------------------------
# Installation (QA-B-171C)
# ---------------------------------------------------------------------------
# The worker ships BESIDE xpe_ai.dll: the DLL looks for xpe_ai_worker.exe in its own directory and
# nowhere else (ai.cpp workerExePath -- never PATH or the working directory, because a different
# executable answering to the worker's name would sit inside a process that handles patient images).
# Both targets therefore go to the same destination. Until this rule existed `cmake --install`
# put NEITHER on the prefix (measured: 0 xpe_ai or onnxruntime files); the build tree only
# happened to place them together because every module shares one runtime output directory.
# A full build also needs onnxruntime.dll next to the worker, for the reason given where it is
# staged above.
include(GNUInstallDirs)
install(TARGETS ${MODULE_NAME} xpe_ai_worker
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WIN32 AND DEFINED ONNXRUNTIME_ROOT AND NOT (XPE_AI_STUB_BUILD OR NOT XPE_AI_USE_ONNXRUNTIME))
    install(FILES "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ---------------------------------------------------------------------------
# Unit Tests (Google Test)
# ---------------------------------------------------------------------------
set(_XPE_AI_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_AI_BUILD_TESTS ON)
endif()

# Test-only hook (QA-B-173, Codex audit #19): xpe_ai.dll calls a callback the tests register right after
# xpe_bone_suppress has locked the module mutex, so a test KNOWS a call holds the mutex instead of inferring
# it. With the option OFF the DLL contains neither the hook call nor the exported setter
# (xpe_ai_test_set_mutex_held_hook), and the tests that need it skip.
#
# DEFAULT: ON when the module's tests are built. Every preset in CMakePresets.json builds them
# (BUILD_TESTS=ON), and BUILD_TESTING is ON by default as well, so the default cannot tell a delivery
# build from a test build. A build whose xpe_ai.dll is DELIVERED must pass -DXPE_AI_TEST_HOOKS=OFF.
# (Checked with dumpbin /exports on both an OFF build of the tests and a no-tests build: no
# xpe_ai_test_* export. See .moai/reports/lane-post/QA-B-173/report.md.)
option(XPE_AI_TEST_HOOKS "Compile the test-only hooks into xpe_ai.dll; OFF for a delivery build" ${_XPE_AI_BUILD_TESTS})
if(XPE_AI_TEST_HOOKS)
    target_compile_definitions(xpe_ai PRIVATE XPE_AI_TEST_HOOKS=1)
    # QA-B-177: also let a test attach a sink to the DLL's own spdlog, to read the log text as emitted.
    if(TARGET spdlog::spdlog)
        target_compile_definitions(xpe_ai PRIVATE XPE_AI_TEST_LOG_CAPTURE=1)
    endif()
endif()

if(_XPE_AI_BUILD_TESTS)
    # Only add tests if source files exist (avoids configure errors when
    # test files have not been generated yet -- tests are written by xpe-qa).
    # Test sources live in modules/ai/tests/, like every other module.
    # They used to sit in the repository-root tests/ai_tests/ and were pulled in
    # from here by absolute path, which made the tree read as orphaned from the
    # root CMakeLists (#109). Moved in QA-B-33; the target is unchanged.
    set(XPE_AI_TEST_SOURCES
        test_ai_abi.cpp
        # QA-B-177: log text must carry values, not printf placeholders (DLL call sites + both macro branches)
        test_ai_log_format.cpp
        test_ai_log_macros.cpp
        test_ai_log_macros_spdlog.cpp
        test_ai_fallback.cpp
        test_ai_model_card.cpp
        test_ai_worker_isolation.cpp
        test_ai_ipc_bridge.cpp
        test_ai_model_versioning.cpp
        # #142 (QA-B-52): guard-page probe for a short dataSize.
        test_datasize_overread.cpp
        # #156 (QA-B-59): does each parameter reach the output
        test_parameter_dependency.cpp
        # #145 (QA-B-60): unread config keys are named, valid configs stay quiet
        test_config_warning.cpp
        # #130 (QA-B-160): the ONNX Runtime inference path, end to end
        test_onnx_session.cpp
        # #130 (QA-B-161): the first C ABI function wired to that path
        test_bone_suppress_abi.cpp
        # #130 (QA-B-168): SRS-ALERT-004 fires when DL was applied -- and, the
        # half that constrains the code, stays quiet on every path that
        # returned an image the model never touched
        test_alert_ai_processed.cpp
        # #130 (QA-B-169): the bridge against the REAL worker process -- the
        # first test that connects the two halves QA-B-167 found speaking
        # different message enums
        test_worker_protocol_conformance.cpp
        # #130 (QA-B-171): the bridge's time budget against a worker that stops
        # answering -- the conditions QA-B-170's five arms never injected
        test_ipc_deadline.cpp
        # #130 (QA-B-171B): the supervisor against the real worker -- restart on
        # death, kill on stall, exit kind read from the held process handle
        test_worker_supervisor.cpp
        # #130 (QA-B-171C): xpe_bone_suppress through the worker, opt-in; failures
        # reported and replaced by the in-process result
        test_bone_suppress_worker_path.cpp
    )

    # Check if test files exist in this module's tests/ directory
    set(_ALL_TEST_SOURCES_EXIST TRUE)
    foreach(_src ${XPE_AI_TEST_SOURCES})
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src}")
            set(_ALL_TEST_SOURCES_EXIST FALSE)
            message(STATUS "xpe_ai test source missing: modules/ai/tests/${_src}")
        endif()
    endforeach()

    if(_ALL_TEST_SOURCES_EXIST)
        # Resolve GTest: prefer already-available target, then FetchContent
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

        # Build full paths to test sources
        set(_XPE_AI_TEST_SOURCE_PATHS "")
        foreach(_src ${XPE_AI_TEST_SOURCES})
            list(APPEND _XPE_AI_TEST_SOURCE_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src}")
        endforeach()

        # The IPC bridge is module-internal and is NOT exported from xpe_ai.dll
        # (REQ-AI-003 keeps the named-pipe protocol private). Its tests therefore
        # compile the implementation directly instead of linking it across the
        # DLL boundary.
        add_executable(xpe_ai_tests
            ${_XPE_AI_TEST_SOURCE_PATHS}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ai_ipc_bridge.cpp
            # QA-B-171B: internal like the bridge, so compiled in rather than exported
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ai_worker_supervisor.cpp
            # #130 (QA-B-160): OnnxSession is a C++ class and xpe_ai.dll exports
            # a C ABI only, so it is compiled in here for the same reason the IPC
            # bridge is. Exporting a C++ class across the DLL boundary instead
            # would put std::vector and std::string in the module's public
            # contract, which the C ABI exists to keep out.
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ai_onnx_session.cpp
        )

        # #130 (QA-B-162): the test data directory is baked in at COMPILE time.
        #
        # It used to be the relative "tests/data/...", which resolves only when
        # the cwd is modules/ai -- i.e. only under ctest, which sets
        # WORKING_DIRECTORY. main went red because the #162 single-process step
        # runs every binary with `working-directory: modules/gsvg` (gsvg's own
        # tests carry the same relative-path habit, which is why that cwd is
        # what it is). Running xpe_ai_tests.exe from the repository ROOT failed
        # too, so this was never about gsvg: a cwd-dependent test breaks under
        # any harness that does not happen to pick the one directory it wants.
        #
        # Baked in rather than passed as an argument: nothing in the harness has
        # to know, and there is no cwd left to get wrong.
        target_compile_definitions(xpe_ai_tests PRIVATE
            XPE_AI_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")

        # QA-B-169: test_worker_protocol_conformance.cpp launches the real
        # worker. The path is the TARGET's output, resolved at generate time,
        # so it cannot drift from what the build actually produced; the source
        # path is for the one assertion that reads the worker's text. The
        # dependency is what guarantees the exe exists before any test runs --
        # without it the worker is only built when someone asks for it, and a
        # test that cannot launch it would fail for the wrong reason.
        target_compile_definitions(xpe_ai_tests PRIVATE
            XPE_AI_WORKER_EXE="$<TARGET_FILE:xpe_ai_worker>"
            XPE_AI_WORKER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/src/ai_worker_main.cpp")
        add_dependencies(xpe_ai_tests xpe_ai_worker)

        # QA-B-171B (Codex audit #11): a pipe server that answers a heartbeat WRONG on
        # request. The real worker never does, so a supervisor test that needs a wrong
        # answer cannot get one from it. Test-only: nothing ships it.
        add_executable(xpe_ai_fake_worker ${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_worker_main.cpp)
        target_include_directories(xpe_ai_fake_worker
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                    $<TARGET_PROPERTY:xpe_common,INTERFACE_INCLUDE_DIRECTORIES>)
        target_compile_features(xpe_ai_fake_worker PRIVATE cxx_std_17)
        target_compile_definitions(xpe_ai_tests PRIVATE
            XPE_AI_FAKE_WORKER_EXE="$<TARGET_FILE:xpe_ai_fake_worker>")
        add_dependencies(xpe_ai_tests xpe_ai_fake_worker)

        # The tests compile that file themselves, so they need the SAME build
        # mode the product got -- otherwise the stub/full arms diverge between
        # xpe_ai.dll and xpe_ai_tests and each would be testing the other's
        # contract.
        if(XPE_AI_STUB_BUILD OR NOT XPE_AI_USE_ONNXRUNTIME)
            target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_STUB_BUILD=1)
        else()
            target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_USE_ONNXRUNTIME=1)
            target_link_libraries(xpe_ai_tests PRIVATE onnxruntime)
        endif()

        if(XPE_AI_TEST_HOOKS)
            target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_TEST_HOOKS=1)   # the option above
            if(TARGET spdlog::spdlog)
                target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_TEST_LOG_CAPTURE=1)
            endif()
        endif()
        # QA-B-177b: test seam in ai_log.h (exe only, never the DLL) so allocation failures can be injected
        # strictly inside log calls.
        target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_LOG_TEST_SCOPE=1)
        if(TARGET spdlog::spdlog)
            # the exe compiles ai_onnx_session.cpp itself; give it the DLL's logging configuration so its LOG_* are live
            target_compile_definitions(xpe_ai_tests PRIVATE XPE_AI_USE_SPDLOG=1)
        endif()
        # QA-B-177: the macro-level log tests compile ai_log.h's spdlog branch themselves and need spdlog.
        if(TARGET spdlog::spdlog)
            target_link_libraries(xpe_ai_tests PRIVATE spdlog::spdlog)
        endif()

        target_include_directories(xpe_ai_tests
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

        target_link_libraries(xpe_ai_tests
            PRIVATE xpe_ai
                    GTest::gtest_main
        )

        # Windows: psapi.lib is required for GetProcessMemoryInfo
        # (heap-growth endurance measurement, #105 G3)
        if(WIN32)
            target_link_libraries(xpe_ai_tests PRIVATE psapi)
        endif()

        # #130 (QA-B-160). In a FULL build the loader needs onnxruntime.dll
        # beside the test executable. Without this the tests do not fail an
        # assertion -- they fail to start, which reads as an infrastructure
        # problem rather than a test result.
        if(WIN32 AND XPE_AI_USE_ONNXRUNTIME AND NOT XPE_AI_STUB_BUILD AND DEFINED ONNXRUNTIME_ROOT)
            add_custom_command(TARGET xpe_ai_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
                        "$<TARGET_FILE_DIR:xpe_ai_tests>"
                COMMENT "xpe_ai_tests: staging onnxruntime.dll next to the test exe")
        endif()

        target_compile_features(xpe_ai_tests PRIVATE cxx_std_17)

        if(MSVC)
            target_compile_options(xpe_ai_tests PRIVATE /W4)
        endif()
        xpe_target_warnings_as_errors(xpe_ai_tests)

        # LABELS/TIMEOUT restored from tests/ai_tests/CMakeLists.txt, which was
        # removed as a duplicate registration (#113 / QA-B-07). The SPEC label is
        # the traceability carrier; the deletion must not drop it.
        gtest_discover_tests(xpe_ai_tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            PROPERTIES
                # semicolons escaped: an unescaped "a;b;c" expands as a CMake list
                # and only the first element survives as LABELS (verified: --print-labels
                # showed only "unit"). The source file this was copied from was never
                # built, so the defect had never surfaced.
                LABELS "unit\;ai\;SPEC-XPE-P3-AI"
                TIMEOUT 30
        )

        message(STATUS "xpe_ai tests configured")
    else()
        message(STATUS "xpe_ai tests SKIPPED (test sources not yet created)")
    endif()
endif()
