# QA-B-78 (#162) 게이트 보고서 — config 값 3건은 지금도 출력에 닿지 않는가

**카드**: QA-B-78 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `fbd20cc`)
**코드·시험 변경 없음, 커밋 없음.** `#162` 에 사실 코멘트를 달았다.
**BUILD_EXIT**: `BUILD=0` / `EXIT=0` (`ConfigValueDependency.*` 8/8 통과, `_run.log`).

---

## 1. 주장

| # | 키 | 지금도 닿지 않는가 | 실행 측정 (두 극단) | 대조군 | SPEC 요구 | 설계 문서(SDD) |
|---|---|---|---|---|---|---|
| 1 | `xpe_fractional_process` `step_size` | **예** — 담길 필드가 없다 | 0.01 대 1.0 → maxdiff **0.000000000** | `iterations` 1 대 3 → **494.17** | **없음** | **있음** — 0.01~1.0, 기본 0.25, 의미 설명 없음 |
| 2 | `xpe_multiscale_process` `levels` 와 `num_levels` 동시 지정 | **예** — `levels` 가 나중에 같은 변수에 쓰인다 | "둘 다" = "`levels`만" (단언 통과, 수치 미출력) | `num_levels` 단독이 출력을 움직임 (단언 통과) | **없음** (REQ-ADV-010 은 "기본 3~4 레벨"만) | `levels` 만 있음(2~8). **`num_levels` 는 SDD 에 없음** |
| 3 | 같은 함수의 레벨별 게인 | **예** — 레벨 3 에서 `texture_gain`, 레벨 2 에서 `edge_gain`·`texture_gain` 이 쓰이지 않는다 | tex@3 **0.000000**, edge@2 **0.000000**, tex@2 **0.000000** | 레벨 4 에서 두 게인이 모두 움직임 (단언 통과). 기본 레벨 4 에서 edge 150.91, texture 56.20 | **없음** (REQ-ADV-010 은 "대역별 계수"만) | 게인 3개 + `levels` 2~8 이 있음. 레벨 수와 게인의 관계는 적혀 있지 않음 |

문턱 = 0.080585 (기준선 동적 범위에 대한 상대값, 런타임 산출).

## 2. 코드 경로 — 파싱 → 클램프 → 소비 (현재 HEAD)

### 1) `step_size`

- 파싱·클램프: `enhance_advanced_helpers.cpp:268-271`
  - `outStepSize = clamp(val, 0.01, 1.0)`
- 호출: `fractional_process.cpp:100-101`
  - `parse_fractional_config(..., iterations, stepSize, ...)`
- 소비:
  - `fractional_process.cpp:114` — `fracConfig.order = order;` 만 채운다.
  - `FractionalConfig`(`detail/fractional_derivative.h:34-35`)의 멤버는 **`float order` 하나**다.
  - `stepSize` 가 나오는 곳은 `:132` 의 `spdlog::debug` 문자열뿐이다.
- 대조: 같은 파서의 `iterations` 는 `:119` 루프 횟수로 **소비된다**.

### 2) `levels` / `num_levels`

- 파싱·클램프: `enhance_advanced_helpers.cpp:171-179`. 두 키 모두 같은 `outLevels` 에 쓰이고, `levels` 가 나중에 쓰인다.
  - 클램프: `[XPE_MFP_MIN_LEVELS=2, XPE_MFP_MAX_LEVELS=8]` (`internal.h:41-42`)
- 소비: `multiscale_process.cpp:103` — `mfpConfig.numLevels = levels`

### 3) 레벨별 게인

- 파싱·클램프: `enhance_advanced_helpers.cpp:181-194` — `[0, 5]`
- 전달: `multiscale_process.cpp:104-106`
- 소비: `mfp_scalar.cpp:166-186` — 루프는 `level = numLevels-2 … 0` 이고, 레벨별로 게인을 고른다.
  - `level == numLevels-2` → `flatGain`
  - `level == 0` → `edgeGain`
  - 그 밖 → `textureGain`
- 레벨 수에 따라 쓰이지 않는 게인이 생긴다.
  - `numLevels=3`: 루프가 level 1, 0 만 돈다 → `flat`, `edge` 만 쓰이고 `texture` 분기가 비어 있다.
  - `numLevels=2`: 루프가 level 0 하나만 돈다. 이 레벨이 `numLevels-2` 조건에 먼저 걸려 `flat` 만 쓰이고, `edge`·`texture` 분기에는 닿지 않는다.
- 판독 결과가 §3 측정값과 일치한다.

**부수 판독**: `mfp_scalar.cpp:16` 에 `MfpConfig::fromJson` 이라는 두 번째 파서가 있다(중첩 `"mfp"` 키, 클램프 `[1,6]`).
- `modules/`·`tests/` 에서 이 함수를 부르는 곳을 grep 으로 찾지 못했다. 검색한 것은 `fromJson` 문자열이고, 찾은 것은 선언과 정의뿐이다.
- 실행으로 도달 여부를 확인하지는 않았다.

## 3. 실행 (`_run.log`, `BUILD=0`)

