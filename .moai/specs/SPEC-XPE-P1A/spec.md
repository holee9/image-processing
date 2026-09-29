# SPEC-XPE-P1A: Pre-processing Module (Gain/Offset/Defect Correction)

> **수치를 읽는 법 (2026-09-17, QA-A-85 전수).** 이 파일에는 **유도 근거를 찾지 못한 성능 목표가 7줄** 있습니다(55/15 · 95/30 · 500/100 ms 계열 등). 탐색 범위: `.moai/reports/lane-pre/`, `.moai/specs/SPEC-XPE-P1A/`, `docs/` 전체. **기계도 적혀 있지 않습니다.** 성능 판정의 근거로 인용하지 마십시오 — 현행 목표는 `spec.md` Performance 절입니다. 줄 목록은 QA-A-85 보고서에 있습니다.
>
> 그리고 **측정값 11줄**(`:222·245·269·289·293~295·297·313·317·475`, 대략)은 인용한 보고서(QA-A-55~69)에는 기계가 적혀 있는데 **여기로 옮기며 떨어졌습니다.** 그 보고서들은 기계를 **i7-12700** 또는 **"이 기계"** 로 적고 있습니다 — 후자를 i7-12700 과 같은 기계로 확인하지는 않았습니다. 이 파일의 절대 ms 측정값은 달리 적혀 있지 않으면 **개발 기계 값**으로 읽으십시오 — CI 러너 값이 아닙니다 (이 파일에 기록된 선례: 같은 게이트가 개발 기계 702 ms, CI 1340.9 ms).
>
> 측정값을 문서에 적을 때는 **측정일·명령·기계**를 함께 적습니다(`lane-sessions.md` §3.5.5).


---
id: SPEC-XPE-P1A
version: 1.3.0
status: M2 Complete (SUP-01 + M2 algorithms implemented)
created: 2026-04-16
updated: 2026-04-19
author: manager-spec (MoAI)
priority: High
issue_number: 16
iec62304_class: B
development_mode: TDD
---

## HISTORY

| Version | Date       | Author  | Changes                  |
|---------|------------|---------|--------------------------|
| 1.3.1   | 2026-09-10 | manager-docs (lead) | #117: converged the SPEC on the implemented global-calibration design (SPEC-XPE-P1A M2, commit e9b8ed4). REQ-P1A-010~012 restated with the `(input, output, metadata)` signatures reading the module-global calibration store; REQ-P1A-014~016 restated as single-path loaders that populate that store; REQ-P1A-016a added (`xpe_calib_state_load` contract); REQ-P1A-020 narrowed to "module not initialized"; REQ-P1A-020a added for `XPE_ERR_CALIB_NOT_LOADED`; REQ-P1A-003 annotated (loaders write the store, processing calls only read it). |
| 1.3.0   | 2026-04-19 | manager-ddd (MoAI Team Mode) | M2 (REQ-P1A-010~013) implementation complete. Offset correction (AVX2 bit-identical), Gain correction (Reciprocal + FMA, 1 ULP parity), Defect correction (Bilinear + cluster fallback), Runtime detection (Hampel 5-sigma). 657 lines added across 7 files. Integration tests added. MX tags applied. |
| 1.2.0   | 2026-04-18 | manager-spec (Pre Lane upgrade) | Strengthen REQ-P1A-010~013 with pixel-accuracy tolerances from research.md v2.0.0. Add REQ-P1A-013 algorithmic recipe (Hampel 5-sigma). Add Section 4.6 (SIMD Parity Contract) referencing simd-parity-harness.md. Research references refreshed to 2022-2026 survey. |
| 1.1.0   | 2026-04-18 | manager-docs | SUP-01 (REQ-P1A-014~019) implemented and tested. 89/90 tests pass. XCal v1 format finalized. PicoSHA2 vendored. |
| 1.0.0   | 2026-04-16 | manager-spec | Initial SPEC creation |

---

## 1. Scope

### 1.1 Overview

xpe_preprocess.dll의 핵심 전처리 알고리즘 3종(SWU-1.1 Offset Correction, SWU-1.2 Gain Correction, SWU-1.3 Defect Correction)과 이를 지원하는 Calibration Data Management(SUP-01)를 구현한다.

이 모듈은 xpe_common.dll(Layer 0)에만 의존하는 Layer 1 알고리즘 DLL로서, C# GUI(ImageProcTest)에서 P/Invoke로 호출된다.

### 1.2 In Scope

- **PRE-02**: Offset/Dark Correction (SWU-1.1) -- 온도 보간, PREP-time 모델, saturating subtraction
- **PRE-03**: Gain/Flat-Field Correction (SWU-1.2) -- UINT16/FLOAT32 포맷 변환, reciprocal gain map, NaN/Inf validation
- **PRE-06**: Defective Pixel Correction (SWU-1.3 baseline) -- Edge-aware bilinear interpolation, static BPM + runtime detection
- **SUP-01**: Calibration Parameter Management -- XCal 포맷 로딩, SHA-256 무결성 검증, session matching, 만료 확인
- API 명세서(api-spec.md v1.3.0) Section 6의 18개 함수 중 본 SPEC 범위 14개 함수 (Ghost/Temp/Nonlinearity/Binning 제외)

### 1.3 Exclusions (What NOT to Build)

- **PRE-04/PRE-05**: Ghost/Lag Correction (SWU-1.4) -- 별도 SPEC으로 분리 (상태유지 handle 기반 아키텍처)
- **PRE-07**: Temperature Compensation (SWU-1.6) -- 별도 SPEC, MCU 이관 가능 구조 필요
- **PRE-08**: Nonlinearity Correction -- 별도 SPEC
- **PRE-09**: Pixel Binning Correction -- 별도 SPEC, 형광투시/CBCT 전용
- **PRE-06 ML/ViT AE**: 고급 defect correction -- Phase 2 Differentiator
- GPU offload (CUDA/OpenCL) -- Phase 3 최적화 단계
- ONNX Runtime 추론 -- AI 모듈(xpe_ai.dll) 범위
- OpenCV 의존성 -- preprocess 모듈은 xpe_common만 의존

---

## 2. Referenced Documents

| Document ID            | Title                                      | Version | Role                  |
|------------------------|--------------------------------------------|---------|-----------------------|
| XPE-API-SPEC-001       | XPE API Specification                      | 1.1.0   | API 계약 정의           |
| XPE-SRS-001            | Software Requirements Specification        | Draft   | 기능 요구사항 원천       |
| XPE-SAD-001            | Software Architecture Description          | Draft   | 아키텍처 참조           |
| XPE-ALG-001            | Algorithm Technical Reference              | Draft   | 수학적 공식 및 성능 기준  |
| XPE-TERM-001           | Terminology and Acronym Control            | 1.0.0   | 용어 표준               |
| SPEC-XPE-P1A-RESEARCH  | Research Report                            | 2.0.0   | 코드베이스 분석 + 2022-2026 deep research |
| SPEC-XPE-P1A-SIMD-PARITY| SIMD Parity Harness Specification         | 1.0.0   | Scalar↔AVX2 동등성 검증 프로토콜          |
| BP-01-05-PREPROCESS    | Benchmark Pack Manifest (Pre Lane portion) | 1.0.0   | BP-01~BP-05 dataset/tolerance/pass criteria |

---

## 3. Definitions and Acronyms

| Term        | Definition                                                   |
|-------------|--------------------------------------------------------------|
| FPD         | Flat Panel Detector (평판 디텍터)                              |
| BPM         | Bad Pixel Map (불량 픽셀 맵)                                   |
| SWU         | Software Unit (IEC 62304 소프트웨어 유닛)                      |
| SOUP        | Software of Unknown Provenance (IEC 62304)                   |
| Gain Map    | Flat-field correction map, per-pixel multiplication factor     |
| Offset Map  | Dark-field correction map, per-pixel dark signal reference     |
| Defect Map  | Boolean map indicating defective pixel locations               |
| XCal        | XPE Calibration file format (magic: "XCal")                   |
| PREP-time   | Time since last detector reset/erase                          |

---

## 4. Requirements (EARS Format)

### 4.1 Ubiquitous Requirements (항상 활성)

#### REQ-P1A-001: Module Initialization

The preprocess module **shall** initialize its internal state when `xpe_preprocess_init()` is called with valid configuration, and report `XPE_OK` on success.

- **SRS**: SRS-INIT-001, SRS-INIT-002
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3

#### REQ-P1A-002: P/Invoke ABI Compliance

The preprocess module **shall** export all functions with `extern "C"` linkage, `__cdecl` calling convention, and `#pragma pack(push, 8)` struct alignment compatible with C# `[StructLayout(Pack = 8)]`.

- **SRS**: SRS-ABI-001
- **Traceability**: All SWUs
- **Verification**: `static_assert` for struct sizes, P/Invoke integration test

#### REQ-P1A-003: Thread Safety

All processing functions **shall** be reentrant with independent caller-supplied buffers. No global mutable state shall be modified during processing calls.

