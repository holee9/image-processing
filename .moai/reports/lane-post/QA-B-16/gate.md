# QA-B-16 — enhance_advanced 커버리지 미달 5파일 보강 (#120)

**레인**: Lane B (`dev/postprocess`)
**증거 원본**: CI 아티팩트 `coverage-coverage-post` (run 34358270943) — `gh run download` 로 확보.
줄 단위 미커버는 `coverage.xml` 에서 추출했다(HTML 이 아니라 XML — 같은 아티팩트, 기계 판독 가능).

**결과**: 기능 케이스 11건 추가, `ci-post` **398/398 무실패**.
**그러나 이 카드의 실질적 수확은 커버리지가 아니라 발견한 결함 2건이다**(§4).

---

## 1. 주장 (Claim)

1. 5개 파일의 미커버 83줄을 **전수 분류**했다 — 추정이 아니라 `coverage.xml` 실측(§2).
2. (b) 옵션/분기 중 도달 가능한 것을 기능 케이스 11건으로 덮었다(§3).
3. **메모리 결함 1건 발견**: `levels` 클램프가 이미지 크기와 무관해 작은 이미지에서
   버퍼를 넘는다. 32×32 는 즉시 폴트, **64×64 는 XPE_OK 를 반환하고 나중에 죽는다**(§4.1).
4. **계약 불일치 1건 발견**: `xpe_calc_exposure_index` 는 널 픽셀 버퍼를 받아들이고
   `XPE_OK` 를 낸다 — `xpe_multiscale_process` 는 같은 입력을 `INVALID_INPUT` 으로 거절한다(§4.2).
5. **5개 파일 중 4개는 이 접근으로 0.85 에 도달할 수 없다**(§5) — 남은 미커버가
   예외/오류 경로라 폴트 주입 없이는 못 닿는다. 근거와 함께 보고한다.
6. 최종 커버리지 수치는 leader 의 CI 재측정 몫이다(카드 명시).

---

## 2. 미커버 줄 분류 (실측)

`coverage.xml` 기준. `exposure_index.cpp` 는 동명 파일이 둘이라 경로로 구분했다 —
`enhance_advanced/src/` 가 0.802, `enhance_basic/src/` 는 1.000(대상 아님).

분류 기준: **(a)** 오류/예외 경로 · **(b)** 옵션/분기 · **(c)** 죽은 코드(구조적 도달 불가)

### 2.1 `enhance_advanced_helpers.cpp` — 66/92 = 0.717, 미커버 26

| 줄 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 64,65 | 레거시 `"levels"` 키 파싱 | (b) | **덮음** |
| 89,90,91 | mfp 파서 `catch (json::exception&)` | (a)→(c) 의심 | 미도달 — §2.6 |
| 156,157 | `"step_size"` 키 파싱 | (b) | **덮음** |
| 161,162,163 | fractional 파서 catch | (a)→(c) 의심 | 미도달 — §2.6 |
| 184,185,186 | collimation `parse` + `is_discarded` | (b) | **덮음**(유효 + malformed) |
| 189-191,194-196,199-201 | `sensitivity`/`min_area_ratio`/`border_margin` | (b) | **덮음** |
| 204 | `return true` | (b) | **덮음** |
| 205,206,207 | collimation 파서 catch | (a)→(c) 의심 | 미도달 — §2.6 |

**26줄 중 18줄을 덮었다.** 남은 8줄은 전부 catch 블록이다.

### 2.2 `multiscale_process.cpp` — 26/36 = 0.722, 미커버 10

| 줄 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 66 | `img->data == nullptr` → INVALID_INPUT | (b) | **덮음** |
| 92,93 | `applyMfpScalar` 실패 시 warn / else | (a) | 미도달 |
| 101,103-108 | `catch (std::exception&)` / `catch (...)` | (a) | 미도달 |

### 2.3 `fractional_process.cpp` — 34/52 = 0.654, 미커버 18

| 줄 | 내용 | 분류 |
|---|---|---|
| 104 | 반복 중 실패 시 `break` | (a) |
| 109 | SAF-100 overshoot 로깅 | (a) |
| 111,112 | 기타 실패 warn / else | (a) |
| 119-127 | `catch (std::runtime_error&)` + SAF-100 문자열 매핑 | (a) |
| 128-135 | `catch (std::exception&)` / `catch (...)` | (a) |

**18줄 전부 (a)** — 덮은 것 0.

### 2.4 `xpe_enhance_advanced.cpp` — 31/39 = 0.795, 미커버 8

