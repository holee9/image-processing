# QA-A-192 (#220 §3) — `xpe_verify_gain` 에 게인 의미 인자: 알려졌으면 0.5%, 모르면 1.0%

> ## ⚠ ABI 변경 (맨 위에 둡니다)
>
> `xpe_verify_gain` 의 시그니처가 **4인자에서 5인자로 바뀌었다**. 공개 C ABI 변경이다.
>
> ```c
> // 전
> XpeErrorCode xpe_verify_gain(const XpeImageBuffer* before_gain, const XpeImageBuffer* after_gain,
>                              const XpeImageBuffer* gain_map, XpeCalibrationMetrics* metrics);
> // 후
> XpeErrorCode xpe_verify_gain(const XpeImageBuffer* before_gain, const XpeImageBuffer* after_gain,
>                              const XpeImageBuffer* gain_map, XpeGainSemantics gain_semantics,
>                              XpeCalibrationMetrics* metrics);
> ```
>
> - 새 열거형 `XpeGainSemantics { XPE_GAIN_SEMANTICS_UNKNOWN = 0, _NORMALIZED = 1, _RECIPROCAL = 2 }` 가 헤더에 생겼다 (C ABI 에서 int32).
> - **수출 이름은 그대로다** (`dumpbin` 이름 53 = 53, C ABI 48 = 48, 차이 없음 — `evidence/12_exports_after.txt` 와 QA-A-190 `03_exports_after.txt` 비교). C ABI 는 이름만으로 인자 수를 장식하지 않으므로 **이름 비교는 이 변경을 보여 주지 못한다.** 옛 4인자 호출은 링크·로드는 되고 `metrics` 자리에 열거형 값이 들어가 잘못 동작한다.
> - 영향 범위: 저장소 안의 호출자는 시험 11곳뿐이다 (제품·GUI·클라이언트 호출자 0 — QA-A-189). 이 11곳을 모두 `XPE_GAIN_SEMANTICS_UNKNOWN` 으로 고쳐 **기존 동작을 그대로** 유지했다. 이 저장소 밖에서 옛 4인자 형태를 부르던 바이너리는 깨진다.
> - 기본값 없음: C 에는 기본 인자가 없어 호출자가 항상 값을 넘겨야 한다.

시작 HEAD `8b9c065b` (`evidence/00_head.txt`). 문턱 값은 SRS-CALIB-FUNC-017 원문에서 가져왔다:

> "System shall compute gain/flat-field metrics: `PRNU_CV`, `FlatResidualPct`, `FPN_Reduction_dB`, and `LineArtifactScore`. Phase 1 acceptance shall require `FlatResidualPct <= 1.0%` and target `<= 0.5%` for release-hardening fixtures where gain semantics are known." (`SRS-CALIB-001:148`)

게인 의미 값의 출처는 SRS-CALIB-FUNC-018 (`SRS-CALIB-001:149`):

> "System shall record calibration gain semantics as `normalized_gain`, `reciprocal_gain`, or `unknown`. Unknown semantics may run exploratory validation but shall not pass release gates."

## 동작

- `gain_semantics` 가 `NORMALIZED` 또는 `RECIPROCAL` (의미를 안다): `FlatResidualPct <= 0.5%` 일 때만 `overall_pass` 의 평탄 잔차 조건을 통과 (`FLAT_RESIDUAL_KNOWN_MAX_PCT = 0.5`).
- `UNKNOWN`: 지금까지처럼 `<= 1.0%` (`FLAT_RESIDUAL_MAX_PCT = 1.0`).
- 인자는 **문턱만** 고른다. 측정값(`prnu_after`, `prnu_before`, `snr_improvement_db`, `measured_mask`)은 인자에 따라 바뀌지 않는다 (시험이 단언).
- 열거형 밖의 값(−1, 3, 99)은 `XPE_ERR_INVALID_INPUT`, `metrics` 는 건드리지 않고 반환 (REQ-P1A-005; 시험이 바이트 단위로 단언).
- 다른 조건(PRNU 개선, SNR 3 dB, 게인 유효 비율)은 그대로다.

## 변경 파일

| 파일 | 변경 |
|---|---|
| `modules/preprocess/include/xpe/preprocess_api.h` | `XpeGainSemantics` 열거형, 새 시그니처, `@param gain_semantics`, 오류 코드 목록, ABI `@note` |
| `modules/preprocess/src/xpe_verify_metrics.cpp` | 범위 검사, 문턱 선택, 상수 `FLAT_RESIDUAL_KNOWN_MAX_PCT` 와 SRS 인용 주석 |
| `modules/preprocess/tests/test_verify_gain_semantics.cpp` | 신규 3건 (`GainSemantics.*`) |
| `test_verify_metrics.cpp`·`test_verify_metrics_edges.cpp`·`test_zz_a186_probe.cpp` | 기존 호출 11곳에 `XPE_GAIN_SEMANTICS_UNKNOWN` 추가 (동작 동일) |
| `modules/preprocess/CMakeLists.txt` | 시험 파일 등록 1줄 |

