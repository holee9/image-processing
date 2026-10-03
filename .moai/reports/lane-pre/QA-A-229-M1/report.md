# QA-A-229 M1 — 빈 시험 바로잡기 (D11, #245)

## 1. 한 줄 결과

- 항상 참이던 시험 세 가지를 실제 단언으로 바꾸고, 각각에 반증(제품 손상 4 + 시험 손상 1)을 달아 5개 모두 발화를 확인했다.
- 꺼진 `test_xpe_preprocess.cpp` 는 다시 켜지 **않았다**. 왜 꺼졌는지는 밝혔고(§3), 다시 켜면 얻는 시험이 없다는 것을 31건 전부 읽어 확인했다(§4). 삭제는 리더 결정으로 남긴다.

## 2. 고친 시험

| 시험 | 이전 | 이후 | 반증(발화) |
|---|---|---|---|
| `GainCorrect_NoNaNInfInOutput` | 보정을 적재하지 않아 항상 `CALIB_NOT_LOADED`, `if (result==XPE_OK)` 본문이 한 번도 안 돎(검사 화소 0개) | 죽은 화소를 가진 평탄장에서 이득 맵을 생성·적재, `ASSERT XPE_OK`, 검사 화소 수 == 64 와 비유한 수 == 0 을 단언, 미기록 화소 없음 확인 | a1 제품이 화소 5 에 Inf 를 씀 → 빨강. a2 시험이 적재를 건너뜀 → `-16`(CALIB_NOT_LOADED) 로 빨강 |
| `BP01` | `min <= max`(uint16 버퍼에선 항상 참, 주석도 "trivially true") | 오류 반환이면 출력이 센티널 그대로여야 한다는 헤더 계약을 단언: 센티널로 남은 화소 수 == 4096 | a5 오류 경로에서 `dst[0]=0` → 4095 != 4096 빨강 |
| `OffsetCorrect_FormatMismatch` | 세 코드 중 하나면 통과 | 코드를 읽어 정한 단 하나 `XPE_ERR_UNSUPPORTED_FORMAT` + 입력 바이트 불변(memcmp). `xpe_offset_correct_in` 은 `input->format` 을 초기화·맵 적재 상태보다 먼저 본다(`offset_correct.cpp` 의 첫 검사들) | a3 맵 검사를 앞에 끼움 → `-16` 빨강 |
| `GainCorrect_FormatMismatch` | 위와 같음 | 동일(`gain_correct.cpp` 도 같은 순서) | a4 → `-16` 빨강 |

증거: `evidence/a2_*`~`a4_*`, `evidence/arm_a1_*`, `evidence/arm_a5_*`(a1·a5 는 `*_output.txt` ignore 패턴을 피해 이름을 바꿨다)(각 build_exit=0, test_exit=1, 실패 메시지), 제품 파일 3개는 반증 뒤 원본으로 복원(`git status` 에 시험·CMake 만 수정으로 남음).
전체 실행: `evidence/full_run.txt` — 967 시험, 959 통과, 8 건너뜀, 종료 코드 0(수정 전과 같은 수).

부수 발견: 새 `NoNaNInf` 시험은 처음에 전역 상태 위생 가드(`test_global_state_hygiene.cpp`)에 걸렸다 — 죽은 평탄장 화소가 경고 1건을 큐에 남긴다. 이 시험이 그 경고를 실제로 만든다는 것 자체가 "이득 맵이 적재돼 경로가 돌았다"는 방증이며, 모든 종료 경로에서 `xpe_clear_alerts()` 로 비운다.

## 3. 왜 `test_xpe_preprocess.cpp` 가 꺼졌나 (blame)

- `e9b8ed46`(2026-04-20, API 이전)에서 이 단일 파일(51 시험)이 `test_xpe_preprocess_init/_calibration/_correction.cpp` 로 쪼개지며 CMake 에서 주석 처리됐다.
- 켜면 링크가 안 된다: 51건 중 20건이 쪼개진 복사본과 `Suite.Name` 이 같아 LNK2005, 그리고 자체 `main()` 이 `test_calib_mode.cpp` 의 것과 겹친다(LNK1169).
- QA-A-21 의 점검은 컴파일 오류(필드 14건)만 세어 이 쪽을 놓쳤다. 그 점검의 "계약 결정이 필요하다"는 결론은 맞았지만 이유는 그것이 전부가 아니다.

## 4. 켜 보았을 때의 결과 (31건 전부)

