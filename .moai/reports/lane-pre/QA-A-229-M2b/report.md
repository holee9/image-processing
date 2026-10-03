# QA-A-229 M2b — 레거시 시험 31건 대조·이관과 파일 삭제, offset 헤더 약속 (#245)

## 1. 결과

- `tests/test_xpe_preprocess.cpp`(분할 전 단일 파일, `e9b8ed46` 에서 꺼짐)를 삭제했다. 중복 20건을 뺀 31건을 살아 있는 시험과 하나씩 대조해, 지금도 맞으면서 현행 시험에 없는 것만 정확한 단언으로 옮겼다(3건 신규, §3).
- offset 헤더의 온도 보간·PREP 시간 모형 약속을 "미구현 — #245" 로 바꾸고 그 문장을 시험으로 고정했다.
- 31건 대조 중 헤더가 거짓인 곳이 하나 더 나왔다: 세 로더(`xpe_calib_load_offset/gain/defect_map`)의 "`XPE_ERR_NOT_INITIALIZED` if module not initialized" — 로더는 초기화 전에도 적재되며 그렇게 시험되어 있다. 같이 정정했다(§4).
- 전체 실행 963 통과·8 건너뜀·종료 0(M2 의 960 + 신규 3), doxygen 1.12.0 종료 0·경고 0. 반증 5개 전부 발화.

## 2. 31건 대조표

현행 시험 열은 파일 이름만이 아니라 같은 입력에 같은 코드를 정확히 단언하는 시험을 가리킨다(이름 grep 후 시험 목록을 읽어 확인). "범위 밖" 은 이번에 옮기지 않은 이유다.

