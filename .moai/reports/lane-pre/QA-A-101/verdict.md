# QA-A-101 — FUNC-031 (3)(4)(5)(8) 모드 상한 강제와 AUTO 선택 (#169)

커밋 `d681019` (dev/preprocess, 미푸시, 기준 main `1820bb3`)

## 1. 주장

1. 바꾸기 전 상태
   - 모드 표 `xpe_calib_mode.cpp:60-66`: SINGLE {1,0}, DUAL {2,1}, MP5 {5,2}, MP8 {8,3}, MP10 {10,3}, AUTO {10,3}.
   - 생성 경로에서 모드를 쓰는 곳은 메타데이터 기록뿐이었다(`xpe_calib_generate_gain.cpp:297,309,624`).
   - `xpe_calib_get_max_points` / `xpe_calib_get_poly_degree` 를 부르는 제품 코드는 없었다.
   - 명시 모드도 레벨 상한을 강제하지 않았다(측정 아래).
2. 옛 선택 로직(`mode_selector.cpp`, `87ff54d` 에서 삭제)
   - `87ff54d^` 에서 읽었다(147–172행).
   - N<2 → SINGLE, N<5 → DUAL, 그 뒤 `snr_db` 조건(>40 → MP10, >30 → MP8), N≥5 → MP5.
   - 요구와 두 가지가 다르다.
     - SNR 을 본다(요구에 없음).
     - N=3·4 를 DUAL(2점)로 보내 그 N 을 받을 수 없는 모드를 고른다.
   - 근거로 쓰지 않았다.
3. 구현(`xpe_calib_resolve_mode`)
   - 명시 모드는 레벨 수 > max_points 이거나 차수 > poly_degree 이면 `XPE_ERR_INVALID_INPUT` 이다.
   - AUTO 는 두 조건을 모두 받는 가장 작은 모드를 고른다.
   - 두 생성 함수(FUNC-026 = N 1, 차수 0 / FUNC-027 = N, max_degree)에 적용했다.
   - 사용한 모드는 다음에 기록한다.
     - `XpeCalibQualityMeta::calibration_mode`
     - JSON `calibration_mode` (요청 모드는 `requested_calibration_mode`)
     - AUTO 일 때 `XPE_ALERT_INFO` 경보 "XPE_CALIB_AUTO resolved to mode N"
4. MP10 차수 상한은 3 → 4(SRS). AUTO 행의 표시 상한도 {10,4} 로 바꿨다.
5. AUTO 를 예전 상수(MP10)로 되돌리면 새 시험 4건이 빨강이 된다.

## 2. 증거

- 사전 측정(임시 시험, 커밋 안 함, `probe-explicit-mode-no-cap.log`): 6레벨·차수 2로 모드 0–5 각각 `xpe_calib_generate_gain_polynomial` → 모두 `rc=0`.
- 경계 동작(시험으로 고정). N = 레벨 수, D = 차수.

| N | D | AUTO | 명시 모드 기준 |
|---|---|---|---|
| 3 | 2 | MP5 | DUAL·SINGLE 거부 |
| 5 | 2 | MP5 | MP5 허용(상한과 같음) |
| 6 | 2 | MP8 | MP5 거부(상한+1) |
| 8 | 2 | MP8 | MP8 허용 |
| 9 | 2 | MP10 | MP8 거부(상한+1) |
| 10 | 2 | MP10 | MP10 허용(최대 상한) |
| 11 | 2 | INVALID_INPUT | MP10 도 INVALID_INPUT(같은 코드) |
| 3 | 3 | MP8 | MP5 거부(차수 3>2) |
| 3·4 / 8 | 4 | MP10 | MP8 거부(차수 4>3) |
| 1 | 0 (FUNC-026) | SINGLE | 모든 명시 모드 허용 |

