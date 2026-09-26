# QA-B-98 (#183, #180) — 콜리메이션 검출 결함 수정, 마스크 없음 경고 핸들당 1회

커밋 `413491e` (미푸시, `dev/postprocess`). 파일 6개:
- `modules/enhance_advanced/include/xpe/enhance_advanced/xpe_enhance_advanced_api.h`
- `modules/enhance_advanced/src/detail/hough_transform.cpp`
- `modules/gsvg/include/xpe/gsvg/gsvg_api.h`
- `modules/gsvg/src/gsvg.cpp`
- `modules/gsvg/tests/test_virtual_grid_mc.cpp`
- `tests/e2e_post_pipeline/test_e2e_collimation_mask.cpp`

(처음에 두 커밋으로 나눴다가, e2e 파일에 두 변경이 섞여 첫 커밋 시점에서 시험이 깨지는 것을 보고 푸시 전에 하나로 합쳤다.)

## 1. 주장

1. **위쪽 변 한 줄 어긋남의 원인은 허프 투표의 rho 버림**이다. 반올림으로 고쳤다.
   - 수정 뒤 균일 장면과 조사야를 0–3 화소 옮긴 네 경우 모두 네 변이 정확하다.
   - 반증: 버림으로 되돌리면 BUILD=0 에서 e2e 3건이 빨강이다.
2. **물체 안 경계를 변으로 잡는 것**은 "세로선 중 가장 강한 두 개" 선택 규칙 때문이다. 설계 판단이 필요해 고치지 않았다(선택지 3.2).
3. **MC 80×80 에서 대체값이 나오는 원인**은 여러 가지가 겹친다(3.3).
   - 수정 전: 가로선이 하나만 남음
   - 수정 뒤: 세로 두 선이 0 과 2 로 겹쳐 넓이 검사에서 탈락
   - 이 역시 설계 판단이다.
4. 공개 헤더에 좌표 규약(양끝 포함, 대체값 `[0, w−1] × [0, h−1]`)을 적었다.
5. 기존 enhance_advanced 시험은 **기대값을 하나도 바꾸지 않고** 통과한다.
   - 기존 콜리메이션 시험의 허용 오차는 ±3 화소(AC-COL-001)라서 한 줄 어긋남을 잡지 못했다.
   - 전체 검증의 `T308_PerformanceBudget`(엣지 강조 시간) 실패 1회는 이 변경과 무관하다(3.5).
6. **마스크 없음 경고는 핸들당 한 번**이다(리더 결정 B).
   - 1000회 호출 뒤 큐에 4개(gsvg 1, 다른 모듈 Info·Warning·Error 각 1)가 남고 아무것도 밀려나지 않는다.
   - 다시 init 하면 한 번 더 난다.

## 2. 증거 — 위쪽 변

### 2.1 원인 (코드 인용)

`hough_transform.cpp` `buildAccumulator`(수정 전):

```cpp
const float theta = static_cast<float>(t) * thetaStep_;
const float rho = static_cast<float>(x) * std::cos(theta)
                + static_cast<float>(y) * std::sin(theta);
int rhoBin = static_cast<int>(rho / rhoStep_) + maxRho_;   // 버림
```

- 가로선의 theta 는 `90 × (π/180)` 을 float 로 곱한 값이다. 로그에서 `theta=1.570796`.
- 이때 cos(theta) 는 약 −4.4×10⁻⁸ 로 0 이 아니다. 그래서 x > 0 인 화소의 rho 는 y 보다 조금 작고(30 → 29.99999), 버림으로 **29 번 칸**에 들어간다.
- 세로선은 theta = 0 이라 cos = 1 이 정확하다. rho = x 가 그대로 칸이 된다. **이것이 세 변과의 비대칭**이다.
- 아래쪽 변이 맞게 나온 것도 우연이었다. 226 행(바깥)이 225 번 칸에 들어갔다.
- 음수 rho(theta 가 180° 근처)에서도 0 쪽 버림은 비대칭이다.

수정:

```cpp
int rhoBin = static_cast<int>(std::lround(rho / rhoStep_)) + maxRho_;
```

### 2.2 수치 (임시 디버그 출력, 커밋하지 않음)

`_dbg2.log`(수정 전) → `_dbg3.log`(수정 후), 균일 장면:

| | 수정 전 | 수정 후 |
|---|---|---|
| 가로선 (rho, 세기) | 29 / 19,633,736, 225 / 19,568,780 | **30** / 19,633,742, 225 / 19,633,742 |
| 세로선 | 40 / 21,836,196, 215 / 21,836,196 | 같음 |
| 사각형 | 40 29 215 225 | **40 30 215 225** |

수정 뒤에는 위·아래 선의 세기가 같아졌다. 경계의 대칭이 돌아왔다는 뜻이다.

### 2.3 시험 (`_b98_1.log`, BUILD=0, e2e 7건 통과)

