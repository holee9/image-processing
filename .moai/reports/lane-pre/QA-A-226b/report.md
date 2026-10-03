# QA-A-226b — 순방향 불안정 지연 설정 거절, tau 는 프레임 단위 (Refs #241)

브랜치 `dev/preprocess`, QA-A-226(`e40c87a3`) 위의 별도 커밋이다. 리더 결정(2026-10-03)을 그대로 구현했다.

## 1. 결론

| 결정 | 구현 |
|---|---|
| (1) S ≥ 1 은 `CONFIG_INVALID` 로 거절 (b) | 네 키를 모두 준 설정에서 `S = α1/(1−e^(−1/τ1)) + α2/(1−e^(−1/τ2))`가 1 이상이거나 유한하지 않으면 생성 거절. 티어 가중치를 곱한 상한(c)은 하지 않았다 |
| (2) tau = 프레임, `acquisitionTime` 미사용 | 성공한 호출마다 dt = 1. `lastAcqTimeSec` 필드를 없앴다. 단절은 `xpe_ghost_reset` |
| 전체 검증 | preprocess 941개 중 933 통과·8 건너뜀(종료 0), 셔플 동일, OOM 65, ctest 1128, export 차이 0, 헤더 문서 0건, 프리셋 12/12, doxygen(WARN_AS_ERROR) 종료 0·경고 0 |

## 2. S 검사의 정확한 규칙

- 대상: 네 키가 모두 주어진 설정(교정된 설정, QA-A-226). 교정되지 않은 설정은 보정하지 않으므로 판정하지 않는다(`ASetThatIsNotCalibratedIsNotJudged`).
- 항별 계산에서 `alpha == 0`인 항은 `tau`와 무관하게 0이다. `tau`가 매우 커서 `exp(−1/tau)`가 정확히 1.0이 되면 분모가 0인데, 알파가 0이면 `0/0 = NaN`이 되어 무해한 "보정 없음" 설정이 거절되기 때문이다. 알파가 양수이고 분모가 0이면 무한 이득이라 거절된다.
- `S ≥ 1`이거나 `S`가 유한하지 않으면 `XPE_ERR_CONFIG_INVALID`, 출력 포인터는 건드리지 않고 알림도 남기지 않는다(다른 `CONFIG_INVALID`와 같다).
- 경계: S = 1.001 배는 거절, 0.999 배는 수락(`TheBoundaryIsSEqualsOne`). 정확히 S == 1.0 은 부동소수점에서 구성할 수 없어 시험하지 않았다.
- 티어 2·3의 노출 가중치(`w = 1 + 0.5·평균/32768`)는 곱하지 않는다. 티어 1에서 S < 1인 설정이 높은 신호의 티어 2에서 0이 될 수 있다는 점(QA-A-226 측정: S = 0.7055, 레벨 30000)은 그대로 남는다.

## 3. tau 단위 변경

`ghost_correct.cpp`의 `dt` 계산(시각이 있으면 정수 초 차이, 없으면 1, 0 이하면 1)을 `decay = exp(−1/tau)`로 바꿨다.
QA-A-226 6절에서 측정한 세 가지(단위 혼재, 간격이 다음 프레임에 늦게 반영, 초당 1프레임보다 빠르면 사실상 dt ≡ 1)가 모두 없어진다.
`xpe_ghost_correct`는 `meta`가 NULL이면 여전히 `INVALID_INPUT`이다(기존 계약 유지). `acquisitionTime`은 읽지 않으며 헤더에 명시했다.

## 4. 옛 값(S = 2.449)을 쓰던 시험의 이동

시험용 헬퍼 `ghost_legacy_lag.h`를 `ghost_stable_lag.h`로 바꿨다: `alpha1 0.1 / tau1 1 / alpha2 0.01 / tau2 20` (S = 0.158 + 0.205 = 0.363, 티어 2 가중치 최대 1.5에서도 S·w < 1).
"단언은 그대로 두고 통과" 기준으로 읽은 시험별 목적 유지 여부는 아래와 같다(전체 실행 결과는 증거 `verify/24_pre_full.txt`).

