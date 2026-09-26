# QA-B-97 (#180) — 콜리메이션 검출 → 조사 마스크 연결, main CI CRLF 실패

커밋 (미푸시, `dev/postprocess`):
- `ded48c6` — §1b. `modules/gsvg/tests/test_virtual_grid.cpp`
- `37761b3` — 본 카드. `tests/e2e_post_pipeline/{CMakeLists.txt, test_e2e_collimation_mask.cpp}`

## 1. 주장

### §1b (main CI 빨강)

1. CI 실패 5건을 **로컬에서 재현**했다. 합성 표를 CRLF 로 바꾸니 같은 5건이 실패했다(`_crlf_repro.log`).
2. 시험을 줄바꿈에 의존하지 않게 고쳤다.
   - 표를 읽을 때 CRLF 를 LF 로 접는다.
   - 구획 줄은 `SectionLine` 으로 찾고, 못 찾으면 `ASSERT` 로 멈춘다.
3. **제품 파서 `ParseParamTable` 은 이미 CRLF 표를 받는다**(`Trim` 이 `std::isspace` 로 `\r` 을 지운다).
   - `ParserReadsCrlfAndLfAlike` 로 고정했다. 원본 바이트에서 LF·CRLF 텍스트를 만들어 두 파싱 결과가 모든 값에서 같음을 단언한다.
   - 반증: `Trim` 이 공백만 지우게 바꾸면 BUILD=0 에서 이 시험이 빨강이다(`_falsify_parser.log`).
4. 세 CSV(합성, MC 커널, MC `[wet]`)를 모두 CRLF 로 바꾼 상태에서 가상 그리드 시험 34건이 통과한다(`_crlf_all.log`). 파일은 LF 로 되돌렸다.

### 본 카드

5. **좌표 규약은 양끝 포함**이다. 헤더에는 적혀 있지 않다. 근거(`collimation_detect.cpp`):
   - 신뢰도가 낮을 때의 대체값이 `x0=0, x1=width−1`
   - 넓이 계산이 `x1 − x0 + 1`
6. 변환 도우미 `MaskFromRect` 의 경계를 고정했다. 네 모서리는 안, 각 변 바로 밖 한 줄은 밖, 개수는 `(x1−x0+1)(y1−y0+1)` 이다. 도우미가 만든 마스크는 손으로 만든 정답 마스크와 같고, 가상 그리드 출력도 같다.
7. **검출 → 마스크 → 가상 그리드 결과는 정답 마스크 결과와 같지 않다.** 검출기가 틀린다(고치지 않고 기록).
   - 균일 물체: 세 변은 정확하고 **위쪽 변만 한 줄 바깥**이다(y0 29, 정답 30).
   - 마스크 차이 176 화소(그 한 줄). 출력 차이 25,586 화소, 평균 7.69 DN, 최대 2,073 DN.
   - 조사야를 1–3 화소씩 옮겨도 위쪽 변은 한 줄 바깥 세 번, 두 줄 바깥 한 번이었다. 다른 변은 늘 정확했다.
   - 물체 안에 강한 경계(x=128 밝기 계단)가 있으면 **그것을 오른쪽 변으로 잡는다**(x1 127, 정답 215). 출력 차이 평균 2,359 DN.
8. 반증: 도우미를 한 열 줄이면 BUILD=0 에서 3건이 빨강이다(`_falsify_helper.log`).
9. **경고 누적**: 마스크 없는 호출 1000회 뒤 큐(최대 64)
   - gsvg 경고 62, loss alert 1(940건 버림), 다른 모듈 Error 1
   - 다른 모듈의 Info 와 Warning 은 밀려났다
10. **MC 계단 팬텀에서 검출기는 영상 전체(0–79)를 돌려줬다.** 그래서 검출 마스크는 효과가 없다. 경계 밝기는 공기 기반 마스크로 쟀다(2.5).

## 2. 증거

### 2.1 §1b

| 실행 | 조건 | 결과 |
|---|---|---|
| `_crlf_repro.log` | 합성 표 CRLF, 수정 전 | BUILD=0, 실패 5건(CI 와 같은 이름), 통과 28 |
| `_crlf_fixed.log` | 합성 표 CRLF, 수정 후 | BUILD=0, 34건 통과 |
| `_falsify_parser.log` | 수정 후, `Trim` 을 공백만 지우게 | BUILD=0, `ParserReadsCrlfAndLfAlike` 실패 |
| `_crlf_all.log` | 세 CSV CRLF | BUILD=0, 가상 그리드 34건 통과 |
| `_gsvg_all.log` | LF, `ctest -R "^Gsvg\|Shapes/GsvgVirtualGrid" -E Performance` | BUILD=0, 110건 통과 |

- 수정 전 원인(카드와 같음): `text.find("\n[spr_cap]\n")` 이 npos 를 돌려주고, `erase(npos + 1)` = `erase(0)` 이 본문을 지웠다.
- 합성 표의 CR 개수 55 = LF 개수 55 로 CRLF 변환을 확인했다.
- 원본 소스 복원은 `git diff --stat` 로 차이 없음을 확인했다.

