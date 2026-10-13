# GUI-C-234d 보고 — 이전 설정 안내의 출처 판별과 확인 시점 (Codex #182 발견 1·2, 646429fa 위, Refs #251)

푸시하지 않았다. E2E 는 돌리지 않았다(카드 지시). `modules/dicom` 은 건드리지 않았다.

## 변경

| 발견 | 변경 |
|---|---|
| 1 출처 판별 | 설정 파일에 `settingsSchemaVersion`(현재 1)을 저장할 때마다 쓴다. 읽을 때 `preprocessInChain=false` 이고 버전이 없거나 현재보다 낮을 때만 "이전 버전에서 넘어온 끔"으로 본다(`AppSettingsService.StoresPreprocessOffFromAnEarlierBuild`, 원본 JSON 을 직접 읽음). 현재 버전 파일의 false 는 선택이라 안내하지 않는다. |
| 2 확인 시점 | 234c 의 `PreprocessOffNoticePending/Shown` 삭제. 저장되는 `preprocessOffNoticeUnacknowledged` 하나만 쓴다: 옛 끔이면 로더가 true 로 세우고, 이후 어떤 저장(종료 시 저장 포함)에서도 유지되며, 명시적 확인(뷰포트 안내의 **OK** 버튼 → `AcknowledgePreprocessOffNoticeCommand`)에서만 false 로 저장된다. 생성자는 파일을 쓰지 않고 알림(WARN)·상태 줄만 낸다. `ClearAlerts` 는 확인이 아니다. 안내(`PreprocessOffNotice`)는 "미확인 && 전처리 꺼짐"인 동안 화면에 남는다. 확인 뒤에도 꺼짐은 렌더의 HUD(`Calib: not run`)로 읽힌다(문구 변경 없음). |
| 추가 결정 | 사용자가 전처리를 직접 **켜면** 안내는 답을 얻은 것으로 보고 해제한다(안 그러면 켜진 상태에서 "꺼짐" 안내가 남음). 카드의 "명시적 확인만" 에 대한 작은 확장이다 — 원하지 않으면 한 줄(`OnSettingsPropertyChanged`)을 지우면 된다. |

## 시험 (카드 ①~⑤) — SelfCheck 19 (view model 수준, "재시작" = 같은 파일을 다시 읽어 새 view model) + 통합 `PreprocessIsOnForANewInstall_…`

| # | 단언 | 반증(되돌리면 빨강) |
|---|---|---|
| ① 새 설치에서 끔→저장→재시작: 안내 없음 | 통과 | 버전 무시(`return true`) → ①·⑤ 빨강 (`falsify_m1_version_ignored.txt`) |
| ② 옛 파일(버전 없음) false: 안내 있음, 시작이 파일을 쓰지 않음 | 통과 | 안내를 시작 때 소모(저장) → ② 빨강 (`falsify_m2_consumed_when_shown.txt`) |
| ③ 확인 전 종료(종료 시 저장) → 재시작: 다시 안내 | 통과 | 플래그를 저장하지 않음 → ③ 빨강 (`falsify_m5_flag_not_stored.txt`) |
| ④ 알림 지우기 뒤 재시작: 다시 안내, 지우기로 안내가 안 사라짐 | 통과 | `ClearAlerts` 가 확인 처리 → ④ 빨강 (`falsify_m3_clear_alerts_acknowledges.txt`) |
| ⑤ 확인 뒤 재시작: 안내 없음, 전처리는 계속 꺼짐 | 통과 | 확인이 플래그를 안 지움 → ⑤ 빨강 (`falsify_m4_ack_does_not_clear.txt`) |
| 보너스 ⑥ 켜면 안내 해제, 키 없음/ON 파일은 안내 없음 | 통과 | — |

통합 시험은 로더 수준(버전 없음/0 → 표시, 저장 후에도 유지, 확인 후 해제, 현재 버전 OFF 는 표시 안 함)을 단언한다. 이 통합 시험을 위한 별도의 변형 반증은 돌리지 않았다(같은 로더 코드를 m1·m5 가 SelfCheck 로 반증).

## 실행 결과 (이번 트리)

- SelfCheck: 28 시나리오 통과 (`selfcheck.txt`)
- 통합(CI DLL 한 벌, 실데이터 환경 포함): 960 통과 · 0 실패 · 2 건너뜀 (`integration_tests.txt`; 로컬 `bin` 의 옛 DLL 4개는 지우고 돌림 — 로컬 산출물 문제)
- 빌드 오류 0

## 하지 못한 것 / 한계

- 안내 배너(XAML)의 실제 WPF 표시와 OK 버튼 동작은 E2E 를 돌리지 않아 **미검증**이다(view model 의 `HasPreprocessOffNotice`/명령만 SelfCheck 로 검증). 배너 위치(`Margin 0,150,0,0`)가 다른 배너와 겹치지 않는지도 눈으로 보지 못했다.
- 234c 의 E2E(W13a/b·P10/P11·L06)는 이 카드 범위 밖(브랜치 CI).
- 이미 234c 코드로 `preprocessOffNoticeShown` 이 저장된 파일이 있다면 그 키는 무시된다(옛 키 → 버전 없는 파일이므로 false 이면 다시 한 번 안내가 뜰 수 있음).

## Card Cross-Check

| 발견 | 카드 |
|---|---|
| #182 발견 1·2 | GUI-C-234d |
