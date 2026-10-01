# QA-A-159 (`#223`) — 구조체를 한 번 넓혔습니다. **ABI 단언은 실제로 깨집니다**

카드가 핵심으로 지목한 둘 — §2 의 P/Invoke 확인과 §5 의 ABI 단언 반증 — 을 먼저 적습니다. **§2 에서 범위 밖 발견이 나왔습니다.**

---

## §2 P/Invoke 확인 — **선언이 없습니다. 그런데 더 중요한 것이 있습니다**

### 답: `XpeCalibrationMetrics` 를 받는 C# 선언은 **0건**입니다

| 검색 | 범위 | 결과 |
|---|---|---|
| `XpeCalibrationMetrics` / `CalibrationMetrics` | 저장소 전체, `*.cs *.cpp *.h *.hpp *.py`, `modules/preprocess/` 제외 | **1건** — `clients/.../MainWindow.xaml.cs:1883` 의 `CalibrationMetricsGrid`, **동명의 WPF 컨트롤**이고 이 구조체와 무관 |
| `xpe_verify` | `*.cs` 전체 | **0건** — 이 API 를 P/Invoke 하는 곳이 없습니다 |
| `dsnu` / `residual_noise` | `*.cs` 전체 | **0건** |

**그러므로 덧붙이기는 C# 쪽에서 안전합니다.** `StructLayout` 을 쓰는 C# 파일 10개를 열어 봤지만 이 구조체를 선언한 것은 없습니다.

### 범위 밖 발견 — `clients/` 에 **같은 지표의 C# 별도 구현**이 있습니다 (읽기만 함, 보고)

`clients/ImageProcTest/Services/MetricsComputationService.cs` 가 `DarkBias`·`DSNU_ADU`·`DarkReduction_dB`·`PRNU_CV`·`FlatResidualPct` 를 **C# 에서 독립적으로 계산**합니다. 모듈을 호출하지 않으므로 ABI 영향은 없지만, **같은 이름의 지표가 두 곳에서 다르게 계산됩니다.**

네 가지를 보고합니다(`:55-68` 인용, 고치지 않음):

1. **모집단이 다릅니다 — `FUNC-035` 의 그 문제입니다.**
   `Stats(output)` (`:201-227`)은 **전 화면**을 돕니다. 어두운 ROI 를 고르지 않습니다. `#221` 에서 우리가 없앤 "전 화면 대체" 와 **같은 양**을 `DarkBias` 로 보고합니다.

2. **`abs` 가 없고, 여기서는 무연산이 아닙니다.**
   `Row("DarkBias", correctedStats.Mean, ..., correctedStats.Mean <= 5.0)` — 우리 쪽은 `corrected` 가 `uint16_t` 라 `abs` 가 무연산이지만, 여기 `output` 은 **`ReadOnlySpan<float>`** 입니다. 음수 잔차가 가능하고, 그러면 `-1000 <= 5.0` 이 **참**이 됩니다. **우리 쪽에서 무연산이던 누락이 여기서는 살아 있는 결함입니다.**

3. **`or` 가 없습니다.**
   `DarkBias` 와 `DarkReduction_dB` 가 **독립된 행**으로 각자 합불을 냅니다. `FUNC-016` 의 `or` 관계가 표현되지 않아, `DarkReduction_dB >= 10` 으로 통과할 프레임도 `DarkBias` 행은 빨갛게 보입니다.

4. **`DSNU_ADU <= 20 ADU` 문턱의 출처를 못 찾았습니다.**
   `FUNC-016` 은 `DSNU_ADU` 를 **계산하라고만** 하고 문턱을 정하지 않습니다. `QA-A-152` 가 표시한 "출처 없는 상수" 와 같은 분류로 보입니다. 검색 범위는 `SRS-CALIB-001` + `Protocol.md` 이고, 값(20)과 개념(DSNU) 양쪽으로 찾았습니다.

**`clients/` 는 gui 소유라 읽기만 했습니다.** gui 카드로 나갈 자리라고 봅니다.

---

## §5 ABI 반증 — **단언이 실제로 깨집니다**

카드가 *"안 깨지는 단언은 단언이 아닙니다"* 라고 한 대로, 필드 하나를 `dark_bias` **앞에** 끼워 넣었습니다.

```
xpe_verify_metrics.cpp(36): error C2338: static_assert failed: 'ABI: dark_bias moved'
xpe_verify_metrics.cpp(37): error C2338: static_assert failed: 'ABI: dsnu moved'
...
xpe_verify_metrics.cpp(48): error C2338: static_assert failed: 'ABI: overall_pass moved'
BUILD_EXIT=1
```

**13개 전부 빨감**, 컴파일 단계에서. 확인 후 되돌렸고(`grep -c abi_probe_inserted` → 0) `BUILD_EXIT=0` 으로 복귀했습니다.

이것이 잡는 것은 **디프에서 무해해 보이는 세 가지 편집**입니다 — 필드 삽입, 순서 변경, 크기 변경. 셋 다 소스는 그대로 컴파일되고 바이너리만 조용히 어긋납니다.

---

## §1·§3 구현 — 세 구멍, 한 번의 확장

기존 13개 필드는 이름·형·**오프셋** 그대로이고, 새 필드 셋은 **뒤에** 붙었습니다.

| 구멍 | 새 필드 | 정본 |
|---|---|---|
| (i) 게이트가 쓰고 버림 | `double dark_reduction_db` | `Protocol.md:205` |
| (ii) 정본 이름이 틀린 값을 가리킴 | `double dsnu_adu` | `Protocol.md:204` |
| (iii) 측정/미측정 상태 없음 | `uint32_t measured_mask` | `FUNC-036` |