### 2.2 검출 (`_e2e6.log`, `_probe.log`)

```
COLLMASK scene=uniform truth x0=40 y0=30 x1=215 y1=225 detected x0=40 y0=29 x1=215 y1=225
  mask pixels differing=176 output pixels differing=25586 (no mask: 65536) |diff| max=2073 mean=7.69 DN
COLLMASK scene=step-inside truth x0=40 y0=30 x1=215 y1=225 detected x0=40 y0=29 x1=127 y1=225
  mask pixels differing=17336 output pixels differing=34584 (no mask: 63999) |diff| max=10286 mean=2359.23 DN
```

`_probe.log`(조사야를 한 칸씩 안쪽으로, 임시 시험, 커밋하지 않음):

| 정답 x0 y0 x1 y1 | 검출 |
|---|---|
| 40 30 215 225 | 40 **29** 215 225 |
| 41 31 214 224 | 41 **30** 214 224 |
| 42 32 213 223 | 42 **31** 213 223 |
| 43 33 212 222 | 43 **31** 212 222 |

- 장면: 256², 조사야 안 30,000 DN(원판 0.6배), 밖은 조사야 가장자리에서 2,500·exp(−거리/30) DN.
- 가상 그리드: 합성 표, 1 mm, 이상 격자.
- 한 줄 어긋난 마스크가 출력의 39 % 를 바꾼 이유: 그 줄(약 2,500 DN)이 산란원으로 들어가 넓은 커널을 통해 영상 전체의 산란 추정에 더해진다. 크기는 평균 7.69 DN 이다.

### 2.3 시험 (`_e2e6.log`, 6건 통과)

| 시험 | 내용 |
|---|---|
| `HelperIsInclusive` | 모서리 4점 안, 변 밖 4점 밖, 개수, 전체 사각형 = 전체 영상 |
| `HelperMaskEqualsHandMadeMask` | 도우미 = 손으로 만든 마스크, 출력 같음, 대조: 마스크 없음과 다름 |
| `KnownDivergence_DetectorTopSideOneLineOutside` | x0·x1·y1 정확, y0 = 정답−1, 마스크 차이 = 176, 출력 차이 > 0, 마스크 없음보다 작음 |
| `KnownDivergence_InteriorEdgeTakenForFieldSide` | (40, 29, 127, 225) |
| `UnmaskedWarningFlood` | 2.4 |
| `EdgeStepOnMcPhantom` | 기록만 (2.5) |

반증(`_falsify_helper.log`): `MaskFromRect` 의 `x0` 에 +1 을 넣으면 위 표의 앞 세 건이 실패한다. 되돌린 뒤 `FALSIFY` 문자열 0건을 확인했다.

### 2.4 경고 누적

```
COLLMASK flood calls=1000 queued=64 gsvgWarnings=62 info=0 otherWarning=0 otherError=1 lossAlerts=1
  'alert queue overflow: 940 alert(s) dropped' pending_count=64
```

- 순서: 다른 모듈 Info·Warning·Error 를 하나씩 먼저 넣고, `xpe_gsvg_process`(마스크 없음, 가상 그리드 켬, 64×64)를 1000회 불렀다.
- 큐 규칙(`modules/common/src/xpe_common.cpp`):
  - `:58-59` `kAlertQueueMax = 64`
  - `:107-118` `evict_one_locked` — Info → Warning → Error 순으로, 같은 등급 안에서는 가장 오래된 것을 지운다. loss alert 는 지우지 않는다.
  - `:122-147` `sync_loss_alert_locked` — 버린 개수를 Error 등급 loss alert 하나에 누적한다.
  - `:150-170` `enqueue_alert` — 가득 차면 먼저 하나를 지운다.
- 결과 해석: gsvg 경고(Warning)가 들어올 때마다 가장 오래된 Warning 이 지워진다. 그래서 **다른 모듈의 Warning 은 첫 번째 넘침에서 바로 사라진다.** Error 는 남는다.

**선택지(고치지 않음, 영향만)**

| 선택지 | 영향 |
|---|---|
| A. 그대로 | 가상 그리드를 마스크 없이 쓰는 호출자에게서 다른 모듈의 Warning 이 조용히 사라진다(loss alert 는 남음) |
| B. 핸들당 한 번만 경고 | 큐 점유 1. 여러 영상 중 어느 것에 마스크가 없었는지는 알 수 없다 |
| C. 같은 문구의 경고를 큐에서 합치기(공통 모듈) | 모든 모듈에 적용된다. common 은 Lane A 소유다 |
| D. 경고 대신 반환 코드/보고 구조로 알리기 | ABI 변경이 필요하다 |

### 2.5 MC 계단 팬텀 경계 (기록만)

