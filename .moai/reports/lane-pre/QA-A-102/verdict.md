# QA-A-102 — 기존 교정 파일 덮어쓰기(−9) 판정, 전처리 속도 요구 실측 (#169, #179)

커밋 `1b63d2a` (dev/preprocess, 미푸시, 기준 main `716dbeb`). 속도 측정은 코드 변경 없음(저장소 밖 측정 프로그램).

## 1. 주장

### 1.1 덮어쓰기
1. 요구는 덮어쓰기를 전제한다.
   - SRS-CALIB-001 FUNC-033 (3)(`SRS-CALIB-001_…md:187`): "When overwriting an existing calibration file, system shall compute and log …".
   - `docs/calibration/CONCEPT-DIAGRAMS.md:626`: 현장 재교정 흐름 "VALIDATE --> OVERWRITE: 새 파일로 덮어쓰기".
   - 저장 코드 설계(`xcal_writer.hpp:6`): "atomic write pattern: write to .tmp, flush+close, rename to final."
   - 공개 헤더와 api-spec 에는 덮어쓰기 거부를 적은 곳이 없다(검색 범위: `.moai/project/api-spec.md`, `modules/preprocess/include`, `docs/calibration`, 단어 overwrit/already exist/atomic/rename/덮어).
   - 판정: −9 는 요구가 아니라 결함이다. 원인은 `std::rename` 이다. Windows 에서는 대상 파일이 이미 있으면 교체하지 않고 실패한다(`xcal_writer.cpp:204`).
2. 수정: Windows 에서는 `MoveFileExA(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` 로 원자적으로 교체한다. POSIX 는 `rename` 을 유지한다.
3. 네 함수 모두 같은 저장 함수(`write_xcal_file`)를 거치므로 동작이 같다.

| 함수 | 수정 전(같은 경로 두 번째 쓰기) | 수정 후 |
|---|---|---|
| `xpe_calib_generate_offset` | −9, 옛 파일 유지 | OK, 새 내용, `.tmp` 없음 |
| `xpe_calib_generate_gain` | −9 | OK |
| `xpe_calib_generate_gain_polynomial` | −9 | OK |
| `xpe_calib_save` | −9 | OK |

`xpe_bpm_generate` 는 파일을 쓰지 않는다(출력은 메모리 버퍼, `preprocess_api.h:922`).

### 1.2 속도
4. 개발 PC 에서 요구를 넘는 항목
   - 결함 보정: 2.77 s. 요구 95 ms 의 29배.
   - 런타임 결함 검출: 145–149 ms. 요구 60 ms / 20 ms.
   - 교정 파일 3개 적재: 472–519 ms. 요구 200 ms.
   - 전체 전처리 파이프라인: 약 3.0–3.7 s. 요구 500 ms.
5. 오프셋·게인·고스트 1–3단계는 요구 안에 있다. 비닝은 요구의 80% 이상이다.
6. 최적화는 하지 않았다.

## 2. 증거

### 2.1 덮어쓰기
- 수정 전: 새 시험 4건 모두 `Which is: -9`, `RUN_EXIT=1`, `BUILD_EXIT=0`(`a102-run-before.log`, `a102-build-before.log`). 수정 코드를 넣기 전 빌드였으므로, 이 실행이 수정을 되돌린 상태의 반증이다.
- 수정 후: `BUILD_EXIT=0`, warning C 0, 4건 통과(`a102-run-after.log`).
- 전체: `ctest --preset ci-preprocess` → 664건 통과, 종료 0(`a102-ctest.log`).

### 2.2 속도 — 기기와 조건
- CPU i7-12700 (12코어/20스레드), RAM 31.7 GB, Windows 11 Pro. 측정 전 여유 메모리 4.85 GB.
- 빌드: `build/ci-preprocess`, RelWithDebInfo(`/Zi /O2 /Ob1 /DNDEBUG`). CMakeCache 의 `COMPILER_SUPPORTS_AVX2` 값은 비어 있다.
- 측정 프로그램: `bench.cpp`(`/O2 /MD`, 위 DLL 링크).
  - 3072×3072 합성 프레임, 오프셋 100, 게인 1.0, 결함 0.1% 무작위.
  - 단계마다 준비 실행 3회를 버리고 15회의 중앙값을 쓴다. 실행 2회(r1, r2).
- 다른 레인 활동: 두 실행 모두 GUI 레인의 `dotnet`/`testhost` 프로세스가 있었다(r1 전 CPU 6%, r2 전 34%). `cl`/`link`/`ninja`/`MSBuild` 는 없었다.

### 2.3 속도 — 요구 대조
판정 기준: 여유 < 80%, 경계 80–100%, 초과 > 100% (로컬 중앙값 ÷ 요구).

