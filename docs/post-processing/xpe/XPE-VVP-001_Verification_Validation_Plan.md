# Software Verification & Validation Plan

**Document ID:** XPE-VVP-001 v1.1  
**IEC 62304 Clause:** 5.5.1 — 5.5.5, 5.6.1 — 5.6.7, 5.7.1 — 5.7.5  
**Safety Classification:** Class B  
**Date:** 2026-04-15  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________

> **Parent System Document**: XPE-SVVP-001 v1.0.0 — System Verification & Validation Plan
> (`docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md`)
>
> **Scope within hierarchy**: XPE-VVP-001 covers Level 1 (Unit Verification), Level 2 (Integration Verification),
> and Level 3 (System Verification) as defined in XPE-SVVP-001 §2. Multi-package (Level 4), Clinical (Level 5),
> and Field (Level 6) validation activities are governed exclusively by XPE-SVVP-001.  

---

## 1. Purpose

XPE 소프트웨어의 unit verification, integration testing, system testing 활동을 정의한다.

## 2. Unit Verification (5.5)

### 2.1 Process (5.5.2)

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

### 3.2.1 Preprocessing Raw/Calibration E2E Verification Addendum (2026-04-16)

The preprocessing integration suite shall execute the protocol defined in `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`.

| Test ID | Description | Input | Expected | Pass Criteria |
|---------|-------------|-------|----------|---------------|
| IT-PRE-E2E-001 | Fixture scan and Git policy | `tests/test_data/calibration_cases` | Case inventory, file size, SHA-256, and `.raw` ignore status captured | All local raw files ignored by Git; every copied raw file has expected size/dimensions or explicit exception |
| IT-PRE-E2E-002 | Synthetic oracle preprocessing | generated offset/gain/nonlinearity/defect/lag micro-cases | deterministic expected outputs | RMSE within oracle tolerance; no NaN/Inf; input SHA preserved |
| IT-PRE-E2E-003 | Real fixture calibration effect | local raw image and matching calibration folder | measurable before/after detector-domain improvement | Dark/gain/defect metrics pass XPE-PRE-E2E-001 gates or are marked `degraded_evidence=true` with reason |
| IT-PRE-E2E-004 | Reference-output comparison | fixture image with known output such as `*_oc.raw` | golden/reference comparison | RMSE/PSNR gate passes after reference semantics are confirmed |
| IT-PRE-E2E-005 | Calibration mismatch negative test | image and calibration data from different cases | hard failure or visible warning | no silent correction with wrong calibration context |
| IT-PRE-E2E-006 | GUI/native preprocessing E2E | Test GUI or backend automation using local fixture paths | JSON and Markdown report generated | same gates as native E2E; GUI displays pass/fail summary and metric details |

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

### 4.1.1 Algorithm V&V References — XPE-ALG-001 v1.5 GAP-AS~BB

아래 알고리즘들은 XPE-ALG-001 v1.5에서 명세된 SWU이며, 각 SWU의 상세 검증 기준은 ALG 문서의 해당 섹션을 참조한다.

| SWU | Algorithm | ALG Section | Acceptance Criterion |
|-----|-----------|-------------|---------------------|
| SWU-18.0 | Perceptual IQM (PSNR/SSIM/MS-SSIM/FSIM) | §18 | PSNR ≥ 35 dB; SSIM ≥ 0.95; MS-SSIM ≥ 0.98; FSIM ≥ 0.90 |
| SWU-1.12 | Temperature-Compensated Gain | §3.12 | PRNU CV < 0.5% (ΔT=5°C); CV < 1.0% (ΔT=10°C) |
| SWU-1.13 | 2D FFT Notch Filter | §3.13 | Residual < −30 dB; MTF loss < 3%; time < 2ms |
| SWU-9.10 | AEC Feedback Loop | §9.10 | delta_mas_ratio and delta_kvp verified; safety clamp tested |
| SWU-9.11 | SPC Calibration Control | §9.11 | Shewhart rules trigger confirmed; CUSUM h_recal validated |
| SWU-14.2 | Sub-pixel ECC Registration | §14.2 | RMS < 0.5 pixel; ECC score > 0.95 |
| SWU-11.5 | Quantum Noise Model (Anscombe) | §11.5 | α, β error < 5%; Anscombe CV < 0.1 |
| SWU-5.5 | Moiré Artifact Suppression | §5.5 | Detection rate > 95%; residual < 10% |
| SWU-17.2 | DICOM SR for CAD Findings | §17.2 | TID 1500/4100 conformance; DCMTK parse success |
| SWU-12.10 | IEC 61223 Acceptance Testing | §12.10 | T1–T6 pass criteria; fail-case maintenance alert |

