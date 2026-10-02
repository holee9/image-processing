# QA-A-226c — 음수 알파·tau ≤ 0 거절, 알파를 옮긴 시험의 둔감화 재확인 (Refs #241)

브랜치 `dev/preprocess`, QA-A-226b(`8379e682`) 위의 별도 커밋이다. 리더 결정(2026-10-03)의 두 항목을 처리했다.

## 1. 결론

| 항목 | 결과 |
|---|---|
| ② 음수 알파·tau ≤ 0 은 `CONFIG_INVALID` | 구현. 네 키를 모두 준 설정에서 알파 < 0, tau ≤ 0 을 S 판정과 같은 자리에서 거절 |
| ③ 알파를 0.9 → 0.1 로 옮긴 시험의 둔감화 | 옮긴 시험마다 그 시험이 지키던 결함을 되살려 확인. **둔감해진 시험은 없었다**(전부 빨강). 단언 문턱 조정 0건 |
| 전체 검증 | preprocess 944개 중 936 통과·8 건너뜀(종료 0), 셔플 동일, OOM 65, ctest 1131, export 차이 0, 헤더 문서 0건, 프리셋 12/12, doxygen 종료 0·경고 0 |

## 2. 범위 검사 — 카드와 달라진 두 곳 (결정 확인용)

- **알파 < 1 의 개별 검사는 두지 않았다.** 각 항은 `alpha/(1−exp(−1/tau)) ≥ alpha`(분모는 0 초과 1 이하)이므로 알파 ≥ 1 이면 S ≥ 1 이 자동으로 성립한다. 알파 < 1 은 S < 1 에 포함된다. 결과(알파 ≥ 1 거절)는 카드와 같고 코드에 죽은 검사가 없다. 경계는 시험으로 고정했다(알파 0.999999 + 아주 작은 tau 수락 / 1.0, 1.5 거절).
- **226b 의 "S 가 유한하지 않으면 거절"(`isfinite`)을 뺐다.** 범위 검사 뒤에서는 항이 음수일 수 없고 NaN 일 수 없다(알파 0 항은 0 으로 취급, 알파 양수 + 분모 0 은 +∞ 이고 `S ≥ 1` 이 거절). 그래서 S 는 NaN 도 −∞ 도 될 수 없다. 226b 에서 이 방어를 지키려고 만든 −∞ 입력(음수 알파 + 퇴화 tau)은 이제 범위 검사가 먼저 거절하므로, 방어가 필요 없어진 근거가 그 입력 자체다. 해당 줄은 `ANegativeAlphaIsRefused` 로 옮겼다.
  - 되돌아볼 지점: 이 근거는 "알파 ≥ 0, tau > 0" 검사가 S 계산 앞에 있다는 순서에 기댄다. 순서를 바꾸는 후속 수정이 생기면 이 방어가 다시 필요해진다(코드 주석에 같은 근거를 적었다).
- tau 의 유한성은 새로 검사하지 않는다. `"inf"`, `"nan"` 은 숫자를 읽는 단계에서 이미 거절되며, 시험에 넣어 확인했다(`TauMustBeAFinitePositiveNumber` 의 문자열 입력).

## 3. 신규 시험 (`test_ghost_lag_contract.cpp` 에 3건, 총 12건)

| 시험 | 고정하는 것 |
|---|---|
| `ANegativeAlphaIsRefused` | 알파 −0.1, −1e−300(아주 작은 음수), −0.5, −0.1 + tau 1e20 × 티어 1~3 거절, 출력 포인터 불변, 알림 0 |
| `TauMustBeAFinitePositiveNumber` | tau 0, −1, −1e−300, 알파가 0 이어도 tau 0, 문자열 `"inf"`/`"nan"`/`"-inf"` × 티어 1~3 거절, 출력 포인터 불변, 알림 0 |
| `TheRangeBoundariesAreInclusiveWhereTheyShouldBe` | 알파 0 허용(두 항 모두 0 포함), 아주 작은 양수 tau(1e−300) 허용, 알파 0.999999 허용 / 1.0·1.5 거절 |

"출력·상태 불변"은 생성 거절이므로 핸들이 나오지 않는다는 뜻이다: 출력 포인터(센티널 값)가 그대로이고 알림이 0 건임을 단언한다.

## 4. ③ 옮긴 시험별 재확인 — 결함을 되살려 보기

