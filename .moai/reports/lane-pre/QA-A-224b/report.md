# QA-A-224b — Codex #82 보류: 0 분기가 유효하지 않은 보정 영상을 합격시키는 회귀 (Refs #242)

기준: dev/preprocess `4bf25d98`(QA-A-221c) 위. 코드 `xpe_verify_metrics.cpp`, 헤더 `preprocess_api.h`, 시험 `test_verify_metrics.cpp`. 증거: `evidence/`. 푸시하지 않음.

## 결론

| # | Codex #82 보류 | 결과 |
|---|---|---|
| 1 | (높음) 평균 ≤ 0·NaN 이 만든 `prnu_after = 0` 을 새 0 분기가 "완벽한 보정"으로 읽어 합격 | **회귀였고 고쳤다.** 완벽한 보정 = 보정 후 프레임이 **측정 가능**(평균이 유한·양수)하고 퍼짐이 **정확히 0**. 그 밖은 측정 불가로 **판정 실패**·개선 0.0·측정 비트 해제 |
| 2 | (중간) 보정 전후가 모두 평탄하면 개선 없이 합격 | **유지**(리더 결정). 헤더 `@note` 와 gain 의 flat→flat **합격 고정 시험**을 넣었다 |
| 3 | (낮음) 200 dB 는 완벽의 별도 표지가 아니다 | 고치지 않고 헤더 필드 문서에 한 줄 |

그리고 이 카드에서 **새로 찾은 것 둘**: `compute_flatness` 가 NaN 화소에서 히스토그램 밖에 쓰는 **메모리 안전 결함**(고쳤다, §2), 보정 전 영상이 측정 불가인 같은 계열의 **이전부터 있던 합격 경로**(고치지 않았다, §5).

## 1. 회귀 — 내가 224 에서 만든 것

### 1.1 무엇이 틀렸나

QA-A-224 는 `prnu_after == 0` 에 "무한 개선 = 200 dB" 라는 뜻을 주었다. 그런데 이 함수는 원래 **보정 후 평균이 0 이하이거나 NaN 이면** 비율이 의미 없어 `prnu_after` 를 0 으로 둔다. 그래서 "측정 불가" 의 자리표시 0 이 "완벽" 으로 읽혔다. Codex 의 재현: 원본 950/1050 교대(PRNU ≈ 5%), 게인 1.0, 보정 후 전부 `-1000.0f` → `overall_pass = true`(이전 코드는 불합격). 224 의 시험은 양의 평균·유한 화소만 썼으므로 이 입력을 건드리지 않았다. 224 의 보고서는 `NaN` 잔차를 지키려고 `== 0.0` 을 골랐다고 적었으나 **NaN 이 아니라 평균 ≤ 0 이 같은 자리를 채운다는 것은 보지 못했다.**

### 1.2 수정 (`xpe_verify_gain`)

- `after_measurable = isfinite(mean_after) && mean_after > 0`, `after_perfect = after_measurable && std_after == 0`.
- 무한 개선(200 dB)은 `prnu_before > 0 && after_perfect` 일 때만.
- `overall_pass` 에 `after_measurable &&` 를 더했다 — 보정 후 영상이 측정 불가면 다른 게이트가 자리표시 0 에서 무엇을 읽든 불합격이다. 여기에 **"이미 평탄" 예외**(`prnu_before < 0.01 && prnu_after < 0.01`)로 들어오는 경로도 닫힌다: 보정 전이 평탄이고 보정 후가 `-1000` 인 영상은 **이전 코드도 합격**시켰다(§3).
- `measured_mask`: 항상 `GAIN_COVERAGE`, `after_measurable` 일 때만 `PRNU | SNR` — 측정 불가인 값을 측정됐다고 표시하지 않는다(SRS-CALIB-FUNC-036).

### 1.3 시험 (신규 5건, 실제 함수)

