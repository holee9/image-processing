# SPEC-XPE-GUI-IT Progress Tracking

> **갱신 2026-10-03 (GUI-C-207, #249):** 실태 대조 정정을 반영했다. 근거: `xpe-gui` `.moai/reports/lane-gui/GUI-C-207/report.md` (dev/gui `efbc74f8`). 아래의 2026-04-18 수치는 기록으로 남기고 정정 주석을 붙였다.

## Timeline

- **SPEC Created**: 2026-04-18
- **Implementation Started**: 2026-04-18
- **Implementation Complete**: 2026-04-18 — All AC done, 78/78 tests passing (all 15 AC listed; per-AC status in spec.md section 10 / GUI-C-207 report)
- **실태 대조 정정**: 2026-10-03 — 정의된 REQ 36개 중 구현됨 19 · 부분 14 · 없음 3 (spec.md §4.6, #249)

## Phase 1 Deliverables

### SPEC Document
- `.moai/specs/SPEC-XPE-GUI-IT/spec.md` — 671 lines (v1.1.0)
- `.moai/specs/SPEC-XPE-GUI-IT/research.md` — 297 lines (codebase analysis)

### Test Project Implementation

| Component | Count | Status |
|-----------|-------|--------|
| Test classes | 10 | Created |
| Test methods (fact+theory) | 78 | PASS |
| Fixtures | 2 | Created (NativeLibraryFixture, DllStagingFixture) |
| P/Invoke wrappers | 18 | Implemented (XpeCommonNative.cs) |
| Configuration files | 3 | Created (.nettoolconfig, Directory.Build.props, expected-versions.json) |

### Test Results Summary

| Category | Count | Time | Status |
|----------|-------|------|--------|
| **Smoke** | 20 | < 5s | PASS |
| **Functional** | 50 | < 60s | PASS |
| **Safety** | 8 | < 180s | PASS |
| **Optional (P1A-ready)** | 9 | — | RUN: 9 cases (CI stages xpe_preprocess.dll) *(2026-10-03 정정, 원문 "SKIP (awaiting xpe_preprocess.dll)")* |
| **Total** | **78/78** | < 2min | **GREEN** |

> 2026-10-03 주석(#249): "78/78 GREEN" 은 2026-04-18 수치다. 현재는 CI `dotnet-tests` 를 본다(2026-10-03 기준 650 통과 / 1 건너뜀, 대부분 다른 SPEC 의 시험).

### Acceptance Criteria Completion

> 2026-10-03 정정(#249): AC 는 15개다(원문 "AC-16" 행은 AC-15 의 오기). 상태 열은 GUI-C-207 실태 대조 기준이다.

| AC # | Title | Status |
|------|-------|--------|
| AC-1 | Project builds (net8.0, x64) | ✓ PASS |
| AC-2 | ABI parity (Marshal.SizeOf assertions) | PARTIAL (REQ-004 부분) |
| AC-3 | DLL resolution validation | PARTIAL (REQ-008·041·042 부분) |
| AC-4 | 15/15 PInvoke symbols covered | ✓ PASS |
| AC-5 | Uninitialized guard tests | ✓ PASS |
| AC-6 | Error code enum parity (18 codes) | PARTIAL (알 수 없는 코드는 비지 않음만 단언) |
| AC-7 | 1000-cycle endurance (no leak) | ✓ PASS |
| AC-8 | Mock backend exclusion | PARTIAL (REQ-007 부분) |
| AC-9 | No managed exceptions (3 + 3 scenarios, 원문 "20+") | PARTIAL |
| AC-10 | Alert queue edge cases | ✓ PASS |
| AC-11 | Log subsystem bounds | PARTIAL (REQ-031 초기화 후 상태 없음) |
| AC-12 | Performance gates (< 30s smoke, < 2min full) | PARTIAL (CI 가 재지 않음) |
| AC-13 | Optional P1A tests | ✓ RUN (9 cases) |
| AC-14 | IEC 62304 Class B trace | PARTIAL (063~065 누락) |
| AC-15 | DoD (all artifacts + MX tags) | PARTIAL (plan/acceptance/tasks·README·MX 미이행) |

## Known Limitations & Next Steps

### Current Limitations
1. **xUnit 2.9.3**: Early-return pattern for native-dependent tests (upgrade to xUnit 3.x for runtime Skip) — *2026-10-03 정정(#249): 낡은 기술이다. 지금은 `[SkippableFact]` + `SkipHelper`(16개 파일)를 쓴다. 고정 건너뜀은 `ErrorCodeMappingTests` 의 `NOT_IMPLEMENTED` 행 하나이며, 네이티브가 이미 매핑하므로 낡은 건너뜀이다.*
2. **P1A Optional Tests**: Skip cleanly when xpe_preprocess.dll not staged (no impact on baseline suite)
3. **Version Pinning**: expected-versions.json tracks xpe_common.dll major version (allows minor/patch freedom)

### Activation Dependencies
- ✓ xpe_common.dll (available, SPEC-XPE-P0 complete)
- ⏳ xpe_preprocess.dll (awaiting SPEC-XPE-P1A SUP-01 completion — NOW READY)

### Integration Ready
The test project is integrated with `clients/ImageProcTest.slnx` and ready for:
- CI/CD pipeline integration (dotnet test gating)
- P1A advanced test suite activation (when P1A SUP-01 complete)
- Release build validation

## IEC 62304 Class B Traceability

### Safety-Critical Requirements
- **REQ-GUI-IT-050**: No AccessViolation propagation (medical device critical failure)
- **REQ-GUI-IT-051**: 1000-cycle init/shutdown leak detection (Class B endurance requirement)
- **REQ-GUI-IT-006**: Error path validation without managed exceptions

### Requirement-to-Test Mapping
36 REQ defined; 33 mapped to a test class by requirement-matrix.json; 063..065 have no test. See spec.md §4.6 and §11. *(2026-10-03 정정, 원문 "All 53 REQ-GUI-IT-* requirements mapped to ≥ 1 automated test", #249)*

## Notes

- **Decision Point**: This SPEC activates after SPEC-XPE-P1A SUP-01 (which just completed). Optional tests can now run.
- **CI Gate**: `dotnet test` green required; failed test blocks PR merge.
- **Artifact Capture**: TRX + SARIF reports available (upload to gui-e2e-reports/ optional).
