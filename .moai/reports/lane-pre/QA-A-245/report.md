# QA-A-245 — 운영자 앱이 읽을 실데이터 보정 세트(xcal) 내보내기와 CalData_6 "다크 이중 차감" 확인

## 1. 결과

| 항목 | 결과 |
|---|---|
| 1. 다크 이중 차감 확인 | **차감하지 않았다.** 1단계 판정은 이 점에서 다시 열 이유가 없다 (§2) |
| 2. 실데이터 xcal 세트 | `D:/workspace-github/xpe-data/calib-real-v2/` 에 `offset.xcal`, `gain.xcal`, `defect.xcal`, `README.txt` |
| 3. 앱 경로 대조 | 앱과 같은 단계 순서의 출력이 기준 영상 v2 (`d152e8eb…`)와 **9,437,184 화소 모두 바이트 동일** (§4) |

제품 코드 변경은 없다 (측정 하니스와 산출물).

## 2. "다크를 다시 빼지 말 것" 이 지켜졌나

`CalData_6/README.md` 는 `bright*` 가 이미 오프셋 보정된 값이니 `dark` 를 다시 빼지 말라고 한다. 이 카드의 확인은 "gain 을 만들 때 `bright` 에서 `dark` 를 뺐는가" 이다.

- **gain 생성은 다크를 넘기지 않았다.** 모든 gain 생성 호출의 세 번째 인자(`dark_reference`)가 `nullptr` 이다. 기준 영상 v2 의 gain (`test_zz_a241_measure.cpp:422` (`B10v2_Baseline`), `xpe_calib_generate_gain(three, 3, nullptr, gainPath.c_str(), nullptr)`, 입력은 `bright04/05/06`), B6 보류 시험(같은 파일 `:304`, `:532`, `:753`, `:1015`, `:1298`, 이 카드의 내보내기 하니스 `:1393`), 240 의 전체 평탄 gain (`test_zz_a240_checklist.cpp:211`, `:937`).
- **제품이 `nullptr` 을 어떻게 다루는지**: `src/xpe_calib_generate_gain.cpp:293-296` — `dark_val = dark_ptr != nullptr ? dark_ptr[j] : 0.0; corrected = flat_val - dark_val;`. 다크 참조가 없으면 뺄셈의 값이 0 이다. 즉 gain 은 `bright` 그대로의 평균이다.
- **`dark` 가 쓰이는 곳은 두 군데뿐이다**: (a) offset 맵 (`dark.raw` 한 프레임), (b) 하니스가 보정 전 원시 프레임을 만들 때 `raw = dark + 평탄` (`B6` 보류 시험 등: 이미 보정된 평탄에 다크를 **더해서** 오프셋 보정 단계를 거치면 평탄이 돌아오게 한 것). 실영상(`wrist_lat`)은 원시 프레임이라 offset 보정이 맞다.
- 따라서 B6 지표(FlatResidualPct, FPN, 줄무늬)와 기준 영상 v2 는 다크 이중 차감의 영향을 받지 않는다.

한계: 줄 번호는 이 커밋의 `grep -n` 값이다 (같은 파일을 고치면 밀린다, 함수 이름이 주소다). 이것은 코드 읽기와 인용이고, "평탄이 이미 보정돼 있다" 는 README 의 판단(생성 이력은 확인되지 않았다고 README 도 적는다)에 기댄다.

## 3. 내보낸 세트

`xpe-data/calib-real-v2/` (SHA-256, 만든 명령, 입력, 한계는 그 폴더의 `README.txt`; 사본 `evidence/30_calib-real-v2_README.txt`):

| 파일 | 크기 | SHA-256 |
|---|---|---|
| `offset.xcal` | 37,748,950 B | `0f888eb6958057706289c8d8d30d63badf4132aab2db850b94988d2bc723ba82` |
| `gain.xcal` | 37,749,092 B | `004056fd2a3fb89a67a1255a8be1454f071e29d115a74eb70c90eca3d32fd142` |
| `defect.xcal` | 9,437,336 B | `ff9df014b3b66898d987d1298855f35669445951d8958e7c7d2858240a4bc007` |

기준 영상 v2 를 만든 것과 **같은** 맵이다: offset = `dark.raw` 한 프레임(100 ms, 25 C), gain = `bright04/05/06`(다크 참조 없음, 선량 가중 ADU 평균), 결함 맵 = `BPMap.map`(23,505 화소). 제품 생성기와 제품 기록기를 썼고 하니스(`A241Measure.DISABLED_A245_ExportAndAppPath`)가 세 파일이 한 세션 ID 를 갖게 하고 만료를 설정했다.

**만료**: 생성기가 쓴 값은 0(만료 없음)이다. 0 이면 적재마다 `XPE_WARN_NO_EXPIRY` 가 나서 앱 알림에 섞이므로 `만든 시각 + 365 일` (`1822893017654` = 2027-10-07 07:10:17 UTC)로 했다. 확인하는 동안 만료가 끼어들지 않으면서 0 이 아닌 값이다. 만료가 지나면 보정이 `XPE_ERR_CALIBRATION_EXPIRED` 로 프레임을 거부한다 (정상 동작).