방법: 새 안정한 값(0.1/1/0.01/20) 상태에서 시험이 지키던 결함을 한 번에 하나씩 제품 코드에 되살리고, 전체 빌드 뒤 관련 묶음(`Ghost*`, `GoldenGhostTest`, `PipelineStageValueTest`, `PipelineComboTest`, `ConfigStrictParse`, `P1A066`, `OomPipeline`)을 돌렸다. 소스는 매번 원복하고 `cmp` 로 확인했다. 결과 전문은 `evidence/02_arms_results.txt`.

| 되살린 결함 | 지키던 시험 | 결과 |
|---|---|---|
| c1 알파2 무시 | `GoldenGhostTest.Frame1MatchesDualExponentialFormula` | 빨강 (+ `EachCallIsOneFrameStep`, `ExplicitConfigurationRaisesNoWarningAndStillCorrects`) |
| c2 알파1 부호 뒤집힘 | `GoldenGhostTest.Frame1…`, `GhostSubtractedAfterFirstFrame`, `GhostCorrectTest.RepeatedFramesApplyHistoryCorrection`, `GhostTierTest.Alpha1OverrideChangesTheCorrection` 외 | 7건 빨강 |
| c3 티어 3 이 이미 고친 픽셀을 읽음(QA-A-218b 되돌림) | `GhostTier3Orientation.AFrameFlipped…`, `TheOutputIsTheTierThreeFormula…` | 2건 빨강 |
| c4 `xpe_ghost_correct` 잠금 제거 | `GhostThreadSafety.SharedHandleLosesNoUpdates` | **프로세스가 접근 위반(0xC0000005)으로 죽음**. 직접 3회 돌렸을 때는 2회 실패, 1회는 출력이 비어 있었다(프로세스가 죽은 것으로 보이나 종료 코드는 스크립트 실행에서만 기록했다) |
| c5 `hist1` 만 커밋하지 않음 | `GhostFailedFrame` 2건, `GoldenGhostTest.Frame1…`, `GhostTierTest.Alpha1Override…` 외 | 8건 빨강. `RepeatedFramesApplyHistoryCorrection` 은 빨강이 아님(아래 설명) |
| c13 이력을 전혀 커밋하지 않음 | `GhostCorrectTest.RepeatedFramesApplyHistoryCorrection` 외 | 19건 빨강 |
| c6 티어 2 노출 가중치 무시 | `GhostTierTest.Tier2ExposureWeightScalesWithSignal` | 빨강 |
| c7 티어 3 `nlcscBeta` 무시 | `GhostTierTest.Tier3BetaStrengthensSignalDependence` 외 | 5건 빨강 |
| c8 알파1 을 상수 0.1 로 고정 | `GhostTierTest.Alpha1OverrideChangesTheCorrection` | 3건 빨강 |
| c9 실패한 프레임의 픽셀을 복원하지 않음 | `GhostInputFinite.AFrameThatFailsHalfWayThroughPutsItsPixelsBack` | 빨강 |
| c10 파이프라인이 고스트 출력을 버림 | `PipelineStageValueTest.GhostCorrectionOfTheSecondFrameReachesTheOutput` | 빨강 |
| c11 평범한 소수 표기를 거절 | `ConfigStrictParse.GhostCreateAcceptsOrdinaryNumbers` 외 | 50건 빨강 |
| c12 실패한 프레임이 이력을 커밋 | `GhostFailedFrame` 2건, `GhostInputFinite` 3건 외 | 11건 빨강 |

풀어 쓸 점:

- **c5 에서 `RepeatedFramesApplyHistoryCorrection` 이 빨강이 아닌 것은 둔감화가 아니다.** 이 시험의 단언은 "두 번째 프레임이 500 보다 작다"뿐이고, 알파 크기와 무관하다. c5 는 이력 한 평면만 빼는 반쪽짜리 결함이라 다른 평면이 보정을 계속하므로 단언이 참으로 남는다. 이력이 아예 적용되지 않는 완전한 결함(c13)은 이 시험이 잡는다. 반쪽짜리 결함은 같은 c5 에서 다른 8건이 잡는다. 알파를 옮기기 전에도 같은 단언이므로 달라진 것이 없다.
- **c9 에서 시험이 1건뿐인 이유.** 픽셀 복원(QA-A-217)을 지키는 시험은 `GhostInputFinite…HalfWay` 이고, `GhostFailedFrame` 은 이력 커밋(QA-A-202c)을, `GhostThreadSafety.FailedFramesGetTheirOwnPixelsBack…` 은 잠금을 지킨다(그 시험의 프레임은 모든 픽셀이 같아 첫 픽셀에서 실패하므로 복원할 픽셀이 없다). 담당이 나뉘어 있고 c4·c12 가 각각을 잡는다.
- **반증으로 확인하지 않은 시험**(알파 크기와 무관하거나 보정 결과를 보지 않는다): `P1A066.T4`(오류 0 건 단언), OOM 파이프라인의 "the ghost stage ran" 대조군(플래그 설정 단언), 비활성 타이밍 프로브. 알파를 바꿔도 이 세 시험이 보는 것은 바뀌지 않는다.
- 결과적으로 단언 문턱을 새 값에 맞게 조정한 시험은 0건, "다른 시험이 덮는다"고 적은 곳은 위 c5·c9 두 곳이다.

