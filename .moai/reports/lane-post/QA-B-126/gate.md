# QA-B-126 게이트 — 요구 27건 최종 점검 (#180 마무리)

코드 변경 없음(`git status` 깨끗, `.bak-*` 2건 제외). 검증 로그: `QA-B-126_verify.log`

**중요 — 제가 본 SPEC 판본**: 이 워크트리의 `spec.md` 는 `37b6f1b` 입니다. 리더가 B-125 반영으로 쓰신 **`d1b23f8` 은 아직 제 트리에 없습니다**(main 미푸시). 따라서 **017·011·025·018 의 최신 문구는 보지 못했고**, 아래 점검은 `37b6f1b` 기준입니다. 그 넷에 대한 지적은 "이미 고치셨으면 해당 없음" 으로 읽어 주십시오.

## 1. 주장 (Claim)

1. **Status 줄이 아예 없는 요구가 4건입니다 — 022 · 024 · 025 · 026.** 넷 다 **위험(HAZ) 연결** 요구입니다.
2. 그 4건은 **실제로는 시험이 있습니다.** 비워 둔 것이지 못 채운 것이 아닙니다.
3. **018 의 "17–40% 덜 뺌" 은 옛 팬텀 값입니다** — 새 팬텀에서는 열평균 봉우리 1.0504(약 5%)입니다.
4. **001 의 문구가 자기모순으로 읽힐 수 있습니다** — "Not implemented" 인데 같은 줄에 "구현 1이 있다" 가 붙어 있습니다.
5. Open/Partial 은 **7건**이고, 그중 **#151(실장비) 대기가 5건**, **범위 밖 판정이 1건**, **DICOM 보류가 1건**입니다.
6. 트리 무변경 상태에서 세 프리셋 + e2e 전부 초록(ci-post **668**), `BUILD_EXIT` 전부 0.

## 2. 증거 (Evidence)

### 2-1. 요구 27건 전수 (SPEC 순서)

`### REQ-GSVG-` 로 세면 **27건**입니다(026 까지 26건 + `019b`). 카드의 "26개" 와 1 차이가 나는 것은 019b 를 세느냐의 문제로 보입니다.

| # | 요구 | SPEC Status (37b6f1b) | 코드·시험 확인 | 판정 |
|---|---|---|---|---|
| 001 | Grid Line Frequency Auto-Detection | Not implemented | `grid_dwt.cpp:232` 스펙트럼 봉우리 검출은 **존재** | **문구 확인 필요**(아래 2-3) |
| 002 | DWT Multi-Scale Decomposition | Implemented (tested) | `Db4DwtReconstructsPerfectly` | 일치 |
| 003 | Gridline Detection per Sub-Band | Implemented (tested) | `SubbandPlacementMatchesHandValues` 외 | 일치 |
| 004 | Gaussian Band-Stop | Implemented (tested) | `GridIsSuppressedByAtLeast40dB` + 반증 | 일치 |
| 005 | Visual Artifact Removal | **Partial** | 잠정 바닥 있음(after/before) | 일치 — **#151 대기** |
| 006 | MTF Preservation | **Partial** | 경계 가로지르는 선 11% | 일치 — **#151 대기** |
| 007 | Grid Frequency Range | Implemented (tested) | 60/103/200 lpi | 일치 |
| 008 | Moire Pattern Removal | **Partial** | 170–186 lpi 잠정 바닥 | 일치 — **#151 대기** |
| 009 | Body Thickness Estimation | Implemented (tested) | `ThicknessInversionRoundTrips` | 일치 |
| 010 | SPR Calculation | Implemented (tested) | `KernelSumIsTheDefaultCap` | 일치 |
| 011 | Scatter Distribution Estimation | Implemented (tested) | + **MC 대비 1.008 (±4%)**, QA-B-125 | 일치 — 한계 문장 필요 |
| 012 | Scatter Subtraction | Implemented (tested) | `CompareToPrimary` | 일치 |
| 013 | Multi-Scale Contrast Enhancement | Implemented (tested) | `UnitGainIsIdentityAndGainRaisesDetail` | 일치 |
| 014 | De-Noising | Implemented (tested) | `DenoiseLowersFlatRegionNoise` | 일치 |
| 015 | CNR Preservation | **Not implemented** | 측정 코드 없음 | 일치 — **#151 대기** |
| 016 | Virtual Grid Ratio Selection | Implemented (tested) | `LoadsWithEveryRatioOfReqGsvg016` | 일치 |
| 017 | Thickness Range | Implemented (tested) | 범위 제한·페이드 + MC 정확도 표 | 일치(리더가 d1b23f8 로 개정) |
| 018 | No Artifacts from Overcorrection | **Partial**, "17–40% 덜 뺌" | **새 팬텀 1.0504 / 1.1928 / 0.0342** | **수치 낡음**(2-2) — **#151 대기** |
| 019 | Processing Time | Measured 713–757 ms | — | 일치 |
| 019b | Virtual Grid Processing Time | Measured 640 ms | — | 일치 |
| 020 | Peak Memory | Met (measured) | 241–384 MB / 512 MB | 일치 |
| 021 | Memory Leak Prevention | Met (measured) | `GsvgEndurance.*` + 가짜 누수 대조 | 일치 |
| 022 | Original Image Protection | **Status 줄 없음** | `GsvgVirtualGridApi.ProcessesAndKeepsTheOriginalOnFailure`(`dst == src`), `GsvgAbiSmoke.…SourceIntact` | **Implemented (tested) 로 채울 것** |
| 023 | DICOM Processing Mark | Not implemented (범위 밖) | gsvg 는 DICOM 미사용 | 일치 — **보류** |
| 024 | Fail-Safe Pass-Through | **Status 줄 없음** | `MalformedConfigFallsBackToPassThrough`, `InitWithNullConfig_DefaultsToPassThrough`, `Lifecycle3072_PassThroughIsByteEqual`, 위 022 시험의 `XPE_ERR_CONFIG_INVALID` + `dst == src` | **Implemented (tested) 로 채울 것** |
| 025 | SPR Clamping | **Status 줄 없음** | `SprCapPreventsOvercorrection`, `KernelSumIsTheDefaultCap`, `LocalKernelSumCapBindsOnCorrectDataToo`, **MC 6.25%**(QA-B-125) | **Implemented (tested) 로 채울 것** |
| 026 | Output Value Range | **Status 줄 없음** | `GsvgAbiSmoke`(헤더에 REQ-026 명시, 65535 포화 0건 단언), `VignetteGainRoundsAndClampsToUint16Range`, `GsvgDegradedMode` `LE 65535` | **Implemented (tested) 로 채울 것** |