```
COLLMASK mc detected x0=0 y0=0 x1=79 y1=79 (air-based field pixels=5476, mask pixels=6400)
COLLMASK mc air-mask x0=3 y0=3 x1=76 y1=76
COLLMASK mc air-mask edge left   inside=13137.9 outside=10385.8 (input inside=17446.7 outside=10385.8)
COLLMASK mc air-mask edge right  inside=59.0    outside=413.7   (input inside=471.4   outside=413.7)
COLLMASK mc air-mask edge top    inside=1114.8  outside=1774.2  (input inside=2458.1  outside=1774.2)
COLLMASK mc air-mask edge bottom inside=1114.7  outside=1773.0  (input inside=2457.9  outside=1773.0)
```

- 값은 행·열 20–59 의 평균 DN 이다. "inside" 는 마스크 안 마지막 줄, "outside" 는 바로 바깥 줄이다.
- 보정 뒤 안쪽이 바깥쪽보다 어두운 변이 셋이다.
  - 오른쪽(25 cm 단): 59 대 414, 바깥이 7배 밝다.
  - 위·아래: 1,115 대 1,774
- 왼쪽(5 cm 단)만 안쪽이 밝다(13,138 대 10,386).
- 바깥 줄은 입력 그대로다(산란만 있는 값).
- 검출기가 대체값(전체 영상)을 낸 원인은 조사하지 않았다. 신뢰도 부족인지 넓이 비율인지 로그(spdlog)를 읽지 않았다.

## 3. 전체 검증 (`_verify.log`, 두 커밋의 작업 트리)

```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 617   ===POST_EXIT=0===
===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
E2E                  100% tests passed, 0 tests failed out of 26    ===E2E_EXIT=0===
```

- 610 → 617: 파서 시험 1건, e2e 6건.
- e2e 선택식(`-R "E2E|Pipeline"`)은 20 → 26 이다.

빌드 실패 기록:
- `_e2e1`·`_e2e2`: BUILD=0, 검출 결과로 실패. 설계를 바꿨다.
- `_e2e3`: BUILD=1, `InField` 선언 순서. 결과는 낡은 바이너리라 읽지 않았다.

## 4. 기준 귀속

- 정답 마스크: 장면을 만든 조건(`InField`)에서 직접 만든 마스크. 도우미를 거치지 않는다.
- 검출 비교: 같은 장면의 정답 사각형 `kField`.
- 경고 누적: 호출 전 `xpe_clear_alerts` 뒤 다른 모듈 alert 3개를 넣은 상태.
- 경계 밝기: 같은 실행의 입력 영상.

## 5. 미검증

- 검출기가 위쪽 변을 한 줄 바깥으로 내는 원인(Hough rho 양자화, Sobel 위치 등). 아래·왼쪽·오른쪽과 다른 이유는 보지 않았다.
- MC 팬텀에서 검출이 대체값으로 떨어진 원인
- 실제 조사야 가장자리(반그림자, 기울기)에서의 검출
- `xpe_detect_collimation` 이 받는 float32 영상 조건(정규화 여부)과 파이프라인의 실제 순서
- CI 러너 결과(§1b 수정이 main CI 에서 초록인지)

## 6. 잔여 위험

- 검출 한 줄 어긋남은 평균 7.7 DN 이지만, 그 줄 자체는 최대 2,073 DN 이 바뀐다(원래 두어야 할 산란 영역이 보정됨).
- 물체 안 강한 경계를 조사야 변으로 잡으면 조사야 일부가 마스크 밖이 된다. 그 영역은 보정되지 않고 산란원에서도 빠진다(평균 2,359 DN 차이).
- 경고 누적(2.4)으로 다른 모듈의 Warning 이 사라질 수 있다.
- 보정 뒤 조사야 경계 안쪽이 바깥보다 어두운 끊김이 MC 에서 최대 7배다(2.5). 표시 단계에서 눈에 띌 수 있다.
- 시험 데이터 경로는 CMake 가 소스 절대 경로(`XPE_GSVG_TEST_DATA`)로 넣는다. 빌드 폴더를 다른 기계로 옮기면 깨진다.

## Card Cross-Check

| 카드 요구 | 결과 |
|---|---|
| §1b (a) 줄바꿈 무관 + 못 찾으면 ASSERT | 1.2, 2.1 |
| §1b (b) 파서 CRLF 시험 고정, 못 받으면 수정 | 1.3 (이미 받음, 반증 포함) |
| 사각형 → 바이트 마스크 → `_masked` | 2.3 |
| 변환은 시험 쪽, gsvg 독립 | `MaskFromRect` (e2e 시험 파일) |
| inclusive/exclusive 확인, 경계 한 줄 고정 | 1.5–1.6 |
| 시험 1: 검출 마스크 = 정답 마스크 결과, 틀리면 차이 보고 | 1.7, 2.2 |
| 시험 2: 도우미 한 줄 줄이면 빨강 | 1.8 |
| 시험 3: 경고 누적, 큐 용량·넘침 인용, 선택지 | 1.9, 2.4 |
| 시험 4: MC 경계 밝기 기록 | 2.5 |
| BUILD_EXIT | 2, 3 |