| 줄 | 내용 | 분류 |
|---|---|---|
| 53 | config catch 블록의 닫는 `}` (52행 `return` 직후) | **(c)** 구조적 |
| 59 | `xpe_init` 이 OK/NOT_INITIALIZED 외를 반환하는 경로 | (a) |
| 127,129,130,131,132 | `ExposureIndexCalculator::calculate` 의 catch | (a) |
| 133 | 함수 닫는 `}` (모든 경로가 앞에서 return) | **(c)** 구조적 |

### 2.5 `exposure_index.cpp` — 85/106 = 0.802, 미커버 21

| 줄 | 내용 | 분류 | 처리 |
|---|---|---|---|
| 118,121,124,127,132,137 | `validateInputs()` 의 널·포맷·치수 검사 | **(c)** | §2.7 — 래퍼가 먼저 잡아 도달 불가 |
| 153 | `calculateGain` 의 `meta == nullptr` | **(c)** | 래퍼가 널 meta 를 먼저 거절 |
| 67 | `validateResult` 전파 | (a)→(c) | 위 (c) 가 발화 못 하므로 함께 미도달 |
| 158 | `!isfinite(kVp/mAs)` → 기본 게인 | (b) | **덮음** (NaN kVp/mAs) |
| 204 | `validCount == 0` | (b) | **덮음** (전 픽셀 NaN) |
| 222 | `meanPixel <= 0` | (b) | **덮음** (전 픽셀 0) |
| 185 | `calculateMean` 의 `img->data == nullptr` | (b) 도달 가능 | **덮지 않음** — §4.2 |
| 30,89,100,218,230,241,245,253,261 | 각종 축퇴 폴백 | (a) | 미도달 |

### 2.6 catch 블록이 (c) 로 의심되는 근거

helpers 의 세 파서는 모두 `nlohmann::json::parse(json, nullptr, false)` — **비throw 모드**다
(실패 시 `is_discarded()`). 이후 접근은 `contains(...) && is_number...()` 로 가드돼 있어
타입 예외가 나기 어렵다. 즉 `catch (json::exception&)` 은 현재 코드 구조상 도달 경로가
보이지 않는다. **단정하지 않고 "의심" 으로 둔다** — 도달 불가를 증명하려면 모든 입력에 대한
논증이 필요하고, 그건 이 카드의 범위가 아니다.

### 2.7 `validateInputs` 는 래퍼 중복으로 죽어 있다

`xpe_calc_exposure_index`(`xpe_enhance_advanced.cpp:87-`)가 호출 **전에** 널 4개·포맷·치수를
모두 검사한다(QA-B-12 에서 순서를 정리한 그 코드다). 그 뒤 호출되는
`ExposureIndexCalculator::validateInputs` 는 **같은 검사를 다시 한다.** 공개 API 로는
이 6줄에 도달할 수 없다 — 방어적 중복이다. 카드 지시대로 **보고만 한다**(수정 안 함).

---

## 3. 추가한 기능 케이스 (11건)

신규 파일 `modules/enhance_advanced/tests/test_coverage_ext.cpp`,
`CMakeLists.txt` 소스 목록에 1줄 추가.

| 케이스 | 덮는 줄 |
|---|---|
| `MultiscaleAcceptsLegacyLevelsKey` | helpers 64,65 |
| `MultiscaleClampsOutOfRangeGains` | helpers edge/texture/flat/noise 분기 |
| `FractionalAcceptsStepSizeKey` | helpers 156,157 |
| `FractionalClampsOutOfRangeStepSize` | helpers step_size 클램프 |
| `CollimationAcceptsAllConfigKeys` | helpers 184,189-201,204 |
| `CollimationClampsOutOfRangeConfigKeys` | helpers 클램프 분기 |
| `CollimationRejectsMalformedConfig` | helpers 185,186 |
| `MultiscaleRejectsNullPixelData` | multiscale 66 |
| `ExposureIndexNonFiniteTechniqueFactorsStayFinite` | exposure 158 |
| `ExposureIndexAllNonFinitePixelsStayFinite` | exposure 204 |
| `ExposureIndexZeroSignalStaysFinite` | exposure 222 |

단언은 전부 **문서화된 계약**이다 — 설정 키를 받으면 `XPE_OK`, 깨진 JSON 은
`CONFIG_INVALID`, 널 픽셀 데이터는 `INVALID_INPUT`, 축퇴 입력에서도 출력은 유한
(REQ-ADV-032). 관측값에 기대치를 맞춘 것이 아니다.

성능 예산 테스트는 추가하지 않았다(카드 금지 — 커버리지 실행에서 제외됨).

---

## 4. 발견한 결함 2건