### 4.1.2 Algorithm V&V References — XPE-ALG-001 v1.6 GAP-BC~BL

아래 알고리즘들은 XPE-ALG-001 v1.6에서 명세된 SWU이며, 각 SWU의 상세 검증 기준은 ALG 문서의 해당 섹션을 참조한다.

| SWU | Algorithm | ALG Section | Acceptance Criterion |
|-----|-----------|-------------|---------------------|
| SWU-9.12 | DAP/KERMA Dose Tracking | §9.12 | Computed vs. measured DAP ±10%; IEC 60601-2-54 §29.201 pass |
| SWU-17.3 | JPEG 2000 Lossless/Lossy Compression | §17.3 | Lossless pixel-exact; 1.5:1 lossy PSNR ≥ 50 dB; time < 100ms |
| SWU-1.14 | Motion Blur Wiener Deblur | §3.14 | PSNR ≥ 30 dB (6 cases); MTF f50 recovery ≥ 80% |
| SWU-1.15 | Metal Artifact Mask | §3.15 | Coverage ≥ 95%; false-positive < 2%; clinical_use_blocked=true |
| SWU-19.0 | Linear Tomosynthesis FBP/SAA | §19 | FWHM ≤ 1.5mm; in-plane resolution ≥ 3 lp/mm (CIRS phantom) |
| SWU-8.3.2 | RANSAC+ORB Panoramic Stitch | §8.3.2 | Cobb angle error ≤ 1.5°; fallback to phase-corr confirmed |
| SWU-2.9 | Gaussian/Laplacian Pyramid | §4.9 | Reconstruct error < 0.001 ADU RMS; energy monotonicity |
| SWU-10.9 | GPU CUDA Pipeline Acceleration | §10.9 | CPU/GPU diff ±0.01 ADU; pipeline < 10ms; fallback 100% CPU |
| SWU-12.11 | Auto QA Phantom Recognition | §12.11 | Recognition accuracy ≥ 90%; UNKNOWN misclassification < 5% |
| SWU-9.13 | Cross-FPD Calibration Transfer | §9.13 | Post-normalization CV ≤ 0.5%; R² > 0.9999 |

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

## Addendum Registry

| Addendum | Scope | Document | Status |
|----------|-------|----------|--------|
| VVP-PREPROCESS-001 | Pre Lane (P1A: offset/gain/defect/SIMD) | `docs/post-processing/xpe/preprocess/VVP-PREPROCESS-001.md` | v1.1.0 |
| VVP-P1B-001 | P1B Post-Processing (ENH/DISP/DICOM) | `docs/post-processing/xpe/VVP-P1B-001.md` | v1.0.0 |
| XPE-VVP-P2ADV-001 | Advanced Post-Processing (P2-ADV) | `docs/project/vvp_adv.md` | v1.0.0 |

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |
| 1.1 | 2026-04-15 | XPE Team | §4.1.1 Algorithm V&V References 추가 (XPE-ALG-001 v1.5 GAP-AS~BB 10건). SWU-18.0/1.12/1.13/9.10/9.11/14.2/11.5/5.5/17.2/12.10 검증 기준 참조. |
| 1.2 | 2026-04-15 | XPE Team | §4.1.2 Algorithm V&V References 추가 (XPE-ALG-001 v1.6 GAP-BC~BL 10건). SWU-9.12/17.3/1.14/1.15/19.0/8.3.2/2.9/10.9/12.11/9.13 검증 기준 참조. |
| 1.2 | 2026-04-22 | main | Addendum Registry 추가 (VVP-PREPROCESS-001, VVP-P1B-001, XPE-VVP-P2ADV-001). |

---

*Document End — XPE-VVP-001 v1.2*
