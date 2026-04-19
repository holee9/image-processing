# XPE Build System Guide

## Build Environment

### Platform Requirements

**Windows (Primary)**:
- Visual Studio 2022 Professional
- CMake 3.20+
- vcpkg (for dependency management)
- Ninja (generator, bundled with VS2022)

**Linux (Secondary)**:
- GCC 11+ or Clang 13+
- CMake 3.20+
- vcpkg or system packages

**Dependencies**:
- spdlog v1.14.1 (logging)
- nlohmann/json v3.11.3 (config)
- fmt 11.0.2 (formatting)
- Eigen3 3.4.0 (matrix operations)
- Google Test (testing)

## Build Scripts

### Local Build (Recommended)

**PowerShell Script**:
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\ci\Invoke-LocalVsCommonBuild.ps1
```

**Features**:
- Finds VS2022 via `vswhere.exe`
- Uses bundled cmake/ninja/ctest
- Sets VCPKG_ROOT automatically
- Runs configure → build → test in one step

**Optional Flags**:
```powershell
-BuildDir build\my-local   # Custom output directory
-Clean                     # Clean build
```

### CMake Presets

**Default Preset**:
```bash
cmake --preset default
```

**Release Preset**:
```bash
cmake --preset release
```

## CMake Configuration

### Root CMakeLists.txt

**Key Options**:
```cmake
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)" ON)
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_GSVG "Build GSVG module" ON)
option(BUILD_PREPROCESS "Build preprocess module" ON)
option(BUILD_ENHANCE_BASIC "Build basic enhancement module" ON)
option(BUILD_ENHANCE_ADVANCED "Build advanced enhancement module" ON)
option(BUILD_AI "Build AI module" ON)
option(BUILD_DISPLAY "Build display module" ON)
option(BUILD_DICOM "Build DICOM module" ON)
option(XPE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(BUILD_COVERAGE "Enable code coverage" OFF)
```

### Module CMakeLists.txt Pattern

```cmake
# modules/enhance_advanced/CMakeLists.txt
add_library(xpe_enhance_advanced SHARED
    src/xpe_enhance_advanced.cpp
    src/enhance_advanced.cpp
    # ... more sources
)

target_include_directories(xpe_enhance_advanced
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xpe_enhance_advanced
    PUBLIC
        xpe_common
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Eigen3::Eigen
)

# DLL export
target_compile_definitions(xpe_enhance_advanced PRIVATE XPE_DLL_EXPORT)
```

### Test CMakeLists.txt Pattern

```cmake
# tests/enhance_advanced_tests/CMakeLists.txt
add_executable(enhance_advanced_tests
    test_api_header.cpp
    test_lifecycle.cpp
    # ... more tests
)

target_link_libraries(enhance_advanced_tests
    PRIVATE
        xpe_enhance_advanced
        GTest::gtest_main
)

gtest_discover_tests(enhance_advanced_tests)
```

## Dependency Management

### FetchContent (Primary)

```cmake
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(spdlog)
```

### vcpkg (Secondary)

**Installation**:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

**Usage**:
```bash
vcpkg install spdlog:x64-windows
vcpkg install nlohmann-json:x64-windows
vcpkg install fmt:x64-windows
vcpkg install eigen3:x64-windows
vcpkg install gtest:x64-windows
```

**CMake Integration**:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ..
```

## Build Outputs

### Directory Structure

```
build/vs2022/
├── bin/                 # Executables
│   ├── enhance_advanced_tests.exe
│   └── xpe_calib_*.exe
├── lib/                 # DLLs and import libraries
│   ├── xpe_common.dll
│   ├── xpe_enhance_advanced.dll
│   └── ...
├── _deps/               # FetchContent dependencies
└── Testing/             # Test results
```

### Output Artifacts

**DLLs**:
- `xpe_common.dll`
- `xpe_preprocess.dll`
- `xpe_enhance_basic.dll`
- `xpe_enhance_advanced.dll`
- `xpe_display.dll`
- `xpe_dicom.dll`
- `xpe_ai.dll`
- `xpe_gsvg.dll`

**Import Libraries**:
- `xpe_common.lib`
- `xpe_enhance_advanced.lib`
- ...

**Test Executables**:
- `enhance_advanced_tests.exe`
- `common_tests.exe`
- `common_smoke_tests.exe`
- `common_unit_tests.exe`

## Compilation Flags

### Windows (MSVC)

**Common Flags**:
```cmake
/W4                 # Warning level 4
WX                  # Warnings as errors (if XPE_WARNINGS_AS_ERRORS=ON)
MP                  # Multi-processor compilation
permissive-         # Strict standard conformance
Zc:__cplusplus      # Enable correct __cplusplus macro
```

**C++ Standard**:
```cmake
/std:c++17          # C++17 standard
Zc:__cplusplus      # Set __cplusplus correctly
```

### Linux (GCC/Clang)

**Common Flags**:
```cmake
-Wall -Wextra       # All warnings
-Wpedantic          # Pedantic warnings
-Werror             # Warnings as errors (if XPE_WARNINGS_AS_ERRORS=ON)
-march=native       # CPU-specific optimizations
```

**Coverage Flags**:
```cmake
--coverage -O0 -g   # Coverage instrumentation
```

## SIMD Configuration

### AVX2 Detection

```cmake
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#include <immintrin.h>
int main() {
    __m256i a = _mm256_set1_epi32(1);
    return 0;
}
" HAS_AVX2)

if(HAS_AVX2)
    target_compile_definitions(xpe_enhance_advanced PRIVATE XPE_HAS_AVX2)
endif()
```

### Runtime Dispatch

```cpp
// Runtime CPU feature detection
#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

bool has_avx2() {
#if defined(_MSC_VER)
    return __cpuidex() & (1 << 5);  // Check AVX2 bit
#else
    unsigned int eax, ebx, ecx, edx;
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    return ecx & (1 << 5);  // Check AVX2 bit
#endif
}
```

## Installation

### CMake Install

```bash
cmake --install build/vs2022 --prefix install
```

**Install Layout**:
```
install/
├── bin/
│   ├── *.dll
│   └── *.exe
├── lib/
│   └── *.lib
└── include/
    └── xpe/
        ├── common/
        ├── enhance_advanced/
        └── ...
```

### Packaging

**CPack Configuration**:
```cmake
set(CPACK_PACKAGE_NAME "XPE")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "ZIP;NSIS")

include(CPack)
```

**Build Package**:
```bash
cpack --config build/vs2022/CPackConfig.cmake
```

## Troubleshooting

### Common Issues

**"cl.exe not found"**:
- Solution: Use `Invoke-LocalVsCommonBuild.ps1` script
- Manual: Run from Visual Studio Developer Command Prompt

**"spdlog not found"**:
- Solution: Ensure vcpkg is installed
- Alternative: FetchContent will download automatically

**"Cannot open include file 'Eigen/Dense'"**:
- Solution: Install Eigen3 via vcpkg or ensure FetchContent works
- Check: `build/vs2022/_deps/eigen-src` exists

**"LNK2038: mismatch detected"**:
- Cause: Mixing debug/release libraries
- Solution: Clean build and rebuild with consistent configuration

### Debug Build

```bash
cmake --preset debug
cmake --build build/vs2022 --config Debug
```

### Verbose Output

```bash
cmake --build build/vs2022 --verbose
```

### Clean Build

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\ci\Invoke-LocalVsCommonBuild.ps1 -Clean
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Build

on: [push, pull_request]

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v3

    - name: Configure
      run: cmake --preset default

    - name: Build
      run: cmake --build build/vs2022 --config Release

    - name: Test
      run: ctest --preset default -C Release

    - name: Package
      run: cmake --build build/vs2022 --target package
```

---

**Last Updated**: 2026-04-19
**Specification Version**: 0.1.0
