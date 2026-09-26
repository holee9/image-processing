# QA-B-149 단계 A — post 소유 8건 전제 훑기

**요약: 8건 중 (A) 2건 · (B) 2건 · (C) 4건.** 그리고 **이슈 2건의 전제가 드리프트**했습니다
(`#162` 세부 하나, `#179` 인용 줄). `#192` 는 **외부 입력 대기가 맞습니다** — 라벨만 빠졌습니다.

| # | 판정 | 한 줄 |
|---|---|---|
| 156 | **(C) 그대로** | 두 분기 대수 동일. **다만 결함 위치는 `EXACT` 가 아니라 `LINEAR`** — 고치려면 `REQ-DISP-009` 개정 필요 |
| 162 | **(A) 해결** | 3건 모두 신호 도착. 단 이슈의 "2레벨이면 `edge_gain` 도 무효" 는 **더 이상 참이 아님** |
| 191 | **(B) 일부** | 사각 2·4 **해결**(QA-B-134 측정), 1·3 잔존 — 둘 다 외부 입력/2.5 h MC 필요 |
| 192 | **(C) + 외부 입력** | 리더 기억이 맞습니다. 비활성 시험 이름에 `PendingDetectorMtf` 가 박혀 있고 라벨만 없음 |
| 177 | **(A) 해결** | 2026-09-17 개정이 코드에 적용돼 있음. 잔여는 `#151` 차단 |
| 179 | **(B) 일부** | 3072² 측정 시험 **이미 존재**하고 벤치마크 워크플로가 수집 중. 남은 것은 문턱 확정 |
| 130 | **(C) 그대로** | 스텁. 착수 전 보고 |
| 71 | **재계수 대상** | 위 결과 반영 시 실제 잔여는 5건 |

---

## 측정 함정 하나 — 먼저 적습니다