### 2-2. 018 의 수치가 낡았습니다

`37b6f1b` 의 018 Status 는 **"계단 경계에서 17–40% 덜 뺍니다(QA-B-95)"** 입니다. 그 값은 **80×80 팬텀**의 세 봉우리(1.170 / 1.344 / 1.395)에서 온 것입니다.

512² 에서 다시 잰 값은 **열평균 봉우리 1.0504, 최악 화소 1.1928, 중앙값 |r−1| 0.0342**(QA-B-123)입니다 — 같은 지표로 말하면 **약 5%**이고, 17–40% 가 아닙니다.

리더가 d1b23f8 에서 018 을 손보셨다고 하셨으니 **이미 반영됐을 수 있습니다.** 아니라면 이 줄이 남습니다.

### 2-3. 001 의 문구

현재: **"Not implemented** — 입력에 DICOM 이 없어 헤더에서 산출하지 않습니다. **구현 1(QA-B-90)은 영상 스펙트럼의 봉우리에서 주파수를 찾습니다**(`src/grid_dwt.cpp:232`)"

한 줄 안에 "구현 안 됨" 과 "구현이 있다" 가 같이 있습니다. 읽는 사람에 따라 갈립니다. 요구 본문이 **DICOM 헤더에서** 산출하라고 한다면 "Not implemented" 가 맞고, **자동 검출**이 요지라면 대안 구현이 있으므로 "Partial" 이 맞습니다.

**제 의견**: DICOM 이 범위 밖으로 결정된 이상, **요구 본문 쪽을 손보는 것**(헤더 의존을 빼거나, 이 요구를 023 처럼 "범위 밖" 으로 명시)이 Status 를 고치는 것보다 정확해 보입니다. **결정은 리더 몫이고, 저는 제안만 합니다.**

### 2-4. Open/Partial 과 무엇이 막고 있는가

