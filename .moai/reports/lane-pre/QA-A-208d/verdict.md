# QA-A-208d — Codex #45 보류 2건: 공개 헤더 범위 동기화, R² 의 존재를 표지값이 아니라 플래그로

기준 커밋 `96a67d97`(QA-A-208c) 위. 감사 원문은 `evidence/00_codex45_audit_original.md`. 이슈 #233.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 공개 헤더 `XpeCalibQualityMeta` 의 Fields 목록이 구현과 같은 말을 한다: `polynomial_degree` 0-4, `r_squared` ≤ 1.0 이고 음수 가능, 존재 여부는 `has_r_squared`. NUL 시험 위의 주석은 "strict 가 NUL 을 거부" 가 아니라 실제(어휘 분석기가 NUL 을 입력의 끝으로 읽어 `memchr` 가 필요했다)를 적는다. | 헤더 diff, 시험 주석 |
| 2 | R² 의 존재는 값이 아니라 **`has_r_squared` 플래그**다. XCal 파일은 `fit_r_squared` 키가 있으면 ≤ 1 인 유한값 전부(정확히 -1.0 포함)를 실제 값으로 받고, 없으면 `has_r_squared = 0`. 이력(`previous_r_squared`)도 같은 방식의 `has_previous_r_squared` 로 존재를 표시한다. | 새 시험 3건, 범위 시험 갱신 |
| 3 | ABI 배치가 변하지 않는다: 두 플래그는 옛 패딩 자리(오프셋 4, 73)에 들어가 `sizeof` 88 과 다른 모든 오프셋이 같다. | `evidence/06_abi_old_vs_new_header.txt` |
| 4 | 이력 연쇄가 -1.0 에서도 맞다: A(0.91) → B(R² = -1.0) → C 에서 C 의 이전 값은 -1.0 이고 "있음" 이며, 아무 R² 도 없던 첫 기록의 이력은 "없음". | `AnR2OfExactlyMinusOneIsARealValueAndChainsAsOne`, `WithNoEarlierR2…` |

## 2. 무엇을 바꿨나

- **공개 헤더** `preprocess_api.h`: `XpeCalibQualityMeta` 에 `uint8_t has_r_squared`(오프셋 4)와 `uint8_t has_previous_r_squared`(오프셋 73)를 추가. Fields 목록을 새 계약으로 다시 적음(차수 0-4, `r_squared` ≤ 1.0·음수 가능·존재는 플래그, `-1.0` 은 "없음" 일 때의 채움 값이지 표지가 아님, `previous_r_squared` 와 두 플래그).
- **`xpe_calib_mode.cpp`**:
  - 파서: `fit_r_squared` 키가 있으면 `has_r_squared = 1` 이고 값은 ≤ 1 만 검사(-1.0 거부를 없앰); 키가 없으면 0. `calibration_pass` 는 `has_r_squared` 일 때만 게이트와 비교.
  - 생성 기록(`xpe_calib_record_quality_meta`): `has_r_squared = 1`(생성은 항상 R² 를 만든다).
  - 이력 규칙 `r2_history_after(replaced, &값, &있음)`: `valid` 이고 `has_r_squared` 인 기록이면 그 R², 아니면 그 기록이 이어받은 이력(값과 플래그). 세 경로(생성, 품질 있는 적재, 품질 없는 적재)가 모두 이 하나를 쓴다. 이전의 `!= -1.0` 판정은 사라졌다.
- **내부 헤더**: `XPE_R_SQUARED_NOT_GIVEN` 의 주석을 "채움 값이지 지표가 아님"으로.
- **시험 주석**: NUL 시험의 설명을 실제로 고쳤다(1.2 절 아래).

## 3. 정정 (이전 보고서의 틀린 서술)

- **208c 보고서**: "-1.0 은 파일이 담을 수 없다", "생성기가 정확히 -1.0 을 내면(사실상 불가능)", "알려진 R² = valid 이고 `r_squared != -1.0`" — **틀렸다.** 생성기는 `1 - SS_res/SS_tot` 를 검사 없이 쓰고, 단조성 대체(끝점 선형)의 해는 최소제곱해가 아니라서 `SS_res = 2·SS_tot` 이 되면 정확히 -1.0 이고, 9자리 출력(`%.9f`)이 `-1.000000000` 으로 반올림되는 입력도 있다. 이 서술들은 이 카드가 대체한다(208c 보고서 머리에 정정 문구를 달았다).
- **202f 보고서**: "문서화된 범위 0..1"(208c 에서 이미 한 번 고침)도 마찬가지로 이 카드의 플래그 규칙이 최종이다.
- **NUL 시험 주석**: 208c 에서 시험 위 주석에 "nlohmann 의 strict 가 NUL 을 거부하니 별도 검사는 없다" 가 들어갔고, 이는 리더 결정문의 틀린 전제를 그대로 옮긴 것이었다(208c 보고서 본문은 실측대로 적었다). 지금은 실측대로: 어휘 분석기가 문자열 밖 NUL 을 `end_of_input` 으로 읽어 strict 도 통과시키므로 `helpers.cpp` 가 `memchr` 로 먼저 거부하고, 그 검사를 지우면 문자열 밖 NUL 행이 실패한다.

## 4. 시험