- **SRS**: SRS-THREAD-001
- **Traceability**: All SWUs
- **Note (2026-09-10, #117)**: This requirement remains true under the global-calibration design. The module-global calibration store (SWU-1.5 `LoadedCalibration`) is written **only** by the calibration loaders (`xpe_calib_load_offset` / `_gain` / `_defect_map`, and `xpe_calib_state_load` which delegates to them); processing calls only **read** it, under the module mutex `g_calib_mutex`, and copy the map they need before running the pixel kernel. Loading is therefore a configuration act, not a processing call.

#### REQ-P1A-004: Error Code Consistency

All exported functions **shall** return `XpeErrorCode` (`int32_t`) values from the defined error code set (XPE_OK through XPE_ERR_NETWORK_FAILED). No function shall return undocumented error codes.

- **SRS**: SRS-ERR-001
- **Traceability**: All SWUs

#### REQ-P1A-005: Input Validation

Every exported function **shall** validate all pointer parameters for non-NULL and all dimension parameters for non-zero before accessing any data. Invalid inputs shall return `XPE_ERR_INVALID_INPUT` without modifying output parameters.

- **SRS**: SRS-SAFE-001
- **Traceability**: All SWUs

### 4.2 Event-Driven Requirements (이벤트 구동)

#### REQ-P1A-010: Offset Correction Execution

**When** `xpe_offset_correct(input, output, metadata)` is called on an initialized module with a previously loaded offset map, valid non-NULL arguments and matching dimensions, the module **shall** subtract the per-pixel dark offset held in the module-global calibration store from `input` and write the result to the caller-allocated `output`, clamping to zero (floor-at-zero behavior). **While** the module is initialized but no offset map has been loaded, the call **shall** return `XPE_ERR_CALIB_NOT_LOADED`.

- **SRS**: SRS-CALIB-001
- **Traceability**: PRE-02, SWU-1.1
- **Algorithm**: `I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)`  (saturating unsigned subtraction; `_mm256_subs_epu16` for AVX2)
- **Performance**: < 55ms for 3072x3072 UINT16 frame (scalar); < 15ms (AVX2)
- **Pixel Accuracy** (research.md v2.0.0 Section 8.1):
  - Residual dark mean: < 2 ADU (sigma < 3 ADU) across 15-40 C operating range
  - Scalar vs AVX2 parity: bit-identical (UINT16 saturating subtract is exact) — see Section 4.6
  - No wrap-around or negative values in output (floor-at-zero guarantee)
- **Research References**: Ranger et al. PMC3965338 (2014, revalidated 2023); Wenz et al. IEEE TMI (2023); EP2148500A1 (Canon dark-current patent); AAPM TG-151 (2022)

#### REQ-P1A-011: Gain Correction Execution

**When** `xpe_gain_correct(input, output, metadata)` is called on an initialized module with a previously loaded gain map, valid non-NULL arguments and matching dimensions, the module **shall** multiply each pixel of `input` by the corresponding gain factor from the module-global calibration store and write the FLOAT32 result to the caller-allocated `output`. **While** the module is initialized but no gain map has been loaded, the call **shall** return `XPE_ERR_CALIB_NOT_LOADED`.

- **SRS**: SRS-CALIB-002
- **Traceability**: PRE-03, SWU-1.2
- **Algorithm**: `I_corrected(x,y) = I_offset(x,y) * G(x,y)` where `G(x,y) = mean(I_flat) / (I_flat(x,y) - I_dark(x,y))`. **Storage convention (2026-09-10, #120):** the XCal gain file stores the *normalized sensitivity* `S = 1/G` (mean ≈ 1, what `xpe_calib_generate_gain` writes); the correction divides by the stored map, i.e. multiplies by the precomputed `1/S = G`. Tests MUST NOT multiply the input by the stored map. Production path uses precomputed reciprocal gain map `R(x,y) = 1/G(x,y)` and `_mm256_mul_ps` (multiplication 3-5x faster than division on modern CPUs).
- **Performance**: < 55ms for 3072x3072 UINT16 frame (scalar); < 15ms (AVX2)
- **Pixel Accuracy** (research.md v2.0.0 Section 8.2):
  - Flat-field residual: sigma/mean < 0.5% over 90% FOV
  - Scalar vs AVX2 parity: FLOAT32 tolerance 1 ULP (FMA rounding order difference documented in simd-parity-harness.md)
  - No NaN / Inf in output (REQ-P1A-033 must-pass cross-check)
  - UINT16 output: no overflow/wraparound at pixel = 65535
- **Research References**: Park & Sharp PMID 25795048 (2016); Wang 2013 Duo-SID heel effect; ACPSEM PMC11408574 (2024); Intel Intrinsics Guide (AVX2 FMA semantics)

#### REQ-P1A-012: Defect Correction Execution

**When** `xpe_defect_correct(input, output, metadata)` is called on an initialized module with a previously loaded defect map, valid non-NULL arguments and matching dimensions, the module **shall** replace all defective pixel values (where the defect map held in the module-global calibration store is non-zero) with values interpolated from valid neighboring pixels, writing the result to the caller-allocated `output`. **While** the module is initialized but no defect map has been loaded, the call **shall** return `XPE_ERR_CALIB_NOT_LOADED`. Interpolation behavior is fixed at this entry point (no per-call `configJsonOrNull` parameter); it is configured at the pipeline level.

- **SRS**: SRS-CALIB-003, SRS-CALIB-004
- **Traceability**: PRE-06, SWU-1.3
- **Algorithm** (baseline):
  - Isolated single-pixel defect: unweighted mean of the valid 4-neighborhood (N/S/E/W); if all four are defective, nearest valid pixels in Chebyshev rings r=1..3 (`helpers.cpp:18-53`). Corrected 2026-09-10 (#125): the earlier "weighted by inverse gradient magnitude" clause described no implemented weighting
  - 2+ adjacent defects (cluster, 4-connectivity): median of valid pixels in the 3×3 neighborhood, centre and other defects excluded (`defect_correct.cpp:72-103`)
  - Edge/corner defects: use only in-bounds neighbors (no out-of-bounds memory access, REQ-P1A-005)
- **Buffer aliasing contract** (신설 2026-09-27, `#209` / QA-A-146): `input->data` 와 `output->data` 는 **완전히 같거나 완전히 분리**돼야 합니다.
  - `input->data == output->data` (in-place) — **허용**. 결과는 분리 버퍼 호출과 **비트 단위로 동일**합니다
  - 두 범위가 겹치지 않음 — 허용 (통상 경로)
  - **부분 겹침** — `XPE_ERR_INVALID_INPUT`, 아무것도 쓰기 전에 반환
  - 비교 기준은 이 함수가 실제로 건드리는 `n * sizeof(float)` 바이트 범위입니다. `dataSize` 는 `#123` 계약상 `0` 이 미지정을 뜻해 길이로 신뢰할 수 없습니다

> **[in-place 가 안전한 이유 — 그리고 부분 겹침을 허용하지 않는 이유]**
>
> **쓰기와 읽기가 같은 화소를 건드리지 않습니다.** 이 함수는 결함 지도가 표시한 화소(`dm[idx] != 0`)에만 쓰고, 두 커널은 표시되지 **않은** 화소에서만 읽습니다 — `median_filter_cluster` 가 `defectMask[idx] == 0` 일 때만 이웃을 취하고(`defect_correct.cpp:96`), `xpe_interpolate_pixel` 의 `try_add` 가 4근방과 r=1..3 링 대체 경로 양쪽에서 같은 조건을 겁니다(`helpers.cpp:30`). 군집 좌표를 모으는 `analyzeCluster` 도 `defectMask[nidx] != 0` 인 것만 큐에 넣습니다. **두 집합이 서로소이므로 읽기가 이미 정정된 값을 볼 수 없습니다** — 별칭 여부와 무관하게.
>
> 이 불변식은 **현재 커널의 성질**이지 구조적 보장이 아닙니다. 결함 이웃을 읽는 커널이 들어오면 in-place 가 조용히 깨집니다. 그래서 지키는 것은 주석이 아니라 시험입니다 — `DefectCorrectTest.InPlaceMatchesOutOfPlace` 가 같은 입력을 두 방식으로 돌려 원소 단위로 비교하고, 링 대체 경로를 강제하는 꽉 찬 3×3 블록까지 태웁니다.
>
> **부분 겹침은 틀렸다고 알려져서가 아니라, 아무도 그렇게 부른 적이 없어 결과가 옳은지 아무것도 재지 않기 때문에** 거부합니다. 계약을 그쪽으로 넓히면 **어떤 시험도 관측하지 않는 동작을 보증**하게 되고, 그것이 `#207` 의 형태입니다(없는 AVX2 경로를 AC 가 보증하던 것). 문서화하지 않고 두면 신호 없는 UB 로 갑니다 — 오류 코드가 계약 위반을 호출자에게 **관측 가능하게** 만듭니다.
>
> **인자 검증 순서**: 이 검사는 `REQ-P1A-020`(미초기화 → `XPE_ERR_NOT_INITIALIZED`)보다 **앞섭니다**. 기존 인자 검증(NULL·치수·버퍼 크기)과 같은 자리이며, 잘못된 인자는 모듈 상태보다 먼저 답한다는 기존 규칙을 따릅니다.
>
> **측정**: 스냅숏 제거로 `18.72 → 10.82 ms` (절감 `7.90 ms`, 42%). 예산 `45 ms` 대비 여유 **4.16배**. 제거 전 값은 `#204` 의 `18.45 ms` 가 아니라 **같은 세션에서 다시 잰 값**입니다 — 다른 세션 수치를 baseline 으로 쓰지 않았습니다.
>
> **미검증**: 부분 겹침이 실제로 틀린 값을 내는지는 재지 않았습니다(이제 거부하므로 잴 수 없고, 그것이 결정의 취지입니다). 겹침 판정의 포인터 비교는 서로 다른 할당 사이에서 표준상 미명세이며 평탄한 주소 공간을 전제합니다 — 그 대가로 조용한 UB 를 막습니다.

- **Performance** (재정의 2026-09-27, `#204` — 아래 주를 함께 읽을 것): `< 45 ms` (scalar, 3072x3072 **FLOAT32**, 결함 밀도 0.1% 군집 포함). AVX2 목표 없음. 이전 줄은 `< 95ms ... UINT16 frame (scalar); < 30ms (AVX2)` 였다.

> **[재정의 근거 2026-09-27, `#204` / QA-A-144]**
>
> 옛 줄은 **세 가지가 동시에 틀렸습니다.**
>
> | 무엇 | 문제 |
> |---|---|
> | `UINT16` | **측정 불가능한 조건**이었습니다 — `defect_correct.cpp:124` 가 FLOAT32 가 아니면 거부합니다 |
> | `< 30ms (AVX2)` | **없는 코드의 목표**입니다 — 보정 경로에 AVX2 가 없습니다(`_mm256` 0건, 대조군 `gain_correct` 13건). 스칼라가 이미 그보다 빠릅니다 |
> | `< 95ms` | 유도 근거 없음(위 출처 정정 참조). 그리고 측정 하한의 **위**라 느슨했습니다 |
>
> **측정** (i7-12700, RelWithDebInfo, 단일 스레드, FLOAT32, 워밍업 폐기 + 7회 최솟값):
>
> ```
> 결함 0      6.23 ms   <- 하한 아님. :173 에서 조기 반환해 보정 루프를 건너뜀
> 결함 1     16.29 ms   <- 진짜 하한
> 결함 9437  18.45 ms (0.1% 고립) / 19.21 ms (군집)
> 결함 94371 33.64 ms (1%)
> 실행 편차 2.9~30.3%
> ```
>
> **밀도 0 을 하한으로 삼을 뻔한 것을 레인이 잡았습니다.** 조기 반환이라 루프를 안 돌고, 결함 1개만 있어도 16.29 ms 입니다. SPEC 조건에서 보정 연산 자체는 전체의 **9%** 뿐입니다.
>
> 커널만은 직접 재지 못했습니다 — `xpe_interpolate_pixel` 이 DLL 에서 안 내보내져 `LNK2019`. *"제외했다고 적고 포함해 재는"* 것을 피하려 **밀도 기울기**로 유도했습니다(184 ns/결함, 교차확인 179).
>
> **`45 ms` 의 유도**: 최악 측정 `19.21` × CI 계수 `1.43`(이 저장소 실측: 검출이 로컬 65.6 → CI 93.8) × 편차 `1.3` → 올림.
>
> **회귀 게이트는 아직 없습니다.** 필요하지만 **로컬에서 유도하면 `#144` 의 실수를 반복**합니다 — CI 실측 뒤 절대 예산 형태로 4배를 잡습니다. 이 경로가 CI 에서 실제로 도는 것은 확인됐습니다(`ci.yml:185`·`:206` 이 필터 없이 전체를 두 번 돌리고, run 36233914831 로그의 `[perf-gate-machine]` 줄이 그 스텝 출력).
- **Pixel Accuracy** (research.md v2.0.0 Section 8.3):
  - Correction recall on BPM-marked defects: >= 99% (no defect left uncorrected)
  - Artifact suppression: zero new edges introduced at defect sites — verified by gradient-magnitude delta at defect boundary (|grad_after - grad_before| < 10% of local contrast)
  - Cluster preservation: when >50% of neighborhood is defective, function returns clamp-to-median-of-in-range-neighbors rather than hallucinating
- **Research References**: Jeon et al. PMC7930811 (2021); Schirrmacher et al. arXiv:2310.11637v2 / Springer (2024, FixPix); AAPM TG-151 detector artifact taxonomy

#### REQ-P1A-013: Runtime Defect Detection

**When** `xpe_defect_detect_runtime(img, defectMapOut, configJsonOrNull)` is called, the module **shall** analyze the input image to identify transient defect pixels and write a boolean defect map to `defectMapOut`.

- **SRS**: SRS-CALIB-005
- **Traceability**: PRE-06, SWU-1.3
- **Algorithm (Hampel 5-sigma detector, research.md v2.0.0 Section 8.3, item 4)**:
  1. For each pixel `p(x,y)`, compute local median `m(x,y)` over 3x3 neighborhood excluding center (8 values)
  2. Compute local MAD (median absolute deviation): `MAD(x,y) = median(|neighbor - m|)`
  3. Modified z-score: `z = 0.6745 * (p(x,y) - m(x,y)) / MAD(x,y)` (0.6745 is the scale factor for Gaussian equivalence)
  4. `defectMapOut[x,y] = 1` if `|z| > lambda` (default `lambda = 5.0`), else 0
  5. `lambda` is configurable via `configJsonOrNull` key `"hampel_threshold"` (range 3.0 to 10.0, default 5.0)
- **Rationale**: Median + MAD is robust to clustered outliers (unlike mean + stddev which gets corrupted when defects cluster). 0.6745 scale factor makes z comparable to standard Gaussian z-score.
- **Pixel Accuracy** (research.md v2.0.0 Section 8.3):
  - True-positive rate (TPR) on injected **10-sigma** transients: >= 99.9%
    - **Amended 2026-09-29 (`#143`).** This line used to say *5-sigma*, which is the same
      number as the detector threshold `lambda = 5.0` in Algorithm step 4. A transient whose
      amplitude equals the threshold sits exactly on the decision boundary, so the neighbour
      median's own noise splits it roughly in half -- **0.5 by construction, not by
      implementation quality.** `QA-A-161` proved this mechanically: an ORACLE detector given
      each pixel's TRUE local sigma (estimation error exactly zero) still measured
      **TPR@5-sigma = 0.5088**. No sigma-estimation improvement can reach 0.999 there.
    - Informative, same measurement, shipping algorithm: TPR **0.5536** @5-sigma,
      **0.71** @6-sigma, **0.96** @8-sigma, **0.9865** @10-sigma.
    - **The shipping algorithm does NOT meet the amended requirement either** (0.9865 < 0.999).
      Corrected 2026-09-29 by `QA-A-162`. The 0.9990 quoted when this amendment was first
      written came from `#143`'s issue body, which predates `QA-A-43`'s global sigma floor.
      `test_runtime_detection_rates.cpp:279` records the transition in place --
      `10 sigma 0.998959 -> 0.986472` -- and two independent harnesses now measure 0.9865.
      The same file (:211) states the trade: the floor costs 9% of the detection rate and
      buys a 168x reduction in false positives. That trade was taken at 5-sigma; it costs here too.
    - Candidates measured by `QA-A-162` reach TPR 1.0000 @10-sigma on every structure class
      while meeting FPR, so the amended pair is reachable -- by a change, not by the code as
      it ships today.
    - The threshold and the amplitude at which TPR is specified are now different numbers
      on purpose. Do not re-align them.
  - False-positive rate (FPR) on clean clinical frames: < 0.001% (< 9 false pixels per 3072x3072)
  - Edge-of-image pixels (where 3x3 neighborhood is incomplete): processed with available subset; at least 5 neighbors required or pixel is skipped (defectMapOut = 0)
  - Output is boolean-like UINT8 (0 or 1); guaranteed `sum(defectMapOut)` does not exceed `width*height * 0.01` for clean input
- **Performance** (redefined **2026-09-29, `#143`** — supersedes the 2026-09-12 `#144` definition;
  see the notes below): improvement target **<= 1.3x the measured lower bound of this algorithm,
  same machine and same timing mode** (AVX2, single thread)
  - **Why a ratio and not milliseconds.** Two measurements made the absolute number unusable.
    (a) The 60 ms figure was derived from a lower bound that **did not contain the tile-sigma
    stage** (`QA-A-56`: 15.97 network + 11.1 global sigma = 27.1), and `QA-A-166` measured this
    algorithm's bound at **~76 ms** -- so 60 ms sat *below* its own floor, exactly the condition
    that retired 35 ms and 12 ms. (b) `QA-A-167` found this lane's timings are **bimodal**: the
    same binary, same arguments, same test alternates 65.50 / 96.19 / 65.35 / 96.17 ms --
    **1.47x**, reproducible to 0.2% *within* each mode. An absolute millisecond target cannot be
    stated against a machine that answers two numbers. **A ratio measured in one mode cancels
    both problems**, and this file already took that step once -- the 810 ms gate became a ratio
    for the same reason (2026-09-16 note below).
  - **The bound is the fast mode.** `QA-A-56` cited a minimum for the same reason: a lower bound
    is what the arithmetic cannot avoid, not what a loaded scheduler happens to deliver.
    Components (`QA-A-166`, `QA-A-167`, 3072x3072): network 7.71 + tile difference 5.28 +
    tile selection ~65 (three independent readings agree: 65.4 fast-mode, 63 in-situ, 64.8
    same-loop) + memory 2.46 = **~76 ms**.
  - **1.3x, and why that number.** The old target was 2.21x its bound (60 / 27.1) -- applied here
    that would be 168 ms, *above* what already ships, so it would ask for nothing. The shipped
    code is already at roughly 1.6x. 1.3x asks for a real improvement while staying above the
    floor. **It is a target, not a gate**; the regression gate remains the ratio gate below.
  - **Both terms must come from the same mode.** A ratio built from a fast-mode bound and a
    slow-mode measurement reports 1.47x of improvement that does not exist. `QA-A-165`'s 131 ms
    and `QA-A-166`'s 122 ms were **not** recorded with their mode, so the current ratio is not
    yet established -- see the status note.
  - **Machine identity closed (`QA-A-167`).** The `QA-A-84` note names the development machine as
    i7-12700 and this lane runs a 12th Gen Intel Core i7-12700. Same **model**; not proof of the
    same **unit**, and the `memcpy` 0.88x gap is within what the bimodality above can produce. for a 3072x3072 FLOAT32 frame. The regression gate is a **machine-relative ratio**, not an absolute time — see the 2026-09-16 note below; the absolute `<= 810 ms` that stood here was retired on that date. The previous line read "< 35ms ... (scalar); < 12ms (AVX2, sorting network for median-of-9)".

> **Which machine the 60 ms target refers to (clarified 2026-09-17, QA-A-84).** Until this date the
> line above named four conditions — AVX2, single thread, 3072x3072, FLOAT32 — and **no machine**. This
> file already recorded that the 810 ms *gate* was machine-dependent (the 2026-09-16 note below: 702 ms
> locally, 1340.9 ms on CI, 1.91x) and replaced it with a ratio; **the same was never said of the target.**
> The target was derived on the development machine (from the 27.1 ms AVX2 lower bound measured there,
> QA-A-56), so that is the machine it refers to. This records where the number came from; it does not
> change the number.
>
> **Status against that definition** (from existing reports, not re-measured): the lane machine
> **62.4-63.9 ms** (QA-A-69) — **not met, 1.04-1.07x away.**
>
> **Transcription corrected 2026-09-29 (`QA-A-166`, `#143`).** This line used to say *development
> machine*. `QA-A-69`'s own words are *"이 기계에서 1.0~1.1배 (62.4~63.9 ms)"* -- **this machine**.
> `QA-A-56` says the same. Both are lane-pre reports and **neither names a machine**; the identification
> with the development machine was made here, not there. What is measurable across the two is
> `memcpy`: **0.88x** (40.7 vs 35.9 GB/s, `QA-A-166`). The AVX2 terms are **not** comparable -- the
> kernels differ (19-CE nine-element vs `MedianOfEight8`), so `QA-A-166`'s 7.71 ms must not be read
> against `QA-A-56`'s 15.97 ms. The "1.6x on the CI runner" figure further
> down has **no absolute CI time behind it in the lane reports** — it is a ratio-derived statement, and
> no report records a CI millisecond value for the current code. CI does not enforce the 60 ms target
> at all: the ratio gate guards CI against regression, and that is its only job there.

> **The 60 ms target predates the algorithm that now meets the accuracy requirements (recorded 2026-09-29, `#143`).**
>
> The derivation above is explicit about what the 27.1 ms lower bound contains: **15.97 ms** (19-CE
> network, AVX2, gather excluded) **+ 11.1 ms global sigma** (`QA-A-56`). It contains **no tile-sigma
> stage**, because none existed. `QA-A-164` landed `6d` (tile 128 + blend w=0.10) because `QA-A-163`
> measured that a single global sigma cannot meet FPR on structured frames -- `edge` was 75x over,
> and the misdetections were 100% on the noisier half of the frame while an ORACLE detector given the
> true local sigma balanced 12-to-16. **The second sigma estimator is what closed `#148`'s family.**
>
> So the target and the algorithm no longer describe the same computation. Measured on the lane
> machine (`QA-A-164`, `QA-A-165`, 3072x3072):
>
> | Stage | ms |
> |---|---|
> | `ComputeGlobalSigma` | 55 -> **removed** (`QA-A-164` found it unread: `ResolveSigma` skips the floor branch when `blendWeight > 0`) |
> | `ComputeTileSigmas` | 72 -> **63** after the `QA-A-59`-shaped allocation fix |
> | detection row loop | 55 |
> | **total** | 187 -> 131 -> **122** |
>
> `QA-A-165` decomposed why the allocation fix bought only 7%: **90-93% of the tile stage is median
> selection**, not buffer growth. `SelectKthSmallest` carries a per-call fixed cost (a 65536-bucket
> histogram allocated and zeroed twice, both ranges walked) that does not scale with `n`. The global
> pass called it twice; the tile pass calls it **1152 times**. Removing that fixed cost entirely
> still leaves `6 + 41 + 53 = 98 ms`, **1.6x the target** -- so 60 ms is not reachable by tuning; it
> needs a different selection algorithm, which would move the accuracy numbers.
>
> **What this file does NOT do here:** it does not change the 60 ms number. A replacement target has
> to be derived from a measured lower bound for *this* algorithm, the way `QA-A-56` derived the
> current one -- that measurement does not exist yet. Until it does, treat 60 ms as **the target for
> the pre-`6d` configuration** and this note as the record that the shipped algorithm is measured
> against a number derived without its largest stage.
>
> **Machine caveat still applies.** `QA-A-165` did not establish that the lane machine is the
> development machine of the line above; the ratio is the transferable part, the absolute ms is not.

> **The 60 ms target now sits below the measured lower bound (`QA-A-166`, 2026-09-29, `#143`).**
>
> The table below retired two earlier targets for exactly this reason -- *"both sat below the measured
> lower bound, so no implementation could reach them"*. **The same verdict now applies to 60 ms.**
>
> Lower bound for the shipped algorithm, lane machine, 3072x3072, best-of-N:
>
> | Term | ms |
> |---|---|
> | detection network (`MedianOfEight8`, AVX2) | 7.71 |
> | tile-sigma difference generation | 5.28 |
> | tile-sigma **selection** (in-situ) | **63** |
> | memory traffic | 2.46 |
> | **lower bound** | **~76** |
> | *(global sigma, measured as a control -- not on the current path)* | *50.7 / 13.04 traffic bound* |
>
> Shipped today: **122 ms = 1.6x that bound.** Target 60 ms is **0.79x the bound** -- unreachable by
> any implementation of this algorithm, as 35 ms and 12 ms were for the previous one.
>
> **Two corrections `QA-A-166` made to its own earlier work, both recorded because they change what is
> believed:**
>
> 1. `QA-A-165` calculated *"remove the fixed cost and selection is 41 ms, total 98 ms"*. Implemented
>    and measured, the fixed-cost-free variant is **20% SLOWER** (116.32 vs 97.28 ms). The calculation
>    took the per-element cost of one large-`n` call as a fixed-cost-free floor, but at large `n` the
>    fixed cost is **buried, not absent**; removing it at small `n` needs a touched-bucket list whose
>    sort costs more than the 65536-slot linear walk. **The original implementation was already the
>    good choice.**
> 2. `nth_element` measured **239.27 ms**, 3.5x slower -- not an alternative. Probe only.
>
> **Open, and it changes the margin:** the selection term has two measurements that differ by 1.5x --
> **63 ms in situ vs 97 ms isolated**. If the isolated figure is the honest one the bound is ~110 ms,
> not ~76. The verdict (target below bound) holds either way; the size of the gap does not. The
> hypothesis -- radix splits on the high 16 bits, so a narrow dynamic range makes the second pass
> expensive -- is **unverified**.
>
> **No replacement number is set here.** It waits on that 1.5x being resolved.

> **Why the old numbers were replaced.** Both sat **below the measured lower bound**, so no implementation could reach them (QA-A-56, this machine, 3072x3072):
>
> | Old target | Measured lower bound | Ratio |
> |---|---|---|
> | 35 ms, scalar | **260.3 ms** (19-CE network only, neighbour gather excluded) | 7.4x over |
> | 12 ms, AVX2 | **15.97 ms** (same network, AVX2, gather excluded) + 11.1 ms global sigma = **27.1 ms** | 2.3x over |
>
> Memory is not the constraint (1.16 ms at a measured 40.7 GB/s); the bound is arithmetic.
>
> **The old line also did not describe this algorithm.** It named **median-of-9** and a **UINT16** frame, while REQ-P1A-013 specifies a 3x3 neighbourhood **excluding the centre** (8 values, QA-A-42) and the detection path runs on FLOAT32 — twice the memory traffic. The figures were carried from `research.md` ("Pixel-accuracy targets for REQ-P1A-013"), where they appear **with no cited source**; no frame-rate or clinical constraint anywhere in this SPEC derives them.
>
> **How the new numbers were chosen.** The regression gate is the current measured value (732.1 ms, QA-A-55) plus ~10%, so a regression fails while ordinary run-to-run variance does not — measured context-dependent spread is 17-23% between a tight loop and a long program (QA-A-55, QA-A-56), which is why the gate sits on the slower context. The improvement target is ~2.2x the 27.1 ms AVX2 lower bound, leaving room for neighbour gather (excluded from that bound), map stores, and that same spread.
>
> **Threading policy (decided 2026-09-12, user).** The **caller** specifies how many threads the detection may use; the **default is 1**, so current behaviour is unchanged. The module does not read hardware concurrency and does not choose for itself: the caller owns the whole pipeline and knows what the other stages need, this module does not. REQ-P1A-003 (re-entrancy) is unaffected, and a deployment CPU smaller than this one cannot surprise the module.
>
> **Consequence for this budget: the numbers above stay single-thread numbers**, because the default is 1. A caller passing N threads is measured separately, against the table below.
>
> **Which caller, exactly — the DLL boundary stays closed for now (leader judgment, 2026-09-16, QA-A-61).** `threadCount` lives in `RuntimeDetectionConfig`, and the public entry point `xpe_defect_detect_runtime` takes no config argument. So "the caller specifies" is true today **only of callers inside the module**; a consumer across the DLL boundary cannot set it and gets the default of 1. That gap is deliberate and is recorded here so nobody reads the policy as more than it is.
>
> The ABI is not opened yet for three reasons. **No consumer needs it** — nothing outside the module asks for threads today, and an exported knob with no caller is exactly the "declared wider than used" shape this project has found repeatedly. **The path it would open does not reach the target anyway** — saturation sits 1.7x over 60 ms, so the remaining work is algorithmic and an ABI widened now would be widened again after that work. **An export is hard to withdraw**, while an internal field is not.
>
> The condition for revisiting is concrete: **a named consumer with a stated core budget.** At that point the change carries the full export ceremony (ABI commit plus a `dumpbin` export diff) rather than riding along with a performance card. Export count is unchanged at **45**, matching the `XPE_API` count in the header.
>
> **Measured on the shipped implementation, this machine (QA-A-61):**
>
> | Threads | Global sigma | Speed-up |
> |---|---|---|
> | 1 | 149.4 ms | 1.00x |
> | 12 | 35.1 ms | 4.26x |
> | **16** | **34.0 ms** | **4.39x (saturation)** |
> | 20 | 41.8 ms | worse than 16 |
>
> Whole-detection measured total at 16 threads: **102.3 ms**.
>
> **These numbers supersede the QA-A-58 probe figures this section previously carried, and the probe was optimistic in two ways.** The probe reported global sigma at 26.0 ms on 12 threads (5.52x) and placed saturation at 12; the shipped code is **35% slower** there and saturates at **16**. The probe's whole-detection figure of 86.5 ms was an **addition of separately-timed stages, not an end-to-end measurement** — QA-A-58 said so at the time — and the measured end-to-end total is **18% worse**. The probable cause of the per-stage gap is that the probe allocated its histogram tables once outside the timed region while the shipped code allocates T x 256 KB per call; that is a hypothesis, not a profiled result.
>
> **The conclusion moves in the other direction from the correction: it gets stronger.** Even at saturation the detection sits **1.7x** over the 60 ms target, against the 1.44x the probe suggested. **Threading alone does not reach the target**; the remainder is an algorithm change, not more cores.
>
> **The gate is now a machine-relative ratio, confirmed on both machines (resolved 2026-09-16, QA-A-60).** The former absolute gate of 810 ms came from this development machine only; the first CI run to execute it measured **1340.9 ms**, **1.91x slower**, and failed. Raising the number would have disabled the gate on the faster machine, so the gate instead divides the measured time by a **reference kernel fixed inside the test file** and asserts on the quotient.
>
> **The reference must not share code with what it guards.** A small-frame run of the same detection path was the obvious candidate and was rejected by measurement: the machine difference is absorbed well (local 8.48 → CI 9.05), but a *real* regression **lowers** that ratio (6.34 on the A-57 regression), because the 1024 frame fits in L3 and loses relatively more when arithmetic grows. A `ratio <= R` gate would have passed that regression. When the reference and the subject share code, the thing being watched cancels out.
>
> **Measured, both machines:**
>
> | | Reference kernel | 3072x3072 | Ratio |
> |---|---|---|---|
> | Development machine, normal | — | — | **7.125 - 7.375** (5 runs, spread 3.5%) |
> | CI runner, normal | 172.3 ms | 1329.4 ms | **7.715** |
> | Development machine, regression (median fast path bypassed) | — | — | **12.190 - 12.711** |
>
> Absolute time differs by **1.91x** between the two machines; the ratio differs by **5.4%**. The gate is **`ratio <= 10.00`** — 29.6% above the worst normal reading across both machines, 18% below the best regression reading.
>
> **The 1024x1024 companion is a diagnostic, not a gate.** Its normal band spans 54% (0.629 - 0.969 local, 0.852 CI) against a regression band of 1.140 - 1.359, so normal-worst and regression-best sit only 1.18x apart and no limit fits between them. A gate inside its own noise is worse than none: it trains the habit of re-running until green, which is the path a real regression takes through.
>
> **Reading the ratio.** `ctest` prints test output only on failure, so a passing run shows the ratio nowhere in the CI log. It is in the `xpe-preprocess-test-results` artifact (`Temporary/LastTest.log`), which is where the numbers above were read.
>
> ---
>
> **Re-derived after the AVX2 change (2026-09-16, QA-A-65 / QA-A-66).** The pixel loop moved to AVX2 with bit-identical output: 3072x3072 single thread went **692.7 -> 163.7 ms**, putting the SPEC target 2.7x away instead of 11.5x. That improvement broke the gate's premise, and the limit was re-derived rather than left alone.
>
> **The reference kernel stopped representing the subject.** Before the change both the subject and the reference were mostly scalar, and their ratio agreed between machines to 5.4%. After it, the ratio diverged by **24%** (local 1.644, CI 1.323) and the ordering flipped. The cause was measured rather than guessed: pinning the same work to this CPU's P-cores and E-cores reproduces the spread **inside one machine**, and the E-core lands within **1.2%** of CI.
>
> | Core | Detection | Reference | Ratio |
> |---|---|---|---|
> | P (Golden Cove) | 166.7 ms | 96.9 ms | 1.720 |
> | E (Gracemont) | 237.2 ms | 181.4 ms | 1.308 |
> | CI runner | — | — | 1.251 - 1.323 |
>
> P->E slows the reference by **1.87x** but the detection by only **1.42x**, because after the AVX2 change 90% of the detection is global sigma (1.41x, memory-shaped) and only 10% is the vector loop (2.40x). The quotient now measures *memory-shaped work against scalar arithmetic*, and that balance differs per core design. Five replacement kernels were measured and all landed at 1.75-1.89, none reaching the subject's 1.42 — the conclusion is not that a better kernel exists but that **the normal band is genuinely wide**.
>
> **Gate: `ratio <= 2.20`.** Normal readings span 1.251 - 1.720 across P-core, E-core, and CI; the regression band (AVX2 forced off, a real rebuild) is 7.060 - 7.154. The limit sits **27.9% above the worst normal** — the same margin A-60 used — and 69% below the best regression. In the regression band the two core types agree to 1.3%, because without AVX2 the detection is scalar again and tracks the reference again: the explanation confirmed a second time.
>
> **[HARD] The limit is re-derived after every large performance change, not inherited.** This is the second time the quotient changed what it measures, and the next one is already visible: global sigma now holds 90% of the remaining time, so improving it will shift the balance again. A limit carried across such a change is not a loosened gate — it is a gate measuring something else.
>
> ---
>
> **Re-derived again, and the improvement target is essentially reached (2026-09-16, QA-A-67).** Global sigma went **137.8 -> 46.7 ms** and the whole detection **163.7 -> 64.4 ms**, putting the 60 ms target **1.1x away on this machine** (1.6x on the CI runner) against 11.5x before the AVX2 work.
>
> **The cost was one branch, and it was arithmetic in appearance only.** Within global sigma, one of two selections took 70% of the time (97.0 ms) while *the same function* on the other selection took 14.2 ms — 6.8x cheaper. The only difference was the input distribution: the difference array is half negative and therefore unpredictable, the absolute deviations are all non-negative and therefore perfectly predicted. The control group settles what the cost was:
>
> | Key form | Input | Time |
> |---|---|---|
> | branching | signed differences | 28.9 ms |
> | branchless | signed differences | 5.6 ms |
> | **branching** | **absolute deviations** | **4.4 ms** |
>
> The third row is the control: with predictable data the branching form is already fast, so the cost was **misprediction, not arithmetic**. Two competing hypotheses were rejected by measurement first — a 256 KB histogram exceeding cache (1 KB 26.8 ms vs 256 KB 27.3 ms) and the probe translation unit missing `/arch:AVX2` (186.5 -> 188.8 ms after adding it).
>
> **The gate moved 2.20 -> 0.85, and the earlier explanation predicted both the branch and its removal.** A-66 attributed the wide normal band to the detection being memory-shaped rather than arithmetic-shaped; that shape *was* this branch. With it gone the detection scales P->E at 1.80x against the reference's 1.84x, and the two core types agree again.
>
> | | P-core | E-core | CI runner |
> |---|---|---|---|
> | Normal | 0.630 - 0.651 | 0.626 - 0.630 | **0.552** |
> | Regression (this change reverted) | 1.583 | 1.289 | — |
> | Regression (AVX2 removed) | 6.183 | 6.490 | — |
>
> **Moving the limit was not optional.** The reverted-change regression reads **1.289 on the E-core, below A-66's own P-core normal of 1.720** — the old limit of 2.20 would have passed that regression on both core types. The new limit sits 30.6% above the worst normal reading and 34% below the nearest regression.
>
> One qualification on the band: the local P/E spread narrowed to 3.3%, but the CI runner reads **0.552**, so the band across all three is about 18% wide. The limit accommodates that; the narrowing is a local-cores observation, not an all-machines one.
>
> **Still unverified, and it is the same shape as the finding above.** `MedianSortCE` also runs a ternary per element. A comment asserts MSVC lowers it to `vminss`/`vmaxss`, and **that assertion has not been checked** — a comment claiming what the compiler does is not evidence of what the compiler does.
>
> *(Checked 2026-09-16, QA-A-68: the comment was wrong on both counts — no `vminss`/`vmaxss`, and four conditional branches remain. It was **not** fixed, because after the AVX2 change that scalar path runs only on the border: 30,704 of 9,437,184 pixels (0.33%), 2.149 ms, and halving it would return under 0.5 ms against a gate sample spread of 8%. Measuring the share before acting is what separated "the comment is wrong" from "there is something worth fixing".)*
>
> ---
>
> **The row tail (2026-09-16, QA-A-69).** The vector run stopped at `x + 8 <= w - 1`, dropping the last seven interior pixels of every row onto the scalar path. That cost is **fixed per row**, so it grows as frames get narrower — 0.23% of interior pixels at width 3072 but about 1.4% at width 512 — and the scalar path is roughly 58x slower per pixel. An overlapping final run at `x = w - 9` removes it: scalar pixels **30,704 -> 12,284**, their cost **2.149 -> 0.985 ms**. The remaining 12,284 are the first and last columns, which have a different neighbourhood and must stay scalar. Processing the overlap twice is safe by construction (the verdict is deterministic and the map write overwrites rather than accumulates) — verified in the column direction rather than inherited from the row-direction result.
>
> **Three residual facts are recorded here because none of them is visible from the code.**
>
> **1. Frame parity did not catch the boundary error.** Moving the final run by one column (`w-9` -> `w-8`) leaves every map assertion passing, because the scalar pass that follows overwrites that column with the correct value. **The map was right and the read was wrong** — the last interior row loaded one element past the end of the frame. Only a guard-page **read** test catches it, and that is a third distinct falsification shape in this module: bit-vs-frame (QA-A-65), two layers that overlap (QA-A-67), and now **an assertion whose subject is the memory access rather than the output**.
>
> **2. The border is correct for two reasons, and the second hides errors in the first.** The formula does not touch the border, *and* the scalar pass rewrites it. Today both hold. If the order ever changes, the masking layer disappears and the only remaining defence is the guard-page test above.
>
> **3. This optimisation is not protected against its own deletion.** Remove the tail run and the map is unchanged, the guard page does not fire, and the gate cannot see the 2.7% difference against an 8% sample spread. Instrumenting the product code with a scalar-pixel counter was considered and **declined**: it would either sit in a hot path or live only in a Debug build that CI never compiles (the same dead end as the assertion in QA-A-64). The gap is recorded rather than papered over — a performance change whose only evidence is a number below the noise floor has no mechanical guard, and saying so is more useful than a guard that does not guard.
- **Research References**: Pearson 2002 (Hampel identifier classic); Schirrmacher et al. 2024 (FixPix detection stage); Jeon et al. PMC7930811 (2021 CNN for clustered defects — out of scope for REQ-P1A-013 runtime path)

#### REQ-P1A-014: Calibration File Loading (Offset)

**When** `xpe_calib_load_offset(filepath)` is called with a valid XCal file path, the module **shall** parse the XCal header, validate SHA-256 integrity, check session matching and expiry, and load the offset map data into the module-global calibration store (SWU-1.5 `LoadedCalibration`) under `g_calib_mutex`. The function takes no output-buffer parameter; the loaded map is consumed by `xpe_offset_correct` and by the pipeline entry points. The LRU-cached variant `xpe_calib_load_offset_cached(filePath, offsetMapOut)` is the form that returns a map to the caller.

- **SRS**: SRS-CALIB-010
- **Traceability**: SUP-01

#### REQ-P1A-015: Calibration File Loading (Gain)

**When** `xpe_calib_load_gain(filepath)` is called with a valid XCal file path, the module **shall** parse, validate, and load the gain map data (with its kVp interpolation table) into the module-global calibration store under `g_calib_mutex`. No output-buffer parameter; the caller-returning form is `xpe_calib_load_gain_cached(filePath, gainMapOut)`.

- **SRS**: SRS-CALIB-011
- **Traceability**: SUP-01

#### REQ-P1A-016: Calibration File Loading (Defect Map)

**When** `xpe_calib_load_defect_map(filepath)` is called with a valid XCal file path, the module **shall** load the boolean defect map into the module-global calibration store under `g_calib_mutex`. No output-buffer parameter; the caller-returning form is `xpe_calib_load_defect_cached(filePath, defectMapOut)`.

- **SRS**: SRS-CALIB-012
- **Traceability**: SUP-01

#### REQ-P1A-016a: Calibration State Load Contract

**When** `xpe_calib_state_load(state, calibPath)` is called with a non-NULL zero-initialized `state` and a valid directory path, the module **shall** compose `offset.xcal`, `gain.xcal` and `defect.xcal` under `calibPath`, delegate to the three single-path loaders — so that the maps land in the **module-global calibration store, not in the caller's struct** — and set only the three `offsetLoaded` / `gainLoaded` / `defectLoaded` flags on `XpeCalibrationState`. The `offsetMap` / `gainMap` / `defectMap` buffer fields of the struct **are not required to be filled**, and callers **shall not** read them as loaded data. A missing file leaves the corresponding flag `false` and the call still returns `XPE_OK`; a NULL `state` or `calibPath` returns `XPE_ERR_INVALID_INPUT`. Correspondingly, `xpe_preprocess_pipeline_ex(img, meta, calibState, ...)` **shall** take offset and gain from the global store and consult `calibState` only for a defect map, accepting `NULL` for `calibState`.

- **SRS**: SRS-CALIB-010, SRS-CALIB-011, SRS-CALIB-012
- **Traceability**: SUP-01, SWU-1.5, SWU-1.11

#### REQ-P1A-017: Calibration Offset Generation

**When** `xpe_calib_generate_offset(dark_frames, num_frames, integration_time_ms, temperature_c, output_path, config_json_or_null)` is called, the module **shall** combine the dark frames per pixel using the method selected by `config_json_or_null` (XPE-ALG-001 §9.8: `mean` default when `NULL`, `sigma_clip`, `median`, …), write the result as an XCal offset file to `output_path`, and — for `sigma_clip` — mark pixels with fewer than `N_min = max(3, ⌊N/4⌋)` surviving samples as static defects merged into the global defect map (§9.8.2.1). *(Corrected 2026-09-11, decision #138 / QA-A-38·A-39: the previous `(frames, frameCount, offsetMapOut, configJsonOrNull)` form never existed in the header; before A-39 the public entry point had no config argument and always used `mean`.)*

- **SRS**: SRS-CALIB-020
- **Traceability**: SUP-01

#### REQ-P1A-018: Calibration Expiry Check

**When** `xpe_calib_check_expiry(filePath, expiryEpochMsOut)` is called, the module **shall** read the embedded expiry timestamp and return `XPE_ERR_CALIBRATION_EXPIRED` if the timestamp is in the past.

- **SRS**: SRS-CALIB-030, SRS-SAFE-010
- **Traceability**: SUP-01

#### REQ-P1A-019: Calibration Save

**When** `xpe_calib_save(filePath, calibType, expiryEpochMs)` is called, the module **shall** write the calibration map of `calibType` (`"offset"` / `"gain"` / `"defect"`) currently held in the global calibration store (decision #117) to `filePath` in XCal format with SHA-256 integrity and the embedded expiry timestamp `expiryEpochMs` (Unix ms; `0` = never expires). `XPE_ERR_CALIB_NOT_LOADED` if that map is not loaded. *(Decision #132, 2026-09-10: the third parameter restores the expiry embedding lost when the signature moved to the global-store form; the implementation before this decision always wrote 0.)*

- **SRS**: SRS-CALIB-021
- **Traceability**: SUP-01

### 4.3 State-Driven Requirements (상태 구동)

#### REQ-P1A-020: Not-Initialized Guard

**While** the module is not initialized (`xpe_preprocess_init()` not called or after `xpe_preprocess_shutdown()`), all processing functions **shall** return `XPE_ERR_NOT_INITIALIZED` without modifying any output parameters. `XPE_ERR_NOT_INITIALIZED` means exactly that — the module was never initialized or has been shut down. It **shall not** be used to report loaded-calibration state.

- **SRS**: SRS-INIT-003
- **Traceability**: All SWUs

#### REQ-P1A-020a: Calibration-Not-Loaded Guard

**While** the module is initialized but the calibration map required by the requested correction has not been loaded into the module-global store, `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct` and the pipeline entry points **shall** return `XPE_ERR_CALIB_NOT_LOADED` without modifying any output parameters. Precedence follows the api-spec "Error code precedence" section: a NULL required pointer yields `XPE_ERR_INVALID_INPUT` (class 1) first; `XPE_ERR_CALIB_NOT_LOADED` sits in class 2 alongside `XPE_ERR_NOT_INITIALIZED` and the other content validations, whose relative order is implementation-defined.

- **SRS**: SRS-INIT-003, SRS-CALIB-001, SRS-CALIB-002, SRS-CALIB-003
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3, SWU-1.5
- **Note**: `XPE_ERR_CALIB_NOT_LOADED` is added to `modules/common/include/xpe/common/xpe_error.h` by QA-A-20 as the next code after the current last entry; until it lands, the reference implementation returns `XPE_ERR_NOT_INITIALIZED` for this condition.

#### REQ-P1A-021: Dimension Mismatch Guard

**While** input and map buffer dimensions differ, all correction functions **shall** return `XPE_ERR_INVALID_INPUT` without modifying the output buffer.

- **SRS**: SRS-SAFE-003
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3

#### REQ-P1A-022: Format Mismatch Guard

**While** input and map buffer pixel formats differ, all correction functions **shall** return `XPE_ERR_UNSUPPORTED_FORMAT`.

- **SRS**: SRS-SAFE-004
- **Traceability**: SWU-1.1, SWU-1.2

### 4.3b Subsystem Requirements — 온도 보상 · 고스트 · 비닝 (신설 2026-09-28, `#211`)

> **왜 신설인가.** 커밋 `bc22093`(2026-04-16)이 이 SPEC 을 585행 → 361행으로 줄이면서 요구 46개를 지웠습니다(`REQ-P1A-` 정의 **71 → 25**). 대부분은 살아남은 번호로 흡수됐지만, **세 서브시스템은 흡수처 없이 사라졌습니다** — 정의행 제목 27개 중 `temp`·`ghost`·`binning` 을 담은 것이 **0건**(대조군 `offset` 3건)인데, 셋 다 구현돼 있고 `xpe_ghost_*` 는 **수출 API** 입니다.
>
> **옛 문구를 복원하지 않았습니다.** 아래는 현재 구현을 읽어 쓴 것이고, 옛 요구와 다른 곳은 그 자리에 적었습니다 — `#204`·`#207`·`#209` 에서 옛 문구가 실재와 달랐던 전례가 있습니다.
>
> 새 번호(`080~`)를 씁니다. 옛 번호를 재사용하면 코드에 남은 옛 인용이 **다른 뜻으로 되살아납니다** — `#197` 이 그 형태였습니다.

#### REQ-P1A-080: Temperature Compensation Execution

**When** `xpe_temp_compensate(img, detectorTempC, configJsonOrNull)` is called with a **UINT16** buffer, the module **shall** scale each pixel by the inverse of the dark-current factor relative to `T_ref = 25 °C`, computed as `exp(-Eg/2kT) / exp(-Eg/2kT_ref)`, writing the result **in place** and clamping to `65535`.

- **측정된 계약** (`temp_compensate.cpp`): 단일 버퍼 in-place — `input`/`output` 쌍이 아닙니다. **UINT16 전용** (`xpe_buffer_has_format(img, XPE_PIXEL_UINT16, …)`); **`REQ-P1A-012`(결함 보정)가 FLOAT32 전용인 것과 반대**입니다
- `exp_ref < 1e-300` 이면 보정 없이 `XPE_OK` — 물리적으로 불가능하나 방어적으로 둡니다
- **Verification**: Test
- **Status**: 구현 있음, 요구는 이 항목이 처음입니다

#### REQ-P1A-081: Temperature Input Guard

**If** `detectorTempC` is `NaN`, the module **shall** substitute `25.0 °C`. **If** the (substituted) value is outside `[-20.0, +60.0] °C`, the module **shall** return `XPE_ERR_INVALID_INPUT` without modifying the image.

- **측정된 계약** (`temp_compensate.cpp:33`, `:36-37`): NaN 치환이 범위 검사보다 **앞섭니다** — `NaN` 은 `25.0` 이 되어 통과합니다
- **⚠️ 옛 `REQ-P1A-007` 과 다른 점**: 옛 문구는 NaN 치환 시 *"post an INFO-level alert"* 를 요구했습니다. **구현에 알림이 0건**입니다(`xpe_alert`·`post_alert`·`XPE_ALERT` 전수). **요구에 넣지 않았습니다** — 없는 동작을 보증하지 않기 위함이고, 알림이 필요한지는 별건입니다
- **Verification**: Test

#### REQ-P1A-082: Temperature Compensation Flag

**While** the pre-processing pipeline runs the temperature stage successfully, the pipeline **shall** set `XPE_FLAG_TEMP_COMPENSATED` in `XpeImageMetadata.flags`.

- **측정된 계약**: 플래그는 **`pipeline.cpp:139` 가** 설정합니다. `xpe_temp_compensate` 를 **직접 호출하면 플래그가 설정되지 않습니다** — 그 함수는 메타데이터를 받지 않습니다
- **⚠️ 옛 `REQ-P1A-008` 과 다른 점**: 옛 문구는 주체를 밝히지 않아 *"보정 함수가 설정한다"* 로 읽혔습니다. 실재는 파이프라인입니다
- **Verification**: Test (`test_pipeline_stages.cpp:115`, `:197`)

#### REQ-P1A-085: Ghost Corrector Handle Lifecycle

**When** `xpe_ghost_create(width, height, …)` is called with non-zero dimensions, the module **shall** allocate an opaque handle holding frame history and return it through `handleOut`; **if** allocation fails it **shall** return `XPE_ERR_OUT_OF_MEMORY`. `xpe_ghost_destroy` **shall** invalidate the handle before freeing it.

- **측정된 계약** (`ghost_correct.cpp:27`, `:33`, `:264`): `!handleOut || width == 0 || height == 0` → `XPE_ERR_INVALID_INPUT`. `destroy` 는 `magic = 0` 으로 **무효화한 뒤** 해제합니다
- **Traceability**: 수출 API 4종 — `xpe_ghost_create`·`correct`·`reset`·`destroy` (`preprocess_api.h:578`·`595`·`608`·`617`)
- **Verification**: Test

#### REQ-P1A-086: Ghost Handle Validity Guard

**If** `xpe_ghost_correct`, `xpe_ghost_reset`, or `xpe_ghost_destroy` receives a handle that was never created or has been destroyed, the module **shall** return `XPE_ERR_INVALID_INPUT` (and `xpe_ghost_destroy` **shall** return without effect) rather than dereference it.

- **측정된 계약**: `GhostCorrectorHandle::isValid(handle)` 가 **세 지점 모두**에서 호출됩니다 — `:202`·`:249`·`:262` (정의 `xpe_preprocess_internal.h:65`)
- **조사 기록**: 리더가 `magic` 을 `grep` 했을 때 `:264`(쓰기) 하나만 보여 *"쓰기만 되고 읽히지 않는다 = use-after-free"* 로 갈 뻔했습니다. 읽는 쪽은 헤더의 `isValid` 안에 있었습니다. **한 파일 grep 으로 부재를 단정하면 없는 결함을 만듭니다**
- **Verification**: Test

#### REQ-P1A-087: Ghost Correction Execution

**When** `xpe_ghost_correct(handle, img, meta)` is called with a valid handle and a **FLOAT32** buffer, the module **shall** subtract the lag contribution estimated from the handle's frame history, in place.

- **측정된 계약** (`:209`): **FLOAT32 전용** — `REQ-P1A-080`(온도, UINT16)과 형식이 다릅니다. 파이프라인 단계 순서상 게인 보정 이후이기 때문입니다
- **Verification**: Test

#### REQ-P1A-088: Ghost Corrector Reset

**When** `xpe_ghost_reset(handle)` is called with a valid handle, the module **shall** clear the accumulated frame history and the exposure state, so that the next `xpe_ghost_correct` behaves as if the handle had just been created.

- **측정된 계약** (`ghost_correct.cpp` `xpe_ghost_reset`): `hist1`·`hist2` 를 `0.0f` 로 채우고, `lastAcqTimeSec = 0.0` · `lastFrameMean = 0.0f` · `exposureWeight = 1.0` 로 되돌립니다. **이력 두 개만이 아니라 노출 상태 셋도 함께** 초기화합니다
- **왜 별도 요구인가**: `REQ-P1A-086`(핸들 유효성 가드)이 `reset` 을 **이름으로 부르지만** *"이력을 비운다"* 는 말하지 않습니다. 유효성과 의미론은 다른 계약이고, `086` 에 끼워 넣으면 그 요구가 두 가지를 말하게 됩니다
- **`#211` 경위**: 옛 `REQ-P1A-034` 를 인용하던 4곳이 실제로는 **이 동작**을 서술하고 있었습니다(옛 `032` 의 내용 — 번호가 밀린 채). 레인(`QA-A-150`)이 *"`096` 으로 옮기면 정반대가 된다"* 며 옮기지 않고 보고했고, 그 판단이 이 요구를 만들었습니다
- **Verification**: Test (`test_ghost_correct.cpp`)

#### REQ-P1A-090: Binning Correction Execution

**When** `xpe_binning_correct(img, binningMode, …)` is called with a **FLOAT32** buffer and `binningMode` is `2` or `4`, the module **shall** normalize each pixel by `1 / binningMode²` to compensate summed charge, in place.

- **측정된 계약** (`binning_correct.cpp:22`, `:36`): FLOAT32 전용, 정규화 계수 `1/mode²`
- **Verification**: Test

#### REQ-P1A-091: Binning Mode Guard

**While** `binningMode == 1`, the module **shall** return `XPE_OK` without modifying the image. **If** `binningMode` is not `1`, `2`, or `4`, it **shall** return `XPE_ERR_CONFIG_INVALID`. **If** any pixel is non-finite during normalization, it **shall** return `XPE_ERR_PROCESSING_FAILED`.

- **측정된 계약** (`:25`, `:30-31`, `:39`): 세 갈래 모두 실재합니다
- **Verification**: Test

> **이 절의 미검증**
>
> - 위 요구는 **현재 구현을 서술**한 것이고, 그 구현이 **임상적으로 옳은지는 이 절이 답하지 않습니다.** 온도 보상의 `Eg`·`k` 상수 출처와 UINT16 선택 근거는 확인하지 않았습니다
> - 옛 요구 중 **`REQ-P1A-007` 의 INFO 알림**은 구현에 없어 요구에 넣지 않았습니다. 필요 여부는 별건입니다
> - `#211` 의 (C) 갈래(파이프라인 6건·성능 예산 3건·로깅·1×1 edge case)는 **아직 판정하지 않았습니다**
> - 원문 추출 실패 4건(`045` `051` `053` `055`)은 아직 읽지 않았습니다

### 4.3c Pipeline Requirements (신설 2026-09-28, `#211`)

> **왜 신설인가.** 수출 API 48개를 요구와 전수 대조한 결과(대조군 양성 `xpe_defect_correct` → 3개 요구, 음성 지어낸 이름 → 0건) **23개가 고유 요구 없이 수출**되고 있었고, 파이프라인 4종이 그중에 있었습니다. `spec.md` 정의행 제목에 `pipeline` 이 **0건**입니다.
>
> 아래는 `pipeline.cpp` 를 읽어 쓴 것입니다. 옛 요구(`043`~`049`)와 다른 곳은 그 자리에 적었습니다.

#### REQ-P1A-095: Pipeline Stage Order

**When** `xpe_preprocess_pipeline(img, meta, calibPath, ghostHandle, configJsonOrNull)` is called, the module **shall** run the correction stages in this fixed order, each consuming the previous stage's output:

`readout validation → temperature → offset → nonlinearity → gain → binning → defect → ghost`

- **측정된 계약** (`pipeline.cpp:122`·`139`·`155`·`181`·`203`·`230`·`273`·`297`): 여덟 단계, 각각 성공 시 대응 `XPE_FLAG_*` 를 설정합니다
- **Verification**: Test

#### REQ-P1A-096: Pipeline Stage Flags

**While** a stage completes successfully, the pipeline **shall** set that stage's bit in `XpeImageMetadata.flags`: `READOUT_VALIDATED` · `TEMP_COMPENSATED` · `OFFSET_CORRECTED` · `NONLINEARITY_CORRECTED` · `GAIN_CORRECTED` · `BINNING_CORRECTED` · `DEFECT_CORRECTED` · `GHOST_CORRECTED`.

- **측정된 계약**: 플래그 설정은 **파이프라인만** 합니다. 개별 보정 함수를 직접 부르면 설정되지 않습니다 — `REQ-P1A-082` 가 온도에 대해 같은 것을 말합니다
- 비선형만 조건이 하나 더 있습니다 — `meta && applied` (`:181`). 적용되지 않으면 플래그가 서지 않습니다
- **Verification**: Test

#### REQ-P1A-097: Per-Stage Bypass Configuration

**If** `configJsonOrNull` sets a stage's bypass key to `"true"`, the pipeline **shall** skip that stage without error and **shall not** set its flag.

- **측정된 계약** (`:52-80`): 여덟 단계 각각에 bypass 키가 있습니다. 문자열 `"true"` 와의 정확한 일치로 판정합니다 — 다른 값은 bypass 하지 않습니다
- **⚠️ 옛 `REQ-P1A-049` 와 다른 점**: 옛 문구는 *"비활성인데 캘리브 데이터가 실려 있으면 DEBUG 로그"* 를 요구했습니다. 확인하지 않았습니다 — 로깅은 이 절의 범위 밖으로 둡니다
- **Verification**: Test

#### REQ-P1A-098: Ghost Stage Handle Dependency

**While** `ghostHandle` is `NULL`, the pipeline **shall** skip the ghost stage and complete the remaining stages successfully, leaving `XPE_FLAG_GHOST_CORRECTED` unset.

- **측정된 계약** (`:280`): `if (!cfg.bypassGhost && ghostHandle)` — 핸들이 없으면 **조용히** 건너뜁니다
- **⚠️ 옛 `REQ-P1A-045` 와 다른 점**: 옛 문구는 건너뛸 때 *"post a WARNING alert indicating lag artifacts may be present"* 를 요구했습니다. **파이프라인 전체에 알림 호출이 0건**입니다(`alert`·`Alert` 전수). **요구에 넣지 않았습니다** — 없는 동작을 보증하지 않기 위함입니다
- **플래그가 안 서는 것이 유일한 신호입니다.** 호출자가 플래그를 안 보면 잔상 보정이 빠진 것을 알 수 없습니다 — 알림이 필요한지는 별건입니다
- **Verification**: Test

#### REQ-P1A-099: Ghost Stage Buffer Isolation

**When** the ghost stage runs, the pipeline **shall** give `xpe_ghost_correct` its own copy of the previous stage's frame rather than the stage-6 buffer itself.

- **근거** (`:292-294`, QA-A-104): 고스트 보정은 in-place 로 동작합니다. 이전에는 stage-6 을 정정하면서 **비어 있는** stage-7 버퍼를 되복사해 **출력이 0** 이었습니다. 이 복사가 그 결함의 수정이고, **요구로 고정하지 않으면 최적화로 다시 제거될 수 있습니다**
- **Verification**: Test

#### REQ-P1A-100: Pipeline Data Domain Transition

**When** the pipeline runs, stages before gain correction **shall** operate on `UINT16` and stages from gain correction onward **shall** operate on `FLOAT32`; the transition **shall** occur inside the gain stage.

- **측정된 계약** (`pipeline.cpp:194`·`:199`): *"This performs UINT16 → FLOAT32 domain transition"* — stage 4(gain) 에서 전이하고, 이후 stage 5·6·7 이 모두 `XPE_PIXEL_FLOAT32`(`:219`·`:262`·`:286`)
- **옛 `REQ-P1A-043` 과의 차이**: 옛 문구는 *"stage 2(gain correction)"* 라 적었습니다. **단계 번호 체계가 달라졌을 뿐** 전이가 게인에서 일어난다는 내용은 같습니다 — 번호가 아니라 **함수 이름**으로 다시 썼습니다
- **왜 요구로 고정하는가**: 이 전이 지점이 바뀌면 이후 모든 단계의 버퍼 형식이 바뀝니다. `REQ-P1A-080`(온도, **UINT16**)과 `REQ-P1A-087`·`090`(고스트·비닝, **FLOAT32**)이 서로 다른 형식을 요구하는 이유가 이 경계입니다
- **Verification**: Test

#### REQ-P1A-101: Defect Stage Calibration Availability

**If** the defect stage is reached and no defect map is loaded, the pipeline **shall** return `XPE_ERR_CALIB_NOT_LOADED` without running that stage or any later stage.

- **측정된 계약** (`pipeline.cpp:248-253`): `defectAvailable = (g_calib.defect_map != nullptr)`, 거짓이면 즉시 반환
- **⚠️ 옛 `REQ-P1A-046` 과 두 곳이 다릅니다:**
  - 옛 문구는 **`XPE_ERR_CALIBRATION_EXPIRED`** 를 요구했습니다. 실재는 **`XPE_ERR_CALIB_NOT_LOADED`** 입니다 — `#117` 결정 B 가 "미초기화" 와 "캘리브 미적재" 를 나눈 뒤의 코드이고, `EXPIRED`(만료)는 또 다른 상태입니다. **실재가 맞습니다**
  - 옛 문구는 *"**각** 단계가 대응 캘리브 가용성을 확인"* 이라 적었습니다. **실재는 결함 단계 하나뿐입니다** — `available` 검사가 `pipeline.cpp` 전체에서 `:248-253` 한 곳입니다
- **Verification**: Test

> **`REQ-P1A-101` 이 덮지 않는 것 — 기록**
>
> 오프셋·게인 단계에는 **파이프라인 수준의 가용성 검사가 없습니다.** 그 단계들은 개별 보정 함수가 자기 안에서 `XPE_ERR_CALIB_NOT_LOADED` 를 반환하고(`REQ-P1A-020a`), 파이프라인은 그 반환을 그대로 올립니다.
>
> **결과는 비슷하지만 계약이 다릅니다** — 결함은 *"단계에 들어가기 전에"* 막고, 나머지는 *"함수가 거부해서"* 막힙니다. 옛 `046` 이 요구한 **균일한 단계별 검사는 구현된 적이 없고**, 이 요구는 실재하는 한 곳만 고정합니다.
>
> 균일하게 만들지는 별건입니다. 지금 요구로 적으면 **없는 동작을 보증**하게 됩니다.


> **이 절의 미검증**
>
> - `xpe_preprocess_pipeline_ex` 와 `_batch` 는 **아직 서술하지 않았습니다.** `_ex` 는 `REQ-P1A-016a` 가 이름을 부르지만 그것은 캘리브 상태 계약이지 파이프라인 계약이 아닙니다. `_batch` 는 어떤 요구도 부르지 않습니다
> - 옛 `043`(uint16→float32 전이 지점) `044`(비닝 비활성 시 건너뜀) `046`(단계별 캘리브 가용성 확인) `047`(단계별 플래그)은 위 요구와 겹치거나 더 구체적입니다 — **건별 대조를 하지 않았습니다**
> - 성능 예산(옛 `050` 500ms)은 여기 넣지 않았습니다 → `#204`
> - 로깅(옛 `049`·`068`)은 범위 밖입니다

### 4.4 Unwanted Behavior Requirements (금지 동작)

#### REQ-P1A-030: No Exceptions Across C ABI

The module **shall not** allow any C++ exception to propagate across the C ABI boundary. All internal exceptions shall be caught and converted to appropriate `XpeErrorCode` values.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs

#### REQ-P1A-031: No Memory Leak

The module **shall not** leak any heap-allocated memory. All temporary allocations during processing shall be freed before function return, including error paths.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs
- **Verification**: 1000-cycle allocation/free test (reference: `modules/common/tests/test_xpe_common.cpp`)

#### REQ-P1A-032: No Uninitialized Output

The module **shall not** leave output buffers in a partially initialized state on error. On failure, the module shall either leave the output unmodified or zero-fill it entirely.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs

#### REQ-P1A-033: No NaN/Inf in Output

The module **shall not** produce NaN or Inf values in output image buffers. All floating-point operations shall include validation to clamp or replace invalid values.

- **SRS**: SRS-SAFE-020
- **Traceability**: SWU-1.2 (gain correction)

### 4.5 Optional Requirements (선택 사항)

#### REQ-P1A-040: SIMD Optimization

The module **shall** use AVX2 intrinsics for performance-critical operations (offset subtraction, gain multiplication, defect interpolation, runtime detection) while maintaining the parity contract defined in Section 4.6.

> **Amended 2026-09-16 (QA-A-75, #160).** The opening clause was *"Where AVX2 is available at runtime"*. That premise does not hold: Section 4.6 makes AVX2 a **minimum platform requirement**, and the module is compiled `/arch:AVX2` as a whole, so AVX2 availability does not vary at runtime for any CPU that reaches our code at all. A requirement conditioned on a condition that is always true is not a requirement — it reads as one while constraining nothing, and it is what kept a runtime-dispatch design alive in this document long after the shipped design stopped having one.

> **The dispatch override never existed, under four different names.** The retired protocol line referenced `xpe.simd.force_scalar`; `.moai/docs/acceptance.md` claimed an `XPE_FORCE_SCALAR=1` environment variable and a `{"force_scalar": true}` init-config flag; `simd_dispatch.cpp` declared an `xpe_simd_force_scalar()` function. **Measured 2026-09-16 (scope: this worktree, excluding `build/` and `.git/`):** `XPE_FORCE_SCALAR` occurs **only** inside `.moai/backups/` copies of superseded SPECs — zero occurrences in live source; the config flag has no reading code; and the function lived in `modules/preprocess/src/simd_dispatch.cpp`, which was absent from the CMake source list and **did not compile** (`XPE_EXPORT` had no definition anywhere in the repository — `error C2143` at its first use). **That file was deleted in QA-A-76**, and git history settled what it had been: no commit ever named it in a `CMakeLists.txt`, no commit ever defined `XPE_EXPORT`, and `generate_export_header` was never used — so it was never a renamed remnant, it was a name that had never existed. The commit that introduced it (`60dd828`) declared **`405/405` parity checks passing** in the same message, which is where that figure came from. Control for that search: `XPE_API` resolves to a real `__declspec` definition in `xpe_types.h`, so the search instrument reads what is there.

> **What survives is the parity contract, not the dispatch.** Sections AC-SIMD-001~004 asked for scalar/AVX2 parity, and that **intent is met** by QA-A-72/A-73: each AVX2 kernel is compared against an inline scalar reference compiled from the same source, with no runtime switch to select between them. The mechanism differs from the one planned here and the **case counts differ too** (the harness asked for 300 cases x 3 shapes; the shipped parity tests compare one frame each) — recorded as two separate facts rather than collapsed into "done".

- **SRS**: SRS-PERF-001
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3
- **Detailed protocol**: `simd-parity-harness.md` describes the planned harness, including the dispatch override. The override is **retired** (above); the parity protocol is superseded by the inline-scalar-reference comparison in `test_offset_correct_avx2_parity.cpp` and its gain counterpart.

#### REQ-P1A-041: Readout Artifact Validation

**Where** the caller provides a raw frame, the module **shall** validate it for line noise, dropped columns, and ADC saturation patterns via `xpe_validate_readout_artifact()`.

- **SRS**: SRS-PERF-001
- **Traceability**: PRE-01

#### REQ-P1A-042: Parameter Range Query

**Where** the caller requests valid parameter ranges, the module **shall** return body-part-specific parameter limits via `xpe_preprocess_get_param_range()`.

- **SRS**: SRS-SAFE-002, SRS-SAFE-005
- **Traceability**: SUP-01

### 4.6 SIMD Parity Contract (NEW IN v1.2.0)

**AVX2 is a minimum platform requirement (decided 2026-09-16, user, #160).** The module is compiled `/arch:AVX2` as a whole (`modules/preprocess/CMakeLists.txt`), so a CPU without AVX2 does not run it slowly — it **faults on the first call**, before any code of ours executes. That is now a stated requirement rather than an accident.

> **What this decision changes, and what it does not.** Behaviour is unchanged: such a CPU faulted before and faults after. What changes is that the fault is **specified** rather than surprising, and that the module stops **appearing** to handle the case — `xpe_gain_has_avx2()` in `gain_correct.cpp` was a correct CPUID/XGETBV probe that **could never protect anything**, because the faulting instruction can be emitted anywhere in the module including ahead of the probe itself. A guard that cannot guard is worse than no guard: it makes a reader stop looking.
>
> **The cost of the alternative is measured.** Supporting a non-AVX2 CPU means splitting `/arch:AVX2` to the vector sources only, which is a structural change whose boundaries break silently when inlining or templates cross them. And giving up AVX2 entirely means giving up **692.7 -> 163.7 ms** on the 3072x3072 detection (QA-A-65, bit-identical), with the 60 ms target unreachable by scalar code — the median stage already sits at the measured scalar lower bound (QA-A-68).
>
> **What was verified, and what could not be.** A build with the vector path switched off produces **byte-identical results** (QA-A-71: flag count and map digest match exactly across the two builds). That establishes the scalar path is correct, **not** that it runs on a non-AVX2 CPU — that build still compiles `/arch:AVX2`, so no build currently exists in which the question could be asked. Do not read the parity result as AVX2-free support.

The scalar path is the reference implementation, and the parity rules below still bind because the scalar path is what defines correct output. The AVX2 path is the shipped path on every supported CPU:

| Operation | Scalar Path | AVX2 Path | Parity Rule |
|-----------|-------------|-----------|-------------|
| Offset subtraction (UINT16) | `max(a - b, 0)` via branch | `_mm256_subs_epu16` | **Bit-identical** |
| Defect interpolation (UINT16 bilinear) | pure-C bilinear | AVX2 gather + weighted average | **Bit-identical** (integer arithmetic only) |
| Gain correction (FLOAT32 reciprocal) | `a * (1.0f / b)` | `_mm256_mul_ps` | **1 ULP tolerance** (FLOAT32) |
| Gain correction (FLOAT32 polynomial) | Horner method scalar | `_mm256_fmadd_ps` chain | **1 ULP tolerance** (FMA rounding) |
| Runtime detection (MAD, UINT16) | sort-9 + median | AVX2 sorting network | **Bit-identical** (integer median) |

Fallback policy:
- Runtime CPUID detection determines dispatch (see `simd-parity-harness.md` Section 2)
- Config flag `"force_scalar": true` (passed via `xpe_preprocess_init(configJsonOrNull)`) forces the scalar path
- Environment variable `XPE_FORCE_SCALAR=1` has equivalent effect

Verification:
- `test_simd_parity.cpp` runs 100 pseudo-random inputs per operation (deterministic seed = CRC32("XPE-SIMD-PARITY-v1"))
- Full protocol in `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md`
- Pass criterion: 100/100 parity checks succeed per operation

---

## 5. API Function Summary

본 SPEC에서 구현하는 14개 함수 (api-spec.md Section 6의 18개 중 P1A 범위):

### 5.1 Lifecycle (2 functions)

| #  | Function                      | SRS          | Priority |
|----|-------------------------------|--------------|----------|
| 1  | `xpe_preprocess_init()`       | SRS-INIT-001 | High     |
| 2  | `xpe_preprocess_shutdown()`   | SRS-INIT-003 | High     |

### 5.2 Calibration Loading (3 functions)

| #  | Function                      | SRS           | Priority |
|----|-------------------------------|---------------|----------|
| 3  | `xpe_calib_load_offset()`     | SRS-CALIB-010 | High     |
| 4  | `xpe_calib_load_gain()`       | SRS-CALIB-011 | High     |
| 5  | `xpe_calib_load_defect_map()` | SRS-CALIB-012 | High     |

### 5.3 Correction Processing (4 functions)

| #  | Function                      | SRS                     | Priority |
|----|-------------------------------|-------------------------|----------|
| 6  | `xpe_offset_correct()`        | SRS-CALIB-001           | High     |
| 7  | `xpe_gain_correct()`          | SRS-CALIB-002           | High     |
| 8  | `xpe_defect_correct()`        | SRS-CALIB-003, SRS-CALIB-004 | High |
| 9  | `xpe_defect_detect_runtime()` | SRS-CALIB-005           | Medium   |

### 5.4 Calibration Management (4 functions)

| #  | Function                      | SRS           | Priority |
|----|-------------------------------|---------------|----------|
| 10 | `xpe_calib_generate_offset()` | SRS-CALIB-020 | Medium   |
| 11 | `xpe_calib_check_expiry()`    | SRS-CALIB-030 | High     |
| 12 | `xpe_calib_save()`            | SRS-CALIB-021 | Medium   |
| 13 | `xpe_validate_readout_artifact()` | SRS-PERF-001 | Low    |

### 5.5 Utility (1 function)

| #  | Function                          | SRS          | Priority |
|----|-----------------------------------|--------------|----------|
| 14 | `xpe_preprocess_get_param_range()` | SRS-SAFE-002 | Medium   |

### 5.6 Excluded Functions (Phase 2+) — **이 표는 더 이상 유효하지 않습니다** (`#211`, 2026-09-28)

> **결론 먼저: 아래 일곱 함수는 이 SPEC 의 모듈에 구현돼 있고, 이 SPEC 의 파이프라인에서 실행되며, 요구는 §4.3b·§4.3c 에 있습니다.** 표가 가리키는 이관 대상은 **하나도 만들어지지 않았습니다.**
>
> **측정** (`#211`, QA-A-150 이 지목 → 리더 확인):
>
> | 이관 대상 | 실재 |
> |---|---|
> | `SPEC-XPE-P1C` (temp) | **없음** |
> | `SPEC-XPE-P1D` (nonlinearity) | **없음** |
> | `SPEC-XPE-P1E` (binning) | **없음** |
> | `SPEC-XPE-P1B` (ghost 4종) | 존재하나 **`-DICOM`·`-DISP`·`-ENH`** 셋뿐이고, 세 문서 전부 `ghost`·`temp_compensate`·`binning`·`nonlinearity` **0건** |
>
> 대조군: 같은 검색이 각 SPEC 의 주제어를 잡습니다 — `DICOM` 163 · `window` 10 · `enhance` 45. **검색이 눈먼 것이 아닙니다.**
>
> **그래서 이 표는 "나중에 다른 SPEC 에서" 를 약속했고, 그 나중이 오지 않은 채 구현이 여기 들어왔습니다.** 일곱 함수 모두 `modules/preprocess` 에 있고 `preprocess_api.h` 에서 수출되며 `pipeline.cpp` 가 단계로 실행합니다. 요구만 없었습니다 — `#211` 이 찾은 것이 그것입니다.
>
> **표를 지우지 않고 남깁니다.** 어느 시점에 그 분리가 계획됐다는 사실은 기록이고, 지우면 다음 사람이 "왜 P1A 에 다 있지" 를 다시 조사합니다. 다만 **현재 상태의 서술로 읽어서는 안 됩니다.**
>
> **되살릴 조건**: 실제로 `P1C`~`P1E` 를 세우고 구현을 옮긴다면 그때 §4.3b 의 `REQ-P1A-080`~`091` 을 그쪽으로 이관합니다. 그 결정은 이 이슈의 범위 밖입니다.


| Function                    | Reason                          | Target SPEC  |
|-----------------------------|---------------------------------|--------------|
| `xpe_ghost_create()`        | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_correct()`       | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_reset()`         | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_destroy()`       | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_temp_compensate()`     | MCU migration design needed     | SPEC-XPE-P1C |
| `xpe_nonlinearity_correct()`| Separate SWU                   | SPEC-XPE-P1D |
| `xpe_binning_correct()`     | Fluoro/CBCT only               | SPEC-XPE-P1E |

---

## 6. Performance Targets

~~출처: XPE-ALG-001, 3072x3072 UINT16 기준~~

> **⚠ 출처 정정 2026-09-17 (QA-A-85).** 이 표는 `XPE-ALG-001` 을 출처로 인용했지만, **그 문서에 이 표의 수치가 없습니다** — `docs/post-processing/xpe/XPE-ALG-001_…md` 에서 `55 ms` 계열 0건, `95 ms` 계열 0건 (대조군: 같은 검색이 그 문서의 ms 값 86줄을 찾음). 오히려 ALG-001 의 `xpe_offset_correct` 주석은 `≤500ms (SRS-PERF-001)` 이고, 이 파일의 offset 목표는 `< 55ms` 입니다.
>
> **아래 표의 수치는 유도 근거를 찾지 못했습니다** (탐색 범위: `.moai/reports/lane-pre/`, `.moai/specs/SPEC-XPE-P1A/`, `docs/` 전체). 기계도 적혀 있지 않습니다. 성능 판정의 근거로 **인용하지 마십시오**. 런타임 검출의 현행 목표는 위 Performance 절(개발 기계 기준 60 ms)입니다.
>
> **[정정 2026-09-27, #144 — 충돌은 검출이 아니라 보정에 있습니다]**
>
> 이 주석이 *"같은 defect 연산에 목표가 둘"* 이라고 적었는데, **두 연산을 섞었습니다.**
> 두 수치는 서로 다른 요구에 붙어 있습니다:
>
> | 요구 | 연산 | `research.md` | 이 파일 · `acceptance.md` |
> |---|---|---|---|
> | `REQ-P1A-012` | defect **보정** (이웃 평균 / 군집 median) | `:218` **`< 60ms`** | `:177`·`:577` **`< 95ms`** ← **충돌은 여기** |
> | `REQ-P1A-013` | 런타임 **검출** (Hampel: median + MAD) | `:224` `< 35ms` scalar / `< 12ms` AVX2 | 위 Performance 절 **60 ms** (2026-09-12 재정의) |
>
> **검출(013)에는 충돌이 없습니다.** 옛 `35/12 ms` 는 측정 하한(`260.3 ms` / `27.1 ms`) 아래라
> 도달 불가여서 위 절이 대체했고, 그 재정의는 근거가 기록돼 있습니다. 현재 개발 기계에서
> **1.1배**(CI 러너 1.6배) 남아 사실상 달성입니다.
>
> **충돌은 보정(012)에 있습니다 — `60` 대 `95`.** 그리고 **어느 쪽도 유도 근거가 없습니다**
> (위 출처 정정 참조). `research.md` 쪽은 *"baseline path, bilinear"* 라 적는데 `#125` 가
> 정정했듯 **구현에 가중치가 없습니다** — 서술부터 현재 알고리즘과 다릅니다.
>
> **따라서 둘 중 하나를 고르지 않습니다.** 근거 없는 수치 둘 중에서 고르는 것은 근거 없는
> 수치를 하나 남기는 일입니다. 검출(013)을 고친 방식 그대로 — **측정 하한을 먼저 재고 그
> 측정에서 목표를 세웁니다.** 그때까지 보정 목표는 **미정**이며 판정 근거로 쓰지 마십시오.

| Algorithm          | Target     | SIMD Target (AVX2) |
|--------------------|------------|--------------------|
| Offset Correction  | < 55ms     | < 15ms             |
| Gain Correction    | < 55ms     | < 15ms             |
| Defect Correction  | < 95ms     | < 30ms             |
| Full Pipeline      | < 500ms    | < 100ms            |

---

## 7. Quality Requirements

| Attribute       | Target                                    |
|-----------------|-------------------------------------------|
| Test Coverage   | >= 85% statement coverage                 |
| Testing Method  | TDD (RED-GREEN-REFACTOR)                  |
| SIMD Parity     | Scalar vs AVX2 bit-exact equivalence      |
| IEC 62304 Class | B (medical device software)               |
| Thread Safety   | All processing functions reentrant        |
| Memory Safety   | Zero leaks in 1000-cycle endurance test   |

---

## 8. Implementation Status

### Phase 1 SUP-01 (Calibration Management) — Completed 2026-04-18

| Requirement | Status | Implementation Files |
|-------------|--------|----------------------|
| REQ-P1A-014 | Implemented | modules/preprocess/src/xpe_calib_load_offset.cpp |
| REQ-P1A-015 | Implemented | modules/preprocess/src/xpe_calib_load_gain.cpp |
| REQ-P1A-016 | Implemented | modules/preprocess/src/xpe_calib_load_defect_map.cpp |
| REQ-P1A-017 | Implemented | modules/preprocess/src/xpe_calib_generate_offset.cpp |
| REQ-P1A-018 | Implemented | modules/preprocess/src/xpe_calib_check_expiry.cpp |
| REQ-P1A-019 | Implemented | modules/preprocess/src/xpe_calib_save.cpp |

### Core Validators & Utilities

| Component | Status | Implementation Files |
|-----------|--------|----------------------|
| XCal Format Reader | Implemented | modules/preprocess/src/xcal_reader.cpp, modules/preprocess/include/xpe/preprocess/xcal_format.h |
| XCal Format Writer | Implemented | modules/preprocess/src/xcal_writer.cpp |
| XCal Validator | Implemented | modules/preprocess/src/xcal_validator.cpp |
| SHA-256 Hash (PicoSHA2) | Implemented | third_party/picosha2/picosha2.h (header-only, MIT-0 license) |

### Test Coverage — 89/90 Tests Passing

| Test Suite | Count | Status |
|-----------|-------|--------|
| Calibration Validators | 18 | PASS |
| XCal Reader/Writer Round-trip | 16 | PASS |
| SHA-256 Integrity | 8 | PASS |
| Calibration Loaders (offset/gain/defect) | 24 | PASS |
| Expiry Check & Date Handling | 8 | PASS |
| Offset Generation | 8 | PASS |
| Endurance (memory leaks) | 1 | SKIP |
| **Total** | **89/90** | **GREEN** |

### Existing Requirements (Phase 0 & Prior Work)

| Requirement | Status | Notes |
|-------------|--------|-------|
| REQ-P1A-001 ~ 009 | Existing | Module initialization, P/Invoke ABI, thread safety (from prior iterations or Phase 0) |
| REQ-P1A-020 ~ 022 | Existing | Guards: uninitialized, dimension/format mismatch |
| REQ-P1A-030 ~ 033 | Existing | Unwanted behaviors: exception safety, memory leaks, NaN/Inf validation |

### Pending Features (Next Sprint — Priority High)

| Requirement | Target SPEC | Notes |
|-------------|----------|-------|
| REQ-P1A-010 | SPEC-XPE-P1A M2 | Offset correction scalar + AVX2 dispatch; parity bit-identical |
| REQ-P1A-011 | SPEC-XPE-P1A M2 | Gain correction reciprocal-map + FMA path; parity 1 ULP |
| REQ-P1A-012 | SPEC-XPE-P1A M2 | Defect correction (bilinear + cluster fallback); 99%+ recall target |
| REQ-P1A-013 | SPEC-XPE-P1A M2 | Runtime detection (Hampel 5-sigma); TPR >= 99.9%, FPR < 0.001% |
| REQ-P1A-040 | SPEC-XPE-P1A M5 | SIMD dispatch + parity harness per simd-parity-harness.md |
| REQ-P1A-041 | SPEC-XPE-P1A M6 | Readout artifact validation (Priority Low) |
| REQ-P1A-042 | SPEC-XPE-P1A M6 | Parameter range query (Priority Medium) |

### Benchmark Pack Freeze (Pre Lane)

Manifest BP-01 through BP-05 must be frozen (SHA-256 dataset hashes, tolerance values, pass criteria locked) before the M2 completion is accepted as release-relevant. See `benchmark/BP-01-05-preprocess-manifest.md`.

### Dependencies

- **xpe_common.dll**: Layer 0 dependency (used for error handling, memory allocation)
- **PicoSHA2**: Third-party header-only library (MIT-0 license, no SOUP classification — vendored in-tree)

---

*Document End - SPEC-XPE-P1A v1.2.0*