### (ii) 를 **중복 필드**로 처리한 이유

`dsnu_adu` 는 `residual_noise` 와 **같은 값**입니다. 그런데도 넣었습니다:

정본 값은 이미 계산되어 저장돼 있었지만 **DSNU 를 찾는 사람이 찾지 못할 이름**으로 있었고, 정작 `dsnu` 라는 이름의 필드는 다른 양(변동계수 %)을 담고 있었습니다. `#222` 가 그 대가를 쟀습니다 — 실물 다크 프레임에서 0.51 ADU 라는 **좋은** 잔차가 `dsnu = 129%` 를 만들고, 이름을 읽고 상한을 건 게이트가 좋은 보정을 떨어뜨렸습니다.

**이름이 잘못된 게이트를 유도한 것**이므로, 고칠 자리는 이름 쪽입니다. `dsnu` 의 의미는 카드 §6 대로 **바꾸지 않았습니다**(`#218` 의 `prnu_after` 와 같은 판단). 표류 방지로 **한 변수에서 인접한 두 줄**로 씁니다.

### (iii) 을 **비트마스크**로 고른 이유

`FUNC-036` 이 요구하는 것은 **메트릭별**입니다 — 한 번의 호출이 어떤 것은 재고 어떤 것은 못 잽니다.

| 후보 | 왜 안 골랐나 |
|---|---|
| 필드별 `bool` | 메트릭이 늘 때마다 구조체가 커지고 **그때마다 ABI 변경** |
| 배열 | 인덱스↔메트릭 대응이 헤더 밖에 살아 오독 가능 |
| **비트마스크** ✅ | 한 필드 안에 **32칸 예약** — 다음 메트릭은 ABI 변경이 필요 없음 |

플래그는 `XpeMetricMeasured` 열거로 이름을 줬고, 각 비트가 어느 필드를 덮는지 주석에 적었습니다.

## §4 — `overall_pass = false` 는 **남깁니다**

요구의 *"전용 상태 필드가 생기기 전까지"* 표현은 **하나가 다른 하나를 대체하는 것처럼 읽히는데, 그렇지 않습니다.** 둘은 서로 다른 호출자에게 답합니다:

- **마스크** — "어느 값이 믿을 만한가" 를 묻는 호출자에게
- **`false`** — `overall_pass` 만 읽고 마스크가 생긴 줄도 모르는 호출자에게

마스크가 들어왔다고 `false` 를 걷으면 **바로 그 호출자에게 `#219` 가 다시 열립니다** — 조용히, 코드는 그대로 컴파일되고 여전히 "합격" 이라고 말하면서. 주석에 그 이유로 적었습니다.

---

## 시험 — 네 개

| 시험 | 고정하는 것 |
|---|---|
| `VerifyOffset_PublishesTheDarkReductionTheGateUsed` | (i) 게이트가 쓴 값 = 호출자가 받는 값 |
| `VerifyOffset_DsnuAduIsTheCanonicalStdNotTheCv` | (ii) `dsnu_adu == residual_noise`, `dsnu` 와는 **다름** |
| `VerifyOffset_UnmeasurableFrameReportsNotMeasured` | (iii) 마스크 미설정 **+ `overall_pass` 여전히 false** |
| `VerifyOffset_MeasurableFrameReportsMeasured` | 위의 **대조군** — 마스크가 아예 안 세워져도 통과할 시험을 막음 |

(i) 은 **공식을 다시 유도해 비교하지 않았습니다.** 그러면 양쪽이 함께 틀려도 통과합니다(`#154` 형태). 대신 **게이트 결과에 묶었습니다** — 이 프레임은 bias 절을 떨어뜨리므로, 합격했다면 그것은 이 값이 10 dB 를 넘겼기 때문이고, 필드는 그 합격을 설명하는 값을 보여야 합니다.

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **769 / 769** (직전 765 + 신규 4) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=700`, `PASSED 692`, 실패 0 |
| ABI 단언 반증 | 삽입 시 **13건 전부 컴파일 오류**, `BUILD_EXIT=1` → 되돌림 |
| 깨진 기존 시험 | **0건** |

## 하지 않은 것 (카드 §6)

- `dsnu` **의미 변경 없음** — (a)안 미채택
- `clients/` **수정 없음** — 읽기만, 위 §2 로 보고
- `xpe_verify_pipeline` 의 `compute_robust_mean` 두 호출 — `#220` 에서 남긴 자리

## 미검증 / 잔여 위험

- **마스크를 세우는 자리는 오프셋 경로만 촘촘히 시험했습니다.** gain·defect·pipeline 경로에도 비트를 세웠지만, 그 경로들의 **측정 불가 분기**(`#219` §2 에서 "없다" 로 확인한 것)가 없으므로 미설정 사례를 시험으로 고정하지 못했습니다. 즉 그쪽 비트는 **양성만** 검증됐습니다.
- **오프셋 필드 오프셋 값(0, 8, 16, …)은 이 툴체인(MSVC x64, `#pragma pack(8)`)에서 관측된 값**입니다. 다른 ABI 에서는 단언이 그 자체로 빨개져 알려 줍니다 — 잘못된 통과가 아니라 잘못된 실패 쪽으로 기웁니다.
- `dsnu_adu` 와 `residual_noise` 의 **중복은 의도적이지만 중복입니다.** 한 변수에서 인접하게 쓰는 것이 표류를 막는 전부이고, 기계적 강제는 없습니다.
- **C# 별도 구현의 네 건은 제가 고치지 않았고 시험하지도 않았습니다** — 읽어서 보고한 것입니다. 특히 (2) 의 음수 가능성은 코드를 읽고 판단한 것이지 실행으로 관측한 것이 아닙니다.

🗿 MoAI