- `AnR2OfExactlyMinusOneIsARealValueAndChainsAsOne`(OOM exe, 적재 방식 4종): A → B(`fit_r_squared` 정확히 `-1.000000000`, 생성기의 인쇄 형식) → C → …. B: `valid=1`, `has_r_squared=1`, `r_squared=-1.0`, 이력 0.91·있음; C 의 이전 값 -1.0·있음; B → 무품질 D: 이력 -1.0·있음; B → 부분 E: 이력 -1.0·있음.
- `WithNoEarlierR2TheHistoryIsFlaggedAsNoneEvenWhenTheFirstRecordIsMinusOne`: 시작 상태 `has_previous_r_squared = 0`; 첫 기록이 -1.0 이어도 그 기록의 이력은 "없음"(0); 다음 기록 C 의 이력은 -1.0·있음; 정확히 -1.0 인 생성 기록(`xpe_calib_record_quality_meta`)도 같은 방식으로 이어짐(그 이력 0.97, 다음 기록의 이력 -1.0).
- 202f 의 부분 기록 시험에 플래그 단언을 더했다(`has_r_squared = 0`, 이력 `has_previous = 1`).
- 범위 시험(208c)에서 `-1.0`, `-1`, `-1.000000000` 은 이제 **받는** 케이스이고, 받는 행마다 `has_r_squared` 가 `fit_r_squared` 만 1 이고 다른 필드 행은 0 임을 단언한다.
- `static_assert`: `has_r_squared` 오프셋 4, `has_previous_r_squared` 73, `sizeof` 88, 나머지 오프셋 불변.
- 기존 시험은 한 건도 약하게 바꾸지 않았다(위 범위 시험의 기대는 "-1.0 거부" → "-1.0 수용"으로 결정이 바뀐 것).

## 5. ABI 증거

같은 소스를 세 헤더로 컴파일해 `sizeof`/`offsetof` 를 찍었다(`evidence/06`, MSVC x64): 202e 이전 헤더, 이 카드 이전 헤더, 새 헤더 모두 `sizeof=88 mode=0 deg=1 pts=2 r2=8 ts=16 serial=24 fw=56 pass=72 prev=80`(`valid=3` 은 202e 이후), 새 헤더의 추가분은 `has_r_squared=4 has_previous_r_squared=73`. 새 헤더로 컴파일한 호출자가 이전 DLL 에서 읽으면 그 두 바이트는 이전 DLL 의 패딩이라 신뢰할 수 없다(202e 의 `valid` 와 같은 한계; 기존 호출자 → 새 DLL 은 구조체 전체를 채우므로 정의됨).

## 6. 수정 전 / 후, 반증

수정 전: 시험만 먼저 넣었을 때 `has_r_squared` 멤버가 없어 컴파일 단계에서 막힘(`01_build_red.txt`). 수정 후 새 시험 초록. 반증(한 번에 하나, 전체 빌드, 복원 뒤 `cmp` 동일):

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| history_by_marker_value | 이력 판정을 플래그 대신 `!= -1.0` 으로(리더의 반증) | -1.0 이력 시험 2건 |
| file_minus_one_refused | 파서가 -1.0 을 다시 거부 | 범위 시험 + -1.0 이력 시험 2건 |
| parse_sets_no_flag | 파서가 `has_r_squared` 를 안 세움 | 범위 시험 + 이력 시험 9건 + 기존 `MetadataSurvivesSaveAndLoad` |
| record_sets_no_flag | 생성 기록이 `has_r_squared` 를 안 세움 | 생성/이력 시험 3건 + 기존 `PreviousRSquared_Regression`, `PreviousRSquared_Stable` |
| history_flag_dropped_in_load | 품질 있는 적재가 `has_previous_r_squared` 를 안 이음 | 부분→C 시험, -1.0 시험 2건 |
| history_flag_dropped_in_none | 품질 없는 적재가 안 이음 | -1.0 시험 |

## 7. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 780 실행 / 772 통과 / 8 건너뜀, 섞기도 동일 |
| preprocess OOM exe | 45 통과 (직전 43 + 2) |
| 할당 실패 스윕 | 91 / 91 / 99 / 101 / 93 / 4 지점 전부 통과(변화 없음) |
| common OOM / common | 12 / 69 통과 |
| `ctest -N` | 946 (직전 944 + 2) |
| 수출 이름 / 헤더 문서 / 프리셋 | 48·16 불변 / 0 findings / 12 of 12 |

## 8. 미검증 (Gaps)

- **생성기 입력으로 실제 -1.0 을 만들지는 못했다.** 카드는 "가능하면 생성기 입력으로"였으나, 단조성 대체의 끝점 선형식이 `SS_res = 2·SS_tot` 이 되는 사다리를 구성하지 않았다(그 구성은 QA-A-210 의 조사 범위와 겹친다). 대신 (a) 생성기의 인쇄 형식 그대로의 `-1.000000000` 파일 왕복(적재 방식 4종), (b) 기록 단위 경로(`xpe_calib_record_quality_meta` 에 정확히 -1.0)를 시험했다. 생성 → 저장 → 재적재 전체 사슬은 -1.0 경계에서 실행하지 않았다.
- C# 쪽 구조체 미러는 저장소에서 찾지 못했다(`XpeCalibQualityMeta` 를 `.cs` 에서 grep 하면 0건) — 플래그가 거기서 읽히는지는 해당 없음.
- 새 헤더로 컴파일한 호출자 → 옛 DLL 의 두 플래그는 신뢰할 수 없다(5절).
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만.

## 9. 잔여 위험

- `r_squared` 의 값 -1.0 은 이제 "플래그가 0 일 때의 채움 값" 이면서 "플래그가 1 일 때 실제 값" 이다. 플래그를 읽지 않는 외부 호출자(구 ABI)는 둘을 구별하지 못한다 — 파일이 키를 담았는지는 새 필드로만 알 수 있다.
- 이력의 "없음" 은 `has_previous_r_squared` 로만 판별된다. 구 호출자가 `previous_r_squared == -1.0` 으로 "없음" 을 판정하면 실제 이전 R² 가 정확히 -1.0 인 드문 경우를 "없음" 으로 읽는다.
