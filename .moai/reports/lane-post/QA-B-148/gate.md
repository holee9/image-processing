# QA-B-148 (#154) — EIT 약분을 걷어냈습니다. 부위 축이 5.05 dB 로 살아났습니다

## 1. 전제 확인 (§1 표)

| # | 전제 | 확인 방법 | 아직 참인가 |
|---|---|---|---|
| 1 | `exposure_index.cpp:103` 이 `ei = eit * (mean / S0_REFERENCE)` | 그 줄을 읽음 | **참** — `:102` 주석 `REQ-ENH-023: EI = EIT * (mean / S0_REFERENCE)`, `:103` 그대로 |
| 2 | `:106` 이 `di = 10*log10(ei / eit)` | 그 줄을 읽음 | **참** |
| 3 | `eit` 가 이 두 줄 밖에서는 결과에 관여하지 않음 | 함수 전체를 읽음 | **참** — `grep -n "eit\|EIT"` 결과 `eit` 는 `:100`(조회), `:103`, `:106` 에만 등장. `*outEI`·`*outDI`·알림 경로는 `ei`/`di` 만 씀 |
| 4 | SPEC `REQ-ENH-023` 이 `K_cal` 식으로 정정돼 있음 | `SPEC-XPE-P1B-ENH/spec.md` §4.5 | **참** — `EI = K_cal * (mean_pixel_value / S0_reference)`, *"SHALL NOT let `EIT` enter the computation of `EI`"*, 그리고 **`REQ-ENH-023a` 신설**(`K_cal = 100.0` 무교정 기본값, IAK 교정 전까지 절대 크기 IEC 적합 주장 금지) |

**4번이 핵심이었고 도착해 있었습니다.** 정정 주석도 함께 있습니다 — 출처가 SPEC 이었고
코드는 그것을 충실히 구현했다는 것, `XPE-ALG-001 §7.3` 이 처음부터 맞았다는 것까지.

**(c) 멈춤 경로는 쓰지 않았습니다** — 4건 모두 참.

## 2. 고친 것

`modules/enhance_basic/src/exposure_index.cpp` — **한 줄**입니다.

```diff
-    float ei = eit * (mean / S0_REFERENCE);
+    constexpr float K_CAL_DEFAULT = 100.0f;
+    float ei = K_CAL_DEFAULT * (mean / S0_REFERENCE);
```

`di = 10*log10(ei / eit)` 는 **안 건드렸습니다.** `lookup_eit`·`kEitTable`·
`REQ-ENH-026` 알림 경로도 그대로입니다.

옛 식을 **주석에 기록으로 남겼습니다** — 무엇이 있었고 왜 틀렸는지, 그리고
**결함의 출처가 SPEC 이었다**는 것. 지우면 다음 사람이 `#155` 처럼 "코드가 틀렸다"
로 읽습니다.

`K_cal = 100` 이 **무교정 기본값**이라는 것도 주석에 박았습니다 — 절대 크기는
IAK 실측 전까지 IEC 적합이 아니고, 지금 복구되는 것은 **상대 거동**입니다.

## 3. 통과 기준 — G1 / G2 / G3

### G1 — 부위 축이 살아났다

명령: `./build/ci-post/bin/xpe_enhance_basic_tests.exe --gtest_filter=*DiSpreads*`

```
  EI spans 123.4..123.4 (x1), DI spread = 5.0515 dB, entries differing from CHEST: 6 of 9
```

| 항목 | 전 (QA-B-147) | 후 |
|---|---|---|
| EI 퍼짐 | 123.4..394.88 (**3.2배**) | **123.4..123.4 (×1)** — 평탄 |
| DI 퍼짐 | **0.00 dB** (비트 동일) | **5.0515 dB** |

**기대값 `10·log10(320/100) = 5.0515` 와 일치.** EI 와 DI 가 **정확히 자리를 바꿨습니다** —
측정값에서 목표가 빠지고, 목표는 비교에만 남았습니다.

### G2 — 표를 바꾸면 출력이 바뀐다 (전 항목)

```
#154 G2 -- DI offset from CHEST vs the table ratio:
  CHEST (EIT 200)    measured 0          expected 0
  HAND (EIT 100)     measured 3.0103     expected 3.0103
  FOOT (EIT 100)     measured 3.0103     expected 3.0103
  ABDOMEN (EIT 250)  measured -0.9691    expected -0.9691
  PELVIS (EIT 250)   measured -0.9691    expected -0.9691
  SPINE (EIT 300)    measured -1.76091   expected -1.76091
  SKULL (EIT 320)    measured -2.0412    expected -2.0412
```

