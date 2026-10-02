# QA-A-222 — #216·#218 마감 대조: 이슈 본문이 지금도 맞는가 (Refs #216 #218)

코드 변경 없음, 보고서만이다. 증거: `evidence/`.

## 결론

| 이슈 | 본문의 주장 | 지금 | 닫을 수 있나 |
|---|---|---|---|
| **#218** | `xpe_verify_*` 가 요구의 합격 기준과 다른 지표로 판정한다 — FUNC-017 FlatResidualPct 0건, FUNC-019 recall/FPR 0건 | **제목의 두 주장은 둘 다 해소**: FUNC-017 은 게이트가 달렸고(`fa3da63b`·`edc53cbc`·`28650606`), FUNC-019 는 시험 쪽 합성 오라클로 구현·CI 통과(`bc51ab45`). 남은 것은 제목과 다른 질문 둘(아래 §1.3) | 제목의 주장은 닫을 수 있다. **남은 둘을 어디에 둘지는 리더 몫**(이슈를 닫고 새 이슈로 옮기거나, 열어 두거나) |
| **#216** | 수출 48개 중 21개가 REQ-P1A 요구에 이름이 없다(상한) | 이름 축으로 **16개**(21 → 16). 16개 **전부** SRS 본문 또는 RTM 줄에는 이름이 있다(셋 다 없는 것 0개) | **아직 아니다.** 남은 것: RTM 줄에만 있는 7개의 SRS 본문 미확인, 그리고 SPEC 범위 결정 1건(§2.3) |

두 이슈 모두 본문의 수는 오래됐다. 특히 #216 의 "21" 은 이름 축의 옛 상한이고, 지금 요구가 전혀 없는 함수는 0개다. 다만 "0개" 는 이름이 어느 요구 줄에든 나온다는 뜻이지, 그 요구가 함수의 동작을 충분히 서술한다는 뜻이 아니다(§2.3).

## 0. 무엇을 기준으로 쟀나 (그리고 한 번 틀린 것)

- **기준 트리**: 측정 시작 때 `origin/main` 은 `d43e80c2` 였고 작업 중 `f9803071` 로 움직였다(새 커밋 18개, 변경 파일 82개). 그 82개 중 **SPEC·`docs/calibration`·`modules/preprocess` 는 0개**라 측정값은 두 시점에서 같다(`git diff --name-only d43e80c2 origin/main`, 대조: 전체 변경 82개는 잡힘). CI 로그 관측은 `d43e80c2` 의 run 이다.
- **SPEC·SRS·RTM**: `origin/main` `d43e80c2` 의 블롭. SPEC-XPE-P1A **1.3.3**(`evidence/20_spec_versions.txt`). 이 워크트리의 `spec.md` 는 **1.3.2** 라 낡았다 — 이 워크트리 파일로 재면 틀린 수가 나온다.
- **수출 이름**: 이 워크트리 빌드의 `xpe_preprocess.dll` 을 PE 수출 표로 직접 읽음(스크립트 `exports_vs_req.py`, 결과 `evidence/31`). C 이름 **48**, 맹글된 C++ 이름 5(`read_xcal_file` 등, 공개 API 아님). 맹글 5개는 센 대상이 아니다.
- **census 도구는 쓰지 않았다**: `tools/docs/api_requirement_census.py` 의 `REPO` 가 `image-processing` 체크아웃으로 고정돼 이 워크트리를 읽지 않고, 심볼 매칭이라 SRS 에 눈멀기 때문이다(도구 docstring 이 스스로 적음). 같은 방식(이름 매칭)을 직접 구현하고, SRS·RTM 은 "같은 줄에 이름이 있는 줄의 `SRS-CALIB-*` ID"로 따로 뽑았다.
- 대조군: 양성 `xpe_defect_correct` → `REQ-P1A-012, 020a` 나옴, 음성 지어낸 이름 → 없음. 수출 표 쪽 대조: `xpe_preprocess_init`·`xpe_defect_correct` 있음, `xpe_crc32` 없음(철회 `498fc3c` 확인). 수출 이름 48개와 `origin/main` 헤더의 `XPE_API` 선언 48개는 **정확히 일치**(`evidence/39`, 양쪽 차집합 0, 대조군 통과).
- **해시도 한 번 틀렸다**: 처음에 카드 이름으로 `git log --grep` 을 돌려 나온 해시를 그대로 적었는데, 그중 둘은 **병합 커밋**(`af21f669`·`ef98cd67`)이고 셋(`83a56595`·`a5193828`·`74d6a70d`)은 다른 일을 한 커밋이었다. 제목에 카드 번호가 있고 병합이 아닌 커밋으로 다시 찾아 조상 관계를 재확인했다(`evidence/11`, 정본). `evidence/35` 의 `--grep` 해시 줄은 그 틀린 값이라 머리에 주의를 달아 두었다.
- **처음에 틀린 것**: `git show origin/main:경로` 가 이 셸의 경로 변환에 걸려 죽었고 `grep` 이 빈 값을 받아 "main 의 SPEC 버전 없음" 으로 읽혔다. 같은 원인으로 헤더 비교가 빈 집합끼리의 비교(차이 0)가 됐다. 둘 다 블롭 해시(`git cat-file blob`)로 바꿔 다시 쟀고, 위 수치는 그 뒤 값이다. 이 저장소에서 이미 두 번 적힌 형태(`feedback_tool_failure_read_as_absence`)라 남긴다.

