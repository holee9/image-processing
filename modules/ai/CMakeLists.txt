# ============================================================================
# xpe_ai -- SPEC-XPE-P3-AI  IEC 62304 Class B
# AI-based image processing: body part recognition, stitching, bone
# suppression, DL denoising. Inference runs in sandboxed worker process.
#
# REQ-AI-001: Layer 1 dependency (xpe_common only, no lateral DLL deps)
# REQ-AI-003: Worker isolation (xpe_ai_worker.exe over named pipe IPC)
# REQ-AI-006: ONNX Runtime 1.20+ integration with multi-EP support
#
# Build modes:
#   XPE_AI_USE_ONNXRUNTIME=ON  -- Full ONNX Runtime integration (default OFF)
#   XPE_AI_STUB_BUILD=ON       -- Stub build without ONNX (default ON)
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
# ONNX Runtime via FetchContent (optional)
# ---------------------------------------------------------------------------
if(XPE_AI_USE_ONNXRUNTIME AND NOT XPE_AI_STUB_BUILD)
    include(FetchContent)

    # ONNX Runtime 1.20+ -- FetchContent downloads pre-built NuGet package
    # on Windows or builds from source on Linux.
    #
    # NOTE: ONNX Runtime does not provide a standard CMake config package.
    # The recommended approach is to use pre-built binaries from:
    #   https://github.com/microsoft/onnxruntime/releases
    #
    # Set ONNXRUNTIME_ROOT to the extracted directory before building,
    # or let this script attempt FetchContent.

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
        # Attempt to find onnxruntime via find_package or pkg-config
        find_package(onnxruntime QUIET CONFIG)

        if(NOT onnxruntime_FOUND)
            message(WARNING
                "ONNX Runtime not found. Set ONNXRUNTIME_ROOT or install onnxruntime. "
                "Falling back to stub build."
            )
            set(XPE_AI_STUB_BUILD ON CACHE BOOL "Fallback to stub" FORCE)
            set(XPE_AI_USE_ONNXRUNTIME OFF CACHE BOOL "ONNX unavailable" FORCE)
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
    target_compile_options(${MODULE_NAME} PRIVATE /W4 /wd4996 /wd4150)  # Disable unsafe function and incomplete type warnings
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
)

target_include_directories(${WORKER_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
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
endif()

# Optional spdlog for worker logging
if(TARGET spdlog::spdlog)
    target_link_libraries(${WORKER_NAME} PRIVATE spdlog::spdlog)
endif()

if(MSVC)
    target_compile_options(${WORKER_NAME} PRIVATE /W4 /wd4996 /wd4150)
else()
    target_compile_options(${WORKER_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

endif()  # End of temporarily disabled worker build

# ---------------------------------------------------------------------------
# Unit Tests (Google Test)
# ---------------------------------------------------------------------------
set(_XPE_AI_BUILD_TESTS OFF)
if(BUILD_TESTING OR BUILD_TESTS)
    set(_XPE_AI_BUILD_TESTS ON)
endif()

if(_XPE_AI_BUILD_TESTS)
    # Only add tests if source files exist (avoids configure errors when
    # test files have not been generated yet -- tests are written by xpe-qa).
    # Note: Test files are in tests/ai_tests/ directory, not modules/ai/tests/
    set(XPE_AI_TEST_SOURCES
        test_ai_abi.cpp
        test_ai_fallback.cpp
        test_ai_model_card.cpp
        test_ai_worker_isolation.cpp
        test_ai_ipc_bridge.cpp
        test_ai_model_versioning.cpp
    )

    # Check if test files exist in tests/ai_tests directory
    set(_ALL_TEST_SOURCES_EXIST TRUE)
    foreach(_src ${XPE_AI_TEST_SOURCES})
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/tests/ai_tests/${_src}")
            set(_ALL_TEST_SOURCES_EXIST FALSE)
            message(STATUS "xpe_ai test source missing: tests/ai_tests/${_src}")
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
            list(APPEND _XPE_AI_TEST_SOURCE_PATHS "${CMAKE_SOURCE_DIR}/tests/ai_tests/${_src}")
        endforeach()

        add_executable(xpe_ai_tests
            ${_XPE_AI_TEST_SOURCE_PATHS}
        )

        target_include_directories(xpe_ai_tests
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        target_link_libraries(xpe_ai_tests
            PRIVATE xpe_ai
                    GTest::gtest_main
        )

        target_compile_features(xpe_ai_tests PRIVATE cxx_std_17)

        if(MSVC)
            target_compile_options(xpe_ai_tests PRIVATE /W4 /WX- /wd4996)
        endif()

        gtest_discover_tests(xpe_ai_tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        message(STATUS "xpe_ai tests configured")
    else()
        message(STATUS "xpe_ai tests SKIPPED (test sources not yet created)")
    endif()
endif()
