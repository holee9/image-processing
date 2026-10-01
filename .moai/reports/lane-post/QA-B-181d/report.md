# QA-B-181d — Codex #48 보류: 검증 전 float→int 변환 (#233)

Refs #233

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `noise_reduce`(bilateral `sigma_space`), `edge_enhance`(`radius`)의 float→int 변환 앞에 유한성 검사를 넣었고, 같은 모양의 `contrast_enhance`(`clip_limit`)도 찾아 고쳤다. |
| C2 | 범위 비교는 유한성 검사가 아니라는 것을 **빌드 설정별로 실측**했다: `/fp:precise` 에서는 NaN 이 기존 범위 검사 세 개를 모두 통과하고, 이 모듈이 쓰는 `/fp:fast` 에서는 우연히 거부된다. |
| C3 | post 모듈 6개의 float→int 변환을 행위 기준으로 다시 세어 표로 남겼다(33행). 공개 파라미터·표에서 오는 미검사 위치 4곳을 더 찾아 같이 고쳤다(`vg_pyramid_levels`, 비네트 gain, 스티칭 크기 추정, GSDF 휘도 표). 화소 값에서 오는 변환은 고치지 않았고 **결정이 필요하다**(6절). |
| C4 | 새 검사 각각을 약화하는 반증을 `/fp:fast`·`/fp:precise` 두 빌드에서 돌렸다. 한 빌드에서 빨개지지 않는 검사는 이유를 적었다(5절). |

## 2. 발견 — 범위 비교는 NaN 을 거르지 않고, 거르는지는 빌드 설정에 달려 있다

기존 검사의 모양 그대로 컴파일한 작은 프로그램(`fp_fast_vs_precise_nan_comparisons.txt`):

| 빌드 | NaN 입력에서 `r < 0.5f \|\| r > 10.0f` | `s <= 0.0f` | `c < 1.0f` | `std::isfinite(NaN)` | `+inf` 에서 `s <= 0`, `c < 1` |
|------|------|------|------|------|------|
| `/fp:precise` | 거부 안 함 | 거부 안 함 | 거부 안 함 | 0 | 거부 안 함 |
| `/fp:fast` | 거부함 | 거부함 | 거부함 | 0 | 거부 안 함 |
| `/arch:AVX2 /fp:fast` (enhance_basic 의 실제 플래그) | 거부함 | 거부함 | 거부함 | 0 | 거부 안 함 |

즉 이 모듈 안에서는 NaN 이 우연히 거부되던 것이고(`/fp:fast` 가 NaN 이 없다고 가정하는 비교로 컴파일), +∞ 는 `<= 0`·`< 1` 같은 하한 검사만 있는 곳을 어느 빌드에서나 통과한다. `std::isfinite` 는 두 설정 모두에서 NaN·∞ 를 올바르게 구분했으므로 그대로 쓴다. Codex #48 의 "NaN 은 모든 비교에서 거짓" 은 IEEE 의미론의 서술이고, 이 코드베이스에서는 그에 더해 **빌드 플래그가 바뀌면(예: `/fp:fast` 제거) 결과가 바뀐다**는 점이 이 검사를 명시적으로 두어야 하는 이유다.

## 3. 수정

### 3.1 Codex 가 지목한 두 곳과 같은 모양의 한 곳 (`enhance_basic`)

| 위치 | 수정 |
|------|------|
| `noise_reduce.cpp` bilateral 검증 | `sigma_space`·`sigma_range` 가 유한하고 > 0 이어야 한다(+∞, NaN 은 `INVALID_INPUT`). |
| `noise_reduce.cpp` `apply_bilateral` | 반경을 **변환 전에** 제한: `sigma ≥ maxRad/2` 이면 `maxRad`, 아니면 `ceil(2σ)`. 옛 순서(변환 뒤 제한)는 큰 σ 에서 정의되지 않은 변환이었고, `2σ` 는 `σ > FLT_MAX/2` 에서 float 가 넘친다. 큰 **유한** σ(`1e6`, `1e30`, `FLT_MAX`)는 이전처럼 허용되고 반경은 `maxRad` 가 된다. 동치 증명: `σ ≥ maxRad/2 ⇒ ceil(2σ) ≥ maxRad ⇒ 옛 코드도 maxRad`. |
| `edge_enhance.cpp` | `amount`·`radius` 는 유한 + 범위, `threshold` 는 NaN 거부(+∞ 는 "아무것도 선명하게 하지 않음" 이라 유효로 유지). |
| `contrast_enhance.cpp` (카드 밖에서 찾음) | `clip_limit` 은 유한 + ≥ 1. `clip_count` 는 float 로 `tile_area` 에서 제한한 뒤 변환(클립이 타일 면적 이상이면 어떤 빈도 잘리지 않으므로 결과는 같다). `+∞`·`1e30` 이 `static_cast<int>` 에 닿았다. |

