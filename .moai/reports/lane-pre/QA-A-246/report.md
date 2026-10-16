# QA-A-246 — `pipeline_out` 이 단계별 비유한(NaN/Inf) 개수를 돌려주게

## 1. 결과

| 항목 | 결과 |
|---|---|
| 새 API | `xpe_preprocess_pipeline_out_ex(…, XpeStageDiagnostics* diag)` + 구조체 `XpeStageDiagnostics` (32 바이트). 기존 `xpe_preprocess_pipeline_out` 은 그대로이고 `diag = NULL` 인 이 함수다 |
| 돌려주는 것 | **gain 단계 출력**의 비유한 화소 수, **최종 출력**의 비유한 화소 수, 화소 수(분모), 어느 것이 측정됐는지 `validMask` |
| `diag == NULL` | 같은 호출, 같은 출력 바이트, 세는 패스 없음. 실영상에서 기준 영상 v2 와 바이트 동일 |
| 비용 | 3072² 실영상, 세는 패스 두 번(gain 뒤, 최종) 합쳐 **+8.2 ms** (중앙값 50.8 → 59.0 ms, 프레임 500 ms 예산의 1.6 %) |
| **리더가 알아야 할 것** | 카드의 시나리오 "gain 이 만든 NaN 을 defect 가 메우면 최종은 깨끗하다" 는 **현재 파이프라인에서 일어나지 않는다**. 결함 단계가 비유한 입력을 **거부**한다 (§3) |
| 부수 발견 | `A241Safety.PipelineOutRefusesWhatCannotHoldTheResultAndWritesNothing` 이 힙 배치에 따라 간헐 실패(60 회 중 5 회). 이 카드가 만든 것이 아니라 241 의 검사 순서 문제다 (§6) |

## 2. API

`preprocess_api.h` (doxygen 은 모든 인자를 문서화해야 통과하고 그렇게 했다):

```c
#define XPE_DIAG_GAIN_NONFINITE  0x1u
#define XPE_DIAG_FINAL_NONFINITE 0x2u
typedef struct XpeStageDiagnostics {
    uint32_t structSize;          /* [in] sizeof(XpeStageDiagnostics) */
    uint32_t validMask;           /* [out] XPE_DIAG_* bits measured; 0 on a failed call */
    uint64_t pixelCount;          /* [out] width * height; 0 on a failed call */
    uint64_t gainStageNonFinite;  /* [out] non-finite pixels in the gain stage's output */
    uint64_t finalNonFinite;      /* [out] non-finite pixels in the final output written to out */
} XpeStageDiagnostics;           /* 32 bytes, static_assert */
XpeErrorCode xpe_preprocess_pipeline_out_ex(in, out, meta, calibState, ghostHandle, configJsonOrNull, XpeStageDiagnostics* diag);
```

- **ABI**: 기존 함수·구조체는 안 바뀌었다. `XpeImageMetadata` 는 P/Invoke 용 96 바이트 고정 구조체라 단계별 통계 자리가 없어서 새 함수와 새 구조체를 택했다 (카드가 허용한 첫 번째 형태). `structSize` 로 앞으로 필드를 늘릴 수 있다: 더 큰 구조체는 받아들이고(앞 32 바이트만 쓴다), 작은 것은 `XPE_ERR_INVALID_INPUT`.
- **세는 위치**: gain 단계 출력은 `XPE_STAGE_HOOK(4)` 직후(binning·defect·ghost 가 덮어쓰기 전), 최종은 호출자 버퍼로 복사하기 직전. 세기는 지수 비트가 모두 1 인지 보는 분기 없는 한 번의 패스다 (NaN, +inf, −inf 모두).
- **정수 프레임**: gain 우회에 float 단계가 뒤따르지 않으면 그 지점의 프레임이 uint16 이라 비유한 값이 있을 수 없다. 그 경우 0 으로 두고 `validMask` 비트는 켠다 (측정했고 0).
- **실패한 호출**: 세어 둔 값은 성공할 때만 호출자 구조체로 복사한다. 실패하면 `validMask`·개수는 0 이다.
- **`diag == NULL`**: 세는 코드가 아예 돌지 않는다. 시험·실영상으로 출력 바이트가 같음을 확인했다 (§4).

## 3. 카드의 전제와 다른 점 (리더 확인 필요)

카드는 "gain 단계가 만든 NaN/Inf 를 defect 단계가 메워도 실패시킨다(M8)" 를 되살리는 것이라고 했다. 시험을 쓰려고 실제 경로에서 비유한이 생기는 경우를 먼저 확인했더니:

