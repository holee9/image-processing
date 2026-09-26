# QA-B-145 (#155 2단계) — 모델이 답에 닿습니다. 하드웨어 없이 검증했습니다

## 1. 주장 (Claim)

1. **`j → L`(Eq 7-1)을 이었고, `L → DDL` 을 입력 배열의 역산으로 바꿨습니다.** 상쇄가
   사라졌습니다 — LUT 은 더 이상 직선 램프가 아닙니다.
2. **하드웨어 없이 정확성을 단언했습니다.** 감마 2.2·1.8 합성 특성 곡선을 넣고,
   표준에서 **해석적으로 유도한 LUT** 과 비교했습니다. 잔차 **72 / 65535**(0.11 %)와
   **48 / 65535**, 평균 0.7. 두 감마의 LUT 은 **4830** 만큼 다릅니다(반증).
3. **게이트는 지우지 않고 뒤집었습니다.** `Stage1_LutIsStillTheStraightRamp…` →
   `Stage2_LutIsNoLongerTheStraightRamp_155`, `EXPECT_LE(worst, 1)` → `EXPECT_GT(worst, 1)`.
   같은 이유로 **결함을 붙들고 있던 시험 7건 전부**를 뒤집었습니다(§4).
4. **헤더 계약을 고쳤습니다** — `display_api.h` 의 *"only the minimum and maximum … are
   used"* 는 지금 거짓입니다.
5. **하드코딩 호출자는 고치지 않았고 전수로 셌습니다**(§6). 새 계약에서 **틀린 입력**입니다.

## 2. 구현

`presentation_lut.cpp:xpe_gsdf_calibrate`.

| 단계 | 전 | 후 |
|---|---|---|
| 범위 | 배열 전체 min/max 훑기 | `[0]` 과 `[count-1]` — 곡선의 양 끝 |
| `j` 일정 간격 | 있음(그대로) | 있음(그대로) |
| `j → L` | **없음** | **Eq 7-1**(ln j 유리함수, double) |
| `L → DDL` | `t` 를 자기 자신에서 재구성 → 약분 | **입력 배열 역산**, 표본 사이 선형보간 |

`t = (target_jnd − jnd_min)/jnd_range == i/1023` 이던 자리가 사라졌습니다. 그 한 줄이
`#155` 였습니다.

Eq 7-1 계수는 시험이 **Table B-1 열 점 + Eq 7-2 왕복**으로 검산합니다 —
전사했으니 믿는 것이 아니라 확인해서 씁니다.

## 3. 대조군 — 하드웨어 없이 정답을 아는 방법

합성 감마 디스플레이 `L(d) = Lmin + (Lmax−Lmin)(d/DDLmax)^γ` 를 **배열로** 넣습니다.
모듈은 표본만 보고, 정답은 표준에서 닫힌 형태로 나옵니다:

```
j_i = j(Lmin) + (i/1023)(j(Lmax) − j(Lmin))      Eq 7-2
L_i = L(j_i)                                      Eq 7-1
d_i/DDLmax = ((L_i − Lmin)/(Lmax − Lmin))^(1/γ)
```

`Stage2_ReproducesTheAnalyticLutForSyntheticGammaDisplays_155`,
0.5..500 cd/m², 257 표본:

| γ | 해석적 정답과의 최대 차 | 평균 | 직선 램프와의 거리(대조) |
|---|---|---|---|
| **2.2** | **72** / 65535 (0.11 %) | 0.67 | 17094 |
| **1.8** | **48** / 65535 (0.07 %) | 0.71 | 13180 |

**γ 2.2 대 γ 1.8: 최대 차 4830** — 모듈이 곡선을 무시하면 둘이 같아지므로, 이것이
반증입니다. 감마를 둘 쓴 이유가 그것입니다.

**잔차의 정체는 오차가 아니라 보간**입니다. 모듈은 표본 사이를 직선으로 읽고
해석해는 그러지 않습니다. 최대 차가 index 0~1(가장 어두운 쪽, 곡률이 가장 큰 곳)에
있는 것이 그 서명입니다. 허용치 **100** 은 측정값 72/48 에서 잡았습니다 — 추측이
아니라 측정이고, 그 경위를 시험 주석에 적었습니다.

## 4. 게이트 은퇴 = 증거. 7건을 지우지 않고 뒤집었습니다

전부 *"say how, and retire this case"* 라고 스스로 적어 둔 시험들입니다.