```
COLLMASK scene=uniform ... detected x0=40 y0=30 x1=215 y1=225 mask pixels differing=0 output pixels differing=0 (no mask: 65536) |diff| max=0 mean=0.00 DN
COLLMASK shift=0 truth 40 30 215 225 detected 40 30 215 225
COLLMASK shift=1 truth 41 31 214 224 detected 41 31 214 224
COLLMASK shift=2 truth 42 32 213 223 detected 42 32 213 223
COLLMASK shift=3 truth 43 33 212 222 detected 43 33 212 222
```

- 시험 변경
  - `KnownDivergence_DetectorTopSideOneLineOutside` → `DetectedMaskMatchesTruth`(네 변 정확, 마스크·출력 차이 0)
  - `DetectionExactForShiftedFields` 추가(네 경우 × 네 변)
  - `KnownDivergence_InteriorEdgeTakenForFieldSide` 의 y0 기대값: 29 → 30. 옛 값은 이 결함의 결과였다.
- 반증(`_falsify.log`): 버림으로 되돌리면 BUILD=0 에서 `DetectedMaskMatchesTruth`, `DetectionExactForShiftedFields`, `KnownDivergence_InteriorEdge…` 3건이 빨강이다. 되돌린 뒤(`_falsify_restored.log`) 7건 통과, `FALSIFY` 0건, `lround` 1건.

## 3. 증거 — 나머지 경우와 기존 시험

### 3.1 물체 안 경계 (`_dbg3.log`, 수정 후)

```
DBGV rho=40.0000  s=21836196
DBGV rho=127.0000 s=14394240   ← 물체 안 계단 30000 → 12000
DBGV rho=216.0000 s=7609656    ← 실제 오른쪽 변 12000 → 산란
DBGRECT 40 30 127 225
```

`extractCollimationRectangle` 은 방향마다 **세기 상위 2개**를 쓴다(`hough_transform.cpp` "Use only the top-2 strongest lines per orientation"). 물체 안 계단이 실제 변보다 세면 그것이 변이 된다.

또 216 은 바깥 화소다(정답 215). 바깥 산란이 안쪽 12,000 보다 훨씬 낮아, 대비가 작은 쪽의 봉우리가 바깥 줄에 섰다.

### 3.2 선택지 (고치지 않음)

| 선택지 | 내용 | 영향 |
|---|---|---|
| A | 방향마다 가장 바깥쪽의 "충분히 센" 선 | 세기 문턱이 필요하다(새 상수) |
| B | 후보 선들로 만든 사각형 중 "안 평균 − 밖 평균"이 가장 큰 것 | 상수 없음. 후보 L 개면 O(L²) 사각형 평가. 조사야 = 밝은 안 / 어두운 밖이라는 가정에 기댄다 |
| C | 극성: 변 바깥쪽이 어두운 선만 | 이 장면의 계단(오른쪽이 어두움)은 걸러지지 않는다 |
| D | 0°/180° 경계를 넘는 NMS(3.3 의 중복선 제거) | A–C 와 함께 필요하다 |

### 3.3 MC 80×80 대체값 (`_dbg2.log` → `_dbg3.log`)

수정 전:

```
DBGH rho=77 (가로선 1개뿐) → 가로선 < 2 → 신뢰도 0 → 대체값
DBGV rho=0(178°), 2, 12, -10(178°), 25, -23(178°)
```

수정 후:

```
DBGH rho=2, 77     ← 조사야(공기 기준 3..76)의 바로 바깥 줄
DBGV rho=2, 0(178°), 12, -10(178°), 25, -23(178°)
DBGRECT 0 2 2 77   → 넓이 비율 미달 → 대체값
```

겹친 원인:

1. **선을 방향 구분 없이 상위 8개만 받는다**(`detectAxisAlignedLines(accumulator, 8)`). 수정 전에는 위쪽 가로 경계가 8개 안에 들지 못했다.
2. **거의 세로인 중복선**: 허용 ±5° 안의 178° 선이 따로 셈해진다. NMS 창(±2 칸)은 theta 0 과 179 를 이웃으로 보지 않는다. 그래서 x≈0 근처의 같은 경계가 두 번 들어가, 세로 상위 2개가 0 과 2 가 됐다.
3. **물체 안 계단 경계**(12, 25)가 조사야 변보다 세다(3.1 과 같은 원인).
4. **25 cm 쪽 조사야 변은 대비가 뒤집혀 있다.** 안쪽(1차 약 137 DN + 산란)이 바깥 산란(약 414 DN)보다 어둡다(QA-B-97 §2.5). 밝은 안/어두운 밖을 가정하는 방법으로는 이 변을 찾기 어렵다.
5. 검출된 가로선(2, 77)은 공기 기준 조사야(3, 76)의 바깥 줄이다. MC 조사야 가장자리는 계단이 아니라 완만하다.

선택지: 3.2 의 D(중복선 제거) + B(사각형 대비 점수)로 1–3 은 줄일 수 있다. 4 는 영상만으로는 어렵다. 조사야 정보를 장비(콜리메이터 설정)나 공기 영상에서 받는 방법이 필요하다(설계 판단).