1. **gain 단계는 유한 입력에서 비유한 값을 만들 수 없다.** 범위 [0.1, 10] 밖이거나 비유한인 gain 값은 전부 결함으로 분류되어 1.0 으로 대체된다 (`gain_correct.cpp`, QA-A-211). 입력은 uint16 이라 `입력 × (1/gain)` 은 항상 유한이다.
2. **그리고 결함 단계는 비유한 입력을 메우지 않고 거부한다.** `defect_correct.cpp` (QA-A-214b): "THE FRAME MUST BE FINITE" — 비유한 화소가 하나라도 있으면 출력을 쓰지 않고 `XPE_ERR_INVALID_INPUT` 과 알림 `XPE_WARN_DEFECT_INPUT_NOT_FINITE` 를 낸다. 결함 맵에 든 화소든 아니든 같다. 같은 코드 주석도 "Inside the pipeline this cannot fire" 라고 적는다.

시험으로 확인했다 (§4 의 4번): 시험 훅으로 gain 단계 출력에 NaN 을 넣고 결함 단계를 켜면 파이프라인은 `XPE_ERR_INVALID_INPUT` 으로 **실패**하고 출력은 쓰이지 않는다. 즉 앱이 `pipeline_out` 으로만 돌려도 "defect 가 메워서 숨긴다" 는 일은 현재 코드에서 일어나지 않고, 비유한이 생기면 호출이 오류를 돌려준다. 카드가 요구한 단언 "gain 개수 > 0, defect 가 메운 뒤 최종 0" 은 그래서 **쓸 수 없었다**. 그런 상태가 만들어지지 않는다.

그래서 이 진단이 하는 일은:
- 결함 단계를 우회한 구성(`bypassDefect`)에서 gain 과 최종의 비유한 수를 알려 준다 (이때는 거부도 대체도 없어서 개수가 유일한 기록이다).
- 앞으로 결함 단계의 거부가 완화되거나 gain 단계가 바뀌면 같은 자리에서 탐지하는 안전망이다.

M8 을 되살린다는 목적에는 현재 코드에서는 **이미 호출 실패가 같은 일을 한다**. 앱 Baseline 이 `pipeline_out` 의 오류 코드를 실패로 다루면 M8 의 효과는 유지된다. 이 진단이 그 위에 더하는 것은 위 두 가지뿐이다. API 는 요청대로 만들었다. 불필요하다고 보면 리더가 닫아도 되지만, 쓰기는 작다(+8 ms).

## 4. 시험

`tests/test_a246_stage_diagnostics.cpp` (`A246Diagnostics`, 8 건). 현재 gain 단계는 비유한을 만들 수 없으므로(위 §3) 비유한이 필요한 시험은 시험 전용 훅 `xpe_pipeline_gain_frame_hook` (gain 단계 출력 직후, 세기 전; `XPE_CACHE_TEST_HOOKS` 빌드에서만)로 출력에 NaN/Inf 를 넣는다. 훅이 필요 없는 5 건은 출하 DLL 에 링크된 `xpe_preprocess_tests` 에서도 돈다.

| 시험 | 단언 |
|---|---|
| `WithANullDiagPointerTheOutputIsByteIdenticalToPipelineOut` | `ex(NULL)` 의 출력 = `pipeline_out` 의 출력 (바이트) |
| `CountingDoesNotChangeTheOutputAndACleanFrameCountsZero` | diag 를 줘도 출력 바이트 동일, 깨끗한 프레임은 0/0, `validMask` 0x3, 화소 수, `structSize` 보존 |
| `ATooSmallStructIsRefusedAndNothingIsWritten` | `structSize` 8 → `INVALID_INPUT`, 출력·diag 안 건드림 |
| `AFailedCallLeavesValidMaskZeroAndTheCountsZero` | 겹치는 버퍼로 실패 → `validMask`·개수 0 (이전 값 지움) |
| `AUint16FinalFrameCountsZeroForBothStages` | 모든 단계 우회(uint16 최종) → 두 개수 0, 비트 켜짐 |
| 훅: `ANonFinitePixelFromTheGainStageStopsTheFrameAtTheDefectStageWhetherOrNotTheMapMarksIt` | 결함 맵에 든 화소와 안 든 화소 모두 → `INVALID_INPUT`, 출력 미작성, `validMask` 0, 알림 `XPE_WARN_DEFECT_INPUT_NOT_FINITE`; diag 없이도 같은 오류 |
| 훅: `WithTheDefectStageBypassedTheCountsSeeNaNAndBothInfinities` | 결함 우회 + NaN, +inf, −inf 3 화소 → gain 3, 최종 3 |
| 훅: `CountingDoesNotChangeAPoisonedOutputEither` | 오염된 프레임에서도 diag 유무에 따라 출력 바이트 동일 |