| 레거시 시험 | 레거시 단언 | 현행 대응 / 처분 |
|---|---|---|
| LoadOffset_CorruptedFileWrongMagic | IO_FAILED 또는 CONFIG_INVALID | `CalibLoadTest.LoadOffset_BadMagic_ReturnsError` 가 덮음 — 삭제 |
| LoadOffset_SHAMismatch | OK 또는 CONFIG_INVALID (항상 `-2` 로 거부됨: 옛 배치) | `LoadOffset_TamperedPayload_ReturnsConfigInvalid`, `test_xcal_reader.cpp` — 삭제 |
| LoadOffset_SessionMismatch | OK 또는 CONFIG_INVALID | 현행 대응 없음 — 세션 검사가 구현에 없다(M3 설계) — 삭제, 구현되면 그때 새로 씀 |
| LoadOffset_ExpiredCalibration | CALIBRATION_EXPIRED(옛 배치라 `-2`) | `LoadOffset_ExpiredFile_ReturnsCalibrationExpired` — 삭제 |
| LoadOffset_DimensionMismatch | OK 또는 CONFIG_INVALID | 적재는 모든 크기를 받고 불일치는 보정 때 `INVALID_INPUT`(`offset_correct.cpp` 의 `calib.offset_width != input->width`) — `OffsetCorrect_DimensionMismatch` 계열이 덮음 — 삭제 |
| LoadOffset_NotInitialized | NOT_INITIALIZED (실제 `-9`) | 낡은 단언: 초기화 전 적재가 정상(`test_xpe_preprocess_init.cpp` 의 "a map can be loaded before xpe_preprocess_init"). **이관**: `CalibLoadTest.LoadBeforeInit_AllThreeLoadersAcceptValidFiles` (3종 로더 전부 `XPE_OK`) |
| OffsetCorrect_TemperatureInterpolation | 코드 3종 중 하나 | 기능 없음(`XpeImageMetadata` 에 온도 필드 없음). **이관**: `OffsetCorrect_MetadataDoesNotChangeTheCorrection` |
| OffsetCorrect_PREPTimeModel | 코드 3종 중 하나 | 위와 같음(acquisitionTime 은 읽히지 않음) — 위 시험이 함께 덮음 |
| GainCorrect_GainMapDivision | 코드 4종 중 하나 | `GainPolyNotAppliedTest.ScalarGainMapIsApplied` (1000/2 = 500 정확) — 삭제 |
| GainCorrect_MultiSIDInterpolation | 코드 4종 중 하나 | 기능 없음(REQ-015) — M2 의 `KvpAndSidDoNotChangeTheCorrection` 가 현행 사실을 단언 — 삭제 |
| GainCorrect_NaNInfValidation | 코드 4종 + 출력 isfinite(보정 미적재라 루프 0회) | M1 의 `GainCorrect_NoNaNInfInOutput`(적재·64화소 계수) — 삭제 |
| DefectCorrect_EdgeAwareInterpolation | 코드 3종 중 하나 | `DefectCorrectTest.EdgeDefectUsesInBoundsNeighbors` — 삭제 |
| DefectCorrect_StaticBPMPriority | 코드 3종 중 하나 | 같은 이름의 동작이 `src/defect*.cpp` 에 없다(`priority` grep 0건) — 삭제 |
| DefectCorrect_TransientDefectDetection | 코드 3종 중 하나 | 런타임 검출은 `test_runtime_detection_*.cpp` 가 다룸 — 삭제 |
| DefectCorrect_ClusterDefect | 코드 3종 중 하나 | `DefectCorrectTest.DefectClusterUsesMedianFilter` — 삭제 |
| DefectCorrect_EmptyBPM | 코드 3종 중 하나 | `DefectCorrectTest.NoDefectsLeavesImageUnchanged` — 삭제 |
| GenerateOffset_DarkFrameAveraging | OK 또는 NOT_IMPLEMENTED | `GenerateOffsetTest.TwoFrames_AverageIsCorrect`, `ThreeFrames_AverageIsCorrect` — 삭제 |
| GenerateOffset_SingleFrame | OK 또는 NOT_IMPLEMENTED | `SingleFrame_OutputEqualsInput` — 삭제 |
| GenerateOffset_NullFrames | INVALID_INPUT (정확) | `GenerateOffsetTest.NullFrames_ReturnsInvalidInput` — 삭제 |
| CheckExpiry_ValidCalibration | OK, 만료 아님, 남은 일수 > 0 | `CheckExpiryTest.FutureExpiry_IsExpiredFalse`, `NeverExpires_…` — 삭제 |
| CheckExpiry_ExpiredCalibration | OK, 만료, 남은 일수 < 0 (실제: 옛 배치라 만료 아님으로 읽힘) | `CheckExpiryTest.PastExpiry_IsExpiredTrue` — 삭제 |
| CheckExpiry_NullFilePath | INVALID_INPUT (정확) | `CheckExpiryTest.NullFilepath_ReturnsInvalidInput` — 삭제 |
| Save_XCalFormatWrite | 코드 4종 중 하나 + 파일 magic "XCAL" | `test_xpe_calib_save.cpp`, `test_xcal_writer.cpp` — 삭제 |
| Save_FileCreationSuccess | 코드 4종 중 하나 | 위와 같음 — 삭제 |
| ValidateReadout_DroppedColumnDetection | OK 또는 NOT_IMPLEMENTED | `test_readout_validate.cpp` — 삭제 |
| ValidateReadout_GainNonUniformity | OK 또는 NOT_IMPLEMENTED | 위와 같음 — 삭제 |
| DefectDetectRuntime_StatisticalOutlierDetection | 코드 4종 중 하나 | `test_runtime_detection_functional.cpp`·`_rates.cpp` — 삭제 |
| DefectDetectRuntime_HotPixelDetection | 코드 4종 중 하나 | 위와 같음 — 삭제 |
| DefectDetectRuntime_StuckPixelDetection | 코드 4종 중 하나 | `test_defect_correct.cpp`, `test_defect_oracle.cpp` — 삭제 |
| GetParamRange_ValidLookup | OK 또는 NOT_IMPLEMENTED, min>0<max | `ParamRangeNamesTest.EveryNamedParameterReturnsAnOrderedRange` — 삭제 |
| GetParamRange_InvalidParams | INVALID_INPUT 두 입력(알 수 없는 이름, 널 출력) | 알 수 없는 이름은 `UnknownParameterIsRejected`. 널 출력은 현행에 없음 — **이관**: `ParamRangeNamesTest.NullOutputPointersAreRejected` |

합계 31건: 새 시험 3개로 옮긴 것 4건(NotInitialized, Temperature, PREP, 널 출력), 현행이 이미 덮는 것 24건, 구현에 없는 기능·검사라 이름만 남아 삭제 3건(SessionMismatch·StaticBPMPriority·MultiSID). 표의 대응 파일·시험 이름은 grep 과 시험 목록 읽기로 확인했고, 각 현행 시험의 단언 본문을 레거시 입력과 하나씩 대조하지는 않았다(§6).

## 3. 신규 시험과 반증

