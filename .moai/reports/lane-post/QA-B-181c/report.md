# QA-B-181c — Codex #37 보류 4건 (#233)

Refs #233

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `xpe_contrast_enhance` 는 영상 가장자리를 넘어서 시작하는 빈 타일에서 화소를 읽지 않는다. 읽기가 접근 위반으로 드러나는 환경(보호 페이지)에서 수정 전 빨강, 수정 후 초록. |
| C2 | 올림 나눗셈 4곳(`virtual_grid.cpp` 의 `cw`/`ch`, 세 모듈의 `parallel_rows.h` `ForRows`)을 나눗셈+나머지로 바꾸고, 반환 범위(`f`, 스레드 수)를 검증했다. 181b 보고서 5절의 "수정 후 0건"은 틀린 기록이었고 정정한다. |
| C3 | 출력 해시 고정 시험을 환경 독립 시험으로 바꿨다(타일 경계를 같은 실행 안의 64비트 옛 식과 비교 + 영상 성질). 고정 해시는 시험에서 뺐다. |
| C4 | `GaussTaps` 의 float→int 변환과 `2*r+1` 을 검사 뒤에 한다. 반경 상한을 두었고 넘는 입력은 거부한다. |

## 2. 항목 1 — 빈 가장자리 타일의 범위 밖 읽기 (기존 결함)

원인: 타일 수 `n` 에 타일 크기 `ceil(size/n)` 를 쓰면 마지막 타일들이 영상 밖에서 시작할 수 있다(11×11, 5×5: 타일 크기 3, 다섯 번째 행 타일 시작 y=12 > 10). 시작은 가장자리로 잘려 빈 범위가 되는데, `build_tile_lut` 가 범위를 보기 전에 `px[y0*w+x0]` 를 읽었다(버퍼 한 칸 뒤).

수정:
- `build_tile_lut` 맨 앞에서 빈 타일(`x0>=x1 || y0>=y1`)을 중립 LUT(0.5)로 돌려보내고 화소를 읽지 않는다.
- 타일 경계와 화소→타일 배정을 헤더 인라인 `xpe_tile_bounds`, `xpe_tile_of` 로 옮겨(64비트 곱, 가장자리로 자름) 함수와 시험이 같은 코드를 본다.
- 빈 타일은 어떤 화소도 배정받지 않는다(`xpe_tile_of(pos)` 가 고르는 타일은 그 화소를 포함하며 비어 있지 않다 — 시험 `TileBoundsMatchTheOldFormulaAndPartitionTheAxis` 가 8개 크기 × 여러 타일 수에서 모든 화소로 단언). 그래서 중립 LUT 는 출력에 영향을 주지 않는다.

증거:
- 수정 전 (`red_before_fix_empty_tile.txt`): `ContrastEnhanceNeverReadsPastThePixelsForAnEmptyEdgeTile` 가 `the 11x11 / 5x5 case read past the buffer` 로 빨강(버퍼를 PAGE_NOACCESS 페이지 직전에 두고 `__try/__except` 로 접근 위반을 잡음). 나머지 16개 시험은 초록.
- 수정 후 (`green_after_fix_exception_guard.txt`): 17/17 초록. 이 시험은 11×11/5×5 뒤에 8×8 가지 크기 × 타일 수 조합 전부(빈 타일이 있는 조합이 1개 이상임을 단언)를 같은 방식으로 돈다.
- 반증 (`arm_empty_tile_guard_removed_sweep.txt`): 빈 타일 가드를 런타임에서만 꺼진 조건(`&& x1 < 0`)으로 바꾸자(경고 없이 컴파일, 되돌림 확인) 조합 시험에서 **접근 위반 287건**.

## 3. 항목 2 — 남은 올림 나눗셈과 보고서 정정

### 3.1 정정

181b 보고서 5절은 "A. `(x + y - 1) / y` 수정 후 0건"이라 썼다. 그때 이 모양은 **실제 소스에 4곳 더 있었다**(`search_ceil_div_before.txt`):

| 위치 | 식 |
|------|----|
| `modules/enhance_basic/src/parallel_rows.h:60` | `(rows + threads - 1) / threads` |
| `modules/gsvg/src/parallel_rows.h:60` | 같음 |
| `modules/enhance_advanced/src/detail/parallel_rows.h:60` | 같음 |
| `modules/gsvg/src/virtual_grid.cpp:734` | `(width + f - 1) / f`, `(height + f - 1) / f` |