`enhance_basic_api.h` 반환 코드 문서에 NaN·무한대를 적었다.

### 3.2 전수 조사에서 나온 네 곳

| 위치 | 입력 | 결함 | 수정 |
|------|------|------|------|
| `gsvg.cpp` `read_virtual_grid_config` | JSON `vg_pyramid_levels` | 정수 여부만 보고 `static_cast<int>` 변환. `1e10` 은 정수이고 int 가 아니며, 4..8 검사는 변환된 값에 대해 나중에 돌았다. | 변환 전에 `0..8` 범위 검사(`vg_pyramid_levels must be 0 or 4..8`). 1..3 은 이전처럼 체인이 거부. |
| `gsvg.cpp` 비네트 | `gainMap` float 배열 | `src * gain` 이 NaN(NaN 이득, 또는 `inf * 0`)이면 clamp 가 무동작이고 `uint16` 변환이 정의되지 않음. | 비네트가 켜진 호출에서 앞 `width*height` 항목 중 비유한이 있으면 쓰기 전에 `INVALID_INPUT`. 큰 **유한** gain 은 이전처럼 65535 로 클램프. |
| `ai.cpp` `xpe_stitch_estimate_size` | 부분 영상 너비 | float 추정치를 `uint32` 로 변환한 **뒤** 4096 으로 제한. 추정치가 `UINT32_MAX` 를 넘으면 x86-64 에서 감겨 값이 틀림(실측: 너비 2526451329 두 장 → 추정 4294967808 → **512** 보고, 정답은 한도 4096). | float 비교로 4096 에서 제한한 뒤 변환. |
| `presentation_lut.cpp` `xpe_gsdf_calibrate` | 휘도 측정값 배열 | 단조 검사(`v[i] < v[i-1]`)가 NaN 에서 거짓이라 통과 → `std::lround(NaN)` 변환. | 모든 항목 유한 검사, 아니면 `INVALID_INPUT`. |

`gsvg_api.h`, `display_api.h` 문서에 새 반환 조건을 적었다.

동작이 바뀌는 입력(모두 이전에는 정의되지 않은 변환이거나 쓰레기 출력): `sigma_space`/`sigma_range` 의 ±∞·NaN, `clip_limit` 의 +∞, `vg_pyramid_levels` 의 음수·8 초과(설정 시점에 거부; 이전에는 처리 시점), 비유한 gain(이전에 `inf` gain 이 `src>0` 에서 65535 로 나오던 경우 포함), GSDF 표의 ±∞·NaN.

## 4. 증거 — 재현(수정 전 빨강)

UB 라 값은 도구체인마다 다르므로 빨강은 "반환 코드가 `INVALID_INPUT` 이 아님"(또는 관측된 감김 값)으로 잡았다.

- `red_before_fix.txt` (enhance_basic, `/fp:fast`): `sigma_space=inf`, `threshold=NaN`, `clip_limit=inf`, `clip_limit=nan` 이 `OK(0)` 로 반환. 이 빌드에서 NaN `sigma_space`/`radius`/`amount` 는 이미 거부되어 빨갛지 않았다(2절). 큰 유한 σ 시험은 옛 코드에서도 초록 — 정의되지 않은 변환이 우연히 `radius=1` 을 낳아 결과가 정상으로 보이기 때문이다(변환 순서는 관측으로 잡히지 않는다; 5절 Gaps).
- `red_before_fix_census_sites.txt`: gsvg `vg_pyramid_levels` {1e10, -1e10, 4294967296, 1e300, 9} 가 초기화 `OK`, 비유한 gain 3종이 `OK` 이고 **출력 버퍼를 덮어씀**, ai 스티칭 추정 `512`(기대 4096), display NaN/∞ 휘도 표 5건이 `OK`.
- 수정 후: `green_after_fix_fast.txt`(enhance_basic `ExceptionGuard.*` 24/24), `green_after_fix_census_sites.txt`(gsvg 9, ai 3, display 1 전부 초록).

## 5. 증거 — 반증 (새 검사를 하나씩 약화, 유한성만 지움; 소스는 되돌려 바이트 동일함을 확인)

`arms_summary.txt`(enhance_basic, 두 빌드), `arms2_summary.txt`(나머지 네 곳). 모든 팔 `build_ok=True`.

