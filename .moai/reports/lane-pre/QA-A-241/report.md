# QA-A-241 — 1단계 실패 B2·B4 처리, 실영상 출처 검증, B1~B10 재판정

기준: 카드 `QA-A-241` (Refs #245), 정정된 SRS 문구(main `d0da9adf`). 측정일 2026-10-05, 로컬 Windows 11. 증거는 `evidence/`.

## 1. 결과

**통과 9 / 실패 1 / 미구현·기준 없음 0** (QA-A-240 은 7 / 3 / 0). 남은 실패는 B6 평탄 잔여이고 값은 그대로예요 (카드가 예고한 대로).

| # | 판정 | 240 → 241 | 근거 |
|---|---|---|---|
| B1 offset 적재 | 통과 | 그대로 | `6x_re_B1_*` |
| B2 gain 범위 | **통과** | 실패 → 통과 (이유는 §2) | `30_B2_limit_boundary.txt`, `6x_re_B2_*` |
| B3 결함 맵 | 통과 | 그대로 (미해결 한 가지는 §7) | `6x_re_B3_*`, `6x_re_B3b_*` |
| B4 안전 동작 | **통과** | 실패 → 통과 (네 항목 모두, §3) | `11_a241_tests.txt`, `6x_re_B4_*` |
| B5 보정 순서 | 통과 | 그대로 | `6x_re_B5_*` |
| B6 보정 품질 | **실패** | 그대로 | `6x_re_B6_*` |
| B7 프레임 시간 | 통과 | 중앙값 55.9 → 56.5 ms | `6x_re_B7_*` |
| B8 메모리 | 통과 | 모듈 최대 118.2 MiB 그대로 | `6x_re_B8_memory.txt` |
| B9 적재 시간 | 통과 | 97.9 → 96.4 ms | `6x_re_B9_*` |
| B10 기준 영상 | 통과 | SHA-256 바이트 동일 (§5) | `50_B10_after_change.txt`, `6x_re_B10_*` |

## 2. B2 — 상한은 이미 있었다: 240 의 판정이 틀렸다

카드는 "비율 상한을 넣는다" 였는데, 읽어 보니 **상한은 이미 코드에 있었어요** (QA-A-211, #233). `XPE_GAIN_DEFECT_MAX_FRACTION = 0.05` (`xpe_preprocess_internal.h`)와 `xpe_gain_scan_over_limit`, 적재 거부 `XPE_ERR_INVALID_CALIB_DATA` + 알림 `XPE_WARN_GAIN_PIXELS_OVER_LIMIT`, 그리고 `test_gain_defect_classify.cpp` 의 시험까지 있어요. 새 SRS 문구(화소 단위 분류 + 경고, 상한 초과 시 거부)와 동작이 이미 같아요.

QA-A-240 보고서는 B2 를 "SRS 는 거부, 구현은 경고만"이라며 실패로 적었어요. 그 판정의 두 가지가 틀렸어요.
- 옛 SRS 문구(전체 거부)를 기준으로 삼았고, 그 문구는 이제 바뀌었어요.
- "구현은 경고만 한다"고 적었지만 **상한 아래만** 시험했어요 (실제 맵 0.445%, 화소 하나). 상한 위를 시험하지 않았고 그 경로를 찾아보지도 않았어요. 코드를 한 번 열어 봤으면 알 수 있었어요.

이번에 경계를 직접 측정했어요 (`30_B2_limit_boundary.txt`, 256×256, 한계 `count > 0.05 × total` = 3276.8):

| 범위 밖 화소 | 비율 | 적재 결과 |
|---|---|---|
| 0 | 0 % | `XPE_OK`, 알림 없음 |
| 292 | 0.445 % (실제 맵 비율) | `XPE_OK`, 분류 경고 1건 |
| 3,276 | 4.9988 % | `XPE_OK`, 분류 경고 1건 |
| 3,277 | 5.0003 % | **`XPE_ERR_INVALID_CALIB_DATA`**, `XPE_WARN_GAIN_PIXELS_OVER_LIMIT` |
| 3,277 (값 20.0 / NaN) | 5.0003 % | 같은 거부 |
| 19,661 | 30 % | 같은 거부 |

그래서 B2 코드는 바꾸지 않았고, 상수 주석에 **"잠정"** 과 근거 출처를 적었어요 (`xpe_preprocess_internal.h`).

### 상한 값 제안 (확정은 리더)

현재 5 % 는 측정에서 나온 값이 아니라 SRS-CALIB-FUNC-003 의 결함 밀도 허용치를 그대로 쓴 거예요 (SRS FUNC-002 는 "측정한 검출기 데이터로 정한다"고 적어 두었어요). 가진 측정은 다음이 전부예요.

- 실제 gain 맵(CalData_6 평탄 6장, 제품 생성기) 범위 밖: 42,026 / 9,437,184 = **0.445 %**. 결함 맵 23,505 = 0.249 %. 두 집합의 합집합 51,543 = 0.546 %.
- 상한을 올렸을 때의 프레임 시간 (`31_B2_fraction_cost.txt`, 실영상·실제 맵, 무작위 결함을 더함, 고스트 끔, 5프레임 중앙값). 결합 비율 = 표의 결함 맵 비율 + gain 분류 0.445 % (겹침은 무시할 정도):

| 결함 맵 | 합친 비율(대략) | 프레임 중앙값 |
|---|---|---|
| 0.249 % (실제) | 0.69 % | 47.6 ms |
| 0.747 % | 1.19 % | 65.6 ms |
| 1.242 % | 1.69 % | 76.5 ms |
| 2.225 % | 2.67 % | 105.8 ms |
| 5.114 % | 5.56 % | 182.9 ms |
| 9.742 % | 10.19 % | 293.7 ms |

시간은 1 %p 당 약 26 ms 씩 늘고, 측정한 10 % 까지도 500 ms 한계 아래예요. 시간이 상한을 정하지는 않아요 (메모리는 이 범위에서 재지 않았어요).

| 후보 | 실제 맵(0.445 %) 대비 여유 | 장단 (사실만) |
|---|---|---|
| 5 % (현재) | 11.2배 | FUNC-003 문구와 같고, 결함 단계의 `UNION_OVER_LIMIT` 경고와 한 상수를 공유한다. 합친 5.56 % 에서 프레임 183 ms |
| 2 % | 4.5배 | 실제 맵 하나 기준으로는 넉넉하지만, 다른 검출기의 비율은 측정한 적이 없다 |
| 1 % | 2.2배 | 실제 맵 하나와의 여유가 가장 좁다 |

제 제안은 **5 % 를 유지 (잠정)** 이에요. 조건: 표본이 이 검출기 하나뿐이라서 더 엄격한 값을 지지할 데이터가 없다는 것. 다른 검출기의 평탄 영상(다중 선량)이 생기면 그 맵의 범위 밖 비율로 다시 정해야 해요. 이건 제안이고, 상수는 하나(`XPE_GAIN_DEFECT_MAX_FRACTION`)라 값만 바꾸면 돼요.

## 3. B4 — 네 항목을 SRS 대로 고쳤다

| SRS | 수정 | 시험 |
|---|---|---|
| SAFE-002, FUNC-009 (적재 뒤 만료) | 프레임마다 적재된 맵 세 개의 만료를 확인한다. 하나라도 지났으면 `XPE_ERR_CALIBRATION_EXPIRED`, 알림, 버퍼 그대로, 단계 실행 없음. 우회된 맵도 본다("any loaded file"). `CalibSnapshot` 에 만료 필드 셋 추가 | `A241Safety.AMapThatExpiresWhileLoadedStopsTheFrame` (offset/gain/defect 각각), `ABypassedMapThatExpiredStillStopsTheFrame`, `ANeverExpiringFileDoesNotStopTheFrameLater` |
| FUNC-009 (만료 0) | 적재가 끝난 뒤 `XPE_WARN_NO_EXPIRY` (종류별로 한 번. 같은 상태에서 다시 적재해도 반복하지 않는다. 알림 큐가 64개라서) | `ExpiryZeroIsReportedOncePerState`, `AFileWithAnExpiryRaisesNoExpiryWarning` |
| SAFE-001 (우회) | `bypassOffset`/`bypassGain` 은 허용하되 메타데이터에 `XPE_FLAG_CORRECTION_BYPASSED` (신규 `0x00004000`) 와 프레임마다 `XPE_WARN_CORRECTION_BYPASSED` 알림. 기존 시험 `PipelineExNullStateSkipsCalibration` 에 플래그·알림 단언을 더했다 | `ABypassedOffsetOrGainIsFlaggedAndLogged` |
| SAFE-004 (입력 불변) | **신규 공개 함수 `xpe_preprocess_pipeline_out(in, out, ...)`**: 입력은 읽기 전용, 결과는 별도 출력 버퍼. 기존 `xpe_preprocess_pipeline(_ex)` 는 호출자 버퍼에 쓴다는 계약이 그대로다 | `PipelineOutLeavesTheInputAndGivesTheInPlaceResult` (입력 다이제스트 동일, 결과가 제자리 호출과 바이트 동일), `...EveryStageBypassed...`, `...RefusesWhatCannotHoldTheResultAndWritesNothing` |

SAFE-004 는 결정이 필요 없다고 하셨는데, 구현해 보니 **새 공개 함수가 필요했어요**. 기존 진입점은 입력 버퍼에 결과를 쓰는 것이 공개 계약이라 그걸 바꾸면 호출자가 깨져요. 그래서 기존 함수는 두고 별도 출력 버전을 더했어요. SRS 문구("작업 버퍼에서 제자리 처리하거나 별도 출력 버퍼를 만든다")는 이 구성으로 맞는데, **기존 진입점을 쓰는 호출자는 여전히 입력을 덮어써요.** 저장소 소스에서 이 진입점을 부르는 곳은 시험과 문서뿐이었어요 (C# 호출은 찾지 못했어요: `*.cs` 에서 이름 검색). `api-spec.md`, 문서, C# 바인딩은 제 소유가 아니라 건드리지 않았어요 (§8).

실제 프레임으로도 확인했어요 (`6x_re_B4_*`): 적재 6초 뒤 만료되는 맵을 올리고 8초 뒤 호출 → `XPE_ERR_CALIBRATION_EXPIRED` (이전 측정은 `XPE_OK`), 우회 세 조합 모두 플래그·알림, 만료 0 알림, `pipeline_out` 은 입력 그대로이고 결과가 제자리 호출과 바이트 동일.

시험 쪽에서 맞춘 것 (모두 이유와 함께 주석):
- `test_output_unchanged_237b.cpp`: 다이제스트에서 신규 플래그 비트만 가렸어요. 화소와 나머지 플래그는 기록된 값 그대로라서 "출력 불변"의 증거가 유지돼요.
- `test_gain_defect_classify.cpp`: "깨끗한 맵은 아무 경고도 안 낸다" 단언이 `XPE_WARN_` 전체였는데, 시험 맵이 만료 0 이라 새 경고가 걸려요. `XPE_WARN_GAIN_`, `XPE_WARN_DEFECT_` 로 좁혔어요.
- `test_global_state_hygiene.cpp`: 시험 맵 대부분이 만료 0 이라 시험 117개가 `XPE_WARN_NO_EXPIRY` 를 큐에 남겼어요. 큐에 **그 경고만** 있을 때에 한해 비우도록 했어요. 다른 경고가 하나라도 있으면 전과 똑같이 보고돼요.
- `test_json_helpers.cpp`: 일부러 우회하는 시험이라 TearDown 에서 알림을 비워요.

## 4. 실영상 출처 검증 (사용자: "스스로 검증해")

**결론: 판정 불가. 가장 가능성이 높은 것은 CalData_6 이지만, 설명되지 않은 신호가 하나 남아 있어요.**

대조군으로 저장소의 다른 검출기 영상을 썼어요: `aed_shock_had1717mc`(HAD1717MC) 암전류·영상, `auradr_release_line_trg`, `corner_blemish_16j20b3`/`17a06b1`. (`wrist_lat` 자체의 출처 문서는 없고 `fixture-manifest.json` 에 크기·형식·SHA 만 있어요.)

| 확인 | 결과 | 읽는 법 | 증거 |
|---|---|---|---|
| (a) 크기·비트 깊이·바이트 순서 | 18,874,368 B = 3072²×2, 14비트 값 (2481~15451), 포화·0 없음. 가로 이웃 차이: 리틀엔디언 13.41, 빅엔디언 5581.6 → 리틀엔디언 | 모든 픽스처에 똑같이 성립해서 구별에는 못 쓴다 | `20_` |
| (d) 오프셋 대좌 | wrist 최솟값 2481, p0.1 2517, 하위 0.5 % 평균 2559. CalData_6 암전류는 p0.1 2483, 평균 2503, p99.9 2520. HAD1717MC 암전류는 평균 3868, p0.1 3076. AuraDR 는 15364 이상 | wrist 의 낮은 끝이 CalData_6 의 대좌에 놓이고, HAD1717MC 의 대좌(3076 이상)보다 낮다. 오프셋 설정이 다르면 뒤집힐 수 있다 | `20_` |
| (b) 고정 패턴, 화소 단위 (9×9 고역 통과, 전 화소) | wrist ~ CalData_6 암전류 0.354, 평탄 6장 0.258~0.498. CalData_6 안의 같은 검출기 쌍은 0.324(02~03), 0.460(01~05). 다른 검출기끼리는 0.03 (CalData_6~HAD1717MC 0.034/0.033, wrist~corner_blemish 0.024/0.026) | wrist 가 CalData_6 와 **같은 검출기 쌍과 같은 범위**의 상관을 보인다 | `22_` |
| 위치 대조 | wrist 를 02 번 평탄에 대해 밀면 0.498 → 1칸 0.32~0.35 → 3칸 0.07 → (3,3) −0.08 | 진짜 화소 위치 패턴이다 (전체 밝기 닮음이 아니다) | `22_` |
| 같은 측정의 양성 대조 | HAD1717MC 영상 ~ 자기 암전류 0.959 | 이 측정은 같은 검출기를 알아본다 | `22_` |
| **설명 안 되는 신호** | wrist ~ HAD1717MC 암전류 0.255, 영상 0.241, 충격 영상 0.247 | CalData_6 ~ HAD1717MC 는 0.03 인데 wrist 는 둘 다와 닮았다. 원인을 찾지 못했다 | `22_` |
| (c) 결함 맵 위치에서 튀는 비율 | wrist 에서 5 로버스트 시그마를 넘는 화소가 BPMap 위치에서 0.6 %, 무작위 위치 평균 0.0 % (200회 중 최대 0.1 %) | 무작위보다 높지만 절대값이 작다. CalData_6 암전류에서는 BPMap 위치 15.8 % 와 무작위 15.9 % 가 같아서 이 측정은 거기서 구별력이 없다. 약한 근거 | `20_` |

지지하는 것: 대좌 수준(d), 화소 패턴이 같은 검출기 쌍의 범위이고 위치에 묶여 있음(b). 막는 것: HAD1717MC 와의 0.25 가 설명되지 않아서 패턴 상관만으로는 HAD1717MC 를 배제하지 못해요. 대좌는 HAD1717MC 를 배제하지만 오프셋 설정은 같은 기종에서도 바뀔 수 있어요.

**정정 (QA-A-240 보고서)**: 240 은 "행 패턴 상관 0.895" 를 `dark.raw` 와의 값으로 적었는데, 이번에 같은 값이 재현되지 않았어요. 행·열 평균 프로파일 상관은 창 크기에 크게 흔들려요 (CalData_6 암전류와 행 0.05~0.33). 0.895 는 창 257 에서 **CalData_6 `bright02` 와의 행 상관**과 같은 값(`21_`)이에요. 240 의 라벨이 틀렸을 가능성이 커요. 240 의 원본 스크립트가 남아 있지 않아 원인은 확정하지 못했고, 그 숫자는 근거로 쓰지 말아 주세요. 같은 검출기라는 240 의 표현("가능성이 높다")도 이 결과로는 "판정 불가, CalData_6 가 가장 유력"으로 낮춰요.

확정하려면: 촬영한 사람이나 원본 DICOM 헤더에서 검출기 일련번호를 받는 것이 가장 빨라요. 데이터로는 HAD1717MC 와의 0.25 가 어디서 오는지(판독 블록 구조, 공통 결함 열 등)를 가려내야 해요. 이번 범위에서는 그 분석까지 못 했어요.

## 5. 기준 영상 불변

변경 후 같은 입력·같은 설정(`{"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}`, 고스트 없음)으로 맵을 처음부터 다시 만들어 출력해서 비교했어요 (`50_B10_after_change.txt`).

- 새 출력 SHA-256 `42e78facd6cd75edbb59d26b9be1cca313d431d07900e3064cd472c068d1ef02`
- 공용 폴더 `D:/workspace-github/xpe-data/m1-baseline/wrist_lat_3072x3072_corrected_f32le.raw` 와 **같은 값**, FNV-64 `3e0aa952d2fc9252`, 플래그 `0x201c` (우회 플래그 없음)
- B8 프로브 출력 다이제스트도 240 과 같은 `a13a2228ae72f237`

즉 제품 변경은 이 입력의 출력을 바꾸지 않아요. (공용 폴더에 `run1.raw` 도 있는데 B10 의 결정성 확인용 복사본이고 기준 영상이 아니에요.)

## 6. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` 전체 (한 프로세스) | 1030 통과, 실패 0 (`DISABLED_` 56개 제외) | `12_suite_final.txt` |
| ctest, 순차 (CI 단계와 같은 방식) | 100 % 통과, 1222개 중 실패 0 | `13_ctest.txt` |
| clang-tidy 기준선 게이트 (`check_clang_tidy.py preprocess`, main 의 최신 기준선) | `GATE_EXIT=0`, NEW 0 (처음엔 2건: 교환 쉬운 매개변수 두 곳 → 열거형 `CalibMapKind` 로 바꾸고, `pipeline_out_impl` 에는 사유 있는 `NOLINT` 한 줄) | `41_clang_tidy_gate.txt` |
| doxygen 1.12.0, CI 와 같은 순서 | 종료 0, 경고 0 | `40_doxygen.txt` |
| 새 시험 | `A241Safety.*` 9개 통과 | `11_a241_tests.txt` |

`xpe_preprocess_oom_tests`, 나머지 타깃도 `--parallel 2` 로 전부 빌드돼요.

## 7. 미검증 · 잔여 위험

- **cppcheck 게이트는 로컬에서 못 돌렸어요.** 이 기계에 cppcheck 가 없어요 (CI 가 설치). 기준선 `cppcheck_baseline_preprocess.txt` 는 4건인데 제 변경이 새 지적을 만드는지는 CI 결과로만 알 수 있어요.
- **ASan 잡은 로컬에서 안 돌렸어요.** 새 시험(`A241Safety`)은 짧은 대기(최대 0.6초×3)를 쓰고 임시 폴더 이름이 고정이라 **ctest `-j` 병렬이면 서로 겹쳐요** (`-j 2` 로 돌렸을 때 제 시험 셋과 기존 몇 개가 실패했고, 순차로는 전부 통과). CI 는 순차예요.
- 새 공개 함수는 호출 코드가 시험뿐이고 C# 바인딩·문서에는 반영되지 않았어요.
- **B3 의 5 % 결함 밀도**: 결함 맵 자체가 5 % 를 넘어도 적재도 파이프라인도 알리지 않아요 (240 `B3b`: 4·6·30 % 모두 알림 0). `UNION_OVER_LIMIT` 은 gain 분류 화소가 합쳐질 때만 나요. 이번 카드 범위가 아니라 두었어요.
- 상한 제안은 검출기 하나의 한 세트 값에 기대요. 메모리는 결합 비율을 올린 조건에서 재지 않았어요.
- B6 평탄 잔여(1.18~3.34 %)의 원인은 여전히 미확인이에요 (240 의 가설 둘).
- 우회 알림은 프레임마다 하나라서 배치·연속 촬영에서 64개 큐를 채워요. 우회를 의도해서 쓸 때만 나는 알림이라 그대로 뒀어요.
- B9 는 콜드 캐시를 재지 않았어요 (240 과 같아요).

## 8. 리더 확인 요청

1. **상한 값**: 5 % 유지(잠정)를 제안해요 (§2). 확정해 주세요.
2. **신규 공개 함수 `xpe_preprocess_pipeline_out`**: SAFE-004 를 맞추려고 추가했어요. `api-spec.md`, 문서, C# 바인딩 반영은 리더 소유 영역이라 안 했어요. 기존 진입점이 계속 입력을 덮어쓰는 것이 받아들일 만한지 결정해 주세요.
3. **신규 메타데이터 플래그 `XPE_FLAG_CORRECTION_BYPASSED = 0x00004000`** (`modules/common/include/xpe/common/xpe_types.h`): 다른 레인(clients, gui)이 플래그를 열거하고 있다면 알려야 해요.
4. **SRS 문구 두 곳**: FUNC-002 "bound to be set from measured detector data, QA-A-241" 는 §2 값이 정해지면 갱신, FUNC-009 의 기한 필드 설명(헤더 offset 8-11 uint32, 2000 기준)은 구현(int64 epoch ms)과 다르다는 점은 이번에도 그대로예요.
5. **실영상 출처**: §4. 일련번호를 아는 사람에게 묻는 것이 가장 빠른 확정 수단이에요.
6. **240 정정 두 건**: B2 판정(§2)과 0.895 의 출처(§4).

## 9. 변경 파일

제품: `modules/common/include/xpe/common/xpe_types.h`, `modules/preprocess/include/xpe/preprocess_api.h`, `.../xpe_preprocess_internal.h`, `src/pipeline.cpp`, `src/preprocess.cpp`, `src/xpe_calibration.cpp`, `src/xpe_calib_load_{offset,gain,defect_map}.cpp`.
시험: 신규 `tests/test_a241_safety.cpp`, `tests/test_zz_a241_measure.cpp` (`DISABLED_`, 측정), 수정 `test_zz_a240_checklist.cpp` (B4 에 `pipeline_out` 확인), `test_gain_defect_classify.cpp`, `test_global_state_hygiene.cpp`, `test_json_helpers.cpp`, `test_output_unchanged_237b.cpp`, `test_pipeline_ex.cpp`, `CMakeLists.txt`.

## Card Cross-Check

| milestone | card |
|---|---|
| B2·B4 수정, 출처 검증, B1~B10 재판정 | QA-A-241 (이 보고서) |
