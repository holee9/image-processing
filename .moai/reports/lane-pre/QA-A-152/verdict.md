# QA-A-152 (#216) — 문턱은 **셋이 아니라 여섯**이고, 둘은 출처가 있으며 **하나는 단위가 어긋났습니다**

카드 §1 이 물은 문턱 셋을 찾다가, `overall_pass` 를 먹이는 상수가 **여섯**이라는 것부터 나왔습니다.
그리고 출처를 찾은 둘 중 하나는 **찾은 요구와 코드의 단위가 다릅니다.**

---

## 0. 대조군

| | 결과 |
|---|---|
| **양성** `DARK_BIAS_MAX`(5.0) → `FUNC-016` | `SRS-CALIB-001:147` *"acceptance shall require `abs(DarkBias) <= 5 ADU`"* — **나옴** ✔ |
| **음성** 지어낸 상수명 `XPE_FROBNICATE_MAX` | `docs/` 0건 ✔ |
| `clients`+`gui` 검색이 눈멀지 않았는가 | `xpe_preprocess_init` **19건** 잡힘 ✔ |
| 도구 | `PYTHONIOENCODING` 없이 `EXIT=0` |

---

## 1. `overall_pass` 를 먹이는 문턱 — **여섯** (카드는 셋)

검색 범위: `docs/`(`SRS-CALIB-001`·`RTM-CALIB-001` 포함) + `.moai/specs/SPEC-XPE-P1A/spec.md`.
**값으로도 개념으로도** 찾았습니다(`3 dB`·`0.99`·`99%`·`2 dB` / `PRNU`·`coverage`·`SNR improvement`
·`DSNU`·`non-uniformity`·`defect density`).

| 상수 | 값 | 출처 | 판정 |
|---|---|---|---|
| `DARK_BIAS_MAX` | 5.0 ADU | **`SRS-CALIB-FUNC-016`** (`:147`) | **있음** — 값 일치 |
| `DEFECT_DENSITY_MAX` | 0.05 | **`SRS-CALIB-FUNC-003`** (`:34`, *"Maximum 5% defect density tolerance"*) | **있음 — 그러나 단위가 어긋남**(§2) |
| `DSNU_MAX_PCT` | 1.0 % | **없음** — §3 참조 | 없음 |
| `PRNU_IMPROVE_MIN_DB` | 3.0 dB | **없음** | 없음 (+ 이름 오용, §4) |
| `GAIN_COVERAGE_MIN` | 0.99 | **없음** | 없음 |
| `SNR_IMPROVE_MIN_DB` | 2.0 dB | **없음** | 없음 |

카드가 셋으로 본 것은 `#216` 코멘트가 셋만 열거했기 때문입니다. `xpe_verify_metrics.cpp:27-38` 의
상수 블록 전체를 보면 여섯이고, **여섯 다 `overall_pass` 에 직접 들어갑니다**(`:240`·`:351`·`:436`·`:525`).

---

## 2. `DEFECT_DENSITY_MAX` — **출처는 찾았는데 단위가 100배 어긋납니다**

`FUNC-003`: *"Maximum **5%** defect density tolerance."* 코드:

```cpp
constexpr double DEFECT_DENSITY_MAX = 0.05;   // 주석: "5% max defect density"
...
metrics->defect_density = (double(metrics->defect_count) / pixel_count) * 100.0;   // :430  ← 퍼센트
metrics->overall_pass   = (metrics->defect_density < DEFECT_DENSITY_MAX);          // :436
```

`defect_density` 는 **퍼센트**입니다 — `× 100.0` 이 붙어 있고, 시험이 그렇게 고정합니다:

```cpp
// test_verify_metrics.cpp:330
double expected_density = (double(num_defects) / (W * H)) * 100.0;
EXPECT_NEAR(metrics.defect_density, expected_density, 0.01);
```

상수는 **분수**(0.05)입니다. 그래서 실제 게이트는 **0.05 %** — 요구가 허용하는 5 % 보다 **100배 엄격**합니다.
결함 밀도 1 % 인 정상 패널이 `overall_pass = false` 를 받습니다.

주석("5% max")이 **의도**를 적고 값이 그 의도를 배반한 형태입니다.

> **고치지 않았습니다.** 합격선을 옮기는 것은 편집이 아니라 **제품 판단**입니다. 상수 옆에
> 요구 ID·불일치·근거(시험 줄 번호까지)를 적었습니다. `5.0` 으로 고칠지, 지표를 분수로 바꿀지는
> 리더/제품 몫입니다.

---

## 3. `DSNU_MAX_PCT` — 없습니다. 그리고 **닮은 숫자는 다른 양입니다**