| 시험 (전 → 후) | 단언 |
|---|---|
| `KnownDivergence_LutIsALinearRamp` → `Stage2_LutIsNoLongerALinearRamp_155` | `LE(maxDev,1)` → `GT(maxDev,1)` |
| `…StepSizeIsConstantUpToRounding` → `Stage2_StepSizeVariesAcrossTheCurve_155` | `LE(max−min,1)` → `GT` |
| `…MeasurementsBarelyChangeTheCurve` → `Stage2_MeasurementsMoveTheCurve_155` | `LE(maxDelta,1)` → `GT` |
| `…LuminanceSweepDoesNotMoveTheCurve_155` → `Stage2_LuminanceSweepMovesTheCurve_155` | `LE(dev,1)`·`LE(worstPair,1)` → `GT` |
| **`Stage1_LutIsStillTheStraightRamp…_155`** → **`Stage2_LutIsNoLongerTheStraightRamp_155`** | **`EXPECT_LE(worst, 1)` → `EXPECT_GT(worst, 1)`** |
| `…OnlyTheLuminanceEndpointsAreUsed_155` → `Stage2_TheInteriorOfTheCurveReachesTheOutput_155` | `EQ(0, worst)` → `GT(worst, 0)` |
| `ParameterDependency.KnownDivergence_GsdfLuminanceDoesNotReachTheOutput` → `Gsdf_LuminanceReachesTheOutput_155` | `LE(maxDelta,1)` → `GT` |

게이트 뒤집기 diff(한 줄):

```
-TEST(GsdfCharacterization, Stage1_LutIsStillTheStraightRampAfterTheCoefficientFix_155) {
+TEST(GsdfCharacterization, Stage2_LutIsNoLongerTheStraightRamp_155) {
-        EXPECT_LE(worst, 1)
+        EXPECT_GT(worst, 1)
```

**두 건은 더 손댔습니다.**

- `Stage2_TheInteriorOfTheCurveReachesTheOutput_155` 에서 **순서 뒤섞은 변형
  `{500,3,1,111,7}` 을 뺐습니다.** 새 계약에서 그것은 "다른 내부" 가 아니라 **호출자
  오류**입니다. 변형이 아닌 것을 변형으로 세면 안 됩니다.
- `KnownDivergence_ShippedLutAgainstTheStandardCurve_155` →
  `Stage2_ShippedLutAgainstTwoAssumedDisplayCharacteristics_155` 로 **이름을
  고쳤습니다.** 그 시험의 기준 `StandardLut()` 은 디스플레이 특성을 **가정**해야
  했습니다(당시 API 에 특성이 없었으니까). 이제 배열이 특성이므로 그 수치(17.4 % /
  40.2 %)는 *"정답과의 거리"* 가 아니라 *"가정한 두 디스플레이와의 거리"* 입니다.
  정답과의 거리는 §3 이 직접 잽니다.

## 5. 결과가 무엇을 말하는가

`Stage2_LuminanceSweepMovesTheCurve_155` 의 여섯 설정, 직선 램프와의 거리:

| 범위 | 거리 (전: ≤1) |
|---|---|
| 80..120 (1 decade 미만) | 2980 |
| 10..100 | 4854 |
| 1..500 | 3990 |
| 0.5..5000 | 10663 |
| 0.01..10000 | 16720 |
| 0.05..0.5 | 2201 |

범위가 넓을수록 커집니다 — GSDF 곡선이 6 decade 에 걸쳐 더 굽기 때문이고, 그것이
바로 램프였을 때 사라졌던 것입니다.

## 6. 할 일 (c) — 하드코딩 호출자 **전수**

검색 범위: 저장소 전체, `grep -rn "gsdf_calibrate|GsdfCalibrate"` (build 제외).
**광도 배열을 실제로 넘기는 호출자는 하나뿐입니다.**

| 위치 | 무엇을 넘기는가 | 새 계약에서 |
|---|---|---|
| `gui/ImageProcTest/Services/RealXpeBackend.cs:165-168` | **하드코딩** `{0.05, 1.0, 10.0, 100.0, 400.0}`, `GsdfEnabled` 일 때만 | **틀린 입력.** 등간격 구동 준위에서 잰 값이 아니라 **10배씩 뛰는 로그 격자**입니다 |
| `clients/ImageProcTest/Diagnostics/XpeDisplayVersionProbe.cs:118-126` | `(IntPtr.Zero, 0, IntPtr.Zero)` — **널 가드 스모크**, `INVALID_INPUT` 만 확인 | 영향 없음. 광도를 넘기지 않습니다 |
| `modules/display/tests/**` (5 파일) | 시험 입력 | 이번 카드에서 정리했습니다 |
| `docs/**`, `.moai/specs/**`, `CHANGELOG.md` | 서명 인용 | 코드 아님 |

**그 호출자가 무엇을 하려던 것인가**: 5점은 *"어두운 곳부터 밝은 곳까지 대표
광도"* 로 읽힙니다 — **끝점 둘만 쓰이던 옛 계약에서는 맞는 사용법**이었습니다
(0.05 가 최소, 400 이 최대). 중간 셋은 어차피 버려졌으니 간격이 문제되지 않았습니다.
새 계약에서는 같은 배열이 *"DDL 0, 16384, 32768, 49152, 65535 에서 각각 0.05, 1,
10, 100, 400 cd/m² 를 낸다"* 는 뜻이 되고, 그건 측정한 적 없는 주장입니다.