### 3.4 기존 enhance_advanced 시험 (`_b98_1.log`)

- `test_xpe_enhance_advanced.exe --gtest_brief=1`: 226 통과, `EdgeEnhancementTest.T308_PerformanceBudget` 1 실패
- 콜리메이션 시험(`test_collimation_detect*.cpp`)은 기대값을 바꾸지 않고 모두 통과한다. 좌표를 ±3 화소로 비교하거나 오류 경로만 보는 시험이라 한 줄 변화에 둔감하다.

### 3.5 T308 실패 판독

- `_t308.log`: 같은 바이너리로 T308 만 5회 → 5회 통과, 89–91 ms
- 실패한 실행은 세 대상을 이어서 빌드한 직후였고 109 ms 였다
- T308 은 `xpe_fractional_process`(1024²)의 시간 시험이고, 문턱은 100 ms 다(`test_edge_enhancement.cpp:639`). `fractional_process.cpp`·`fractional_derivative.cpp` 에 `hough` 검색 결과는 0건이다.
- `_verify.log` 전체 ctest 에서는 통과했다(618 모두 통과).
- **로컬 시간 흔들림으로 판단한다.** CI 선택식(`-E "...PerformanceBudget..."`)은 이 시험을 뺀다.

## 4. 증거 — 경고 핸들당 1회

- `GsvgHandle::warned_no_mask` 를 두고 첫 번째 마스크 없는 호출에서만 alert 를 넣는다.

`_b98_1.log`:

```
COLLMASK flood calls=1000 queued=4 gsvgWarnings=1 info=1 otherWarning=1 otherError=1 lossAlerts=0 '' pending_count=4
```

QA-B-97 에서는 64 / 62 / 0 / 0 / 1 / 1(loss 940)이었다.

- 재 init 뒤 10회 호출 → 경고 1(`UnmaskedWarningFlood` 후반부)
- MC 공개 진입점 시험(`_b98_1.log` MC 6건 통과)
  - 같은 핸들에서 두 번째 쌍의 경고는 0
  - 새 핸들에서 3회 호출하면 경고 1

## 5. 전체 검증 (`_verify.log`)

```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 618   ===POST_EXIT=0===
===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
E2E                  100% tests passed, 0 tests failed out of 27    ===E2E_EXIT=0===
```

617 → 618: `DetectionExactForShiftedFields` 가 늘었다(위쪽 변 KnownDivergence 는 이름만 바뀜).

빌드 실패 기록: `_dbg1.log` 는 BUILD=1(heredoc `\n` → C2001)이라 버렸다.

## 6. 기준 귀속

- 검출의 정답: 장면을 만든 사각형(`kField`, 옮긴 사각형 `f`).
- 수정 전후 비교: 같은 장면·같은 바이너리 구성에서 디버그 출력만 넣은 실행.
- 경고: `xpe_clear_alerts` 뒤 다른 모듈 alert 3개를 넣은 상태.

## 7. 미검증

- 가로선이 float `π/2` 가 아닌 다른 각도 칸(thetaStep 2·3)에서의 반올림 효과. `confidence_strictness` 기본값에서만 봤다.
- 실제 영상(반그림자, 기울어진 조사야)에서의 검출
- 3.2·3.3 의 선택지를 구현했을 때의 결과
- 경고 한 번 제한의 반증(한 번 제한을 빼면 1000회 시험이 빨강인지). QA-B-97 측정(62개)으로 대신했다.
- CI 러너 결과

## 8. 잔여 위험

- 반올림으로 칸이 바뀌어, 경계 양쪽 두 줄의 투표가 같은 칸에 모이는 경우의 봉우리 위치가 달라졌다. 합성 장면에서는 안쪽 줄이 선택됐다(네 변 모두). 잡음이 많은 영상에서 안/밖 중 어느 줄이 선택될지는 보장하지 않는다.
- 물체 안 경계와 대비가 뒤집힌 조사야 변은 여전히 틀리게 잡는다(3.1, 3.3). 가상 그리드에 이 결과를 그대로 넘기면 조사야 일부가 보정되지 않는다.
- 한 핸들로 여러 환자를 처리하는 호출자는 두 번째부터 마스크 없음을 알 수 없다(선택지 B 의 대가).

## Card Cross-Check

| 카드 요구 | 결과 |
|---|---|
| 경고 선택지 B | 4 |
| 재 init → 다시 한 번, 시험 고정 | 4 |
| 1000회 재측정, 다른 모듈 Warning 남음 | 4 |
| #183-1 원인 코드 인용, 세 변과 차이 | 2.1 |
| 고친 뒤 정확 일치 + 옮긴 네 경우 | 2.3 |
| #183-2 물체 안 경계·80×80 원인, 선택지 | 3.1–3.3 |
| #183-3 헤더 좌표 규약 | 1.4 |
| #183-4 반증 | 2.3 |
| #183-5 기존 시험, 기대값 변경 사유 | 3.4 (변경 없음), 2.3 (e2e y0 29→30) |
| BUILD_EXIT | 2.3, 3.4, 5 |