| 약화한 검사 | `/fp:fast`(출하) | `/fp:precise`(엄격 IEEE) |
|-------------|------|------|
| bilateral `sigma_space` 유한성 | 빨강 (`Bilateral…NonFinite…`) | 빨강 |
| bilateral `sigma_range` 유한성 | **빨강 아님** | 빨강 (`Bilateral…NaNRangeSigma`) |
| edge `amount` 유한성 | **빨강 아님** | 빨강 |
| edge `radius` 유한성 | **빨강 아님** | 빨강 |
| edge `threshold` NaN | 빨강 | 빨강 |
| contrast `clip_limit` 유한성 | 빨강 | 빨강 |
| 새 코드 기준선 | 0 빨강 | 0 빨강 |
| gsvg 레벨 상한 / gsvg gain 유한 / ai 스티칭 제한 / display 휘도 유한 | 각각 해당 시험 하나 빨강 (`arms2_summary.txt`) | (이 네 모듈은 `/fp:fast` 를 쓰지 않아 한 구성만 실행) |

`/fp:fast` 에서 빨개지지 않는 세 검사(`sigma_range`, `amount`, `radius`)는 **이 빌드에서는 컴파일러의 비교가 이미 NaN 을 거부해** 약화해도 결과가 같다는 뜻이다(2절). 그 검사의 효과는 `/fp:precise` 팔에서 입증됐고, 그 빌드의 기준선(새 코드)은 초록이다. 즉 이 검사들은 현재 출하 빌드에서는 중복이고 빌드 플래그가 바뀌면 필요해진다.

## 6. 전수 조사 (`census_float_to_int.tsv`, 33행)

**검색 명령과 대조군** (`search_float_to_int_census.txt`): 수정 전 커밋 `547fa8cd` 에서 모듈 6개(`src`·`include`)를 `git grep -n -E "static_cast<(int|int8_t|…|ptrdiff_t)>\(|\((int|unsigned|size_t|long)\)\s*[A-Za-z_(]|lround|llround|lrint"` 로 훑었다. 후보 줄: enhance_basic 75, enhance_advanced 72, gsvg 125, ai 55, display 5, dicom 27. **대조군**: 같은 검색이 Codex 가 지목한 `noise_reduce.cpp:185`, `edge_enhance.cpp:100` 과 내가 찾은 `contrast_enhance.cpp:80` 을 수정 전 트리에서 잡는다. 후보 줄 대부분은 정수 피연산자(인덱스·크기·열거형) 캐스트라 제외했다. 행위 기준(피연산자가 부동소수)으로 읽은 결과가 33행이다 — 세 감사 에이전트가 모듈별로 읽고 분류했고(`census181d_*.tsv` 원본의 병합), PARAM·TABLE 11행 중 수정한 7곳은 내가 코드를 직접 읽고 시험으로 입증했고, 가드가 있던 4행(`collimation_detect.cpp:140`, `gsvg.cpp:262`, `virtual_grid.cpp:422·429`)은 에이전트의 읽기를 받았다(`virtual_grid` 두 곳은 QA-B-181c 에서 내가 쓴 코드).

| 출처 \ 수정 전 가드 | 유한+상한 / 제한된 float / 상수 | 범위 비교만 | 변환 뒤 클램프 | 없음 | 합 |
|---|---|---|---|---|---|
| 공개 파라미터(PARAM) | 2 | 3 (contrast:80, edge:100, gsvg:295) | 2 (noise:185, ai:796) | 1 (gsvg:267) | 8 |
| 표(TABLE) | 2 | 1 (display:264) | 0 | 0 | 3 |
| 화소(PIXEL) | 0 | 4 | 3 | 0 | 7 |
| 내부(INTERNAL) | 8 | 0 | 6 | 1 | 15 |
| 합 | 12 | 8 | 11 | 2 | 33 |

(표는 `census_float_to_int.tsv` 에서 스크립트로 센 값이고 합계가 33행과 일치한다.)

수정 후 상태: PARAM·TABLE 11행 중 가드가 있던 4행을 뺀 미검사 7곳을 **전부 고쳤다**(3절; 비네트 gain 은 `gsvg.cpp:295` 의 한 위치이고 표의 `FIXED 181d` 가 7행이다). 나머지 미결:

### 미결 — 화소 값에서 오는 변환 (고치지 않음, 결정 필요)

| 위치 | 변환 | 가드 |
|------|------|------|
| `enhance_basic/contrast_enhance.cpp:71`, `:111` | `(v - tmin) * scale` → 빈 인덱스 | 변환 **뒤** 클램프. NaN·∞ 화소, 범위가 `inf` 로 넘칠 때의 `inf*0` |
| `enhance_advanced/detail/hough_transform.cpp:55` | `static_cast<int>(magnitude)` | `magnitude < 3` 비교만(NaN 통과), ∞·2^31 이상은 정의되지 않음 |
| `display/display_internal.h:67` (`xpe_round_to_int`) ← `presentation_lut.cpp:46` | `roundf(v)` → int32 | `xpe_clamp` 가 비교만(NaN 통과) |
| 같은 함수 ← `modality_lut.cpp:62` | 같음 | 변환 뒤 클램프 |
| `gsvg/gsvg.cpp:497` | `std::clamp(std::round(img[i]), 0, 65535)` → uint16 | `clamp(NaN)` 은 NaN |
| `gsvg/grid_dwt.cpp:511` | 같은 모양 | 입력이 uint16 이라 NaN 은 기대되지 않음 |