기대값은 **`10·log10(EIT_CHEST / EIT_부위)`** 로 표에서 유도했습니다 —
`K_cal` 과 `mean/S0` 이 차이에서 약분되므로 **모듈이 계산하는 어떤 값과도 무관**합니다.
`#151` 에서 `K_cal` 이 교정돼도 이 단언은 그대로 참입니다.

### G3 — 노출 축이 죽지 않았다

```
  exposure x2: -2.09715 -> 0.913152 (delta 3.0103)
```

기대값 `10·log10(2) = 3.0103` 과 일치. **G2 와 같은 시험 안에 넣었습니다** — 부위 축을
살리면서 노출 축을 죽이는 변경이 통과하지 못하게 하기 위해서입니다.

### 귀결 — `REQ-ENH-026` 경고가 **갈립니다**

같은 노출(mean 6000, EI 600)에서:

| 부위 | EIT | DI | 경고 |
|---|---|---|---|
| HAND | 100 | **7.78151** | 1건 |
| CHEST | 200 | **4.77121** | 1건 |
| SKULL | 320 | **2.73001** | **0건** |

**같은 선량이 손에는 과하고 두개골에는 허용**입니다. 그것이 DI 의 존재 이유이고,
QA-B-147 에서는 셋 다 4.77121 로 동일했습니다.

## 4. 반증 — 카드가 지정한 둘, 전부 터졌습니다

### F1 — `kEitTable` 을 전부 같은 값(200)으로

```
===BUILD=0===                                   (_falsify_F1.log)
  EI spans 123.4..123.4 (x1), DI spread = 0 dB, entries differing from CHEST: 0 of 9
[  FAILED  ] Stage2_DiSpreadsAcrossEveryBodyPart_154
[  FAILED  ] Stage2_EveryTableEntryReachesDi_154
```

**퍼짐이 5.05 → 0.00 dB 로 떨어졌습니다.** G1 이 눈멀지 않았습니다.

### F2 — `SKULL` 항목만 ×1000 (320 → 320000)

```
===BUILD=0===                                   (_falsify_F2.log)
  SKULL (EIT 320)  measured -32.0412  expected -2.0412
[  FAILED  ] Stage2_EveryTableEntryReachesDi_154
```

**정확히 −30.0 dB 이동** — 카드의 예측값 그대로입니다.

두 반증 모두 **`===BUILD=0===` 을 먼저 읽고** 판정했고, 되돌린 뒤 표 수치를
`grep` 으로 확인했습니다(`HAND 100.0f`, `SKULL 320.0f`).

## 5. 시험 — 뒤집기 6건, 갱신 8건, 신설 1건

### 뒤집은 것 (지우지 않음)

| 전 | 후 | 단언 |
|---|---|---|
| `KnownDivergence_EiScalesWithTheTarget` | `Stage2_EiIsIndependentOfTheTarget_154` | `EXPECT_NE(chest.ei, skull.ei)` → `EXPECT_FLOAT_EQ`, 기대 200/320 → **100/100** |
| `KnownDivergence_DiIsIndependentOfBodyPart` | `Stage2_DiSeparatesBodyPartsByTheTableRatio_154` | `FLOAT_EQ(chest.di, skull.di)` → **표 비율과의 일치** |
| `KnownDivergence_DiIsBitIdenticalAcrossEveryBodyPart_154` | `Stage2_DiSpreadsAcrossEveryBodyPart_154` | 비트 동일 → **EI 평탄 + DI 퍼짐 5.05 dB** |
| `KnownDivergence_TheAlertThresholdIsBodyPartIndependent_154` | `Stage2_TheAlertThresholdSeparatesBodyParts_154` | `EXPECT_EQ(3, alerted)` → **`EXPECT_EQ(2, alerted)`** |

파일 머리말의 *"NOTHING IS FIXED HERE"* 문단도 **해소 기록**으로 다시 썼습니다 —
결함의 출처가 SPEC 이었다는 것을 포함해서.

### 신설

`Stage2_EveryTableEntryReachesDi_154` — 표 7항목 전부 + G3. **이 시험은 이전에는
존재할 수 없었습니다**: 옛 공식에서는 모든 쌍의 차이가 항등적으로 0 이었습니다.

### 갱신 (`test_exposure_index.cpp` 8건) — **그리고 왜 이 시험들이 결함을 놓쳤는가**

기대값이 이렇게 쓰여 있었습니다:

```cpp
float expected_EI = EIT_CHEST * (mean / S0_REFERENCE);
float expected_DI = 10.0f * std::log10(expected_EI / EIT_CHEST);
```