왜 놓쳤나: **181b 에서 쓴 검색 명령은 저장하지 않았다**(그래서 정규식의 어느 부분이 틀렸는지는 확인할 수 없다). 이번 정규식은 4곳 중 세 곳이 헤더(`.h`)이고 한 곳이 한 글자 변수 `f` 라는 점에서 "`.cpp` 만 검색했거나 식별자 길이를 가정한" 부류가 의심되지만, 이것은 추정이다. 이번에는 명령과 결과를 저장했고 **대조군**을 붙였다: 같은 정규식을 181b 직전 커밋(`213d4dbd~1`)에 돌리면 그때의 6곳(contrast 두 줄 포함)을 찾아낸다(`search_ceil_div_after.txt` 첫 구간; 정정 대상 4곳은 `search_ceil_div_before.txt`). 수정 후 작업 트리에서 모든 모듈의 `src/`·`include/`(주석 줄 제외)를 같은 정규식과 세 가지 다른 모양(`x + (y - 1)`, 리터럴 `(x + k) / k`·`>> n`, `-(-a / b)`)으로 훑어 **모두 0건**(`search_ceil_div_after.txt`).

### 3.2 수정

- `parallel_rows.h` ×3 (`ForRows`): 나눗셈+나머지로 `band`, `t * band`·`y0 + band` 는 64비트로 계산한 뒤 `rows` 로 자름, `threads > rows` 이면 `rows` 로 줄임(호출자가 준 스레드 수를 믿지 않음 — 이전에도 `ResolveThreads` 가 줄였지만 `ForRows` 는 직접 호출될 수 있다). 세 복사본은 동일한 변경이다. 또 이 헤더를 `windows.h` 가 있는 번역 단위에서 쓸 수 있도록 `std::min`/`std::max` 를 괄호로 감쌌다(시험이 `windows.h` 를 포함하는 gsvg 에서 옛 헤더는 컴파일되지 않았다).
- `virtual_grid.cpp`: `CeilDivInt`(헤더 인라인), 축소 계수 `f` 는 계산 전에 검증한다.
  - 호출자가 준 `reductionFactor` > `kMaxReductionFactor`(2^20) → `fail("reduction factor above the supported maximum")`.
  - 유도한 계수 `floor(0.5*sMin/pitchCm)` 가 NaN 이거나 2^20 초과(픽셀 피치가 커널 폭보다 터무니없이 작거나, 커널 항이 하나도 없어 `sMin=1e300` 인 경우) → `fail("derived reduction factor is out of range …")`.
  - 상한 2^20 근거: 계수는 블록 한 변의 화소 수이고, 2^20 은 쓰이는 가장 넓은 검출기 축(약 14000 화소)의 약 75배라 정상 설정은 닿지 않는다. 이 상한이 있으면 `2*f+1`(최소 필터 창)과 블록 색인이 `int` 에서 멀다.
  - 거부의 반환: 기존 가상 격자 거부 경로와 같다 — 알림 + 원본 복원 + `XPE_ERR_CONFIG_INVALID`(`gsvg.cpp:477`).

### 3.3 증거

- 수정 전 (`red_before_fix_forrows.txt`): 세 모듈 모두 `ForRowsPartitionsRowsNearIntMaxWithoutOverflow` 빨강. 범위 끝이 `INT32_MAX` 대신 `-1073741824`, `-715827882`, `-536870911`, … (스레드 2·3·4·7·64) 로 나왔다(오버플로가 감긴 값. 정의되지 않은 동작이므로 이 값은 이 도구체인에서 관측한 것이고 보장이 아니다). `ForRowsNeverPlansMoreBandsThanRows` 는 옛 코드에서도 초록이다(옛 코드는 `break` 로 처리) — 대조군.
- 수정 전 (`red_before_wiring_gsvg.txt`, 순수 함수는 정의만 하고 배선 전): 배선 시험 3개가 빨강 — `ScatterEstimateRefusesAKernelWiderThanTheLimit`(옛 코드는 폭 1.4×한도 이상의 커널로도 추정을 반환), `AReductionFactorAboveTheLimitIsRefused`(2^20+1 을 수락, **`reductionFactor = INT32_MAX` 에서는 접근 위반 0xC0000005**), `ADerivedReductionFactorThatCannotBeRepresentedIsRefused`(피치 1e-9 mm 를 수락).
- 수정 후 (`green_after_wiring_gsvg.txt`): 6/6 초록. 한도 값 자체(`kMaxReductionFactor`)는 수락됨도 시험.
- 반증 (`arm_old_ceil_div_gsvg.txt`): `CeilDivInt` 를 옛 식으로 되돌리면 `CeilDivIntMatchesTheWideReferenceNearIntMax` 가 빨강(`-1`, `0` 등, 되돌림 확인).

