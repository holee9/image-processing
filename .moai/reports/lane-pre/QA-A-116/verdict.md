# QA-A-116 (#187) — 게인 다항식 적재 시 보정이 죽는 문제: 전제 확인 · 선택지 · 조사

**(b) 구현은 하지 않았다** — 카드가 리더 결정 뒤로 정했고, (a) 에서 **전제가 이슈 작성 시점과 달라진 것**을 찾았기 때문이다.

## 1. (a) 전제 확인 — 절반은 그대로, 절반은 바뀌었다

이슈는 QA-A-104 기준이다. 지금 코드에서 줄 단위로 확인했다.

| 이슈의 서술 | 현재 코드 | 판정 |
|---|---|---|
| `xpe_calib_load_gain` 이 POLY 를 읽으며 **스칼라 게인 맵을 지운다** | `xpe_calib_load_gain.cpp:99-101` — `gain_poly_coeffs = std::move(map); ... gain_map.reset();` | **그대로** (이슈는 76-90 이라 했으나 QA-A-107 의 범위 검사가 앞에 들어와 99-101 로 밀렸다) |
| 적재는 성공(`XPE_OK`)을 돌려준다 | 같은 함수가 커밋 뒤 `XPE_OK` | **그대로** |
| `xpe_gain_correct()` 가 다항식을 적용하지 않는다 | `gain_correct.cpp:286-291` | **그대로** |
| 그때 **`CALIB_NOT_LOADED`** 를 낸다 | `gain_correct.cpp:286-291` 이 **`XPE_ERR_UNSUPPORTED_FORMAT`** 과 ERROR 알림을 낸다. `CALIB_NOT_LOADED`(293행)는 다항식도 스칼라도 없을 때만 | **바뀌었다** |