**시험이 DI 를 피시험 표현식으로 되짚습니다.** 양변에서 목표가 약분되므로
`EIT` 가 무엇이든 통과합니다 — **상쇄가 자기 시험 묶음을 통과한 이유**입니다.
`K_cal` 과 표에서 **독립적으로** 유도하도록 바꿨고, 그 경위를 파일 상단에 적었습니다.

경계값도 옮겼습니다: `DI = 0` 이 `mean = S0` 에서 `mean = 2·S0` 로,
`|DI| > 3` 이 `mean > 2000` 에서 `mean > ~3990` 으로 — **`EIT` 가 이제 실제 제수**라서
문턱이 움직였습니다.

## 6. 재빌드 확인 (§4)

**주의할 점이 있었습니다**: `xpe_enhance_basic_tests.exe` 의 타임스탬프는
**안 바뀝니다**(18:19:18 고정). 시험 실행 파일은 DLL 을 **동적 링크**하므로,
제품만 고치면 재링크 대상이 아닙니다. 바이너리 갱신의 증거는 **DLL** 쪽입니다.

```
[1/4] Building CXX object ... \src\exposure_index.cpp.obj
[2/4] Linking CXX shared library bin\xpe_enhance_basic.dll
xpe_enhance_basic.dll        2026-09-19_18:39:59   ← 갱신됨
xpe_enhance_basic_tests.exe  2026-09-19_18:19:18   ← 변화 없음(정상)
```

exe 타임스탬프만 봤으면 **"안 지어졌다"** 로 잘못 읽었을 자리입니다. 추가 증거로,
제품을 고친 직후 시험 12건이 **빨강으로 바뀌었습니다** — 낡은 바이너리라면 불가능합니다.

## 7. 범위 밖 (건드리지 않음)

- **EIT 표 수치**: `ALG-001 §7.4` 와 4항목 불일치(Skull 200/320, Hand·Foot 80/100,
  Spine 200/300, Extremity 100/없음). **손대지 않았습니다** — 카드 §5 의 순서 이유
  그대로: 표를 먼저 맞추면 "표를 맞췄는데 출력은 그대로" 가 되어 무엇이 고쳐졌는지
  알 수 없습니다. 구조가 살아난 지금은 표를 바꾸면 **G2 가 즉시 반응**합니다.
- **`K_cal` 절대 교정**: `#151` 급 보류.
- **ROI**: `ALG-001 §7.3` 요구 대 전체 프레임 평균. 별건.

## 8. baseline 귀속 · 미검증 · 잔여 위험

**귀속**: 트리 `dev/postprocess`, `origin/main c6162f3` 병합 후, `85412d7` 위.
SPEC 은 리더의 `image-processing` 체크아웃에서 읽었습니다(`72b86e1` 미푸시).

**미검증**

- **절대 EI 크기는 IEC 적합이 아닙니다.** `K_cal = 100` 은 무교정 기본값이고,
  `EI = 100 × IAK` 와 **형태만** 같습니다. 실측 IAK 없이는 주장하지 않습니다.
  단언한 것은 **상대 거동**뿐입니다.
- **EIT 표 수치가 맞는지 판정하지 않았습니다** — 구조만 고쳤습니다.
- **`enhance_advanced` 는 이번에도 실행하지 않았습니다.** 공식이 다르다는 것은
  코드 읽기이고, 그쪽 DI 가 부위에 반응하는지는 여전히 미측정입니다.
- **알림 문구·심각도는 확인하지 않았습니다** — 건수만 셌습니다.

**잔여 위험**

- **보고되는 EI 값이 바뀝니다.** CHEST 기준 같은 영상에서 `246.8 → 123.4`
  (정확히 `EIT/K_cal = 2` 배). 외부 소비자가 EI 절대값을 기록해 뒀다면 불연속이
  생깁니다 — 다만 그 값은 목표가 섞인 값이었으므로 **비교 가능한 적이 없었습니다.**
- **`|DI| > 3` 경고 빈도가 부위별로 달라집니다.** SKULL 은 덜, HAND 는 더 울립니다.
  이것이 의도한 복구이지만, 현장 문턱을 EI 기준으로 잡아 둔 곳이 있으면 재조정이
  필요합니다.

## 9. 검증

```
===BUILD=0===                          (_build.log)
ctest ci-post: 691/691 통과             (_verify.log, ===CTEST=0===)
한 프로세스 전체: 12 바이너리, 0 실패     (_inprocess.sh)
G1/G2/G3 원문                          (_gates.log)
반증 F1 (표 평탄화) → 퍼짐 0.00, 빨강    (_falsify_F1.log)
반증 F2 (SKULL x1000) → -30.0 dB, 빨강  (_falsify_F2.log)
```

**푸시하지 않았습니다** — 카드 §6 대로 커밋까지만.

---

Refs #154