카드가 지목한 `docs/quality-eval/01_Noise_…:1185` 에 *"DSNU RMS | **< 1% of full scale**"* 이 있습니다.
값이 1 로 같습니다. **그러나 채택할 수 없습니다 — 두 가지 이유가 각각 독립적으로 충분합니다.**

**(a) 그 문서는 요구 출처가 아닙니다.** `docs/quality-eval/README.md` 가 스스로 이렇게 적습니다:

> **모듈**: Python 기반 FPD 측정 알고리즘 라이브러리 / **소유자**: FPD 품질 평가 팀
> **안전 등급**: 해당 없음 (생산 라인 QA 도구, **진단 소프트웨어 아님**)

**다른 모듈의, 안전 등급 해당 없는 QA 도구 문서**입니다. Class B 함수의 합격선 근거가 될 수 없습니다.

**(b) 분모가 다릅니다.** 코드는 `dsnu = (stddev / mean) * 100` (`:236`) — **다크 영역 평균 대비
변동계수**입니다. 문서는 **full scale 대비**입니다. 오프셋 보정 후 다크 영역 평균은 0 근처라
`stddev/mean` 은 크게 나오고 `stddev/full_scale` 은 작게 나옵니다 — **같은 "1%" 가 아닙니다.**

`#155`(σ 를 cm 인데 mm 로 읽어 10배 틀림)와 같은 형태이고, 여기서는 **자릿수가 맞는 우연**이
채택을 부를 뻔했습니다.

→ **출처 없음**으로 판정합니다. 카드 §1 표의 셋째 행("정말 없음 → 폐기 판단 대상")입니다.

---

## 4. 나머지 셋 — 출처 없음, 그리고 하나는 **이름이 쓰임과 다릅니다**

`PRNU_IMPROVE_MIN_DB`·`GAIN_COVERAGE_MIN`·`SNR_IMPROVE_MIN_DB`: 위 범위에서 값·개념 모두 0건.

추가로 **`PRNU_IMPROVE_MIN_DB` 는 PRNU 에 쓰이지 않습니다**:

```cpp
// :348
bool snr_improved = (metrics->snr_improvement_db >= PRNU_IMPROVE_MIN_DB) || ...
```

**SNR 값을 PRNU 이름의 상수와 비교**합니다. 그리고 진짜 `SNR_IMPROVE_MIN_DB`(2.0)는 다른 함수
(`:525`)에서 쓰입니다. 같은 파일 안에서 SNR 합격선이 **3.0 과 2.0 두 개**인 셈인데, 어느 쪽도
요구가 없습니다.

---

## 5. **SRS 에 있는 합격 기준과 코드가 재는 지표가 다릅니다**

문턱의 출처를 찾다 나온 것입니다. `SRS-CALIB-001` 이 실제로 적은 합격 기준:

| SRS | 기준 | 코드가 재는 것 |
|---|---|---|
| `:148` `FUNC-017` | `FlatResidualPct <= 1.0%` | **`FlatResidualPct` 를 계산하지 않습니다**(전수 0건). PRNU·coverage·SNR 을 봄 |
| `:150` `FUNC-019` | **100% defect recall**, FPR `< 0.001%` | **recall·FPR 을 계산하지 않습니다**(전수 0건). defect **density** 를 봄 |
| `:151` | ghost removal `>= 90%` | 해당 함수 없음 |

즉 게인·결함 경로는 **요구가 정한 지표가 아닌 다른 지표로** 합격을 판정합니다.
"문턱에 출처가 없다" 보다 한 겹 깊은 문제입니다 — **출처가 있는 기준 쪽이 구현되지 않았습니다.**

---

## 6. `:199` — **잴 수 없는 것을 "합격" 으로 보고합니다**

```cpp
if (!has_variation) {          // raw_range / raw_mean <= 0.01, 즉 균일 영상
    metrics->dark_bias = 0.0;
    metrics->dsnu = 0.0;
    metrics->overall_pass = true;    // ← 측정하지 않고 통과
    return XPE_OK;
}
```

같은 파일의 게인 경로는 **반대로** 합니다 — 유효 화소가 없으면 `overall_pass = false`(`:315`).
**한 파일 안에서 "잴 수 없음" 이 한쪽은 합격, 한쪽은 불합격**입니다.

호출자는 `overall_pass` 만으로 *"통과했다"* 와 *"잴 수 없었다"* 를 구분할 수 없습니다. Class B 에서
위험한 방향은 전자로 보고하는 쪽이고, **균일 합성 프레임이 정확히 이 분기에 들어옵니다** —
`#148`(균일 합성 프레임에서만 검증한 하한이 구조 있는 영상에서 무력화)과 같은 자리입니다.

→ **고치지 않았습니다.** 어느 쪽으로 보고할지는 제품 판단입니다. 주석으로 남겼습니다.