`gsvg_tests.exe` 를 **직접 돌리면 빨강 2건**이 납니다. 진짜 빨강이 아닙니다 —
팬텀·커널 표를 **cwd 상대경로**로 읽어서, ctest 의 `WORKING_DIRECTORY` 밖에서 돌리면
`"section [kernels] has no rows"` 로 실패합니다. ctest 아래서는 같은 시험이
**Passed**(#651, #652, `_verify.log`) 입니다.

**낡은 바이너리 함정의 반대 방향**입니다 — 이쪽은 **없는 빨강**을 만듭니다.
그래서 아래 `#191` 수치는 전부 **ctest `-V`** 로 뽑았습니다(`_mc191.log`).

---

## 1. `#156` — (C) 그대로. **다만 결함 위치를 정정합니다**

### 재현

`modules/display/src/voi_lut.cpp:39` 와 `:50`:

```
LINEAR : (x - (center - width/2)) / width * range + minOut
EXACT  : ((x - center)/width + 0.5) * range + minOut
```

전개하면 **같은 식**입니다. 실행 측정(`test_display_parameter_dependency.exe`):

```
LINEAR vs LINEAR_EXACT maxdiff=5.96046e-08 (one ulp at this scale is ~6e-08)
[       OK ] ParameterDependency.KnownDivergence_VoiLinearExactEqualsVoiLinear
[ DISABLED ] ParameterDependency.DISABLED_VoiLinearExactMustDifferFromLinear
```

**이슈 그대로입니다.** 비활성 시험 이름이 이미 "달라야 한다" 를 말하고 있습니다.

### 그런데 어느 쪽이 틀렸는가 — 이슈와 다른 답이 나옵니다

이슈는 *"`REQ-DISP-010` 이 금지한 오프셋(`+0.5`)이 두 분기에 다 있다"* 로 읽습니다.
**그 읽기를 채택하면 `EXACT` 를 고쳐야 하는데, 고치면 같은 요구의 앞부분이 깨집니다.**

`REQ-DISP-010` 은 *"the full window maps exactly from minOut to maxOut"* 도 함께
요구합니다. `+0.5` 를 빼면 창 `[c−w/2, c+w/2]` 이 `[−0.5, +0.5]·range` 로 가서
**minOut..maxOut 을 채우지 못합니다.** 한 문장 안에서 모순입니다.

**저장소 안 2차 문서가 갈라 줍니다** — `docs/post-processing/xpe/XPE-SDD-002…md:963-968`:

```
IF input <= center - 0.5 - (width - 1) / 2:  ...
ELSE:
  output = ((input - (center - 0.5)) / (width - 1) + 0.5) * (maxOut - minOut) + minOut
```

**DICOM `LINEAR` 에도 `+0.5` 가 있습니다.** 두 모드를 가르는 "half-value offset" 은
`+0.5` 가 아니라 **중심의 `−0.5` 와 폭의 `−1`** 입니다. 즉:

| 모드 | 지금 코드 | SDD-002(= DICOM) |
|---|---|---|
| `LINEAR` | `c`, `w` | **`c − 0.5`, `w − 1`** ← **빠져 있음** |
| `LINEAR_EXACT` | `c`, `w`, `+0.5` | `c`, `w`, `+0.5` ← **맞음** |

> **`EXACT` 는 맞고 `LINEAR` 가 틀렸습니다.** 둘이 같아진 이유가 그것입니다.

### 그래서 막힙니다 — `REQ-DISP-009` 가 틀린 식을 **명시**합니다

`spec.md:305` 가 `output = clamp((input − (center − width/2)) / width * …)` 를
**SHALL** 로 적습니다. 코드는 그 요구를 정확히 구현했습니다 — **`#154` 와 같은 형태**
(SPEC 이 틀리고 코드가 충실)이고, `#155`(SPEC 이 맞고 코드가 틀림)와 반대입니다.

**SPEC 은 리더 소유이므로 §5 에 문구를 적었습니다.** 개정 없이 코드만 고치면
요구를 어기는 커밋이 됩니다.

### 미검증

- **DICOM PS3.3 C.11.2.1.3 원문은 못 봤습니다.** 기준은 `XPE-SDD-002`(저장소 2차 문서)
  이고, `#154` 와 같은 등급입니다. `test_parameter_dependency.cpp:238` 이
  *"Blocked on DICOM PS3.3 C.11.2.1.3 (#156)"* 라고 이미 적어 둔 것과 같은 자리입니다.

---

## 2. `#162` — (A) 해결. 전제 하나는 **드리프트**했습니다

### 측정 (`test_xpe_enhance_advanced.exe --gtest_filter=ConfigValueDependency.*`)

```
multiscale edge_gain        maxdiff=150.909668 (threshold 0.080585)
multiscale texture_gain     maxdiff=56.201782
multiscale flat_gain        maxdiff=42.214478
multiscale noise_threshold  maxdiff=102.016541
multiscale num_levels       maxdiff=5.628662
num_levels=5 + levels=2: maxdiff to num_levels-only=0.000000000, to levels-only=10.797485
fractional step_size 0.01 vs 1.0 maxdiff=0.000000000 -- not parsed, reported inert
gain reach by level count: tex@3=0.000000 edge@2=150.909546 tex@2=0.000000
```

| 이슈 항목 | 오늘 |
|---|---|
| 1 `step_size` 가 읽히고 버려짐 | **해소** — 파싱 자체를 걷어내고 `inert` 로 **신호**를 보냅니다(QA-B-139) |
| 2 `levels` 가 `num_levels` 를 덮어씀 | **해소** — 순서가 뒤집혀 `num_levels` 가 이깁니다(0.000 대 10.797) |
| 3 레벨별 게인 무효 | **부분 해소 + 전제 드리프트** ↓ |

### 전제 드리프트 (§0 의 "이슈 본문은 스냅샷")

이슈는 *"2면 `edge_gain`·`texture_gain` 둘 다 무효"* 라고 적습니다.
**오늘 `edge@2` 는 150.909546 으로 움직입니다.** 무효로 남은 것은
`texture@2`·`texture@3` 뿐이고, 그것은 *"레벨이 적으면 중간 대역이 없다"* 는
**설계 결과**로 판정돼 `warn_inert_keys_once` 로 신호가 나갑니다(QA-B-122/140).

### 대조군 (판정 (A) 에 필수)

**같은 측정·같은 실행에서 다른 키들은 움직입니다** — `edge 150.9`, `flat 42.2`,
`noise 102.0`. 즉 `step_size 0.000` 과 `tex 0.000` 은 **측정이 눈멀어서가 아닙니다.**

### 미검증

- `xpe_ai` 의 config 4건(`confidence_threshold` 등)은 **여전히 미측정**입니다.
  추론 경로가 스텁이라 "안 움직임" 과 "스텁 조기 반환" 이 구별되지 않습니다 — `#130` 과 묶입니다.

---

## 3. `#191` — (B) 일부. 사각 **2·4 는 해결**, 1·3 잔존

QA-B-134 가 두 건을 실험으로 닫았습니다. 수치는 ctest `-V` 로 다시 뽑았습니다.

### 사각 2 (σ₁ 설명) — **해결. 그리고 설명이 틀렸습니다**

```
VGMC134 sigma1 k=1.0 sigma1=10.17 mm peak_at=0.42 mm peak/sigma1=0.0413
VGMC134 sigma1 k=1.5 sigma1=15.25 mm peak_at=0.21 mm peak/sigma1=0.0138
VGMC134 sigma1 k=2.0 sigma1=20.34 mm peak_at=0.84 mm peak/sigma1=0.0413
VGMC134 sigma1 k=3.0 sigma1=30.50 mm peak_at=0.42 mm peak/sigma1=0.0138
```

σ₁ 을 **3배** 넓혀도 봉우리는 0.21–0.84 mm 에서 **추세 없이** 머뭅니다.
**가장 좁은 항이 봉우리 위치를 정하지 않습니다** — 이슈의 설명이 반증됐습니다.

덧붙여 **애초에 단위가 틀렸습니다**: 표의 σ 는 **cm** 인데 mm 로 읽어 0.92–1.21 mm 로
적혀 있었습니다. 실제 σ₁ 은 **10.17 mm** 이고, 봉우리는 그 **0.04 배**입니다 —
"자릿수가 같다" 가 성립한 적이 없습니다.

### 사각 4 (상한이 막아 주는가) — **해결. 막아 줍니다**

```
factor=  1.0 None      negativePrimary=0      | GlobalSum negativePrimary=0
factor=  2.0 None      negativePrimary=227    | GlobalSum negativePrimary=0
factor=  3.0 None      negativePrimary=7613   | GlobalSum negativePrimary=0
factor=  5.0 None      negativePrimary=21521  | GlobalSum negativePrimary=0
factor= 10.0 None      negativePrimary=34353  | GlobalSum negativePrimary=0
```

커널 진폭을 과대평가해 **음수 1차가 실제로 생기는 상황**을 만들었고, 상한을 켜면
**모든 배율에서 0** 입니다. `CapMode::None` 대조군이 있어 "상한이 한 일" 이
분리됩니다. **"걸린다" 에서 "막아 준다" 로 올라갔습니다.**

### 사각 1·3 — 잔존, 그리고 **제가 닫을 수 없습니다**

| 사각 | 필요한 것 |
|---|---|
| 1 (4.5 mm 너머) | **계단 간격 2 cm 이상인 새 팬텀 + MC 재실행(약 2.5 h)**. `tools/mcsim/` 는 리더 소유 |
| 3 (REQ-011 독립성) | **다른 출처의 산란 자료 또는 해석적 기준.** 현 커널 표가 같은 MC 코드로 적합된 것이라 자기 비교입니다 |

3번은 `#155` 에서 배운 그 구별입니다 — 자기 자신과 비교하면 일관되게 틀려도 통과합니다.

---

## 4. `#192` — (C) + **외부 입력 필요. 리더 기억이 맞습니다**

라벨에 `status:blocked` 가 **없습니다**(확인). 그러나 **코드가 이미 그렇게 적고
있습니다**:

```
modules/gsvg/tests/test_grid_suppression.cpp:457:
TEST(GsvgGridSuppression, DISABLED_SevereAliasing180_UndecidedPendingDetectorMtf_192)
```

ctest 목록에도 `Disabled` 로 올라옵니다. **라벨이 빠진 것이지 기억이 틀린 것이
아닙니다.**

### 왜 제가 고칠 수 없는가

이슈 본문의 확인 항목 셋 중 둘이 **저장소 밖**입니다.

1. 임상에서 쓰이는 **격자 선밀도 분포** — 178·180·182 lpi 가 흔한지
2. **검출기가 보는 주파수 대역**(MTF) — 0.562 lp/cm 가 그 아래인지
3. *"알리아스가 거의 DC 면 육안으로도 안 보이니 억제할 것이 없는 것 아닌가"*
   — **이슈 스스로 "가장 중요한 질문" 이라 적고 확인하지 않았습니다**

3번은 **검출 실패가 아니라 결함 자체의 부재**일 수 있습니다. 판정 없이 검출 대역을
넓히면 해부학적 구조를 격자로 오검출하게 됩니다(이슈 본문 경고).

**고치지 않았습니다.** 라벨 부착은 리더 몫입니다.

---

## 5. `#177` — (A) 해결. 잔여는 `#151` 차단

`modules/display/src/voi_lut.cpp:77-97` 에 **개정이 적용돼 있습니다**:

> `REQ-DISP-017 (revised 2026-09-17, #177): the presets act on detector DN, not HU …
> The former CT HU windows (e.g. Abdomen 40/400) crushed DN input to a single output
> level (GUI-C-84).`

네 부위 전부 `center 32768 / width 65535`(전체 16비트 창)입니다. 고정 시험도 있습니다 —
`ParameterDependency.KnownDivergence_VoiPresetIgnoresBodyPartUntil151`,
실행 로그 `preset BONE c=32768 w=65535 | LUNG c=32768 w=65535 | HEAD c=32768 w=65535`.

**카드 §2-5 가 요구한 "SPEC 개정 선행" 은 이미 끝났습니다.** 남은 것은
**부위별 DN 창을 실제 검출기 자료에서 유도**하는 것이고 `#151` 차단입니다.
합성 자료로 값을 고르지 말라는 경고까지 코드에 박혀 있습니다(`#148`).

### 미검증 (판정 (A) 의 약한 곳)

**대조군을 돌리지 않았습니다.** "HU 창을 되돌리면 뭉개진다" 를 실측하지 않았고,
판정은 **코드·SPEC·고정 시험 읽기**에 서 있습니다. 뭉개짐의 원 관측은 GUI 레인의
`GUI-C-84` 이고 제 실행이 아닙니다.

---

## 6. `#179` — (B) 일부. **측정 시험은 이미 있습니다**

### 전제 드리프트

이슈가 `spec.md:264` 를 `REQ-ADV-061` 로 인용하는데, **오늘 `:264` 는 제목 줄**이고
본문은 `:266` 입니다. 그리고 그 제목은 **"Edge Enhancement Performance Budget"** 인데
본문은 `xpe_fractional_process` 를 말합니다 — **제목/본문 불일치**(문서, 리더 소유).

### 이슈의 "다음" 3단계 중 1단계는 **끝났습니다**

- `modules/enhance_advanced/tests/test_edge_enhancement.cpp:772`
  `BenchmarkFreeze_ADV061_FractionalMeasure3072` — 3072², 문턱 없이 min/med/max 출력
- `.github/workflows/benchmark-regression.yml:68` 의 `-R` 패턴에 `BenchmarkFreeze` 가
  있어 **수집됩니다.** `-V` 를 쓰는 이유도 주석에 적혀 있습니다(측정 전용 시험은
  통과하므로 `--output-on-failure` 가 출력을 버림 — QA-B-84 가 실제로 겪음)
- SPEC 에 **측정 조건과 실측이 이미 기입**돼 있습니다(`:268` 인용 블록):
  order 1.0 = **302 ms**, order 1.2 = **320 ms**, 단일 스레드 733 ms (QA-B-103, 개발 PC)

### 남은 것

1. **문턱 확정** — 로컬 수치로 정하면 안 됩니다(702 ms 로 잡은 게이트가 CI 에서
   1340 ms, **1.91배**였던 전례). **CI 수치를 읽어야** 합니다
2. **AVX2 120 ms 조항** — SPEC 이 이미 *"이 모듈에 명시적 SIMD 코드가 없으므로 아직
   해당 없음"* 으로 적었습니다. 조항 자체를 어떻게 할지는 SPEC 판단

→ 단계 B 에서 **CI 측정치를 모아 문턱을 제안**하되, **게이트를 넣은 커밋은 CI 를
한 번 거치기 전에 닫지 않습니다.** 푸시는 리더 몫이므로 여기서 닫을 수 없습니다.

---

## 7. `#130` — (C) 그대로. 착수 전 보고

`modules/ai/src/ai.cpp:9` *"Current implementation: stub phase (ONNX Runtime not yet
linked)"*, `:278` `"model_version":"0.1.0-stub"`, `:430` *"In stub mode, we signal that
AI is not available."*