| 시험 | 단언 | 반증 | 결과 |
|---|---|---|---|
| `CalibLoadTest.LoadBeforeInit_AllThreeLoadersAcceptValidFiles` | 초기화 안 된 상태에서 offset·gain·defect 로더가 모두 `XPE_OK`, 적재가 모듈을 초기화하지 않음 | c1 offset 로더가 미초기화면 NOT_INITIALIZED, c2 gain 로더 동일 | 둘 다 빨강(`-6`) |
| `ParamRangeNamesTest.NullOutputPointersAreRejected` | 널 min / 널 max / 둘 다 널 / 널 이름 각각 INVALID_INPUT, 거부된 호출은 출력을 쓰지 않음 | c3 널 출력이면 OK 반환 | 빨강(`0`) 둘 |
| `PreprocessCorrectionTest.OffsetCorrect_MetadataDoesNotChangeTheCorrection` | offset 맵(값 100) 적재, kVp·SID·acquisitionTime 4쌍에서 모든 화소가 `max(in-100,0)`(독립 계산)이고 결과가 같음 | c4 `kVp==150` 이면 화소 0 에 +1, c5 `acquisitionTime==1700000000` 이면 화소 2 를 0 으로 | 둘 다 빨강 |

c1~c5 는 원본으로 복원했다(`git status` 에 제품 소스 수정 없음). 증거: `evidence/arm_c1…c5_*.txt`, `evidence/full_run.txt`, `evidence/doxygen_run.txt`.

defect 로더는 시험은 포함하나 반증은 달지 않았다(c1·c2 와 같은 구조의 offset·gain 만 손상) — §6.

## 4. 헤더 변경

- `xpe_offset_correct`: REQ-P1A-010 "with temperature interpolation", AC-OFF-002, AC-OFF-003 을 "미구현 — #245" 로. 지금 metadata 가 하는 일: 없음. M2 와 같은 처리.
- 로더 3종: "XPE_ERR_NOT_INITIALIZED if module not initialized" 줄을 "never XPE_ERR_NOT_INITIALIZED: 초기화 전에도 적재된다 — 처리 함수가 미초기화 모듈을 거부한다"로. 근거는 `test_xpe_preprocess_init.cpp` 의 기존 시험과 이번 `LoadBeforeInit_…`. 카드 지시에는 없던 정정이며, 레거시 `LoadOffset_NotInitialized` 가 왜 틀렸는지 확인하다 나온 것이라 같이 넣었다. 되돌릴 이유가 있으면 이 줄만 되돌리면 된다.
- `xpe_calib_load_offset` 의 "check session matching" / "XPE_ERR_CONFIG_INVALID if session mismatch" 는 건드리지 않았다 — M3 설계 소관.

## 5. 부수 사실

- `get_param_range` 의 표에는 `temperature_c`·`kVp`·`SID_mm` 등의 범위가 있지만 보정 경로는 이 값을 쓰지 않는다. 범위 조회가 기능이 있다는 인상을 줄 수 있다 — 손대지 않았다.
- `xpe_calib_generate_offset` 은 `temperature_c`·`integration_time_ms` 를 받는다. 이 값이 offset 파일에서 무엇이 되는지(config 기록 외에 쓰이는지)는 이번에 확인하지 않았다.

## 6. 미검증 (Gaps) · 잔여 위험

- 표의 "현행 시험이 덮는다" 24건은 같은 API 를 정확히 단언하는 시험이 있다는 것까지다. 레거시 입력(크기 1024×1024, 특정 값)과 현행 시험의 입력이 같다는 것은 확인하지 않았다. 레거시 단언이 대부분 "여러 코드 중 하나"였으므로 현행이 더 강하지만, 입력 범위가 같다는 뜻은 아니다.
- defect 로더(`xpe_calib_load_defect_map`)가 미초기화에서 `XPE_OK` 인 것은 시험에서만 관측했고 반증(c3 와 같은 손상)은 달지 않았다.
- 신규 시험은 단일 구성(`ci-preprocess`)에서만 돌렸다. Mock/다른 CI 구성에서는 돌리지 않았다(이 모듈은 단일 구성이지만 CI 잡 구성 전부를 확인하지는 않았다).
- 삭제된 파일의 내용은 git 에 남아 있다(`git show e9b8ed46^:modules/preprocess/tests/test_xpe_preprocess.cpp` 가 분할 직전 판).
- `acquisitionTime` 은 시험에서 4값만 바꿨다. 경계값(UINT64_MAX 등)은 시험하지 않았다.
