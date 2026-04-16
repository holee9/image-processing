## Task Decomposition
SPEC: SPEC-XPE-P0

## Task Summary

| Task ID | Description | Requirement | Dependencies | Planned Files | Status |
|---------|-------------|-------------|--------------|---------------|--------|
| T-001 | Module directory scaffolding | REQ-P0-032, REQ-P0-033 | - | modules/enhance_advanced/CMakeLists.txt<br>modules/ai/CMakeLists.txt<br>modules/display/CMakeLists.txt<br>modules/dicom/CMakeLists.txt<br>gsvg/CMakeLists.txt | pending |
| T-002 | Test infrastructure integration | REQ-P0-005, REQ-P0-006, REQ-P0-007 | - | tests/CMakeLists.txt<br>tests/common/CMakeLists.txt<br>tests/common/test_xpe_common.cpp<br>CMakePresets.json (coverage flags) | pending |
| T-003 | C++ standard version unification | REQ-P0-001 | - | modules/common/CMakeLists.txt | pending |
| T-004 | Export verification and cleanup | REQ-P0-008 | T-002 | modules/common/include/xpe_common_api.h<br>modules/common/src/xpe_common.cpp | pending |
| T-005 | Pack=8 static_assert | REQ-P0-009 | - | modules/common/include/xpe_types.h | pending |
| T-006 | C# WPF scaffolding | REQ-P0-029, REQ-P0-030, REQ-P0-031 | T-001, T-002, T-004 | clients/ImageProcTest/ImageProcTest.csproj<br>clients/ImageProcTest/PInvokeWrapper.cs<br>clients/ImageProcTest/MainWindow.xaml | pending |
| T-007 | CI pipeline setup | REQ-P0-001 | T-001, T-002 | .github/workflows/ci.yml | pending |

## Detailed Task Specifications

### T-001: Module Directory Scaffolding
**Priority**: Must
**Requirements**: REQ-P0-032, REQ-P0-033
**Dependencies**: None

**Description**: Create 5 missing module directories with placeholder CMakeLists.txt files

**Planned Files**:
- `modules/enhance_advanced/CMakeLists.txt`
- `modules/ai/CMakeLists.txt`
- `modules/display/CMakeLists.txt`
- `modules/dicom/CMakeLists.txt`
- `gsvg/CMakeLists.txt`

**Acceptance Criteria**:
- [ ] All 5 module directories exist
- [ ] Each directory contains CMakeLists.txt with minimal shared library target
- [ ] Each module exports placeholder version function
- [ ] `cmake --preset release` builds all 8 modules successfully
- [ ] Root CMakeLists.txt optional subdirectory pattern works

**Implementation Notes**:
- Use `add_library(${MODULE_NAME} SHARED)` for each module
- Export version function: `extern "C" XPE_API const char* ${MODULE_NAME}_version()`
- Follow pattern from existing modules/common/CMakeLists.txt

---

### T-002: Test Infrastructure Integration
**Priority**: Must
**Requirements**: REQ-P0-005, REQ-P0-006, REQ-P0-007
**Dependencies**: None

**Description**: Integrate Google Test with CTest and add coverage reporting

**Planned Files**:
- `tests/CMakeLists.txt` (refactor)
- `tests/common/CMakeLists.txt` (new)
- `tests/common/test_xpe_common.cpp` (move from modules/common/tests/)
- `CMakePresets.json` (add coverage flags)

**Acceptance Criteria**:
- [ ] `ctest --preset default` discovers and runs all Google Test suites
- [ ] Coverage report generates successfully (OpenCPPCoverage for Windows)
- [ ] xpe_common coverage >= 85%
- [ ] Test structure: root tests/ contains both common/ (Google Test) and common_smoke/ (integration)

**Implementation Notes**:
- Move `modules/common/tests/test_xpe_common.cpp` to `tests/common/test_xpe_common.cpp`
- Use FetchContent for gtest in tests/CMakeLists.txt
- Add coverage preset to CMakePresets.json:
  ```json
  {
    "name": "coverage",
    "displayName": "Coverage",
    "binaryDir": "${sourceDir}/build/coverage",
    "cacheVariables": {
      "CMAKE_BUILD_TYPE": "Debug",
      "ENABLE_COVERAGE": true
    }
  }
  ```

---

### T-003: C++ Standard Version Unification
**Priority**: Must
**Requirements**: REQ-P0-001
**Dependencies**: None

**Description**: Fix C++ standard version mismatch between root and modules/common

**Planned Files**:
- `modules/common/CMakeLists.txt`

**Acceptance Criteria**:
- [ ] Root CMakeLists.txt sets C++17 (already done)
- [ ] modules/common/CMakeLists.txt inherits C++17 (remove C++23 override)
- [ ] All modules use consistent C++17 standard

**Implementation Notes**:
- Remove `set(CMAKE_CXX_STANDARD 23)` from modules/common/CMakeLists.txt
- Let root CMakeLists.txt's C++17 setting propagate

---

### T-004: Export Verification and Cleanup
**Priority**: Must
**Requirements**: REQ-P0-008
**Dependencies**: T-002 (build environment normalized)

**Description**: Verify xpe_common.dll exports exactly 18 public API functions