- N=0(최소 미만)과 N=2 는 공개 API 로 닿지 않는다. FUNC-026 은 1레벨이고, FUNC-027 은 기존에 N<3 을 거부한다. 해석기 함수는 DLL 에서 내보내지 않아 직접 시험하지 않았다.
- 독립 비교: 경계 8행마다 AUTO 실행과 표가 가리키는 명시 모드 실행을 비교했다. rc, 계수 페이로드, 기록 모드, JSON 이 같다.
- 빌드·실행
  - 구현: `BUILD_EXIT=0`, warning C 0. 관련 필터 45건 통과(`a101-run-impl.log`).
  - 반증 1차: `return` 을 끼워 C4702 → `BUILD_EXIT=1` 이었다. 이때 찍힌 "PASSED 7" 은 낡은 바이너리이므로 버렸다.
  - 반증 2차: 선택 루프 시작을 MP10 으로 바꿨다. `BUILD_EXIT=0`, warning C 0, `RUN_EXIT=1`, FAILED 4건(`AutoResolvesToTheSmallestFittingMode`, `AutoMatchesTheExplicitModeAtEveryBoundary`, `SingleDoseGeneratorUnderAutoRecordsSinglePoint`, `AutoResolutionIsReportedOnTheAlertQueue`).
  - 복원: 보관본 복사 후 `ctest --preset ci-preprocess` → 660건 통과, 종료 0.

## 3. 기준 귀속

- 요구: `docs/calibration/SRS-CALIB-001` FUNC-031 (3)(4)(5)(8)과 모드별 "degree ≤" 서술. "smallest fitting" 은 max_points ≥ N 이면서 poly_degree ≥ 요청 차수인 첫 모드로 해석했다(FUNC-031 vs FUNC-027 비고: "mode enforcement (max levels, max degree)").
- 비교 대상: 같은 입력을 명시 모드로 돌린 실행(해석기와 무관한 경로의 결과).

## 4. 미검증 · 호출자 영향

- **호출자 조사**(검색 범위: 저장소 전체 `git grep`, 제목 `xpe_calib_generate_gain*`, `xpe_calib_set_mode`, `CalibrationMode`)
  - `modules/preprocess/tests` 5개 파일: 모두 기본 모드(MP8)에서 레벨 ≤5, 차수 ≤2 → 영향 없음. 전체 ctest 로 확인했다.
  - `modules/preprocess/tools/xpe_calib_fixture_gen.cpp:337`: FUNC-026 만 사용 → 모든 모드 허용. 실행하지 않았다.
  - `clients/ImageProcTest.IntegrationTests` (Lane C 소유): `PreprocessCorrectionChainSmokeTests.cs:205` 가 FUNC-026 사용, `set_mode` 호출 없음 → 코드상 영향 없음. 실행하지 않았다.
  - `gui/`: 호출 없음.
  - 깨지는 호출자는 찾지 못했다.
- MP10 차수 3→4 로 바뀌는 것
  - `xpe_calib_get_poly_degree()`(MP10·AUTO 반환값 3→4, 시험 2행 수정)
  - MP10 에서 `max_degree=4` 허용
  - 이 값을 쓰는 저장 데이터·fixture 는 찾지 못했다.
- `max_degree` 가 모드 차수보다 클 때의 거부는 기본 모드(MP8)에서 `max_degree=4` 를 넘기는 기존 호출자를 깨뜨린다. 저장소 안에는 그런 호출자가 없었다(외부 호출자는 모른다).
- Doxygen 빌드는 돌리지 않았다(`\#169` 이스케이프는 두 곳 모두 확인).
- CI 실행 없음.

## 5. 잔여 위험

- 계수 페이로드는 모드가 아니라 `max_degree` 로 맞춰진다. 그래서 AUTO 선택이 달라져도 페이로드는 같다. 반증에서 빨강을 만든 것은 기록 모드였다. 선택 결과가 맞춤에 영향을 주는 경로는 없다(요구도 요구하지 않음).
- 관측: 같은 출력 경로에 파일이 있으면 두 번째 생성이 −9(`XPE_ERR_IO_FAILED`). 고치지 않았다(리더 지시).
- 모드는 프로세스 전역 상태다. 다른 스레드가 생성 도중 모드를 바꾸면 기록 모드와 요청 모드가 어긋날 수 있다.
