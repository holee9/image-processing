# GUI-C-174 — SelectBodyPart 가 콤보 드롭다운을 열어 둔 채 끝나지 않게 한다

레인: gui · 이슈: `#225` · GUI-C-173 이 가린 원인(열린 `BodyPartSelector` 가 R01 의 첫 클릭을 소모)의 근본 정리. 푸시는 리더가 했다(`2a485aaf`).

## 결론

수정 뒤 R01 이 클릭 전에 읽은 "열려 있던 것" 목록이 **Mock·Native 모두 비었다** (`(nothing)`). 직전 run 에서는 `ComboBox:BodyPartSelector` 였다. R01·R02 는 둘 다 Passed, Native 는 179/179 통과(이 브랜치에서 처음으로 끝까지 돌았다).

| 항목 | 값 |
|---|---|
| CI run | 36838242699 (dev/gui `2a485aaf`), 전체 success |
| R01 의 "expanded before the click" | Mock `(nothing)` · Native `(nothing)` (이전: `ComboBox:BodyPartSelector`) |
| Mock | Passed 145 + NotExecuted 34 = 179 |
| Native | Passed 179 / 179, 테스트 합 22.6분, 잡 약 24분(예산 30분) |
| 증거 | `ci_evidence.txt` |

## 변경

- `WorkbenchObservation.CloseTheDropDown(ComboBox)`: Expanded 이면 UIA `Collapse`(입력을 보내지 않는다).
- `BodyPart`(읽기) · `SelectBodyPart`(선택, 이미 선택된 조기 반환 포함) · `W07` 의 `combo.Select` 두 곳이 끝난 뒤 호출. FlaUI 래퍼의 어느 호출이 목록을 여는지는 확인하지 않고 세 지점 모두 닫았다.
- R01 의 사전 조건(열린 것을 읽어 기록하고 닫기)은 방어로 유지.

## 미검증 · 한계

1. **대조 실험을 하지 않았다.** 수정을 빼면 목록이 다시 차는지는 이 run 으로 모른다. 목록이 빈 것은 "이 순서에서 R01 직전에 열린 것이 없었다"까지다.
2. **R01 이 닫기를 시도하므로 `(nothing)` 은 닫을 것이 없었다는 뜻**이다. 순서가 바뀌면(필터·재실행) 다른 항목이 열릴 수 있다.
3. 범위 밖으로 둔 `.AsComboBox().Select(...)` 4곳 중 `TwoLaneWorkbench`·`UnappliedSettings` 는 R01 보다 먼저 돌았고 목록이 비었으므로 이 순서에서는 누수가 없었다. **`NativeAlertScenarios` 는 R01 뒤에 돌아 이 목록으로 판별되지 않는다 — 미검증.**
4. 로컬 실행 없음(CI 로만 검증).

## Native 시간 (첫 완주, 테스트 합 22.6분)

`AutomationReportBackendTests` 259초(13건; A12 48초) · `GsvgWristSliceScenarios` 234초(14건; P08 60초) · `SelectionVisibilityObservation` 110초(3건) · `ProcessingChainScenarios` 98초 · `NonlinearityNoopAlertScenarios` 92초(1건) · `SettingsWarningRecoveryScenarios` 86초 · `ClampAlertOnScreenScenarios` 79초(1건). 입력 게이트 13건과 R01·R02 를 합친 15건은 67초(약 5%).

🗿 MoAI