**의존성 추가 결정이 필요합니다** — 카드 §2-7 대로 리더 판단 사항입니다.
`#162` 의 미측정 4건(`xpe_ai` config)이 여기에 묶여 있습니다.

---

## 8. `#71` — 우산. 위 결과로 재계수

| 상태 | 건 |
|---|---|
| 실제로 남은 것 | `#156`(SPEC 개정 후) · `#191` 사각 1·3 · `#179` 문턱 · `#130` · `#192` |
| 이미 닫힌 것 | `#162` · `#177` · `#191` 사각 2·4 |
| 그중 외부 입력 대기 | `#192`(MTF·임상 분포) · `#191` 사각 1(팬텀+2.5 h MC)·3(독립 산란 자료) · `#177` 잔여(`#151`) |

**post 가 스스로 끝낼 수 있는 것은 실질 2건**입니다 — `#156`(SPEC 개정 뒤) 과
`#179`(CI 수치 확보 뒤). 나머지는 리더 결정이나 외부 입력을 기다립니다.

---

## 9. 단계 B 계획 (카드 §2 순서 대비)

| 순서 | 카드 | 실제 |
|---|---|---|
| 1 `#156` | 스스로 | **SPEC 개정 선행 필요** — §5 문구 제공, 리더 적용 후 착수 |
| 2 `#162` | 스스로 | **할 일 없음** — 이미 해결. 전제 드리프트만 보고 |
| 3 `#191` | 시험 추가 | 사각 2·4 완료. **1·3 은 외부 입력** |
| 4 `#192` | 조건부 | **외부 입력 필요 → 하지 않음** |
| 5 `#177` | 보고 후 대기 | **이미 개정 완료** — 잔여는 `#151` |
| 6 `#179` | 게이트 | CI 수치 수집 → 문턱 제안 |
| 7 `#130` | 보고 후 대기 | 스텁 확인, 보고 |
| 8 `#71` | 재계수 | §8 |