### 4.1 `levels` 클램프가 이미지 크기를 무시한다 — 메모리 손상

케이스를 쓰다가 `MultiscaleClampsOutOfRangeGains` 가 **접근 위반(0xc0000005)** 으로 죽었다.
표준 프로브로 키를 하나씩 분리했다(`_probe_cfg.log`):

```
levels=99            32x32 -> 크래시(0xc0000005)
edge_gain=42         32x32 -> rc=0
texture_gain=-3      32x32 -> rc=0
flat_gain=7.5        32x32 -> rc=0
noise_threshold=999  32x32 -> rc=0
levels=4             32x32 -> rc=0
```

원인: `levels` 는 `XPE_MFP_MAX_LEVELS`(=8, `internal.h:42`)로 클램프되지만 **이미지 크기와
무관하다.** 32×32 에 8단계 피라미드를 쌓으면 버퍼를 넘는다.

**그리고 이 결함의 진짜 위험은 크래시가 아니다.** 처음에 크기별 프로브에서
64×64 가 `rc=0`, `exit=0` 이라 "64 이상은 안전" 으로 적었는데 **틀렸다.**
그 입력으로 gtest 케이스를 만들자 **어서션은 통과하고 프로세스가 종료 시점에 죽었다**:

```
[  PASSED  ] 183 tests.        <- 전부 통과
===FULL_EXIT=-1073741819===    <- 그런데 프로세스는 접근 위반으로 종료
```

케이스를 하나씩 걸러 `MultiscaleClampsOutOfRangeLevels`(64×64, levels=99)가 유일한
원인임을 확인했다(`_iso.bat` / `_iso2.bat` 출력). **힙 손상은 즉시 폴트하지 않는다** —
`rc == XPE_OK` 와 `exit == 0` 은 안전의 증거가 못 된다는 뜻이고, 이것이 내 초기 판단이
틀린 이유다.

정리:

| 이미지 | 관측 |
|---|---|
| 32×32 | 호출 중 접근 위반 |
| 64×64 | **`XPE_OK` 반환 후 프로세스가 나중에 폴트** |
| 128 이상 | 폴트 미관측 — 위와 같은 이유로 **안전하다고 주장하지 않는다** |

조치: 메모리를 손상시키는 입력은 **활성 테스트에서 제거**했다(실패가 아니라 실행 전체를
무너뜨리므로). 결함은 `DISABLED_MultiscaleMaxLevelsOnSmallImage` 로 관측값과 함께
코드에 남겼다. 수정(레벨 수를 이미지 크기로 제한하거나 조합을 거절)은 별도 카드 사안이다.

### 4.2 `xpe_calc_exposure_index` 가 널 픽셀 버퍼를 받아들인다

`exposure_index.cpp:185` 를 덮으려다 발견했다. 래퍼는 `img->data` 를 검사하지 않고
(`xpe_enhance_advanced.cpp` 는 널 포인터 4개·포맷·치수만 본다), `calculateMean` 이
`data == nullptr` 를 만나면 0 을 돌려준다. 실제 동작을 프로브로 관측했다(`_probe_ei.log`):

```
exposure_index with data=nullptr: rc=0 ei=0.0001 di=-63.9794
```

**같은 모듈의 `xpe_multiscale_process` 는 동일 입력을 `INVALID_INPUT` 으로 거절한다**
(`multiscale_process.cpp:66` — 이번에 덮은 그 줄이다). 두 진입점이 갈린다.

이 줄은 덮을 수 있었지만 **덮지 않았다.** 덮으려면 `EXPECT_EQ(..., XPE_OK)` 를 써야 하고,
그것은 의심스러운 동작을 계약으로 굳히는 일이다 — 카드 원칙 4(관측값에 기대치를 맞추지
말 것)에 정면으로 걸린다. 판정 후 별도 카드로 처리할 사안이다.

---

## 5. 5개 파일 중 4개는 이 접근으로 목표에 못 닿는다

| 파일 | 현재 | 이번에 덮은 줄 | 도달 가능 상한(추정) | 0.85 도달 |
|---|---|---|---|---|
| `enhance_advanced_helpers.cpp` | 0.717 | 18 | (66+18)/92 = **0.913** | **가능** |
| `multiscale_process.cpp` | 0.722 | 1 | (26+1)/36 = 0.750 | 불가 — 남은 9줄이 (a) |
| `exposure_index.cpp` | 0.802 | 3 | (85+3)/106 = 0.830 | 불가 — (c) 7줄 + (a) 다수 |
| `xpe_enhance_advanced.cpp` | 0.795 | 0 | 0.795 | 불가 — (a)6 + (c)2 |
| `fractional_process.cpp` | 0.654 | 0 | 0.654 | 불가 — 18줄 전부 (a) |