수정 전(`evidence/10`): 신규 5건 중 **4건 빨강**, 통제 1건 초록. 수정 후(`evidence/11`): `*Verify*` 45건 전부 통과.

| 시험 | 입력 | 수정 전 |
|---|---|---|
| `VerifyGain_AllNegativeCorrectedFrameDoesNotPass` | 보정 후 전부 `-1000` (Codex 재현) | **빨강** |
| `VerifyGain_AllZeroCorrectedFrameDoesNotPass` | 보정 후 전부 0 (평균 0) | **빨강** |
| `VerifyGain_ANanPixelInTheCorrectedFrameDoesNotPass` | 평탄 영상에 NaN 화소 하나 | **빨강**(충돌, §2) |
| `VerifyGain_AnAlreadyFlatPanelWithAnUnmeasurableCorrectedFrameDoesNotPass` | 보정 전 평탄, 보정 후 `-1000` | **빨강**(이전 코드도 합격) |
| `VerifyGain_FlatBeforeAndFlatAfterPassesWithoutAnyImprovement` | 평탄 → 평탄 | 초록(통제, 합격 고정) |

앞의 넷은 같은 단언(`expectUnmeasurableAfter`)을 쓴다: 불합격, `snr_improvement_db == 0.0`(개선을 주장하지 않는다), `measured_mask` 에 `XPE_METRIC_PRNU`·`XPE_METRIC_SNR` 없음. 224 의 `VerifyGain_PerfectCorrectionPasses`(완벽한 보정은 통과)는 그대로 초록이다.

