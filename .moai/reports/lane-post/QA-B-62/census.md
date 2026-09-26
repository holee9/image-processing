# QA-B-62 — 소비 키 전수표 (정적 census, 진행 중)

**상태**: 정적 census 완료, 런타임 단언 작성 중. 막힌 곳 없음.
**작성 시점 HEAD**: `b45ca28`

> 이 표는 **코드에서 센 것**이다. 문서·주석·B-60 표를 옮겨 적지 않았다.
> 각 행의 "소비처" 는 파싱된 값이 실제로 **읽히는** 지점이며, 없으면 `없음` 이다.
> **정적 census 는 후보를 만들 뿐 결론이 아니다** — 런타임 단언이 결론을 준다.

## 0. config 를 받는 진입점 — 9개 (헤더에서 기계로 셈)

`enhance_basic`·`display`·`dicom` 의 공개 함수는 config JSON 인자를 받지 않는다.

| 모듈 | 진입점 |
|---|---|
| enhance_advanced | `xpe_enhance_advanced_init` · `xpe_multiscale_process` · `xpe_fractional_process` · `xpe_detect_collimation` |
| ai | `xpe_ai_init` · `xpe_stitch_images` · `xpe_bone_suppress` · `xpe_dl_denoise` |
| gsvg | `xpe_gsvg_init` |

## 1. 소비 키별 정적 추적

### 1.1 `xpe_fractional_process` — **후보 A (가장 큼)**

| 키 | 파서 처리 | 소비처 | 판단 |
|---|---|---|---|
| `iterations` | clamp [1, MAX] | `fractional_process.cpp:119` 반복 루프 | 도달 |
| `step_size` | clamp [0.01, 1.0] | **없음** — `fractional_process.cpp:132` 의 debug 로그 문자열뿐 | **후보** |
| `safety` (중첩) | SAF-100 금지 키 검사 | 거절 기전 (다른 축) | 해당 없음 |

`step_size` 는 구조적으로 소비가 불가능하다: 값이 전달될 그릇인
`FractionalConfig` 에 **필드 자체가 없다**(`detail/fractional_derivative.h:34-35`,
멤버는 `float order` 하나). `fractional_process.cpp:113` 은 `fracConfig.order` 만 채운다.

**이것이 B-60·B-61 잔여 위험의 실물이다.** `step_size` 는 B-61 의 알려진 키 목록에
있으므로 경고가 **울리지 않는다**. 이름도 타입도 맞고, 파서가 읽고 클램프까지 하는데,
결과에 닿지 않는다.

### 1.2 `xpe_multiscale_process`

| 키 | 파서 처리 | 소비처 | 판단 |
|---|---|---|---|
| `num_levels` | clamp | `mfp_scalar.cpp:94~166` 피라미드 깊이 | 도달 |
| `levels` (레거시) | clamp, **`num_levels` 뒤에 기록** | 같은 출력 변수 | **후보 B** — 둘 다 주면 `levels` 가 `num_levels` 를 조용히 덮는다 |
| `edge_gain` | clamp [0,5] | `mfp_scalar.cpp:183` (`level == 0`) | 조건부 |
| `texture_gain` | clamp [0,5] | `mfp_scalar.cpp:185` (중간 레벨) | **후보 C** — 아래 |
| `flat_gain` | clamp [0,5] | `mfp_scalar.cpp:181` (`level == numLevels-2`) | 도달 |
| `noise_threshold` | clamp [0,50] | `mfp_scalar.cpp:191` | 도달 |

**후보 C — 레벨 수에 따라 게인이 사라진다.** `mfp_scalar.cpp:180-185` 의 분기는
`numLevels` 에 의존한다:

- `num_levels = 2` → 루프가 `level = 0` 한 번, `numLevels-2 == 0` 이라 **첫 분기가 이겨**
  `flat_gain` 만 쓰인다. `edge_gain` · `texture_gain` **둘 다 무효**.