**막는 것은 예외/오류 경로다.** `catch (...)` 안쪽을 덮으려면 하위 구현이 던지게 만들어야
하고, 그건 폴트 주입(모의 객체·테스트 훅)이 필요하다 — 카드가 요구한 "기능 케이스" 범위 밖이다.
`exposure_index` 는 여기에 더해 (c) 7줄(래퍼 중복 §2.7)이 분모에 남아 있어,
그 중복을 제거하지 않으면 산술적으로도 0.85 가 빠듯하다.

**판단 요청**: (a) 폴트 주입 도입, (b) `validateInputs` 같은 방어적 중복 제거,
(c) 파일별 임계값을 예외 경로 비중에 맞게 조정 — 셋 다 이 카드 범위 밖이다.

---

## 6. 재실측

| 항목 | 결과 | 로그 |
|---|---|---|
| 스위트 단독 | **182 passed / 0 failed**, 1 DISABLED, `FULL_EXIT=0` | `_full.log` |
| `cmake --preset ci-post` | `CFG_EXIT=0` | `_verify.log` |
| `ctest` 전체 (ci-post) | **398 passed / 0 failed**, `ALL_EXIT=0` | `_verify.log` |

## 7. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` 전체 | 387 (`QA-B-12/gate.md` §6) | **398** | +11 = 활성 신규 케이스 |
| enhance_advanced 스위트 | 171 (`QA-B-12`) | **182 실행 + 1 DISABLED** | +11 / +1 |
| 파일별 커버리지 | 0.717 / 0.722 / 0.654 / 0.795 / 0.802 (CI run 34358270943) | **미측정** | leader CI 재측정 |
| 프로세스 종료 코드 | 0 (QA-B-12 이전) | **0** | 중간에 한 번 깨졌다가 §4.1 로 복구 |

## 8. 미검증 (Gaps)

- **커버리지 수치를 로컬에서 재측정하지 않았다.** 로컬 OpenCppCoverage 는 x86 설치본만
  있고 x64 대상 계측 가능 여부를 확인하지 않았다. 카드가 허용한 경로(leader CI 재측정)를 택했다.
  **따라서 §5 의 "도달 가능 상한" 은 줄 수 산술이지 측정값이 아니다.**
- **덮었다고 표기한 줄이 실제로 커버됐는지 확인하지 않았다.** 케이스가 그 코드 경로를
  지난다는 것은 소스 판독으로 확신하지만, 줄 단위 확인은 CI 재측정에서만 가능하다.
- **§2.6 의 catch 블록 도달 불가는 "의심" 이지 증명이 아니다.**
- **128×128 이상이 안전한지 확인하지 않았다**(§4.1). 폴트 미관측은 힙 손상의 부재를
  뜻하지 않는다 — 64×64 가 정확히 그 반례다.
- **§4.2 의 널 데이터 동작이 다른 진입점에도 있는지 전수 확인 안 했다.**
  `xpe_detect_collimation` / `xpe_fractional_process` 는 보지 않았다.
- **비-MSVC / Linux 미검증.** CI 커버리지는 Debug 구성인데 로컬 확인은 RelWithDebInfo 다.

## 9. 잔여 위험 (Residual risk)

- §4.1 은 IEC 62304 Class B 모듈의 **메모리 손상**이다. `DISABLED_` 케이스가 자리를
  지키지만, 후속 카드가 없으면 잠든다. 현재 프로덕션 호출자가 `levels` 를 설정에서
  받는다면 작은 이미지에서 재현될 수 있다.
- §4.2 는 널 버퍼를 조용히 받아들여 그럴듯한 EI/DI(0.0001 / -63.98)를 만든다.
  호출자가 이 값을 표시하면 **오류가 아니라 측정값처럼 보인다.**
- 이번에 추가한 케이스는 축퇴 입력에서 "유한한 출력" 만 단언한다. 값의 **정확성**은
  단언하지 않는다 — 계약이 값을 규정하지 않기 때문이며, 그만큼 회귀 감지력도 약하다.

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 분류표 | PASS | §2 — 5파일 83줄 전수, (a)/(b)/(c) 구분 + 근거 |
| 추가 케이스 diff | PASS | §3 — 신규 파일 11 케이스 + CMakeLists 1줄 |
| ctest 재실측 | PASS | §6 — 398/398, 스위트 182/182 |
| footer `Refs #120` | PASS | 커밋 메시지 |
| 최종 수치 | 해당 없음 | 카드 명시대로 leader CI 재측정 |