## 4. 항목 3 — 해시 시험의 이식성

`ContrastEnhanceOutputIsBitIdenticalToItsPreRewriteBehaviour`(FNV-1a 고정값 5개)를 삭제하고 아래로 바꿨다:

- `TileBoundsMatchTheOldFormulaAndPartitionTheAxis`: 같은 실행 안에서 `xpe_tile_bounds` 를 64비트로 계산한 옛 식과 비교하고, 비어 있지 않은 타일이 축을 빈틈없이 덮으며 모든 화소의 타일이 그 화소를 포함하는지 확인(크기 4~3072, 타일 수 2~size/2).
- `TileBoundsStayInRangeNearIntMax`: `INT32_MAX` 근처와 가장자리 훨씬 너머의 타일.
- `ContrastEnhanceKeepsItsImagePropertiesOnEveryTileShape`: 7개 모양(11×11/5×5, 3072²/8×8, 3070×2051/7×9 포함)에서 출력이 유한하고 입력 값 범위 안에 있으며, 같은 입력은 같은 비트(실행 간)이고, 한 타일 안에서는 밝은 입력이 어두운 출력을 받지 않는다(타일별 단조 LUT).

기록으로만 남기는 이전 값(이 기계·이 컴파일러에서 수정 전 코드로 얻은 FNV-1a): 101×67/8×8 `0x229DE8749741D30D`, 64×64/4×4 `0x8FBE1CD7F07C2A45`, 17×9/2×2 `0x8BEBE1B1FBE154F1`, 5×8/2×4 `0xD5831760FDB2011F`, 3072²/8×8 `0x1EDF0E316336B714`. 시험에는 없다.

## 5. 항목 4 — GaussTaps

- `GaussKernelRadius(sigmaPx, radius)`: sigma 가 양의 유한수가 아니면(0, 음수, NaN, ∞) 거부, `ceil(4*sigma)` 가 `kMaxGaussRadius`(2^20) 를 넘으면 거부. 변환은 검사 뒤에만 한다. `2*r+1 ≤ 2^21+1` 이라 넘치지 않는다.
- 상한 근거(자원): 탭 벡터는 `2r+1` 개의 double, 한도에서 16 MiB. 표의 가장 넓은 항이 수 cm 이고 가장 가는 피치에서도 수백 화소이므로 2^20 화소 반경은 단위를 잘못 준 값이지 검출기가 아니다. 합성 표(s = 0.9~5.9 cm)에서 가장 좁은 항이 한도를 넘으려면 피치가 약 3.4e-6 cm(34 nm) 미만이어야 한다.
- 거부의 전달: `GaussTaps` 가 빈 벡터를 돌려주고 `ScatterEstimate` 가 빈 결과(`{}`)를 돌려준다 — 이 함수의 기존 실패 신호(두께가 표 범위를 넘을 때와 같은 신호). 호출한 쪽 `RunVirtualGrid` 는 `fail("scatter estimate failed (thickness above the table range, or a kernel width out of range)")` 로 끝내고, 위의 `XPE_ERR_CONFIG_INVALID` 경로를 탄다. 메시지는 이전의 "estimated thickness above the table range" 에서 바뀌었다(이 메시지를 인용하는 시험·문서는 검색에서 0건).
- 증거: 위 3.3 의 `ScatterEstimateRefusesAKernelWiderThanTheLimit`, `GaussKernelRadiusRejectsWhatCannotBeAKernel`(경계값 한도 / 한도×1.0001, NaN, ∞, 1e300, 음수, 0; 거부 시 반경 불변).

## 6. 전체

| 구성 | 결과 |
|------|------|
| ci-post 전체 ctest (`after_full_ci_post_ctest.txt`) | `100% tests passed, 0 tests failed out of 1059` (QA-B-183 의 1044 에서 순증 15: 신규 시험 16개, 삭제 1개) |
| ci-ai 전체 ctest, `XPE_AI_EXPECT_ONNX=1` (`after_full_ci_ai_ctest.txt`) | `100% tests passed, 0 tests failed out of 375` |
| 두 빌드 로그 `warning C` | 0건 |
| 시험 후 남은 `xpe_ai_worker.exe` | 0개 |