---

## 7. 카드 §3·§4·§5 — 전부 해소

### §3 `xpe_crc32` 수출 철회 — 호출자 0건 확인

검색 범위: `clients/`·`gui/`·`modules/`·`tools/`·`tests/` (빌드 산출 제외).

| 디렉터리 | 건수 |
|---|---|
| `clients` · `gui` · `tools` · `tests` | **0** |
| `modules` | **2** — 선언(`preprocess_api.h:1006`) + 정의(`calibration_manager.cpp:29`) |

대조군: 같은 범위에서 `xpe_preprocess_init` 는 `clients`+`gui` 에 **19건** — 검색이 눈멀지 않았습니다.

→ **호출자 0건 확정.** 철회해도 깨지는 호출자가 없습니다. 다만 수출 제거는 **ABI 변경**이므로
이 카드에서 실행하지 않았습니다 — 리더 판단.

### §4 `bpm_generate` — **(A) 확정**, 상한 1건 해소

`QA-A-151` 의 미검증 항목이었습니다(RTM 만 확인). 본문을 읽었습니다:

> *"**SRS-CALIB-FUNC-022 vs FUNC-007의 관계**: FUNC-007은 'BPM 적용 및 보간'을 정의하며,
> FUNC-022~023은 BPM **생성**의 알고리즘 상세를 명시합니다."*

`FUNC-022`~`025` 가 **BPM 생성**을 규정합니다. `xpe_bpm_generate` 는 **(A) 요구 있음** 확정입니다.

### §5 `REQ-P1A-034` → `REQ-P1A-088` — 4곳 이관

`REQ-P1A-088`: *"**When** `xpe_ghost_reset(handle)` is called with a valid handle, the module
**shall** clear the accumulated frame history and the exposure state, so that the next
`xpe_ghost_correct` behaves as if the handle had just been created."*

인용 4곳이 서술하던 것과 **정확히 같습니다**. 옮겼습니다:

| 자리 | 서술 |
|---|---|
| `preprocess_api.h:601` | "Clear accumulated frame history" |
| `ghost_correct.cpp:251` | "clear accumulated frame history" |
| `test_ghost_correct.cpp:65` | "`xpe_ghost_reset` clears history" |
| `test_golden_reference.cpp:386` | "After `reset()`, next frame uses zero history" |

`REQ-P1A-034` 인용은 `modules/preprocess` 에서 **0건**이 됐습니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **757 / 757** (`CTEST_EXIT=0`) |
| `check_req_citations.py` | 3 → **2** ID, 14 → **10** 인용 (`EXIT=0`, `PYTHONIOENCODING` 없이) |
| 대조군 | 양성 1 통과 · 음성 0 · `clients`/`gui` 검색 19건으로 확인 |

**동작 변경 없음** — 이관과 주석뿐입니다. 문턱 값은 하나도 바꾸지 않았습니다.

## 리더 판단 필요 (넷)

1. **`DEFECT_DENSITY_MAX` 단위** — `5.0` 으로 고칠지, 지표를 분수로 바꿀지 (합격선 이동)
2. **출처 없는 문턱 넷** — 요구를 세울지 폐기할지. `DSNU_MAX_PCT` 는 채택 가능한 숫자가 없습니다(§3)
3. **`:199` 의 무조건 합격** — 잴 수 없을 때 어느 쪽으로 보고할지 (게인 경로와 불일치)
4. **`xpe_crc32` 수출 철회** — 호출자 0건 확인됨, ABI 변경이라 미실행

추가로 **§5(SRS 기준과 코드 지표 불일치)** 는 `#216` 보다 넓은 자리로 보입니다 — 별도 이슈 후보입니다.

## 미검증 / 잔여 위험

- 출처 없음 판정의 **검색 범위**는 `docs/`(`SRS-CALIB-001`·`RTM-CALIB-001` 포함) +
  `.moai/specs/SPEC-XPE-P1A/spec.md` 입니다. `docs/` 밖의 다른 SRS 계열은 보지 않았습니다.
- `quality-eval` 을 요구 출처에서 제외한 근거는 **그 README 의 자기 기술**입니다. 문서 소유자에게
  확인한 것은 아닙니다.
- `FUNC-017`·`FUNC-019` 의 기준이 **다른 곳에서** 구현됐을 가능성은 보지 않았습니다 —
  `xpe_verify_metrics.cpp` 안에서 `FlatResidualPct`·`recall`·`FPR` 이 0건인 것만 확인했습니다.
- `:199` 의 균일 영상 판정 문턱(`raw_range/raw_mean > 0.01`) 자체도 출처를 찾지 않았습니다.

🗿 MoAI