```
multiscale edge_gain        maxdiff=150.909668   ← 대조군 (기본 레벨)
multiscale texture_gain     maxdiff=56.201782
multiscale flat_gain        maxdiff=42.214478
multiscale noise_threshold  maxdiff=102.016541
multiscale num_levels       maxdiff=5.628662
[OK] KnownDivergence_LevelsSilentlyOverridesNumLevels
gain reach by level count: tex@3=0.000000 edge@2=0.000000 tex@2=0.000000
[OK] KnownDivergence_LowLevelCountSilencesGains      (레벨 4 도달 증거 단언 포함)
fractional iterations maxdiff=494.171753             ← 대조군
fractional step_size 0.01 vs 1.0 maxdiff=0.000000000 -- read, clamped, discarded
[OK] KnownDivergence_FractionalStepSizeIsReadAndDiscarded   (XPE_OK + 미지 키 경고 침묵 = 도달 증거)
[  PASSED  ] 8 tests.
```

이 시험들은 QA-B-62 가 썼다. 각 무반응 단언 옆에 **도달 증거**(이 키가 실제로 읽힌다는 증거)를 짝지어 두었다.
- `step_size`: `XPE_OK` 를 받고, 파서가 이 이름을 안다(미지 키 경고가 뜨지 않는다).
- `levels`: `num_levels` 단독이 출력을 움직인다.
- 레벨별 게인: 레벨 4 에서 두 게인이 모두 출력을 움직인다.

이번 카드는 **현재 HEAD 에서 다시 실행**해 결과가 그대로임을 확인했다.

## 4. 경고가 왜 잡지 못하나

- 미지 키 경고의 알려진 이름 목록에 세 키가 모두 있다.
  - `fractional_process.cpp:94` `kKnown[] = { "iterations", "step_size", "safety" }`
  - `multiscale_process.cpp:87` 목록에 `"num_levels", "levels", "edge_gain", "texture_gain", …`
- 그래서 경고가 뜨지 않는 것은 **설계대로**다. 경고는 이름을 볼 뿐, 값이 쓰였는지는 보지 않는다.
- `ParserRecognises(…, "step_size")` 가 참이라는 것도 위 실행에서 단언으로 통과했다.

## 5. 요구가 있는가 — 결함인지 설계인지 가를 재료

- **SPEC(`SPEC-XPE-P2-ADV/spec.md`)** 요구 조항에는 세 키의 이름이 없다.
  - REQ-ADV-010: "Laplacian pyramid (3-4 levels by default), per-band enhancement coefficients derived from `meta->bodyPart` and configuration" — 키 이름, 레벨 수와 게인의 관계는 없다.
  - REQ-ADV-011: config 키를 명시하지 않는다.
  - 검색 범위: `SPEC-XPE-P2-ADV/` 전체에서 `step_size`, `num_levels`, `texture_gain` 등을 grep 했다. 걸린 것은 `_workspace/` 설계·구현 노트뿐이고 요구 조항은 없었다.
- **설계 문서 `docs/project/sdd_adv.md` §4.3** 은 세 키를 정의한다.
  - MFP: `levels` 2~8 기본 4, `edge_gain` 기본 1.5, `texture_gain` 기본 1.0, `flat_gain` 기본 0.8, `noise_threshold` 기본 5.0
  - Fractional: `iterations` 1~5 기본 1, `step_size` 0.01~1.0 기본 0.25
  - 코드 상수(`internal.h:40-56`)와 범위·기본값이 일치한다.
  - 그러나 `step_size` 가 **무엇을 하는지**, 레벨이 적을 때 게인이 어떻게 되는지는 적혀 있지 않다.
  - `num_levels` 는 SDD 에 없다. 코드 주석은 `levels` 를 "legacy" 라 부르지만, SDD 에는 `levels` 만 있다.
- **`SAD-ENHANCE-ADV-001` §6.1** 에는 리더가 적은 "Measured divergence (2026-09-11)" 가 있다. 거기서 `step_size`·`levels`·`num_levels` 는 "코드가 실제로 파싱하는 키" 목록에 들어 있다.
- 정리하면: **요구(SPEC)에는 없고, 설계(SDD)에는 이름·범위·기본값만 있다.**
  - 기본 레벨 4 에서는 세 게인이 모두 쓰인다.
  - 레벨 2·3 은 SDD 가 허용하는 범위(2~8) 안인데, 그때 게인이 쓰이지 않는다는 사실은 SDD 에 없다.

## 6. 미검증 (Gaps)

- **반증을 이번에 다시 하지 않았다.** QA-B-62 의 반증(step_size 최소 배선 → 해당 단언만 실패)은 그때 코드에 대한 것이다. 이후 해당 경로의 소스가 바뀌었는지는 위 판독으로만 확인했다.
- `levels` override 시험은 maxdiff 수치를 출력하지 않는다. 단언 통과만 확인했다.
- REQ-ADV-010 의 "`meta->bodyPart` 로부터 계수 유도" 가 구현됐는지는 이 카드 범위 밖이라 보지 않았다.
- `MfpConfig::fromJson` 의 도달 여부는 grep 판독뿐이다.
- `xpe_ai` 4개 키와 `gsvg` 키는 `#162` 본문의 Gap 그대로이고, 이번에 재지 않았다.
- 합성 구조 영상 한 종류만 썼다(`#148`).

## 7. 잔여 위험

- 기본 레벨 4 에서는 게인 문제가 드러나지 않는다. 호출자가 `levels` 를 2 나 3 으로 줄이면 설정한 게인이 조용히 무시된다.
- `num_levels` 와 `levels` 를 둘 다 주면 `levels` 가 이긴다. SDD 기준 이름이 `levels` 이므로, 이 순서가 의도인지는 문서에 없다.

## 부록 — 증거

`_env.bat`, `_b78.bat`, `_run.log`