- `num_levels = 3` → `level = 1, 0`. `texture_gain` **무효**.
- `num_levels >= 4` → 셋 다 도달.

### 1.3 `xpe_detect_collimation`

| 키 | 파서 처리 | 소비처 | 판단 |
|---|---|---|---|
| `sensitivity` | clamp [0,1] | `collimation_detect.cpp:134` (theta step) · `:181` (신뢰도 임계) | 도달 |
| `min_area_ratio` | clamp [0.01,1] | `:210` 면적 비율 게이트 | 도달 |
| `border_margin` | clamp [0,64] | `:198-201` — **검출 성공 경로에서만** | 조건부 (B-58 이 걸린 폴백 경로) |

### 1.4 `xpe_ai_init` — **후보 D**

| 키 | 파서 처리 | 소비처 | 판단 |
|---|---|---|---|
| `execution_provider` | 5개 문자열 → enum | `ai.cpp:348` **로그 줄뿐** | **후보** |
| `timeout_ms` | 정수 | `ai.cpp:349` **로그 줄뿐** | **후보** |
| `confidence_threshold` | float 저장 | **리포지토리 전체에 읽는 코드 0곳** | **후보 (가장 명확)** |
| `fallback_mode` | atomic 저장 | 읽는 코드 0곳 (`:415` 는 주석, `:649` 는 setter) | **후보** |

`confidence_threshold` 는 getter 도 없다 — 쓰기 전용 필드다.

**주의 — stub 경계.** ai 의 추론 경로는 stub 이다. 위 넷이 출력에 닿지 않는 것을
"stub 이라서" 로 읽으면 안 된다: `confidence_threshold`·`fallback_mode` 는 **stub 여부와
무관하게 읽는 코드가 정적으로 0곳**이다. 이것은 런타임 관측이 아니라 코드 사실이다.

### 1.5 `xpe_stitch_images` · `xpe_bone_suppress` · `xpe_dl_denoise` — **후보 E**

셋 다 `configJsonOrNull` 을 `(void)` 캐스트로 버린다(각 함수 본문). 파싱 자체가 없다.
B-60 이 경고를 배선하지 않은 진입점이므로 호출자에게는 **아무 신호도 없다.**

### 1.6 `xpe_gsvg_init`

| 키 | 파서 처리 | 소비처 | 판단 |
|---|---|---|---|
| `vignette_correction` | bool | `gsvg.cpp:330` — **`gainMap != nullptr` 일 때만** | 조건부 (B-59 가 짚은 기본값 함정) |
| `grid_suppression` | bool | `gsvg.cpp:338` | 도달 |

### 1.7 `xpe_enhance_advanced_init`

B-60 판정대로 config 를 통째로 버린다((b) 문서화로 결정됨). 새 발견 아님.

## 2. 죽은 파서 2개 (부수 발견)

| 파서 | 읽는 키 | 호출처 |
|---|---|---|
| `MfpConfig::fromJson` (`mfp_scalar.cpp:16`) | `mfp.{num_levels,edge_gain,texture_gain,flat_gain,noise_threshold}` | **0곳** (테스트 포함) |
| `FractionalConfig::fromJson` (`fractional_derivative.cpp:101`) | `order`, SAF-100 금지 키 | **0곳** (테스트 포함) |

살아 있는 파서와 **클램프 범위가 다르다** — `MfpConfig::fromJson` 은 `num_levels` 를
`[1,6]` 으로, 살아 있는 `parse_mfp_config` 는 `[XPE_MFP_MIN_LEVELS, XPE_MFP_MAX_LEVELS]`
로 자른다. 헤더만 읽는 사람은 죽은 쪽을 계약으로 읽게 된다.

## 3. 다음 단계

런타임 단언으로 후보 A~E 를 확인한다. 하나만 바꾸고 나머지를 고정, 출력 이동을 요구.
임계값·픽스처 바닥·도달 증거는 `gate.md` 에 적는다.