| 요구 | 상태 | 막고 있는 것 | 실장비(#151) 없이 닫을 수 있나 |
|---|---|---|---|
| 005 Visual Artifact | Partial | **"보이지 않는다" 의 합격 기준.** 잠정 회귀 바닥(after/before)만 있음 | **아니오** — 임상 판정이 필요 |
| 006 MTF Preservation | Partial | 경계를 가로지르는 선 **11% 손실**이 요구(5%)를 넘음. 완화할지 고칠지 미정 | **아니오** — 요구 개정이면 임상 근거 필요 |
| 008 Moire Removal | Partial | **170–186 lpi 구간의 합격 기준.** 현재는 기록만 | **아니오** |
| 015 CNR Preservation | Not implemented | **물리 격자 기준 영상이 없음.** CNR 측정 코드도 없음 | **아니오** — 기준 영상이 곧 실장비 |
| 018 No Artifacts | Partial | **"인공물 없음" 의 합격 기준.** 잠정 바닥만 있음 | **아니오** |
| 001 Grid Freq Auto-Detect | Not implemented | **DICOM 범위 밖 결정**(사용자). 헤더 경로가 생기지 않음 | **예 — 다만 닫는 방식이 "요구 개정" 임**(2-3) |
| 023 DICOM Processing Mark | Not implemented | **DICOM 범위 밖 + 소비자 없음**(GUI 에 DICOM 쓰기 경로 없음) | **예 — "범위 밖" 으로 닫으면 됨** |

**정리**: Open/Partial **7건** = #151 대기 **5건**(005·006·008·015·018) + 범위 밖 판정으로 지금 닫을 수 있는 것 **2건**(001·023).

여기에 **Status 줄이 빈 4건**(022·024·025·026)은 Open 이 아니라 **기록 누락**입니다 — 시험이 있으므로 채우면 닫힙니다.

### 2-5. `#180` 을 닫을 때 후속으로 갈 목록 (한 줄씩)

| 항목 | 왜 못 닫는가 |
|---|---|
| REQ-005 | 잔여 격자가 "보이지 않는" 수준인지 판정할 임상 기준이 없다 |
| REQ-006 | 경계를 가로지르는 선의 11% 손실을 허용할지 고칠지 정해지지 않았다 |
| REQ-008 | 170–186 lpi 에일리어싱 구간의 허용선이 없다 |
| REQ-015 | CNR 비교의 기준이 될 물리 격자 영상이 없다 |
| REQ-018 | 경계 덜 뺌이 인공물로 보이는 문턱이 정해지지 않았다 |
| 경계 근처 정확도 4.5 mm 너머 | 계단 간격 2.3 mm 팬텀으로는 그 거리의 화소가 존재하지 않는다 |
| σ₁ 설명의 직접 확인 | 커널 항을 하나씩 끄는 실험을 하지 않았다(카드 지시로 보류) |
| REQ-011 의 독립성 | 커널 표가 같은 MC 코드로 적합돼 완전한 독립 검증이 아니다 |
| REQ-025 의 보호 기능 | 이 팬텀이 음수 1차를 만들지 못해, 상한이 막아 주는 상황을 재지 못했다 |

### 2-6. 검증 (타깃 없는 빌드, 트리 무변경)

```
===CI_POST===   ===POST_BUILD=0===   100% tests passed, 0 tests failed out of 668   ===POST_EXIT=0===
===CI_AI===     ===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===CI_DICOM===  ===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
===E2E===       100% tests passed, 0 tests failed out of 28   ===E2E_EXIT=0===
```

## 3. 기준 귀속 (Baseline-attribution)

- Status 인용은 이 워크트리의 `spec.md`(`37b6f1b`)에서 직접 읽었습니다 — **`d1b23f8` 은 보지 못했습니다**
- 시험 이름은 `modules/gsvg/tests/*.cpp` 에서 `TEST(` 로 찾아 확인했습니다(검색 범위: 이 디렉터리)
- 018 의 새 값은 QA-B-123 의 `VGMC512 floor018` 출력이고, 이번 실행에서도 같은 값으로 초록입니다
- 011 의 1.008 은 QA-B-125 의 `VGMC125 REQ-011` 출력입니다

## 4. 미검증 (Gaps)

- **`d1b23f8` 의 실제 문구**를 확인하지 못했습니다. 017·011·025·018 에 대한 제 지적은 그 커밋이 이미 해결했을 수 있습니다
- **022·024·026 의 시험이 요구를 완전히 덮는지**는 시험 이름과 단언 몇 줄로 판단했습니다. 각 시험을 전부 읽어 요구 문장과 한 줄씩 대조하지는 않았습니다
- **HAZ-001~004 와 요구의 대응**이 위험 분석 문서 쪽과 일치하는지는 보지 않았습니다(gsvg SPEC 안만 봤습니다)
- 카드가 하지 말라고 한 셋(팬텀 재생성·σ₁ 확인·합격선)은 손대지 않았습니다

## 5. 잔여 위험 (Residual-risk)

- Status 줄이 빈 4건을 "구현 안 됨" 으로 오해하면 **위험 연결 요구 4건이 미구현으로 집계**됩니다. `#180` 을 닫는 집계에서 이 4건의 분류가 결과를 바꿉니다
- 001 을 Status 만 고쳐 닫으면, **요구 본문은 여전히 DICOM 헤더를 요구**합니다 — 다음 감사에서 같은 지적이 다시 나옵니다
- 018 의 새 값(5%)이 옛 값(17–40%)보다 좋아 보이지만 **팬텀이 다릅니다.** 개선으로 읽으면 안 됩니다(QA-B-124 에서 화소 크기 가설도 기각됐고, 어느 팬텀 성질인지는 모릅니다)