### 1.4 반증 (한 번에 하나, 전체 빌드, `*Verify*` 실행; `evidence/30_arm_*`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| a1 0 분기를 다시 `prnu_after == 0` 으로(내 224 의 조건) | **음수·0·NaN 셋**(`snr == 0.0` 단언이 200 을 잡는다). 평탄-전 시험은 초록 — 판정은 아래 게이트가 가리기 때문 |
| a2 `overall_pass` 에서 `after_measurable &&` 제거 | **평탄 전 + 음수 후 하나만**(다른 셋은 `snr_improved` 가 0.0 으로 가려 준다) |
| a3 `measured_mask` 를 다시 무조건 설정 | 넷(음수·0·NaN·평탄-전 음수) |
| a4 a1 + a2 (Codex #82 의 원래 상태) | 넷 — **Codex 의 재현이 시험으로 잡힌다** |
| a5 `compute_flatness` 의 비유한 검사 제거 | NaN 시험(충돌) |

a1 과 a2 는 서로를 부분적으로 가린다(a1 만 되돌리면 판정이 아니라 **필드**가 틀리고, a2 만 되돌리면 평탄-전 경로만 샌다). 그래서 둘을 각각 별도 단언이 지킨다. a1·a4 의 첫 빌드는 `/WX`(미사용 변수)로 실패해 손상을 의미는 같게(`prnu_after == (after_perfect ? 0.0 : 0.0)`) 바꿔 다시 돌렸다.

## 2. 새로 찾은 것 — `compute_flatness` 의 메모리 안전 결함

NaN 화소 시험이 **수정 전부터** 접근 위반(`0xc0000005`)으로 죽었다(`evidence/10`). 원인: `compute_flatness` 가 NaN 을 `static_cast<int>((v - min) / bin_width)` 로 바꾸면 INT_MIN 이 되어 히스토그램의 아주 먼 곳을 가리킨다(범위 밖 쓰기). 보정 후 영상에 NaN 화소 하나만 있어도 `xpe_verify_gain` 이 죽는다. 224 에서 "NaN 은 실패" 를 지키려던 가정 위에서는 이 충돌이 판정 이전에 일어난다.

수정: 값 중 하나라도 비유한이면 `0.0`(이 필드가 "측정하지 못했다"일 때 이미 쓰던 값)을 돌려준다. 이것은 **원래 계획에 없던** 코드 변경이지만 요구한 시험이 이 충돌 위에서는 통과할 수 없어 같은 카드에서 고쳤다. a5 가 지킨다. `compute_flatness` 의 호출자는 이 함수 하나다.

## 3. flat→flat 의 결정과 서술

보정 전과 후가 모두 평탄(PRNU < 0.01%)하면 개선 없이도 **합격**한다 — 유지한다. 헤더 `xpe_verify_gain` 의 `@note` 에 그렇게 적고, `xpe_verify_pipeline` 은 필드가 SNR 차이라 두 평탄 프레임이 **불합격**이라 둘이 다르다는 사실도 적었다. `#242` 가 정해질 때 다시 본다. 이 카드로 바뀐 합격 경로는 하나: **보정 전이 평탄이고 보정 후가 측정 불가인 영상**(이전에는 합격, 이제 불합격).

## 4. 200 dB 에 대한 한 줄

헤더 필드 문서에: 200 은 유한 개선과 **같은 축의 값이지 표지가 아니다.** 완벽했는지는 값 200 이 아니라 **측정된 프레임의** `prnu_after == 0` 으로 판단하라(`measured_mask` 참조). 측정 불가한 보정 후 프레임도 `prnu_after` 를 0 으로 두지만 여기엔 0.0 을 보고하고 실패한다.

## 5. 이 카드가 고치지 않은 것 — 보정 **전** 쪽 (측정함)

같은 계열이 보정 전 영상에도 있다(`evidence/12`, 실제 DLL):

| 입력 | `overall_pass` | `measured_mask` |
|---|---|---|
| 보정 전 **전부 0**(평균 0) → 보정 후 평탄 1000 | **True** | 44(측정됨으로 표시) |
| 대조: 보정 전 평탄 1000 → 보정 후 평탄 1000 | True | 44 |

보정 전 평균이 0 이하이면 `prnu_before` 가 자리표시 0 이 되고 "이미 평탄" 예외가 개선 게이트를 면제해 합격한다. 이전부터 있던 동작이고 카드(Codex #82)의 범위는 보정 **후**였으므로 고치지 않았다. 고친다면 `before_measurable` 로 같은 방식(실패·0.0·비트 해제)이 자연스럽지만, 죽은 센서에서 온 0 영상이 통과하는 것이 지금 동작이라 의미 결정이 필요하다 — 리더 판단.

## 6. 검증 (`evidence/verify/`, 반증 뒤 다시 빌드)

빌드 `BUILD_EXIT=0`, preprocess 시험 **915**(기존 910 + 신규 5) 중 907 통과·8 건너뜀(기존), 셔플 907 동일, 할당 실패 시험 65, common 69·12, ctest 총 **1102**(1097 + 5), 공개 export 변화 0, 헤더 문서 0건, 프리셋 일치 12/12.

## 7. 미검증 (Gaps)

- **보정 전 쪽**(§5): 측정했지만 고치지 않았다.
- 비유한 입력 전반: 이번 수정은 `compute_flatness` 의 한 곳과 보정 후 평균의 조건뿐이다. `compute_robust_mean`(정렬)·`xpe_verify_offset`·`xpe_verify_defect` 가 비유한 입력에서 어떻게 동작하는지는 이 카드에서 보지 않았다(224 의 "보장 밖" 그대로). `+inf` 화소의 필드(pipeline `-inf`)도 그대로다.
- 보정 후 평균이 **0 이하이지만 퍼짐이 의미 있는** 영상(예: 음수 평균에 큰 분산)을 "측정 불가" 로 일괄 불합격시키는 것이 요구에 맞는지는 확인하지 않았다. 이 함수의 PRNU 정의(`std/mean`)가 평균 ≤ 0 에서 정의되지 않는다는 것에 근거했다.
- 시험 영상은 32×32 뿐이다.

## 8. 잔여 위험

- 224 에서 정한 200 보고값은 이제 `measured_mask` 를 함께 읽어야 의미가 완전하다. 소비자는 아직 없다(Codex 확인).