## 5. 측정 도구의 구멍 (이번에 찾음)

첫 반증 실행에서 c4 가 "안 터짐(NONE)"으로 나왔다. 직접 돌려 보니 시험은 빨강이었다. 경쟁 상태가 힙을 망가뜨려 프로세스가 죽으면 `FAILED` 줄이 남지 않는데, 반증 스크립트가 FAILED 줄만 세고 종료 코드를 보지 않았기 때문이다. 스크립트가 종료 코드를 함께 기록하도록 고쳤고(`01_arms226c.py.txt`), 고친 뒤 c4 는 `PROCESS DIED, exit 3221225477` 로 나온다. "안 터졌다"는 결론을 내리기 전에 같은 입력을 직접 한 번 돌려 본 것이 이 구멍을 드러냈다. 같은 구멍이 226·226b 의 반증 스크립트에도 있었으므로 그때의 "발화" 판정은 FAILED 줄을 본 것이고, 프로세스가 죽는 경우를 놓쳤을 가능성은 있어도 거짓 발화를 만들지는 않는다(빨강으로 읽은 것은 전부 FAILED 줄이 실제로 있었다). 반대 방향, 즉 안 터졌다고 적은 항목은 226b 의 b8 하나뿐이었고 그것은 시험 추가 뒤 FAILED 줄로 발화했다.

## 6. 증거 — 5절 형식

**주장**: 음수 알파와 tau ≤ 0 인 교정 설정은 생성이 거절되고, 알파를 옮긴 시험은 둔감해지지 않았다.

**증거**:

- 빨강 먼저: `evidence/00_red_first.txt` — 새 시험 3건 중 2건 실패, 1건 통과(경계 대조군, 이미 참인 동작)
- 반증 13개와 첫 시도의 빌드 실패·도구 구멍 기록: `evidence/02_arms_results.txt`(스크립트 `01_arms226c.py.txt`). c1·c7 은 첫 시도에서 `/WX`(미사용 인자·변수)로 빌드가 실패해 사용을 유지하는 형태로 바꿔 다시 돌렸다
- 전체 검증 `evidence/verify/`(`verify226c.sh`): `xpe_common_oom_tests` 12, `test_xpe_common` 69, `xpe_preprocess_tests` 944/936/건너뜀 8(종료 0), 셔플(seed 22603) 동일, `xpe_preprocess_oom_tests` 65, ctest 1131, preprocess export 차이 0, 헤더 문서 0건, 프리셋 12/12, doxygen 1.12.0 `Doxyfile` 종료 0·경고 0

**baseline 귀속**: 위 명령과 출력은 이번 실행, 이 작업 트리(HEAD `8379e682` + 이번 변경)에서 측정했다.

**미검증(Gaps)**:

- 4절의 "빨강"은 되살린 결함 한 가지씩에 대한 것이다. 알파 크기에 민감한 다른 결함(예: 알파2 가 알파1 의 1/10 일 때만 드러나는 오차)은 만들어 보지 않았다.
- 반증으로 확인하지 않은 세 시험(`P1A066.T4`, OOM 대조군, 타이밍 프로브)은 알파와 무관하다는 읽기에 기댄다.
- 티어 2·3 의 `S·w` 상한은 여전히 검사하지 않는다.
- 실장비 지연 영상은 없다.

**잔여 위험**:

- 알파 < 0 또는 tau ≤ 0 을 명시하던 호출자는 이제 `CONFIG_INVALID` 를 받는다. 저장소 안에서는 226b 에서 확인했듯 해당 키를 쓰는 곳이 시험뿐이다. 저장소 밖 소비자는 확인할 수 없다.
- `isfinite` 제거는 범위 검사가 S 계산 앞에 있다는 순서에 기댄다(2절).