20건 중복·`main()` 을 제거하고 컴파일 오류를 고친 뒤(접두사 `Legacy` 로 스위트 이름 분리) 링크한 시험 exe 에서 실행했다: **18 통과, 13 실패**(`evidence/legacy_unique31_run.txt`). 이후 단언을 전부 읽었다.

- **실패 13건의 원인 — 낡은 단언 또는 낡은 파일 배치**: `LoadOffset_SHA/Session/Dimension/Expired` 4건은 옛 XCAL 배치로 만든 파일이라 지금 로더가 전부 `-2`(INVALID_INPUT)로 거부해 SHA·세션·차원·만료 검사에 도달하지도 못한다. `LoadOffset_NotInitialized` 는 `-9` 를 받는다(기대 `NOT_INITIALIZED`). `CheckExpiry_Expired` 는 같은 이유로 만료가 아니라고 읽는다(`remaining_days = INT_MAX`). `Save_*`·`ValidateReadout_*`·`DefectDetectRuntime_*` 8건은 보정 미적재 상태에서 `-1` 을 받아 단언한 코드 집합에 없다.
- **통과 18건 중 단언이 하는 일**: 단언 목록을 전부 뽑아 읽었다. `result == A || B || C…` 형 단언만 가진 시험이 약 24건(`OffsetCorrect_Temperature/PREP`, `GainCorrect_GainMapDivision/MultiSID/NaNInf`, `DefectCorrect_*` 5건, `GenerateOffset_*` 2건 등) — 반환 코드가 무엇이든 그 집합이면 통과하고, 다수는 `CALIB_NOT_LOADED` 를 받고 끝난다(보정을 적재하지 않으므로). 통과가 곧 행위의 증거가 아니다. 의미 있는 단언을 가진 것(`GenerateOffset_NullFrames`, `CheckExpiry_NullFilePath`, `CheckExpiry_Valid`, `GetParamRange_Valid/Invalid`)은 아래 현행 시험이 같은 것을 덮는다.
- 이 시험들이 이름 붙인 행위의 현행 시험: 만료·적재 `test_xpe_calib_check_expiry.cpp`·`test_xpe_calib_load.cpp`·`test_xcal_reader.cpp`, 저장 `test_xpe_calib_save.cpp`·`test_calib_save_expiry.cpp`, 판독 검증 `test_readout_validate.cpp`, 런타임 검출 `test_runtime_detection_*.cpp`, 파라미터 범위 `test_param_range_names.cpp`.
- 존재하지 않는 기능을 이름으로 가진 시험 3건: `OffsetCorrect_TemperatureInterpolation`·`OffsetCorrect_PREPTimeModel`(온도·적분시간 필드가 없다 — QA-A-21 이 확인), `GainCorrect_MultiSIDInterpolation`(REQ-015, 미구현). 이 셋은 "이 입력이 출력을 바꾸지 않는다"는 현재 동작을 고정할 수 있지만, M2 가 kVp 를 이미 같은 방식으로 문서화하고 gui C02 가 단언하므로 여기서 중복을 만들지 않았다.

## 5. 결정으로 남기는 것 (리더)

- `tests/test_xpe_preprocess.cpp` 삭제 여부. 삭제하면 CMake 주석 블록도 같이 정리된다. 지금은 파일을 건드리지 않고 CMake 주석에 실제 이유와 이 보고서 경로를 적었다.
- 제품 결함 후보는 새로 나오지 않았다 — `-9` vs `NOT_INITIALIZED`(`LoadOffset_NotInitialized`)는 확인하지 못했다: 현행 시험이 이미 적재 전/종료 후 코드를 단언하는지는 보지 않았고, 그 시험은 낡은 배치 위에서 돈 것이라 결함 근거로 쓰지 않았다.

## 6. 미검증 (Gaps) · 잔여 위험

- 31건 각각을 현행 시험과 1:1 로 대조하지는 않았다. "덮는다"는 근거는 같은 API 를 단언하는 파일의 존재(grep)와 단언 목록 읽기까지다.
- `NoNaNInf` 가 쓴 8×8 평탄장 한 가지와 입력 한 가지에서만 확인했다. 3072×3072 와 다른 분포에서의 유한성은 이 시험이 보증하지 않는다.
- 반증 a1 은 화소 5 한 곳의 Inf 만 본다. 다른 위치·NaN 은 주입하지 않았다(루프가 모든 화소를 센다는 단언으로 갈음).
- 반증은 이 exe(`ci-preprocess`, 단일 구성)에서만 돌렸다.