**재현성**: 파일 바이트는 다시 만들면 달라진다 (헤더의 생성 시각과 자동 생성되는 세션 ID). 맵의 화소 값과 그 결과 영상은 바이트 동일하다.

## 4. 앱 경로 대조

앱(`gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs`)의 순서를 그대로 따랐다: `xpe_preprocess_init` → `xpe_calib_load_offset` → `xpe_calib_load_gain` → `xpe_calib_load_defect_map` → `xpe_offset_correct`(uint16→uint16) → `xpe_nonlinearity_correct(&offsetOut, nullptr)` → `xpe_gain_correct`(uint16→float32) → `xpe_defect_correct`(float32→float32). 입력은 실영상 `wrist_lat_3072x3072.raw`, 맵은 **내보낸 세 파일을 읽어서** 쓴다 (`evidence/20_export_and_app_path_run.txt`):

| 비교 | 바이트 동일 | 다른 화소 | 평균 \|차\| | 최대 \|차\| | 분위수 (p50 / p99 / p99.9) |
|---|---|---|---|---|---|
| 앱 단계 순서 ↔ 기준 영상 v2 | **예** | 0 / 9,437,184 | 0 | 0 | 0 / 0 / 0 |
| 출하 `xpe_preprocess_pipeline_out` (내보낸 파일) ↔ 기준 영상 v2 | **예** | 0 | 0 | 0 | 0 / 0 / 0 |
| 앱 단계 순서 ↔ 출하 파이프라인 (같은 파일) | **예** | 0 | 0 | 0 | 0 / 0 / 0 |

비선형 단계는 LUT 가 없어 프레임을 바꾸지 않는다 (앱 주석이 말하는 대로 바이트 동일). 입력 버퍼는 앱 경로에서 바뀌지 않았다. 기준 영상 v2 의 평균은 2198.1751 ADU.

세트를 적재하고 앱 순서를 한 번 돌렸을 때 알림 3건이 쌓인다 (`evidence/21_alerts_of_the_exported_set.txt`), 사용자가 앱에서 보게 될 것:
1. `XPE_WARN_CALIB_SESSION_UNSPECIFIED` — 세션 ID 가 생성기가 자동으로 만든 값이다 (세 파일이 같은 값이라 충돌은 아니다).
2. `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT` — gain 이 [0.1, 10.0] 밖인 44,053 화소(0.467 %, 한도 5.0 %)를 결함으로 분류. 가장자리 64 화소 띠 안이 44,051 개다.
3. 비선형 단계가 아무것도 하지 않았다는 알림 (LUT 없음).

### 이 대조가 보이는 것과 못 보이는 것

보이는 것: 앱이 호출하는 단계 함수의 순서로 같은 맵을 적용하면 1단계 기준 영상과 같은 float32 영상이 나온다. 즉 앱이 계산하는 값은 1단계 기준과 같다.

못 보는 것 (보고서에서 숨기지 않는다):
- 앱이 **화면에 그리는 영상**은 이 float 를 `ScaleToUInt16` 으로 줄인 것(앞서 본 코드: 프레임 최대값으로 정규화)이다. 그 단계와 화면 렌더링은 이 대조에 없다.
- 앱은 단계 함수에 `bodyPart`, kVp 등 메타데이터를 넘긴다. 이 하니스는 빈 메타데이터를 넘겼다. 단계 함수가 메타데이터로 출력을 바꾸지 않는다는 것은 출하 파이프라인(메타데이터 인자가 있는 호출)과 바이트 동일했다는 점에서 간접으로만 보인다. 앱의 메타데이터 값으로 직접 돌려 보지는 않았다.
- 앱 UI 로 직접 열어 확인하지는 않았다.

## 5. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` | 1037 통과, 실패 0 (`DISABLED_` 64개 제외) | `40_suite.txt` |
| clang-tidy 기준선 게이트 (`preprocess`) | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |
| 내보낸 파일 해시 | 알림 확인 실행 뒤에도 같다 (파일을 다시 쓰지 않았다) | `31_exported_sha256.txt` |

doxygen·순차 ctest 는 돌리지 않았다 (제품 코드·헤더 변경 없음, 시험 목록 변경 없음). cppcheck 는 이 기계에 없다.

## 6. 한계

- 검출기 1 대(CalData_6)의 데이터이고 평탄 4·5·6 이 한 촬영 조건이라는 것은 추정이다 (선량·kVp·mAs 기록이 데이터에 없다).
- 이 세트는 1단계 확인용이고 임상 검증된 보정이 아니다.
- 이 보고서는 `moai-domain-humanize` 최종 패스를 거치지 않았다.

## 7. 변경 파일

시험: `modules/preprocess/tests/test_zz_a241_measure.cpp` (`DISABLED_A245_ExportAndAppPath`, `DISABLED_A245_AlertsOfTheExportedSet`).
산출물(리포 밖): `D:/workspace-github/xpe-data/calib-real-v2/`.
증거: `.moai/reports/lane-pre/QA-A-245/`.

## Card Cross-Check

| milestone | card |
|---|---|
| 실데이터 xcal 세트 내보내기와 다크 이중 차감 확인 | QA-A-245 (이 보고서) |