| 시험 | 바뀐 것 | 원래 목적이 유지되는가 |
|---|---|---|
| `test_golden_reference` ghost 4건 | 파일 안 상수 `kAlpha1/kAlpha2`를 0.1/0.01로, 설정은 같은 상수에서 만든 `lagConfig()` | 유지: 프레임 1 공식(`V·(1−α1−α2)`)을 시험 자신의 상수로 독립 계산해 비교하는 구조 그대로. 설정과 기대값이 한 상수에서 나오므로 공식 시험이 더 직접적이다 |
| `test_ghost_tier3_orientation` | 기준 공식의 상수 0.1/0.01(헬퍼와 같은 값, 주석으로 연결) | 유지: 이웃을 들어오는 프레임에서 읽는다는 독립 유도 공식과 방향 독립성 단언은 그대로 통과 |
| `test_ghost_thread_safety` | `kNoForgetting`을 알파 1e-13(S = 0.2)으로 | 유지: 이 시험은 출력이 아니라 **이력**(갱신 횟수)을 직렬 재생과 비교한다. 이력은 알파와 무관하므로 잃은 갱신이 보이는 정도가 같다. 다른 한 건은 헬퍼 사용 |
| `test_config_strict_parse` `GhostCreateAcceptsOrdinaryNumbers` | 알파를 "0.05"/"0.01"로(문자열 알파, 숫자 tau 1.5, 문자열 tau2 표기는 유지) | 유지: 보는 것은 값이 아니라 숫자 표기. 표기 다양성 그대로 |
| `test_ghost_tiers`, `test_ghost_correct`, `test_pipeline_stage_values`, `test_ghost_failed_frame`, `test_ghost_input_finite` | 헬퍼 값 | 유지: 단언 문장은 바꾸지 않았고 모두 통과. 실패·넘침 시험은 극단값(3e38, β 1e37)을 쓰므로 알파 크기와 무관 |
| `test_req_p1a_066` T4, OOM 파이프라인("the ghost stage ran" 대조군), 타이밍 프로브 | 헬퍼 값 | 유지: 보정이 실제로 실행되어야 의미가 있는 시험이고, OOM 대조군은 통과 |

헬퍼 이름과 값이 다른 곳에서 의존되지 않도록 `ghost_stable_lag.h` 머리말에 근거를 적었다.
위 표의 "유지"는 단언이 그대로 통과한다는 관측과 제 해석이다. 값이 바뀐 만큼 단언의 민감도가 달라졌는지는 시험마다 반증으로 확인하지 않았다(아래 미검증).

## 5. 신규 시험 `test_ghost_lag_contract.cpp` (9건)

| 시험 | 고정하는 것 |
|---|---|
| `AForwardUnstableSetIsRefusedInEveryTier` | S ≥ 1 네 가지(옛 값 2.449 포함) × 티어 1~3 거절, 출력 포인터 불변, 알림 0 |
| `TheBoundaryIsSEqualsOne` | S = 1.001 배 거절 / 0.999 배 수락 |
| `AStableSetIsAcceptedInEveryTierAndRaisesNoWarning` | S < 1 수락, 경고 없음 |
| `ANonFiniteGainIsRefused` | 알파 1e308(S = +∞)과, S ≥ 1 검사를 통과하는 −∞(음수 알파 + 퇴화 tau) 거절 |
| `AZeroAlphaContributesNothingWhateverItsTauIs` | 알파 0 항은 tau 1e20에서도 수락, 알파 양수 + 분모 0은 거절 |
| `ASetThatIsNotCalibratedIsNotJudged` | 교정되지 않은 설정은 S를 판정하지 않음 |
| `TheOutputDoesNotDependOnTheTimes` | 시각 5가지(1초 간격, 같은 초, 분·월 간격, 거꾸로, 실제 epoch) × 티어 1~3의 출력이 시각 없는 실행과 비트 동일 |
| `EachCallIsOneFrameStep` | 600초 간격 뒤에도 프레임 2 = `1000 − α1·1000 − α2·1000`, 프레임 3은 정확히 한 프레임 감쇠한 이력 |
| `AResetStartsTheSequenceOver` | `xpe_ghost_reset` 뒤 첫 프레임은 이력 없음 |

## 6. 증거 — 5절 형식

**주장**: S ≥ 1 인 교정 설정은 생성이 거절되고, tau는 프레임 단위이며 시각은 출력에 영향을 주지 않는다.

**증거**:

- 빨강 먼저: `evidence/00_red_first.txt` — 제품 코드 수정 전 신규 시험 8건 중 5건 실패, 3건 통과(대조군). 영 알파 시험과 −∞ 줄은 그 뒤에 추가했다
- 반증 8개, 각각 전체 빌드 후 해당 시험에서 빨강(`evidence/04_arms_results.txt`, 스크립트 `03_arms226b.py.txt`):