**카드가 "스스로 끝내라" 한 1~4 중 실제로 손댈 것은 `#156` 하나이고, 그것도 SPEC 이
선행합니다.** 나머지 셋은 이미 끝났거나(162) 외부 입력 대기(191 일부, 192)입니다.

---

## 10. SPEC 문구 (리더가 적용, 제가 확인)

### `REQ-DISP-009` — 식 교체

> **REQ-DISP-009**: WHEN `xpe_apply_voi_lut` is called with `mode == XPE_VOI_LINEAR`,
> the system SHALL apply DICOM PS3.3 C.11.2.1.3.1 linear windowing to each pixel
> in-place, using the standard's half-value adjustment:
> `output[i] = clamp(((input[i] − (center − 0.5)) / (width − 1) + 0.5) * (maxOut − minOut) + minOut, minOut, maxOut)`,
> with `input[i] <= center − 0.5 − (width − 1)/2` mapping to `minOut` and
> `input[i] > center − 0.5 + (width − 1)/2` mapping to `maxOut`.
> `width` SHALL be greater than 1 for this mode.

### `REQ-DISP-010` — 무엇이 "half-value offset" 인지 못박기

> **REQ-DISP-010**: … exact linear mapping: `output[i] = clamp(((input[i] − center) / width + 0.5) * (maxOut − minOut) + minOut, minOut, maxOut)`.
> The "half-value offset" this mode omits is the standard's **`center − 0.5` / `width − 1`**
> adjustment required by REQ-DISP-009 — **not** the `+ 0.5` normalisation term, which
> both modes carry and without which the window would not map onto the full
> `[minOut, maxOut]` range.

**두 번째 문장을 빼지 마십시오.** 그 한 문장이 없어서 `#156` 본문이 `+0.5` 를
금지 대상으로 읽었고, 그 읽기를 따르면 같은 요구의 앞부분이 깨집니다.

### 부수 (문서, 리더 판단)

- `SPEC-XPE-P2-ADV` `REQ-ADV-061` 의 **제목이 "Edge Enhancement"** 인데 본문은
  `xpe_fractional_process` 입니다.
- `voi_lut.cpp:47` 주석이 `REQ-DISP-010` 을 맞게 인용합니다 — 이슈가 지적한
  `REQ-DISP-011` 오인용은 **이미 고쳐져 있습니다**(드리프트 3번째).

---

## 11. 검증

```
ctest -V -R "NarrowestTerm…|OverEstimated…"  →  ===CTEST=0===   (_mc191.log)
test_xpe_enhance_advanced.exe ConfigValueDependency.*  →  전건 통과
test_display_parameter_dependency.exe  →  maxdiff 5.96e-08 (#156 재현)
```

**이 단계에서 코드는 한 줄도 바꾸지 않았습니다.**

---

Refs #156, #162, #191, #192, #177, #179, #130, #71