**Planned Files**:
- `modules/common/include/xpe_common_api.h`
- `modules/common/src/xpe_common.cpp`

**Acceptance Criteria**:
- [ ] `dumpbin /exports xpe_common.dll` shows exactly 18 public API functions
- [ ] Internal test functions are either:
  - Option A: Exported with separate XPE_TEST_API macro and documented
  - Option B: Not exported (removed from XPE_API macro)
- [ ] API count matches SPEC-XPE-P0 requirement (REQ-P0-008)

**Implementation Notes**:
- Run `dumpbin /exports build/common/Debug/xpe_common.dll` to verify
- Consider creating XPE_TEST_API macro for test-only exports
- Update xpe_common_api.h documentation to clarify public vs test API

---

### T-005: Pack=8 Static Assert
**Priority**: Must
**Requirements**: REQ-P0-009
**Dependencies**: None

**Description**: Add compile-time verification of struct packing

**Planned Files**:
- `modules/common/include/xpe_types.h`

**Acceptance Criteria**:
- [ ] `static_assert(sizeof(XpeImageBuffer) == expected_value)` compiles
- [ ] `static_assert(sizeof(XpeImageMetadata) == expected_value)` compiles
- [ ] All structs verified Pack=8 compatible
- [ ] P/Invoke compatibility verified

**Implementation Notes**:
- Add static_assert after each struct definition
- Use alignas(8) if needed to ensure proper alignment
- Calculate expected size manually for verification

---

### T-006: C# WPF Scaffolding
**Priority**: Must
**Requirements**: REQ-P0-029, REQ-P0-030, REQ-P0-031
**Dependencies**: T-001, T-002, T-004 (xpe_common.dll must be buildable and testable)

**Description**: Create ImageProcTest WPF project with P/Invoke bridge

**Planned Files**:
- `clients/ImageProcTest/ImageProcTest.csproj`
- `clients/ImageProcTest/PInvokeWrapper.cs`
- `clients/ImageProcTest/MainWindow.xaml`
- `clients/ImageProcTest/MainWindow.xaml.cs`
- `clients/ImageProcTest/App.xaml`

**Acceptance Criteria**:
- [ ] `dotnet build` creates ImageProcTest.exe successfully
- [ ] P/Invoke wrapper declares all 18 xpe_common.dll functions
- [ ] Struct layouts use [StructLayout(LayoutKind.Sequential, Pack=8)]
- [ ] MainWindow.xaml displays version string from xpe_version()
- [ ] xpe_init() called on startup, xpe_shutdown() on exit
- [ ] xpe_common.dll loads without DllNotFoundException

**Implementation Notes**:
- Use .NET 8 WPF project
- P/Invoke declaration example:
  ```csharp
  [StructLayout(LayoutKind.Sequential, Pack=8)]
  public struct XpeImageBuffer { ... }

  [DllImport("xpe_common.dll", CallingConvention = CallingConvention.Cdecl)]
  public static extern XpeErrorCode xpe_init();
  ```
- Place xpe_common.dll in build output directory or use PATH

---

### T-007: CI Pipeline Setup
**Priority**: Should
**Requirements**: REQ-P0-001
**Dependencies**: T-001, T-002 (all tests must be integrated)

**Description**: Create CI pipeline for automated build, test, and coverage

**Planned Files**:
- `.github/workflows/ci.yml` (or `.gitlab-ci.yml` for GitLab)

**Acceptance Criteria**:
- [ ] CI pipeline triggers on push to main and pull requests
- [ ] Pipeline stages: Configure, Build, Test, Coverage
- [ ] CTest results uploaded as artifacts
- [ ] Coverage report uploaded as artifacts
- [ ] Pipeline passes green on main branch

**Implementation Notes**:
- Use GitHub Actions or GitLab CI based on platform
- Example stages:
  ```yaml
  stages:
    - configure
    - build
    - test
    - coverage
  ```
- Set coverage threshold gate at 85%

---

## Coverage Verification

All SPEC requirements are covered by at least one task:

| Requirement | Covered By Task |
|-------------|------------------|
| REQ-P0-001 through REQ-P0-007 | T-001, T-002, T-003, T-007 |
| REQ-P0-008 | T-004 |
| REQ-P0-009 | T-005 |
| REQ-P0-010 through REQ-P0-022 | T-004 (existing implementation) |
| REQ-P0-023 through REQ-P0-025 | T-002 (existing implementation) |
| REQ-P0-026 through REQ-P0-028a | T-002 (existing implementation) |
| REQ-P0-029 through REQ-P0-031 | T-006 |
| REQ-P0-032 through REQ-P0-033 | T-001 |

## Execution Order

```
Tier 1 (Parallel):
  T-001: Module scaffolding
  T-002: Test infrastructure
  T-003: C++ standard unification

Tier 2 (Sequential):
  T-004: Export verification (depends on T-002)
  T-005: Pack=8 static_assert

Tier 3 (Sequential):
  T-006: C# WPF (depends on T-001, T-002, T-004)

Tier 4 (Sequential):
  T-007: CI pipeline (depends on T-001, T-002)
```

---

*Document End -- SPEC-XPE-P0 Task Decomposition*