헤더 오류 코드 목록도 코드에 맞게 고쳤다 (치수 불일치는 `XPE_ERR_BUFFER_TOO_SMALL`, 화소 수 0 은 `XPE_ERR_INVALID_INPUT`). 이 두 줄은 이 함수의 문서에 이미 있던 부정확한 기술(치수 불일치를 `INVALID_INPUT` 으로 적음)을 새 `@return` 블록을 쓰면서 바로잡은 것이다.

## TDD 순서와 증거

1. **시그니처만** (동작 불변): 헤더·구현에 인자를 추가하고 기존 호출 11곳을 `UNKNOWN` 으로 고쳤다. 빌드 `BUILD_EXIT=0`, 전체 725 통과 (`01_`~`03_`). 이 단계에서 `check_header_docs.py` 0 findings.
2. **빨강**: 새 시험 3건 추가 후 (`04_`, `05_red_run.txt`): 알려진 의미의 0.55%·0.80% 프레임이 통과해 단언 실패 (`semantics=1`·`2`, `Which is: false / true`), 열거형 밖 값 −1·3·99 가 거부되지 않고 `metrics` 가 수정됨. 측정값 독립 시험은 이 단계에서도 통과(측정은 처음부터 의미와 무관).
3. **구현**: 범위 검사 + 문턱 선택. 새 시험 3건 통과 (`06_`, `07_green_run.txt`).

최종 (`09_`~`14_`): **전체 시험 728 통과, 0 실패** (종료 코드 0), 셔플(시드 192) 종료 0, `check_header_docs.py` 20 헤더 0 findings, `ctest -N` 총계 845·DISABLED 40, 프리셋 점검 OK.

### 시험이 보는 것

`GainSemantics.FlatResidualLineIsHalfAPercentWhenKnownAndOnePercentWhenUnknown`: 프레임 5종(`after` 가 평균에서 ±0.40/0.45/0.55/0.80/1.20% 떨어짐)을 세 가지 의미로 돌린다. 모든 프레임에서 PRNU 개선·SNR ≥ 3 dB·게인 커버리지 ≥ 0.99 를 먼저 단언해 평탄 잔차 조건만이 결과를 가르게 했다.

| 잔차 | 알려진 의미 (0.5%) | 모름 (1.0%) |
|---|---|---|
| 0.40 | 통과 | 통과 |
| 0.45 | 통과 | 통과 |
| 0.55 | **실패** | 통과 |
| 0.80 | **실패** | 통과 |
| 1.20 | 실패 | 실패 |

### 반증 팔 (제품을 망가뜨림, 매 팔 `BUILD_EXIT` 확인)

`evidence/08_arms_summary.txt`:

| 팔 | 망가뜨린 곳 | 빨개진 시험 |
|---|---|---|
| A | 의미를 무시하고 항상 1.0% | `FlatResidualLine…` |
| B | 항상 0.5% | `FlatResidualLine…` (모르는 의미의 0.55·0.80 이 실패) |
| C | `RECIPROCAL` 을 모름으로 취급 | `FlatResidualLine…` |
| D | 열거형 범위 검사 삭제 | `AnUnknownEnumerationValueIsRejected…` |

네 팔 모두 `BUILD_EXIT=0` 이고 예상한 시험이 빨개졌다. 원복 후 `diff` 로 파일 동일 확인.

## Gaps / Residual-risk

- **알려진 의미가 더 엄격하다.** 사용자 결정대로 알려진 의미는 0.5%, 모르면 1.0% 이므로 같은 프레임이 알려진 의미로는 실패하고 모르는 의미로는 통과할 수 있다 (0.55~1.0% 구간). SRS-CALIB-FUNC-018 은 "unknown 은 릴리스 게이트를 통과할 수 없다"고 하는데 이 함수의 `overall_pass` 는 `UNKNOWN` 으로 1.0% 를 통과한다. 이 함수의 판정을 Phase 1 판정으로 읽는 한 모순이 아니지만 릴리스 게이트로 쓰는 호출자가 생기면 `UNKNOWN` 통과를 릴리스 통과로 오해할 수 있다.
- 인자 값을 **게인 맵과 대조하지 않는다.** 호출자가 `NORMALIZED` 라고 해 놓고 실제로 역수 맵을 넘기는 불일치는 이 함수가 잡지 못한다 (FUNC-018 의 "mismatch negative tests" 는 매니페스트 파서 쪽).
- 어떤 문턱이 적용됐는지를 `XpeCalibrationMetrics` 에 싣지 않았다 (구조체를 또 넓히면 ABI 가 한 번 더 바뀐다). 호출자는 자기가 넘긴 값으로 안다.
- 로컬에는 `doxygen` 이 없어 Doxygen 자체는 돌리지 못했다. 대신 헤더 문서 점검 스크립트(`tools/docs/check_header_docs.py`, `@param` 이름·중복·문서 블록 유무)는 0 findings. Doxygen 의 권위 있는 검사는 CI 의 `Header docs (doxygen)` job 이 한다. 생성물(`docs/help/generated/`)은 저장소에 추적되지 않아 갱신할 파일이 없다.
- C# P/Invoke 선언은 없다 (제품·GUI·클라이언트 호출자 0).
- 문서 쪽 후속은 리더 몫: `SRS-CALIB-001`·RTM 의 `xpe_verify_gain` 행이 시그니처를 적지는 않아 이번에 바꿀 곳은 없었다.