**반증** (`evidence/43_mutation_log.txt`, 한 스크립트의 연속 출력, 변형마다 실행 파일 SHA-256, 원복 후 `touch` 재빌드, 소스 해시 `af3ac15a25d359c3` 가 처음과 같음):
- 변형 A (gain 개수를 안 셈) → 훅 시험 2 건 빨강.
- 변형 B (NaN 만 세고 무한대를 놓침) → 같은 2 건 빨강.
- 변형 C (성공 전에 호출자 구조체를 채움) → 오염 프레임이 결함 단계에서 실패하는 시험이 빨강 (실패한 호출이 개수를 남김).
- 원복 뒤 전부 초록.

실영상 (`evidence/20_real_frame.txt`, 하니스 `DISABLED_A246_RealFrameDiagnostics`, calib-real-v2 로 `wrist_lat_3072x3072.raw`): `pipeline_out` 과 `pipeline_out_ex(diag)` 모두 기준 영상 v2 와 **바이트 동일**, 개수 0/0, `validMask` 0x3, `pixelCount` 9,437,184. 12 회 번갈아 돌린 시간: `pipeline_out` 중앙값 50.77 ms (47.62~54.15), `_ex` 중앙값 58.97 ms (54.03~61.66), 차 8.20 ms.

## 5. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` | 1042 통과 (간헐 실패 §6 이 한 번 나온 다른 실행이 있고, 같은 코드로 다시 3 번 통과) | `30_xpe_preprocess_tests.txt` |
| `xpe_preprocess_clock_tests` | 48 통과 | `30_xpe_preprocess_clock_tests.txt` |
| `xpe_preprocess_oom_tests` | 88 통과 | `30_xpe_preprocess_oom_tests.txt` |
| ctest 순차 | 100 %, 1282 개 중 실패 0 | `31_ctest.txt` |
| clang-tidy 기준선 게이트 | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |
| doxygen 1.12.0 | 종료 0, 오류·경고 0 (첫 시도에서 `@param` 누락 오류 1 건, 고침. 이전 카드들의 "경고 0" 은 `warning` 만 셌는데 이 오류는 `error:` 라서 이번에는 둘 다 센다) | `42_doxygen.txt` |

cppcheck 는 이 기계에 없다. ASan 도 안 돌렸다.

## 6. 간헐 실패 (이 카드가 만든 것이 아님)

`A241Safety.PipelineOutRefusesWhatCannotHoldTheResultAndWritesNothing` 이 같은 코드에서 어떤 실행은 통과하고 어떤 실행은 `-8`(BUFFER_TOO_SMALL)을 기대했는데 `-1`(INVALID_INPUT)이 나왔다 (60 회 반복 중 5 회, `evidence/50_flaky_pipelineout_toosmall_before_fix.txt`). 원인: `pipeline_core` 가 출력 버퍼의 겹침 검사(`ranges_overlap(in, 입력 크기, out, 필요한 결과 크기)`)를 **크기 부족 검사보다 먼저** 하고, 겹침 범위를 호출자가 준 `out->dataSize` 가 아니라 **필요한 크기**로 잡는다. 시험의 작은 출력 버퍼가 입력 버퍼 바로 앞에 할당되면 필요한 float 크기 범위가 입력과 겹쳐 INVALID_INPUT 이 먼저 나온다. 힙 배치에 달려 있어 CI 에서도 나올 수 있다. 241 (Codex #157) 이 만든 순서이고 내 변경은 이 코드 앞에 아무것도 넣지 않았다. 고치는 것은 별도 커밋으로 올린다(아래 §7).

## 7. 변경 파일

제품: `include/xpe/preprocess_api.h`, `include/xpe/preprocess/xpe_preprocess_internal.h` (시험 전용 훅 선언), `src/pipeline.cpp`, `CMakeLists.txt` (새 시험 파일).
시험: `tests/test_a246_stage_diagnostics.cpp` (신규), `tests/test_zz_a241_measure.cpp` (실영상 하니스).
증거: `.moai/reports/lane-pre/QA-A-246/`.

## Card Cross-Check

| milestone | card |
|---|---|
| `pipeline_out` 의 단계별 비유한 개수 | QA-A-246 (이 보고서) |