**이 카드에서 고치지 않았습니다.** 고치려면 **실제 디스플레이 측정**이 있어야 하고,
그건 `#151` 과 같은 종류의 차단입니다. 리더가 묶어 주십시오.

## 7. SPEC 문구 (리더가 적용, 제가 확인)

**`REQ-DISP-025` 교체안:**

> **REQ-DISP-025**: WHEN `xpe_gsdf_calibrate` is called with the display's measured
> characteristic curve, the system SHALL compute a DICOM PS3.14 GSDF-compliant
> Presentation LUT whose 1024 entries are spaced equally in JND index across the
> measured luminance range, each entry holding the digital driving level whose
> **measured** luminance satisfies the GSDF at that P-Value, and populate
> `outParams->lutData[0..1023]` with the resulting uint16 values.

**순서 계약 — 새 요구사항(번호는 리더 판단; `REQ-DISP-029` 로 제안):**

> **REQ-DISP-029**: The `luminanceValues` array SHALL be the display's
> characteristic curve sampled at **equally spaced driving levels**: element `i`
> SHALL be the luminance in cd/m² measured at `DDL_i = i / (count − 1) × 65535`,
> and the values SHALL be non-decreasing in `i`. All elements SHALL be used —
> the interior samples define the curve that the GSDF-required luminances are
> inverted against. The system does NOT detect a violation of this contract;
> an array not measured at equally spaced, ascending driving levels yields
> `XPE_OK` and an incorrect LUT.

마지막 문장은 빼지 마십시오 — 검출하지 않는다는 것이 **지금 코드의 사실**이고,
`display_api.h` 의 기존 *"degenerate input is silently coerced, not rejected"* 주석과
같은 성질입니다.

## 8. 화면에 닿는 경계

**이 변경은 표시 화소를 바꿉니다.** LUT 이 직선 램프에서 GSDF 곡선으로 바뀌었고,
1..500 cd/m² 설정에서 최대 3990/65535 만큼 움직입니다.

**다만 임상 적합 주장은 아직입니다.** 그러려면 호출자가 **실제 측정 곡선**을 넘겨야
하고 §6 이 그것이 아님을 보입니다. 지금 GUI 를 통해 나오는 LUT 은 *"하드코딩된 5점을
등간격 구동 준위 측정으로 해석한"* 결과입니다 — **구현이 맞은 것과 임상적으로 옳은
것은 다른 문턱**이고, 후자는 §6 의 차단이 풀려야 넘습니다.

## 9. baseline 귀속 · 미검증 · 잔여 위험

**귀속**: 트리 `dev/postprocess`, `origin/main 22851e1` 병합(`d1352e1`) 이후.

**미검증**

- **실제 디스플레이 측정은 없습니다.** 검증은 전부 합성 특성 곡선입니다. 합성 곡선이
  실제 패널의 거동을 대표하는지는 **이 카드가 답하지 않습니다** — 답한 것은
  "정답을 아는 입력에서 모듈이 그 정답을 낸다" 입니다.
- **식의 이미지 원문은 여전히 못 봤습니다**(QA-B-142 와 동일). Eq 7-1 의 형태는
  Table B-1 열 점과 Eq 7-2 왕복이 지지합니다.
- **`count` 가 작을 때의 품질을 재지 않았습니다.** 대조군은 257 표본입니다. 2 표본
  호출(`GsdfCalibrate_MinCount2`)은 통과하지만 그때 곡선은 직선 두 점이므로
  **구동 준위에 선형인 디스플레이를 가정하는 것과 같습니다**. 몇 점부터 충분한지는
  재지 않았습니다.
- **비오름차순 입력을 거부하지 않습니다.** 계약 위반은 `XPE_OK` 와 함께 통과합니다.
  §7 마지막 문장이 그것을 SPEC 에 적자는 제안입니다.

**잔여 위험**

- 허용치 **100** 은 이 플랫폼의 72/48 에서 잡았습니다. 다른 컴파일러의 `pow`/`log`
  구현이 다르면 흔들릴 수 있습니다 — 구조적 차이(수천 카운트)와는 두 자릿수 떨어져
  있으므로 빨강이 나면 그것은 보간이 아니라 **구조가 바뀐 것**으로 읽으십시오.
- GUI 경로가 지금 **틀린 입력**을 넘기므로, 이 커밋 뒤 GUI 가 만드는 LUT 은
  이전보다 "더 GSDF 같지만" 근거 없는 곡선입니다. §6 이 풀리기 전까지 그 상태입니다.

## 10. 검증

```
===BUILD=0===                         (_build5.log)
ctest ci-post: 684/684 통과            (_verify.log, ===CTEST=0===)
한 프로세스 전체: 12 바이너리, 0 실패    (_inprocess.sh)
```

---

Refs #155