바뀐 지점은 QA-A-107(#187, 커밋 `2ea598c`)에서 넣은 것이다. 지금 코드는:

```cpp
// gain_correct.cpp:286-291
if (!g_calib.gain_map && g_calib.gain_poly_coeffs) {
    xpe_alert_push("gain polynomial (XCAL_TYPE_GAIN_POLY) is loaded; "
                   "xpe_gain_correct does not apply it (issue #187) -- "
                   "load a scalar XCAL_TYPE_GAIN map to correct", XPE_ALERT_ERROR);
    return XPE_ERR_UNSUPPORTED_FORMAT;
}
```

**"조용한" 부분은 이미 없다.** 적재 시점에 WARNING 알림("no correction applies G(x,y,E)"), 보정 시점에 ERROR 알림 + "적재는 됐으나 이 함수가 적용하지 못한다" 를 구분하는 오류 코드가 나간다.

**남아 있는 결함은 조용함이 아니라 기능 상실이다**: 사용자가 정상 교정 파일을 넣으면 **작동하던 스칼라 보정이 사라지고**, 그 프레임은 게인 보정 없이 남는다. 알림이 있으니 보이기는 하지만, 회복하려면 스칼라 파일을 다시 적재해야 한다.

### 재현은 이미 고정돼 있다

카드가 요구한 재현 시험을 새로 만들지 않았다 — **QA-A-107 이 같은 것을 이미 넣었고 통과한다**(`BUILD_EXIT=0`, `RUN_EXIT=0`):

- `GainPolyNotAppliedTest.PolynomialGainLoadsWithAWarningAndCorrectionRefuses` — 적재 `XPE_OK` + 경고, 보정 `XPE_ERR_UNSUPPORTED_FORMAT` + 알림, 출력 버퍼 불변.
- `GainPolyNotAppliedTest.PolynomialLoadReplacesAWorkingScalarMap` — **작동하던 스칼라 보정(500.0)이 다항식 적재 뒤 사라지는 것**을 고정한다. 이것이 이 카드가 말하는 결함 자체다.
- `GainPolyNotAppliedTest.ScalarGainMapIsApplied` — 대조군.

중복 시험을 쓰는 대신 이 세 건을 근거로 인용한다.

## 2. (b) 선택지 — 결정은 리더

호출자 관점의 현재 동작: **적재 성공 → 스칼라 맵 소실 → 이후 모든 게인 보정이 `XPE_ERR_UNSUPPORTED_FORMAT`.** 알림 2건이 나가지만, 적재 호출 자체는 성공을 돌려주므로 "성공했는데 뒤가 안 된다" 는 모양은 남아 있다.

| 선택지 | 무엇이 바뀌나 | 기존 호출자 영향 | 비고 |
|---|---|---|---|
| **A. 적재 시점 거절** — 적용 경로가 없는 형식이면 `load_gain` 이 실패를 돌려주고 **맵을 지우기 전에** 멈춘다 | 적재가 `XPE_OK` → 실패 코드. 기존 스칼라 보정이 **살아남는다** | POLY 파일 적재를 성공으로 기대하는 호출자가 깨진다. **QA-A-37(#140) 이 "POLY 파일이 적재된다" 를 요구로 넣었고**(`PolyFileWrittenByTheApiLoads` 등 `GainPolyLoadTest` 8건) 메타데이터 왕복 시험이 여기 걸린다 | 가장 강한 보호. 다만 #140 과 정면 충돌 |
| **B. 적재하되 스칼라 맵을 지우지 않음** | 다항식은 보관만, 보정은 스칼라로 계속 | 적재 결과는 그대로. 보정이 계속 동작 | **SRS-CALIB-SAFE-003("no partial / mixed calibration")과 충돌한다** — 코드 주석(`xpe_calib_load_gain.cpp:91-94`)이 지우는 이유로 이 요구를 인용하고 있다. 조작자가 고르지 않은 맵으로 프레임을 보정하게 된다 |
| **C. 현상 유지 + 적재 반환값만 구분** | 적재가 `XPE_OK` 대신 "적재됐으나 적용 미구현" 을 뜻하는 코드(신규 또는 기존 중 하나)를 돌려준다 | 반환값을 `== XPE_OK` 로 보는 호출자가 실패로 읽는다 | 보호 수준은 A 와 비슷하되 파일은 적재된 상태로 남아 #140 의 메타데이터 경로는 유지 |
| **D. 적용을 구현** (FUNC-027) | 결함 자체가 사라진다 | 없음 | 카드가 "적용은 그 다음" 으로 뺐다. §3 의 미해결 문제가 선행 |

**세 선택지 모두 요구끼리의 충돌을 건드린다** — A 는 #140(POLY 적재 가능), B 는 SAFE-003(혼합 교정 금지). 그래서 구현 전에 결정이 필요하다는 카드의 판단이 맞다.

## 3. (c) 적용에 필요한 것 — 조사만

### FUNC-027 원문 (`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:182`)

> "System shall support multi-point gain polynomial fitting via `xpe_calib_generate_gain_polynomial()`. Input: array of gain maps at **N ≥ 3 dose levels** (each from FUNC-026), **dose levels array**. Algorithm: For each pixel (x,y), fit polynomial **`G(x,y,E) = Σ(c_k × E^k)`** where k=0..degree-1 (degree ≤ 4). Use least-squares regression. ... polynomial must be **monotone in [E_min, E_max]**."
>
> 근거란: "Clinical X-ray uses multiple **kVp values (50-120 kVp)**. Single gain map at 70 kVp RQA-5 introduces errors at other clinical **energies**. Multi-point polynomial captures **energy-dependent** sensitivity variation."

**요구 안에서 변수가 두 가지로 쓰인다.** 입력은 "dose levels" 인데 모델과 단조성 구간은 **E**(에너지)이고, 근거란은 전부 kVp·에너지를 말한다. 구현도 입력 인자 이름이 `dose_levels` 다(`preprocess_api.h:408`).

**이것이 이슈 #187 의 "선량 D 가 필요하다" 와 다른 결론으로 이어진다:**
- 모델 변수가 **E** 라면, 적용에 필요한 값은 **kVp 이고 `XpeImageMetadata` 에 이미 있다**(`xpe_types.h:110`). 추가 정보가 필요 없다.
- 모델 변수가 **선량** 이라면, §아래대로 메타데이터만으로는 구할 수 없다.

어느 쪽인지 요구가 정하지 않았다. **이것이 적용을 막는 실제 장애물이고, 리더 결정 사항이다.**

### 선량 D 를 kVp·mAs 에서 구할 수 있는가 — 구할 수 없다

`XpeImageMetadata` 가 가진 것(`xpe_types.h:108-116`): `bodyPart`, `kVp`, `mAs`, `SID_mm`, `pixelPitch_mm`, `acquisitionTime`, `flags`. **선량 항목은 없다.**

검출기 입사 선량은 대략 `D ∝ mAs × f(kVp) / SID²` 형태이지만, 이 셋만으로는 값이 정해지지 않는다. 빠진 것:
- **튜브 출력 계수**(같은 kVp·mAs 에서도 장비·타깃 각도·리플에 따라 다르다),
- **여과**(고유 여과 + 부가 필터 — 스펙트럼과 공기커마를 크게 바꾼다),
- **피사체 감쇠**(검출기에 도달하는 것은 피사체를 지난 뒤이고, 그 두께는 메타데이터에 없다).

즉 kVp·mAs·SID 로는 **관구 출구 공기커마의 비례식**까지만 가능하고, 화소가 실제로 받은 선량은 얻을 수 없다. 자동노출제어(AEC) 값이나 선량계 판독이 메타데이터에 들어오면 달라진다 — 지금은 없다.

**검색 범위**: `modules/common/include/xpe/common/xpe_types.h` 전체, `docs/calibration/*.md` 전체(`grep -rn "FUNC-027"`, `dose|exposure|kvp|mAs`), `modules/preprocess/include/` 전체. 이 범위에서 선량을 메타데이터로부터 유도하는 식이나 표는 **없다**.

## 4. 미검증 · 한계

- (b) 를 **구현하지 않았으므로 반증도 하지 않았다**(카드가 결정 뒤로 정했다). 결정이 오면 "되돌리면 다시 조용히 실패" 를 확인하겠다.
- 선택지 A 가 실제로 `GainPolyLoadTest` 8건을 깨는지는 **실행으로 확인하지 않았다** — 코드를 읽고 판단한 것이다. 결정이 A 로 가면 먼저 실행으로 확인하겠다.
- `clients`·`gui` 가 `xpe_calib_load_gain` 의 반환값을 어떻게 쓰는지 보지 않았다(다른 레인 소유). 선택지 A·C 는 그쪽에 영향이 간다.
- FUNC-026(`xpe_calib_generate_gain`)의 dose/energy 용법은 따로 보지 않았다.
- 시험은 `GainPolyNotAppliedTest` 만 돌렸다(`BUILD_EXIT=0`). 전체 ctest 는 이 카드에서 코드를 바꾸지 않았으므로 돌리지 않았다.