| 손상 | 빨강이 된 시험 |
|---|---|
| b1 S 검사 제거 | 4건 |
| b2 문턱 1.0 → 1.5 | 2건(불안정 표, 경계) |
| b3 알파 0 항을 특별 취급하지 않음 | `AZeroAlphaContributesNothingWhateverItsTauIs` |
| b4 감쇠 간격이 시각에 의존 | `EachCallIsOneFrameStep`, `TheOutputDoesNotDependOnTheTimes` |
| b5 검사를 티어 1에만 | `AForwardUnstableSetIsRefusedInEveryTier` |
| b6 교정되지 않은 설정도 판정 | 11건 |
| b7 한 호출 = 두 프레임 | `EachCallIsOneFrameStep` |
| b8 `isfinite` 제거 | 처음엔 **발화하지 않았다**. 시험 입력(+∞)이 `S ≥ 1`로도 걸러졌기 때문이다. 중복이라고 단정하지 않고, 그 방어만 걸리는 입력(음수 알파 + 분모 0 → S = −∞)을 구성해 시험에 추가하니 `ANonFiniteGainIsRefused`가 빨강이 되었다 |

  반증 후 `cmp`로 소스 원복을 확인하고 다시 빌드했다. b6의 첫 형태 `true || all`은 `/WX`(조건식이 상수) 대상이라 런타임에서 거짓/참이 정해지는 조건(`handle->width != 0 || all`)을 썼다.
- 전체 검증 `evidence/verify/`(`verify226b.sh`): `xpe_common_oom_tests` 12, `test_xpe_common` 69, `xpe_preprocess_tests` 941/933/건너뜀 8(종료 0), 셔플(seed 22602) 동일, `xpe_preprocess_oom_tests` 65, ctest 1128, preprocess export 차이 0, 헤더 문서 0건, 프리셋 12/12, doxygen 1.12.0 `Doxyfile`(CI와 같은 명령) 종료 0·경고 줄 0(`33_doxygen.txt`).
  doxygen 실행 파일은 `xpe-post` 워크트리의 `build/tools`에 있는 것을 읽기·실행만 했다. 226 커밋(`e40c87a3`)의 헤더 변경은 이 실행 때까지 doxygen으로 확인하지 않았고, 이번에 두 커밋의 변경이 함께 통과한 것으로 확인했다.

**baseline 귀속**: 위 명령과 출력은 이번 실행, 이 작업 트리(HEAD `e40c87a3` + 이번 변경)에서 측정했다.

## 7. 범위 밖 관찰 (고치지 않았고, 결정이 필요하면 리더에게)

`evidence/01_out_of_scope_inputs.txt` — 현재 코드에서 상수 입력 1000, 3프레임:

| 입력 | 생성 | 프레임 1~3 |
|---|---|---|
| alpha1 = −0.1, tau1 = 1 | 수락 | 1000 / **1100** / **1136.8** (신호를 더한다) |
| alpha1 = −0.1, tau1 = 1e20 (S = −∞) | 거절(−4) | — |
| tau1 = 0 | 수락 | 1000 / 900 / 900 |
| tau1 = −1 | 수락 | 1000 / 900 / 628.2 (감쇠 계수가 1보다 커 이력이 불어난다) |

음수 알파와 `tau ≤ 0`은 이번 결정(S ≥ 1) 범위 밖이어서 그대로 두었다. 둘 다 물리적으로 의미가 없는 설정이라 거절할지는 별도 결정이다.

## 8. 미검증(Gaps)과 잔여 위험

**미검증**:

- 4절 표의 "목적 유지"는 시험마다 반증으로 민감도를 다시 확인하지 않았다. 알파가 0.9에서 0.1로 작아져 보정량이 줄었으므로, 일부 시험이 이전보다 둔감해졌을 가능성은 배제하지 못한다.
- 티어 2·3에 대해 `S·w` 상한은 검사하지 않는다(결정 (c) 미채택). 높은 신호에서 출력이 0이 되는 설정은 여전히 생성된다.
- 정확히 `S == 1.0`은 구성할 수 없어 `>=`와 `>`의 차이는 시험으로 가르지 못했다.
- 실장비 지연 영상은 여전히 없다.
- `docs/project/api-spec.md` §6.5/6.6은 리더가 반영한다(결정대로).

**잔여 위험**:

- 옛 값(0.9/1/0.05/20) 또는 S ≥ 1 인 설정을 명시해 핸들을 만들던 호출자는 이제 `CONFIG_INVALID`를 받는다. 이 저장소 안에서는 `alpha1`/`tau1` 키를 쓰는 곳이 `modules/preprocess`의 시험뿐임을 확인했다(검색 범위: 저장소 전체에서 `build/`, `modules/preprocess`, `docs/`, `.moai/reports`, `.git` 제외 — 일치 0건). 저장소 밖 소비자는 확인할 수 없다.
- `acquisitionTime`을 감쇠에 쓰던 호출자는 이제 프레임 단위 감쇠를 얻는다. 초당 1프레임 이상인 영상에서는 이전에도 사실상 같았으므로(QA-A-226 6절 3번) 영향은 초당 1프레임보다 느린 영상에 한정된다.
