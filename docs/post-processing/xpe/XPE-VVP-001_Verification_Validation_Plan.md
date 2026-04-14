# 소프트웨어 검증 & 밸리데이션 계획

**문서 ID:** XPE-VVP-001 v1.0  
**IEC 62304 Clause:** 5.5.1 — 5.5.5, 5.6.1 — 5.6.7, 5.7.1 — 5.7.5  
**안전 분류:** Class B  
**날짜:** 2026-04-03  
**작성자:** XPE 개발 팀  
**승인:** __________________ 날짜: __________  

---

## 1. 목적

XPE 소프트웨어의 unit 검증, 통합 테스트, 시스템 테스트 활동을 정의한다.

## 2. Unit 검증 (5.5)

### 2.1 프로세스 (5.5.2)

| Item | Description |
|------|-------------|
| Framework | Google Test 1.14 (C++), NUnit 4.x (C#) |
| Coverage tool | gcov + lcov (C++), dotCover (C#) |
| Static analysis | cppcheck, clang-tidy (MISRA C++ subset) |
| Memory check | AddressSanitizer (ASan), LeakSanitizer (LSan) |
| Execution | CI pipeline (Gitea Actions) — every commit to develop/feature |

### 2.2 Acceptance Criteria (5.5.3)

| Criterion | Target | Blocking |
|-----------|--------|:--------:|
| Statement coverage | ≥ 80% per unit | ✓ |
| Branch coverage | ≥ 70% per unit | ✓ |
| All tests pass | 100% (zero failures) | ✓ |
| Coding standard | Zero critical violations | ✓ |
| Memory leaks | Zero (ASan clean) | ✓ |
| Static analysis | Zero critical/high findings | ✓ |

### 2.3 Unit Test Naming Convention

```
UT-{UnitID}-{Seq:3d}
Example: UT-1.1-001  (OffsetCorrector, test case 001)
```

### 2.4 Verification Execution (5.5.5)

각 software unit(SWU-x.y)에 대해:

1. Test suite 작성 → PR에 포함 (코드와 동시 제출)
2. CI에서 자동 실행 (build → test → coverage → static analysis)
3. Coverage report + test report → CI artifact 보관
4. Acceptance criteria 미달 시 PR merge 차단
5. Code review (≥1 reviewer) 통과 필수

## 3. Integration Testing (5.6)

### 3.1 Integration Strategy (5.6.1)

**방식:** Bottom-up

| Integration Level | Scope | Pre-condition |
|:-:|---------|--------------|
| I-1 | SWU-1.1→1.4 (Pre-Processing chain) | All Phase 1 units pass UT |
| I-2 | SWI-1 → SWI-2 (Pre → Core) | I-1 pass |
| I-3 | SWI-2 → SWI-3 (Core → Display) | I-2 pass |
| I-4 | SWI-1 → SWI-4 (Full pipeline) | I-3 pass |
| I-5 | SWI-4 ↔ External (DICOM network) | I-4 pass |

### 3.2 Integration Verification (5.6.2, 5.6.3)

| Test ID | Description | Input | Expected | Pass Criteria |
|---------|------------|-------|----------|---------------|
| IT-001 | Offset→Gain chain 정합성 | Synthetic raw + cal data | Pre-calculated reference | PSNR ≥ 60dB |
| IT-002 | Full pre→core flow | Phantom image | Visual IQ ≥ 3.5/5 | Expert review |
| IT-003 | Pipeline → DICOM output | Full pipeline input | Conformant DICOM | DVTk pass |
| IT-004 | W/L interactive response | W/L drag event | Display update | ≤ 16ms measured |
| IT-005 | Error propagation | Corrupted cal data | Graceful error | No crash, alert shown |
| IT-006 | Memory stability | 100 images sequential | No growth > 5% | Measured RSS |
| IT-007 | Thread safety | 2 concurrent pipelines | Both complete | No race, no crash |
| IT-008 | SOUP interface | OpenCV CLAHE call | Correct output | Pixel-exact vs reference |

### 3.3 Regression Testing (5.6.4)

- 모든 IT는 regression suite에 자동 포함
- Release branch merge 전 full regression 필수
- Regression failure → release 차단
- 신규 IT 추가 시 기존 regression suite에 즉시 편입

### 3.4 Test Record Contents (5.6.5)

각 실행에 대해 기록:

| Field | Description |
|-------|-------------|
| Test ID | IT-xxx |
| Date | 실행 일시 |
| SW Version | Git commit SHA |
| Environment | OS, HW, compiler version |
| Result | Pass / Fail |
| Measured values | 해당 시 수치 (PSNR, latency 등) |
| Anomalies | Problem report reference (있을 경우) |
| Executor | 이름 |

### 3.5 Problem Resolution (5.6.6)

Integration test 실패 시 XPE-SPR-001 절차에 따라 처리한다.

### 3.6 Test Procedure Verification (5.6.7)

Integration test procedure 자체를 formal review로 검증한다. Reviewer는 test가 해당 interface를 충분히 cover하는지 확인한다.

## 4. System Testing (5.7)

### 4.1 System Test Establishment (5.7.1)

모든 SRS 요구사항에 대해 ≥1 system test case를 정의한다.

| SRS Req | System Test ID | Method | Pass Criteria |
|---------|---------------|--------|---------------|
| SRS-FUNC-001 | ST-001 | Synthetic data + ref comparison | PSNR ≥ 60dB |
| SRS-FUNC-002 | ST-002 | Flat-field uniformity test | Non-uniformity < 2% |
| SRS-FUNC-003 | ST-003 | Known bad pixel injection | All defects corrected |
| SRS-FUNC-004 | ST-004 | Sequential exposure ghost test | Ghost ≤ 10% of initial |
| SRS-FUNC-010 | ST-010 | Log transform linearity | R² ≥ 0.999 |
| SRS-FUNC-011 | ST-011 | Noise reduction SNR improvement | SNR gain ≥ 3dB |
| SRS-FUNC-012 | ST-012 | Clinical image set (N=50) | Reader IQ ≥ 3.5/5 |
| SRS-FUNC-013 | ST-013 | Edge enhancement artifact check | No overshoot > 5% |
| SRS-FUNC-020 | ST-020 | Modality LUT calculation | Pixel exact ± 0 |
| SRS-FUNC-021 | ST-021 | VOI W/L preset application | Output ± 1 gray level |
| SRS-FUNC-022 | ST-022 | GSDF P-value output | Δ JND ≤ 1% |
| SRS-FUNC-030 | ST-030 | DICOM conformance | DVTk full pass |
| SRS-FUNC-031 | ST-031 | GSPS create + apply | Round-trip pixel identical |
| SRS-SAFE-001 | ST-SAFE-001 | Raw preservation after processing | Byte-identical raw |
| SRS-SAFE-003 | ST-SAFE-003 | Force defect correction failure | Warning within 2s |
| SRS-SAFE-006 | ST-SAFE-006 | W/L out-of-range | Warning displayed |
| SRS-SAFE-008 | ST-SAFE-008 | AI output label | "AI-processed" visible |
| SRS-SAFE-009 | ST-SAFE-009 | Toggle original/processed | Switch within 100ms |
| SRS-PERF-001 | ST-PERF-001 | Pre-processing timing | ≤ 500ms |
| SRS-PERF-002 | ST-PERF-002 | Full pipeline timing | ≤ 3s |
| SRS-PERF-003 | ST-PERF-003 | W/L interactive timing | ≤ 16ms |
| SRS-PERF-004 | ST-PERF-004 | Peak memory | ≤ 2GB |

### 4.2 Problem Resolution (5.7.2)

System test 실패 시 XPE-SPR-001 절차에 따라 처리한다.

### 4.3 Retest After Change (5.7.3)

변경된 코드에 대해 관련 system test + full regression 재실행한다.

### 4.4 Test Procedure Verification (5.7.4)

System test procedure는 formal review로 검증한다. SRS → ST 1:1 매핑 완전성을 RTM으로 확인한다.

### 4.5 System Test Record Contents (5.7.5)

| Field | Description |
|-------|-------------|
| Test ID | ST-xxx |
| SW Version | Release candidate version + Git tag |
| Environment | Full HW/SW spec |
| Test Data | Input dataset ID |
| Result | Pass / Fail + measured values |
| Anomalies | Problem report ref |
| Tester | Name + signature |
| Date | Execution date |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-VVP-001 v1.0*