이들은 표준상 정의되지 않은 변환이지만 x86-64 에서는 `INT_MIN`(0x80000000)으로 포화된 뒤 클램프되어, 이 도구체인의 결과는 "범위 안으로 들어온 값"이다(관측한 것이 아니라 코드 읽기와 이전 카드의 같은 부류 관측에서 온 추론 — **이 카드에서 NaN 화소로 실행해 보지 않았다**). 고치려면 호출마다 비교·선택이 추가되는 열 루프(대비 향상 빈 계산, 3072² 화소)라 성능 예산(contrast 20 ms 급)과 NaN 화소의 의미(0 으로 보낼지, 오류로 거부할지)를 정해야 한다. 이 카드는 요청대로 **세어서 표로 남기고**, 결정은 리더에게 넘긴다.

### 내부(INTERNAL) 중 "변환 뒤 클램프"

`hough_transform.cpp:68, 159, 274-277` 은 인덱스·격자 크기에서 오는 유한한 값(`rho`, `pos`)이라 NaN 이 오지 않는다고 에이전트가 읽었고 나는 일부만 확인했다(Gaps). `hough_transform.cpp:29`(`diag/rhoStep_`)는 차원이 약 1.5e9 를 넘을 때만 넘친다.

## 7. 전체

| 구성 | 결과 |
|------|------|
| ci-post 전체 ctest (`after_full_ci_post_ctest.txt`) | `100% tests passed, 0 tests failed out of 1068` (QA-B-181c 의 1059 에서 +9) |
| ci-ai 전체 ctest, `XPE_AI_EXPECT_ONNX=1` (`after_full_ci_ai_ctest.txt`) | `100% tests passed, 0 tests failed out of 376` (375 에서 +1) |
| 두 빌드 로그 `warning C` | 0건 |
| 시험 후 남은 `xpe_ai_worker.exe` | 0개 |

## 8. Gaps (미검증)

- 전수 조사의 분류는 세 에이전트가 `git show`/`git grep` 로 읽은 결과다. 수정한 7곳은 내가 직접 읽고 시험으로 입증했지만, **가드가 있다고 분류된 4행, INTERNAL 15행, PIXEL 7행은 에이전트의 읽기를 그대로 받았다**(내가 열어 보지 않은 것이 있다).
- 정규식 검색은 명시적 캐스트·`lround` 류만 본다. 암시적 float→int 대입은 빌드가 `/W4 /WX` 에서 경고 0건이라 없다고 읽었지만, 이는 경고 수에 기댄 간접 근거이고 직접 점검하지 않았다. 함수형 캐스트 `int(x)` 는 gsvg 에서만 별도 검색(0건)했다. 후보 줄 수에는 주석 줄이 일부 섞여 있다(에이전트별 집계 기준이 약간 다르다).
- 큰 유한 `sigma_space`·큰 `clip_limit` 이 변환 순서를 바꿔도 결과가 같다는 것은 관측 가능한 차이가 없어 시험으로 입증하지 못했다(옛 코드의 정의되지 않은 변환이 우연히 정상 결과를 낸다). 동치는 코드 논증이다.
- `sigma_space`·`sigma_range` 가 아주 작은 양수(예: 1e-30)일 때의 출력은 이 카드에서 보지 않았다.
- 화소 기반 변환(6절)의 NaN 화소 동작은 실행해 보지 않았다.
- gsvg `vg_pyramid_levels` 상한 검사는 init 만 시험했고 처리 경로의 4..8 검사는 이전 시험이 계속 덮는다.

## 9. Residual-risk (잔여 위험)

- 설정 오류가 이른 시점(`init`)에 `CONFIG_INVALID` 로 나오게 된 입력이 있다(`vg_pyramid_levels` 9 이상·음수): 그것을 처리 시점 거부에 기대던 호출자가 있다면 반환 시점이 달라진다.
- `/fp:fast` 로 빌드된 enhance_basic 의 `isfinite` 는 이 컴파일러에서 시험했지만(2절 프로그램), 다른 컴파일러에서 `-ffast-math` 류가 `isfinite` 를 접을 수 있다. 시험이 그런 빌드에서 빨개지는 것이 신호다.
- 화소 기반 변환이 남아 있는 동안 NaN·∞ 화소가 들어오는 입력 경로(검출기 이상값, 앞 단계 산출)가 있으면 정의되지 않은 변환에 닿는다. 이 경로가 실제로 닿는지는 정해야 할 결정과 함께 확인이 필요하다.