## 1. #218 — 항목 × (해소 / 남음 / 결정 대기)

근거 표기: 코드 이름은 `xpe_verify_metrics.cpp` 의 현재 줄, 시험 이름은 grep, 커밋은 `git merge-base --is-ancestor <c> origin/main` 결과(`evidence/10_ancestry.txt`; 대조: `origin/main` 이 `HEAD` 의 조상이 **아님**을 같은 명령이 답함).

### 1.1 본문·코멘트의 항목

| # | 항목(출처) | 상태 | 근거 |
|---|---|---|---|
| 1 | FUNC-017 `FlatResidualPct <= 1.0%` 를 계산·게이트하지 않는다(본문) | **해소** | 코드 `:101` `FLAT_RESIDUAL_MAX_PCT = 1.0`, `:609-614` `flat_residual_limit` 로 `overall_pass` 에 연결. 커밋 `fa3da63b`(QA-A-154) main 조상 ✔. 시험 `VerifyGain_ImprovedButStillAboveOnePercentFails` 통과(실행: `evidence/35`) |
| 2 | 지표 정의가 정본과 다르다(분모가 중앙값 기반, #220) | **해소** | `edc53cbc`(QA-A-156) main 조상 ✔. 시험 `VerifyGain_FlatResidualUsesTheArithmeticMean` 통과 |
| 3 | release 목표 `<= 0.5%`(알려진 게인 의미일 때) | **해소** | `:106` `FLAT_RESIDUAL_KNOWN_MAX_PCT = 0.5`, `28650606`(QA-A-192) main 조상 ✔(병합 `af21f669` 로 들어감). 시험 `GainSemantics.FlatResidualLineIsHalfAPercentWhenKnownAndOnePercentWhenUnknown` 통과 |
| 4 | FUNC-019 recall 100% · FPR < 0.001% 를 계산하지 않는다(본문) | **해소(리더 결정대로 시험 쪽)** | 판정(2026-09-28 코멘트)이 "제품 게이트가 아니라 시험 하네스 요구". `test_defect_oracle.cpp`(QA-A-188, `bc51ab45` main 조상 ✔, 병합 `ef98cd67` 로 들어감)가 recall 100%·FPR < 0.001%·GoodPixelDeltaP99 ≤ 1 ADU 를 세 시드로 단언, 대조 시험 3건(`ControlOneMissedDefectBreaksRecall`·`ControlOneFalseAlarmShowsInFpr`·`ControlFlaggingManyGoodPixelsBreaksGoodPixelDelta`). **CMake 등록**(`modules/preprocess/CMakeLists.txt:467`), 이 워크트리에서 4건 통과, **main CI 로그에서 관측**: run `36987723461`(ci.yml, main `d43e80c2`) `preprocess-tests` 잡 `#899~#902` 전부 `Passed`(`evidence/40`). `xpe_verify_defect` 자체는 여전히 `defect_density < 5%` 로 판정한다(`:700`) — 결정에 따라 그대로 둔 것 |
| 5 | `DEFECT_DENSITY_MAX` 단위가 100배 어긋남(#217) | **해소** | 코드 `:114` 가 `5.0`(퍼센트), 지표는 `:693` 에서 `*100`. `0814afbb`(QA-A-153) main 조상 ✔ |
| 6 | `DSNU_MAX_PCT` 1.0 — 출처 없음·분모 다름 | **해소(게이트에서 뺌)** | 코드 `:82-94`: 게이트가 더는 읽지 않고 값만 `metrics->dsnu` 로 보고. `a98e6287`(QA-A-158) main 조상 ✔. 관련 #223(dsnu 필드 의미)도 닫힘(issue 검색 `evidence/38`) |
| 7 | `:199` 잴 수 없는 영상을 "합격" 으로 보고(균일 영상) | **해소** | 코드 `:311-357`: 측정 불가는 `overall_pass = false`, `measured_mask` 로 구분(SRS-CALIB-FUNC-036), `12d49ed1`(QA-A-157) main 조상 ✔. 시험 `VerifyOffset_UnmeasurableFrameDoesNotPass`·`VerifyOffset_UnmeasurableFrameReportsNotMeasured` 존재(`evidence/36`). 게인 쪽 동등 분기는 코드 `:548` 에서 `false` 로 확인했고 **게인 시험은 따로 찾지 않았다**(§4) |
| 8 | `PRNU_IMPROVE_MIN_DB` 3.0 이 PRNU 가 아니라 SNR 에 걸림, 출처 없음 | **남음, 결정 대기** | 코드 `:97` `3.0` "NO REQUIREMENT FOUND", `:583` `snr_improvement_db >= PRNU_IMPROVE_MIN_DB` — 이름과 용도가 아직 어긋남. 코멘트(2026-10-01)의 "열린 결정" |
| 9 | 같은 `snr_improvement_db` 에 합격선 둘(3.0 / 2.0) | **남음, 결정 대기** | 이제 **다른 함수**에 있다: `xpe_verify_gain`(`:583`, 3.0)과 `xpe_verify_pipeline`(`:801`, `SNR_IMPROVE_MIN_DB = 2.0`). 같은 필드 이름이지만 두 함수가 같은 양을 재는지는 확인하지 않았다(§4) |
| 10 | `GAIN_COVERAGE_MIN` 0.99 — 출처 없음 | **남음, 결정 대기** | 코드 `:100` "NO REQUIREMENT FOUND", `:582` 에서 `overall_pass` 에 그대로 쓰임 |
| 11 | `xpe_verify_pipeline` 의 SNR 2.0 dB — 근거 없음 | **남음, 결정 대기** | 코드 `:793-801` 이 "NO REQUIREMENT BASIS" 를 주석으로 명시(동작 불변, QA-A-187 `986bbce8` main 조상 ✔; SNR 근거 주석은 QA-A-188 `bc51ab45` 에 함께 있음). 정본에 정의할지 판정에서 뺄지는 리더가 열린 결정으로 둠 |
| 12 | SRS `:151` 고스트 제거율 ≥ 90% (본문 코멘트 §5, "해당 함수 없음") | **남음, 이슈 없음** | 이 줄의 요구 ID 는 **`SRS-CALIB-FUNC-020`** — `GhostRemovalPct`·`LagResidualPct` 를 계산하고 "측정 가능한 지연이 있는 시퀀스에서 고스트 제거 90% 이상" 을 요구한다. `GhostRemovalPct`·`LagResidualPct`·`ghost_removal`·`lag_residual` 은 `modules/`·`clients/`·`gui/`·`tools/` 어디에도 **0건**(대조군 `prnu_after` 는 `modules/` 6파일에서 잡힘, `evidence/37`). 이슈 검색(`FUNC-020`·`ghost removal`)에서 이 요구를 추적하는 이슈는 **나오지 않았다**(대조군 `FlatResidualPct` 검색은 #218·#220·#223·#216 을 잡음, `evidence/38`) |
| 13 | GUI `MetricsComputationService` 합격선이 정본과 다르다(리더 부수 발견, recall ≥ 95%·P99 ≤ 0) | **해소로 보임(gui 레인 몫)** | `clients/ImageProcTest/Services/DefectMetricGates.cs` 가 `RecallMinPercent = 100.0`, `GoodPixelDeltaP99MaxAdu = 1.0` 로 정본값을 쓴다(`evidence/37`). 같은 파일 12행 주석의 "recall >= 95%…P99 <= 0" 은 "GUI-C-182 이전에는 이 값을 썼다" 는 **이력 설명**이다(낡은 것이 아님). 커밋 `22a486df`(GUI-C-182, `fix(gui)`) main 조상 ✔. **이 카드가 확인한 것은 상수뿐이며 GUI 가 그 상수를 판정에 쓰는지는 보지 않았다.** gui 레인 소유라 손대지 않았다 |

### 1.2 집계

해소 7(1·2·3·4·5·6·7), 해소로 보임·gui 몫 1(13), 남음 5(8·9·10·11·12). 남은 다섯 중 넷(8~11)은 리더가 이미 "열린 결정" 으로 기록한 같은 꾸러미(출처 없는 문턱 셋과 SNR 2.0 — 요구를 세울지 거둘지)이고, 하나(12)는 어디에도 추적되지 않는다.

### 1.3 남은 것, 범위 한 줄씩 (요구 문구 변경은 리더 몫)

- **문턱 꾸러미(8·9·10·11)**: `PRNU_IMPROVE_MIN_DB 3.0`·`GAIN_COVERAGE_MIN 0.99`·`SNR_IMPROVE_MIN_DB 2.0` 에 요구를 세울지 거둘지 + 이름(`PRNU_IMPROVE_MIN_DB` 가 SNR 에 쓰이는 오용). 정본이 이 지표를 정의하는 일이므로 요구 문구 변경 → 리더.
- **FUNC-020 고스트 지표(12)**: `GhostRemovalPct`·`LagResidualPct` 를 계산하는 곳이 없고 ≥ 90% 를 확인하는 시험도 없다. 두 갈래 — 구현·시험을 세운다 / SRS 문구를 현재 범위에 맞게 바꾼다. 어느 쪽이든 새 이슈가 필요하다.

## 2. #216 — 수출 48개를 요구와 다시 맞추기

### 2.1 수(이름 축, `evidence/31`·`32`)

| | 이슈 본문(2026-09-28) | 지금(SPEC 1.3.3) |
|---|---|---|
| 수출 C 이름 | 48 | **48** (헤더 선언 48과 일치) |
| REQ-P1A 본문이 이름을 부르지 않음 | 21(상한) | **16** |
| REQ-P1A 도 SRS 줄도 RTM 줄도 이름을 부르지 않음 | 미측정 | **0** |

21 → 16 의 5건은 옛 목록 기준으로 `xpe_calib_load_{offset,gain,defect}_cached` 3·`xpe_preprocess_pipeline_ex`·`xpe_preprocess_version` 이 **REQ-P1A-102~106**(SPEC 1.3.2, `6dfd6cb8`, 병합 `af21f669`)으로 이름을 얻은 것이다(SPEC `:838`·`:858`·`:871`·`:884`·`:899`, `evidence/33`). `xpe_calib_cache_clear`·`xpe_calib_cache_set_max_size` 는 102 의 본문과 FUNC-038 이 이름으로 묶는다. `xpe_crc32` 는 수출을 거뒀다(`498fc3c`).

> **이 계산에서 나는 "옛 목록의 어느 5개가 풀렸나"를 옛 목록 파일과 대조하지 않았다.** 위 5건은 SPEC 본문에서 그 이름이 지금 나오는 줄을 확인한 것이고, "옛 21개" 의 정확한 구성은 이슈 본문의 묶음 표에서 읽었다. 이 대응은 맞다고 본다 — 다만 21개 이름 목록 파일 자체는 이 카드에서 열지 않았다.

**SPEC 1.3.3 이 늘린 서술의 반영**: 1.3.3 의 변경 행(`spec.md:27`)은 "No new requirement number" 로, 게인 분류·결함 채움 규칙·비유한 입력 거부 등을 **이미 이름이 있는 요구**(011·012·013·015·019·032·087·091)에 더한 것이다. 이름 축 수는 1.3.3 본문으로 직접 쟀으므로 그 서술은 반영돼 있고, 1.3.2 → 1.3.3 사이 이름 축 수가 달라지지 않았다고 본다(1.3.2 본문으로 같은 계산을 **돌리지는 않았다**; 변경 행이 새 요구 번호가 없다고 적은 것에 기댄 추론이다).

### 2.2 이름이 없는 16개와 그 요구 (`evidence/32`)

| 묶음 | 함수 | SRS 본문에서 이름이 나옴 | RTM 줄에만 있음 |
|---|---|---|---|
| 검증 4 | `xpe_verify_offset`·`_gain`·`_defect`·`_pipeline` | offset·gain → `SRS-CALIB-FUNC-036` | defect → `FUNC-019`, pipeline → `FUNC-015`·`FUNC-021`, offset → `016`, gain → `017`(RTM 은 네 개 모두) |
| 관리 표면 5 | `xpe_calib_get_max_points`·`_get_poly_degree`·`_state_release`·`xpe_preprocess_is_initialized`·`xpe_preprocess_pipeline_batch` | **`SRS-CALIB-FUNC-038`** 이 이름으로 묶는다 | — |
| 모드 2 | `xpe_calib_get_mode`·`_set_mode` | `SRS-CALIB-FUNC-031` | — |
| 비선형 4 | `xpe_calib_generate_nonlin_lut`·`_load_nonlin_lut`·`_unload_nonlin_lut`·`xpe_nonlinearity_correct` | 없음 | `FUNC-006` |
| 생성 1 | `xpe_bpm_generate` | 없음 | `FUNC-022` |

**SRS 본문에 이름이 있는 것은 9개, RTM 줄에만 이름이 있는 것은 7개**(검증 defect·pipeline, 비선형 4, `xpe_bpm_generate`). 7개는 RTM 이 함수 이름을 요구 ID 에 **매핑**할 뿐 요구 본문이 그 함수를 서술하는지 확인되지 않았다 — `SRS-CALIB-001` 이 요구를 함수명이 아니라 능력으로 쓰기 때문이다(QA-A-151 이 이미 적은 한계). 즉 "0개가 요구 없음" 은 "7개는 이름이 매핑 표에만 있음" 과 함께 읽어야 한다.

### 2.3 남은 것

- **RTM 줄에만 이름이 있는 7개**: SRS 의 해당 능력 문단(`FUNC-006`·`015`·`019`·`021`·`022`)을 읽어 그 함수를 정말 서술하는지 확인하는 일. 읽는 일이라 pre 가 할 수 있다(별도 카드).
- **`xpe_nonlinearity_correct` 의 공개 계약을 SPEC 에 세울지**(QA-A-189 가 "열린 결정 2" 로 남김): 이름 축으로는 아직 REQ-P1A 에 없고 RTM `FUNC-006` 매핑뿐이다. 제품 경로에 있는 함수(GUI 가 부름, 189 보고)라 요구 신설 여부는 요구 문구 변경 → 리더.
- **SRS 에만 있는 함수를 SPEC 범위로 볼지**(189 의 열린 결정 3): 지금 이름 축 16개가 그 크기다. 범위 결정 → 리더.

## 3. 닫을 수 있나

- **#218**: 제목의 두 주장(FUNC-017·FUNC-019)은 코드·시험·CI 로그로 해소가 관측됐다. 남은 둘(문턱 꾸러미, FUNC-020)은 제목과 **다른 질문**이라, 닫고 옮길지 열어 둘지는 리더가 정할 일이다. 닫기 전에 남은 둘이 어느 이슈에도 없다는 점(특히 FUNC-020)은 새 이슈가 필요하다.
- **#216**: 아직 못 닫는다. 이름 축 본문 수는 21 → 16, 요구가 전혀 없는 함수는 0 이지만 RTM 줄에만 있는 7개의 SRS 본문 확인(읽기)과 SPEC 범위 결정(리더)이 남았다.

## 4. 미검증 (Gaps)

- 이 대조는 **이름 축**이다. 동작(능력) 축은 다시 하지 않았다(QA-A-189 가 한 것: 이름 일치 22·동작만 일치 1·SRS 에만 20·요구 없음 5 — 지금은 그 "요구 없음 5" 가 0 이 된 것까지만 확인).
- SRS·RTM 의 요구 ID 는 "함수 이름이 있는 같은 줄" 에서 뽑았다. 문단 단위로 요구에 속한 경우는 놓치므로 SRS/RTM 쪽 수는 **하한**이다.
- 항목 7: 게인 경로의 "측정 불가는 합격 아님" 을 코드(`:548`)로만 확인했고 전용 시험은 찾지 않았다.
- 항목 9: 두 함수의 `snr_improvement_db` 가 같은 양인지 확인하지 않았다.
- 항목 13: GUI 상수만 읽었고, 그 상수가 판정에 연결돼 있는지는 보지 않았다.
- 시험 실행은 **13건**(검증 게인·오라클·게인 의미)만 이 워크트리에서 돌렸다. 전체 스위트는 이 카드에서 돌리지 않았다(코드 변경 없음; 전체는 QA-A-221b 검증 `ed16b134` 에서 894/894).
- 이 워크트리 빌드는 `main` 에 아직 없는 212d·221b 를 포함한다. 수출 이름은 `origin/main` 헤더와 일치(차집합 0)하지만, **main 의 DLL 은 직접 빌드하지 않았다**.
- CI 로그 관측은 run `36987723461` 하나(main `d43e80c2`, 2026-10-02)다.

## 5. 잔여 위험

- SPEC 1.3.3 이 main 에 있고 이 워크트리에는 1.3.2 가 있다. 이 워크트리에서 SPEC 으로 재는 도구·카드는 같은 오류를 낸다(census 도구의 `REPO` 고정도 같은 부류). 도구 쪽 수정은 이 카드 범위가 아니다.