| 항목 | 요구 (문서:줄) | 로컬 중앙값 r1 / r2 (ms) | CI 최근 값 | 판정 |
|---|---|---|---|---|
| 전처리 전체 | ≤500 ms (SRS-CALIB-001:274 PERF-001, pipeline-spec.md:164 PERF-P1A-001) | `pipeline_ex` 고스트 없음 3069 / 3082; 파일 적재 포함 `pipeline` 3388 / 3533 | 없음(`Integration.PipelinePerformance3072x3072` Disabled) | 초과 (6.1×) |
| (0) 교정 3파일 적재 | ≤200 ms (SRS-CALIB-001:274 (0), :286 PERF-003) | 519 / 472 | 없음 | 초과 (2.4–2.6×) |
| (1) 오프셋 | ≤55 ms (SRS-CALIB-001:274 (1); SPEC-XPE-P1A spec.md:145) | 10.0 / 9.9 | 없음 | 여유 |
| (1.5) 비선형 | ≤20 ms (SRS-CALIB-001:274 (1.5)) | 0.00 / 0.00 (설정 없음과 `"standard"` 모두) | 없음 | 여유 — 3072² 에서 0.00 ms 이므로 화소 연산이 수행되지 않는 것으로 보인다(미확인) |
| (2) 게인 | ≤55 ms (SRS-CALIB-001:274 (2); spec.md:159) | 34.2 / 37.2 | 없음 | 여유 (62–68%) |
| (2.5) 비닝 | ≤10 ms (SRS-CALIB-001:274 (2.5)) | 모드 2: 8.1 / 8.2 | 없음 | 경계 (81–82%) |
| (3) 결함 보정 | ≤95 ms (SRS-CALIB-001:274 (3); spec.md:177; SRS-DEFECT-001:168 PERF-105) | 2772 / 2777 | 없음 | 초과 (29×) |
| 결함 군집 보정 | <25 ms (SRS-DEFECT-001:165 PERF-102) | 따로 재지 않음. 결함 보정 전체(2772)에 포함 | 없음 | 초과(포함 값 기준) |
| 런타임 결함 검출 | ≤60 ms, 개발 PC (spec.md:202) / <20 ms (SRS-DEFECT-001:67 FR-204, :164 PERF-101) | 149 / 145 | 비율 게이트만 통과(`Frame3072SquaredWithinMachineRatio`, 절대 ms 기록 없음) | 초과 (2.4× / 7.3×) |
| (4) 고스트 Tier 1 | ≤140 ms (SRS-CALIB-001:274); ≤150 ms (pipeline-spec.md:174) | 19.7 / 20.2 | 없음 | 여유 |
| 고스트 Tier 2 | ≤190 ms (pipeline-spec.md:175); Tier1+2 <70 ms (srs_ghost_correction.md:119 NFR-101) | 33.7 / 30.7 | 없음 | 여유 |
| 고스트 Tier 3 | ≤240 ms (pipeline-spec.md:176); Tier 2–3 ≤+130 ms (SRS-CALIB-001:274) | 122 / 135 (Tier 1 대비 +103 / +115) | 없음 | 여유(240 기준) / 경계(+130 기준, 79–88%) |
| Tier1+2+Gain+Defect | <200 ms (srs_ghost_correction.md:120 NFR-102) | 게인 34 + 결함 2772 + T2 34 ≈ 2840 | 없음 | 초과 |
| Tier 3 포함 전체 | <500 ms (srs_ghost_correction.md:121 NFR-103) | `pipeline_ex` Tier 3 3737 / 3292 | 없음 | 초과 |
| 라인 결함 보정 | <30 ms (SRS-DEFECT-001:100 FR-406, :166 PERF-103) | 기능 없음 | — | 미측정 |
| 그리드 억제 | <15 ms (SRS-DEFECT-001:167 PERF-104) | 기능 없음 | — | 미측정 |
| 시간적 일관성 검사 | <10 ms (SRS-DEFECT-001:79 FR-255) | 기능 없음 | — | 미측정 |

- "기능 없음"의 검색 범위: `modules/preprocess/src` 에서 `line.?(defect|correct)|grid.?(suppress|filter)|temporal.?check` 0건.
- 요구가 없는 단계(참고): readout 검사 4.4 ms, 온도 보정 7.0–7.7 ms.
- 제외한 출처
  - `xray-detector-calibration-prd.md:1891-1898`: 오프셋 <1 ms 는 FPGA 대상이라 규범이 아니다.
  - `sprint-plan.md`: 계획 문서다.
  - `SPEC-XPE-P1A spec.md:575-578` 표: SPEC 자신이 569–571행에서 "출처를 찾지 못함, 인용 금지"라고 적었다.
- CI(`716dbeb`, run 35226237458/35226237361)
  - 전처리 단계별 절대 시간 출력이 없다.
  - Benchmark Regression 의 `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` (325 ms)는 후처리 파이프라인이라 이 표에 쓰지 않았다.
  - CI 설정 로그에 `COMPILER_SUPPORTS_AVX2 - Failed` 가 있다.

## 3. 기준 귀속
- 덮어쓰기: 같은 시험 파일을 수정 전후 빌드에서 실행했다.
- 속도: 이 레인 로컬 빌드(`1b63d2a` 트리) 위에서 측정 프로그램을 두 번 실행했다(`run-r1.log`, `run-r2.log`). 요구 수치는 위 문서:줄에서 옮겼다.

## 4. 미검증
- 런타임 검출: 이전 보고(QA-A-69)의 62.4–63.9 ms 와 이번 145–149 ms 가 다르다. 빌드 설정(AVX2 경로 포함 여부, `/Ob1`) 차이인지 확인하지 않았다.
- 비선형 보정이 실제로 화소를 바꾸는지 확인하지 않았다(시간만 쟀다).
- 결함 보정 시간이 결함 비율에 따라 어떻게 변하는지(0%, 1%) 재지 않았다.
- 파이프라인 시간에 들어간 단계별 내역(복사 포함)은 나누어 재지 않았다.
- Release 빌드나 다른 기기에서는 재지 않았다. CI 절대값은 없다.
- 덮어쓰기: 다른 프로세스가 대상 파일을 열고 있을 때의 동작, POSIX 경로는 시험하지 않았다.

## 5. 잔여 위험
- 두 번째 실행 전 다른 레인 부하가 34% 였다. 두 실행의 중앙값 차이는 대부분 10% 이내였지만, 부하 영향을 따로 떼어 보지 않았다.
- 초과 항목(결함 보정, 적재, 전체)은 요구보다 한 자릿수 이상 느리다. 기기 차이로 설명되는 범위가 아니다.
- `MOVEFILE_WRITE_THROUGH` 는 디스크 반영까지 기다리므로 저장이 느려질 수 있다. 시간을 재지는 않았다.
