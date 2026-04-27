# Gate G1b → G2 Performance Verification Plan

**Document ID**: DEV-GATE-G1B-G2-001
**Version**: 1.0.0
**Date**: 2026-04-23
**Status**: Active

---

## Overview

This document defines the performance verification plan for Gate G1b → G2 transition.
The gate validates that the Phase 1b pipeline meets performance requirements for production deployment.

---

## Performance Requirements

| Metric | Target | Measurement Method |
|--------|--------:|-------------------|
| **E2E Latency** | < 3000ms | 3072×3072 Raw DICOM → Full pipeline timing |
| **Peak Memory** | <= 190MB | 1000 frames continuous processing |
| **Test Pass Rate** | 100% | All unit and integration tests PASS |

---

## Verification Environment

### Hardware Requirements
- CPU: x86_64, AVX2 support required (Haswell+ 2013)
- RAM: 8GB minimum, 16GB recommended
- Storage: SSD for DICOM I/O

### Software Requirements
- Windows 11 Pro 10.0.26200+
- Visual Studio 2022 Professional
- CMake 3.20+
- vcpkg dependencies: dcmtk, openjpeg, spdlog, fmt

### Build Configuration
```powershell
cmake --preset ci-common -DBUILD_DICOM=ON -DBUILD_PREPROCESS=ON -DBUILD_ENHANCE_BASIC=ON -DBUILD_DISPLAY=ON
cmake --build build\ci-common --config RelWithDebInfo
```

---

## Verification Steps

### Step 1: Unit Test Verification

**Objective**: Verify all Phase 1b module tests pass 100%.

| Module | Test Executable | Test Count | Pass Criteria |
|--------|----------------|-----------:|---------------|
| xpe_preprocess | xpe_preprocess_tests.exe | 202/202 | ✅ PASS |
| xpe_enhance_basic | xpe_enhance_basic_tests.exe | 67/67 | ✅ PASS |
| xpe_display | xpe_display_tests.exe | 48/48 | ✅ PASS |
| xpe_dicom | xpe_dicom_tests.exe | 35/35 | ✅ PASS |

**Execution**:
```powershell
ctest --test-dir build\ci-common --output-on-failure --build-config RelWithDebInfo
```

**Acceptance Criteria**:
- [ ] All 403 tests PASS (202 + 67 + 48 + 35)
- [ ] Zero test failures
- [ ] Zero memory leaks detected
- [ ] All static analysis warnings resolved

---

### Step 2: Integration Test Verification

**Objective**: Verify ImageProcTest integration tests pass 100%.

**Execution**:
```powershell
dotnet build clients\ImageProcTest\ImageProcTest.csproj -c RelWithDebInfo
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll --run-preprocess-fixture-e2e
```

**Acceptance Criteria**:
- [ ] 78/78 integration tests PASS
- [ ] E2E fixture reports generated
- [ ] Raw SHA-256 preservation verified
- [ ] Calibration loads successful
- [ ] Native stage execution successful

---

### Step 3: Performance Measurement

**Objective**: Measure E2E latency for 3072×3072 Raw DICOM.

**Test Data**:
- Input: 3072×3072 Raw DICOM (16-bit grayscale)
- Calibration: Null/identity (DegradedMode)
- Iterations: 100 frames (warmup: 10 frames, measured: 90 frames)

**Execution**:
```powershell
# Use ImageProcTest E2E fixture with timing measurement
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll `
  --run-preprocess-fixture-e2e `
  --input "testdata\3072x3072_raw.dcm" `
  --iterations 100 `
  --warmup 10
```

**Acceptance Criteria**:
- [ ] Average E2E latency < 3000ms
- [ ] P95 latency < 3500ms
- [ ] P99 latency < 4000ms
- [ ] Zero timeout errors

---

### Step 4: Memory Profiling

**Objective**: Verify peak memory usage <= 190MB for 1000 frames.

**Execution**:
```powershell
# Use Windows Performance Monitor or custom memory profiler
# Measure private bytes during 1000-frame continuous processing

dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll `
  --run-preprocess-fixture-e2e `
  --frames 1000 `
  --measure-memory
```

**Acceptance Criteria**:
- [ ] Peak memory <= 190MB
- [ ] Memory growth < 1MB per 100 frames (no leaks)
- [ ] Zero OutOfMemoryException

---

## Full Pipeline Integration Test

**Pipeline Sequence**:
```
Raw DICOM Input
  → xpe_preprocess (Offset/Gain/Defect/Ghost correction)
  → xpe_enhance_basic (Log transform/Noise reduction/Contrast/Edge/Exposure Index)
  → xpe_display (Modality LUT/VOI LUT/Presentation LUT)
  → xpe_dicom (DICOM Write)
→ DICOM Output
```

**Verification Command**:
```powershell
dotnet clients\ImageProcTest\bin\RelWithDebInfo\net8.0-windows\ImageProcTest.dll `
  --run-full-pipeline-e2e `
  --input "testdata\3072x3072_raw.dcm" `
  --output "output\processed.dcm"
```

**Acceptance Criteria**:
- [ ] All stages execute successfully
- [ ] Output DICOM is valid (validator passes)
- [ ] E2E latency < 3000ms
- [ ] Peak memory <= 190MB

---

## Gate Decision Criteria

### PASS Conditions

All of the following must be met:
1. ✅ All 403 unit tests PASS (100%)
2. ✅ All 78 integration tests PASS (100%)
3. ✅ E2E latency < 3000ms (3072×3072)
4. ✅ Peak memory <= 190MB (1000 frames)
5. ✅ Zero critical bugs or regressions

### FAIL Conditions

Any of the following:
- ❌ Any test failure
- ❌ E2E latency >= 3000ms
- ❌ Peak memory > 190MB
- ❌ Memory leak detected
- ❌ Critical bug or crash

---

## Issue Resolution

### Common Issues and Workarounds

| Issue | Symptom | Workaround |
|-------|---------|------------|
| DCMTK not found | CMake error: "Could not find dcmtk" | Install dcmtk via vcpkg: `vcpkg install dcmtk:x64-windows` |
| Missing test data | Test file not found error | Generate test data using ImageProcTest fixture |
| Timeout | E2E test timeout after 60s | Check DICOM file size, reduce iterations |
| Memory leak | Memory growth > 1MB/100 frames | Run with memory profiler, identify leaking allocator |

---

## Reporting

### Test Report Template

```markdown
## Gate G1b → G2 Verification Report

**Date**: YYYY-MM-DD
**Build**: ci-common-RelWithDebInfo
**Commit**: <git-sha>

### Unit Test Results
- xpe_preprocess_tests: 202/202 PASS ✅
- xpe_enhance_basic_tests: 67/67 PASS ✅
- xpe_display_tests: 48/48 PASS ✅
- xpe_dicom_tests: 35/35 PASS ✅
**Total**: 403/403 PASS (100%)

### Integration Test Results
- ImageProcTest.IntegrationTests: 78/78 PASS ✅

### Performance Results
- E2E Latency (3072×3072): XXXXms (target: <3000ms)
- Peak Memory (1000 frames): XXXMB (target: <=190MB)

### Gate Decision
**[ PASS | FAIL ]**

### Notes
<Additional observations, issues, or recommendations>
```

---

## Version History

| Date | Version | Changes |
|------|--------|---------|
| 2026-04-23 | 1.0.0 | Initial creation |