참고: ci-post 첫 전체 실행에서 `WorkerSupervisor.AStalledWorker…`(2.98 s)와 `…AnswersWithAnError…`(7.53 s) 2건이 실패했다(`first_full_ci_post_ctest_2_supervisor_timeouts.txt`). 이 변경과 무관한 워커 기동 시간 초과 모양이고 QA-B-183 7절에서 이미 기록한 부류와 같다. 그 16개를 다시 돌리자 16/16, 전체를 다시 돌리자 1059/1059.

## 7. Gaps (미검증)

- `xpe_contrast_enhance` 를 `w` 가 `INT32_MAX` 근처인 실제 영상으로 끝까지 돌린 시험은 없다(영상 전체를 읽어야 해 수십 GB). 큰 값은 헬퍼 시험과 코드 읽기로만 뒷받침된다.
- `ForRows` 시험은 본문이 아무것도 하지 않는 호출로 `INT32_MAX` 행을 분할한다. 실제 모듈 함수가 그 크기로 이 경로에 닿는지는 보지 않았다.
- 같은 형태의 다른 오버플로(`MinFilter1D` 의 `n + w`·`2*r+1`, 영상 크기 곱 등)는 이 카드의 범위가 아니다. `f` 가 2^20 이하라 `2*r+1` 은 안전하지만 `n + w` 는 `n` 이 `INT32_MAX` 근처이면 여전히 넘칠 수 있다(검사하지 않음).
- 정규식 검색은 이름·모양 한 축이다. 매크로·람다·템플릿으로 감싼 올림 나눗셈, 뺄셈 모양이 아닌 변형은 못 찾는다. `float→int` 변환 후보 목록(`search_ceil_div_after.txt` 끝: display, enhance_advanced `hough_transform`·`mfp_scalar`, preprocess `std::floor(pos+0.5)`, enhance_basic `ceil(2*sigma)` 둘)은 **후보일 뿐 결함으로 확인하지 않았다**(enhance_basic 둘은 181b 에서 시그마 범위 검증 뒤라고 읽었다).
- 해시 대신 쓴 성질 시험은 "모든 환경에서 참인 것"만 단언한다. 영상 값 자체의 회귀(알고리즘이 조용히 다른 값을 내는 것)는 잡지 못한다. 그 목적은 이 카드가 아니라 허용 오차 기반 기준 구현 시험이 필요하다.
- gsvg 의 `reductionFactor` 거부는 기존 `RunVirtualGrid` 시험 외의 호출자(공개 API 경로)에서 JSON 으로 설정할 수 있는지 확인하지 않았다(`reductionFactor` 는 `VgSwitches` 이고 `gsvg.cpp` 는 기본 `VgSwitches{}` 를 넘긴다 — 코드 읽기).

## 8. Residual-risk (잔여 위험)

- 한도(2^20)는 근거를 적은 자원 상한이지 물리 한계가 아니다. 더 넓은 커널이 정당한 장치가 생기면 이 상수를 올려야 한다(헤더 한 곳).
- `DerivedReductionFactor` 가 0 을 돌려주는 경우를 이제 거부한다. 이전에는 커널 항이 없으면(`sMin=1e300`) 정의되지 않은 변환을 거쳐 우연히 `f=1` 로 돌았을 수 있다. 그런 표가 실사용에 있었다면 동작이 "조용히 통과"에서 "거부"로 바뀐다. 제품 표 로더는 항이 없는 표를 이미 거부하는지 확인하지 않았다.
- 빈 타일의 중립 LUT 는 도달하지 않는다는 성질(모든 화소는 비어 있지 않은 타일을 가진다)에 기댄다. 그 성질은 시험으로 단언했지만 `xpe_tile_of` 를 바꾸는 후속 변경이 깨뜨리면 중립 값 0.5 가 출력에 섞일 수 있다.

## 9. 변경 파일

- `modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h` — `xpe_tile_bounds`, `xpe_tile_of`
- `modules/enhance_basic/src/contrast_enhance.cpp` — 빈 타일 처리, 헬퍼 사용
- `modules/{enhance_basic/src,gsvg/src,enhance_advanced/src/detail}/parallel_rows.h` — `ForRows`
- `modules/gsvg/src/virtual_grid.{h,cpp}` — `CeilDivInt`, `DerivedReductionFactor`, `GaussKernelRadius`, 한도 상수, `RunVirtualGrid`/`GaussTaps`/`ScatterEstimate`
- 시험: `modules/enhance_basic/tests/test_exception_guard.cpp`, `modules/gsvg/tests/test_exception_guard.cpp`, `modules/gsvg/tests/test_virtual_grid.cpp`, `modules/enhance_advanced/tests/test_exception_guard.cpp`
