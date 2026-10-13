# GUI-C-234e 보고 — 이전 설정 안내 배너의 실제 UI 경로 E2E (Codex #183 발견 2, ccef6e31 위, Refs #251)

푸시하지 않았다. 제품 코드는 바꾸지 않았다(새 시험 파일 하나).

## 시나리오
`clients/ImageProcTest.E2ETests/Scenarios/Workflows/PreprocessOffNoticeScenarios.cs` — `AnEarlierBuildsPreprocessOff_IsOnScreenUntilOkIsInvoked_AndDoesNotComeBack`. UIA 패턴만 사용(Invoke·Toggle·ExpandCollapse), 전역 키보드·마우스 입력 없음. 시험 전용 설정 경로(`--automation-settings`).

| 단계 | 결과 (`scenario_output.txt`) |
|---|---|
| 1. 옛 파일(버전 없음, `preprocessInChain=false`)로 기동 | 배너 `PreprocessOffNoticeText` 가 보이고(IsOffscreen=false) 문구에 "earlier version", OK 버튼도 화면에 있음, 전처리 체크박스 꺼짐 |
| 2. Clear Alerts (Invoke) | 배너 그대로 있음 |
| 3. OK (Invoke) | 배너 사라짐, 설정 파일에 `"preprocessOffNoticeUnacknowledged": false` 저장, `preprocessInChain: false` 유지, 체크박스 꺼진 채 |
| 4. 같은 설정으로 재시작 | 배너 없음, 전처리 꺼진 채 |
| 5. 대조: `settingsSchemaVersion=1` + false 파일 | 배너 없음, 전처리 꺼짐 |

실행: 로컬 Native(CI DLL 한 벌 `main386/onedll`), 이 시나리오 하나만 — **통과 1/1, 1분 5초**. 시작 전 가용 메모리 6602 MB / 32509 MB, 끝난 뒤 6705 MB(`memory_before.txt`, `memory_after.txt`). 끝나고 앱·testhost 프로세스를 정리했다.

## 반증 (각각 해당 단언에서 빨강, 바인딩은 되돌려 다시 빌드함)
- OK 의 `Command` 바인딩 이름을 바꿈 → "The notice stayed after OK was invoked." (`falsify_ok_binding_broken.txt`)
- 배너 `Visibility` 바인딩 이름을 바꿈 → 같은 지점 빨강: 배너가 처음부터 보이지 않으면 1단계에서 먼저 실패해야 하는데 "OK 뒤에도 남음"으로 실패했다 — 바인딩 오류 시 WPF 가 Visibility 를 기본값(Visible)으로 두어 배너가 항상 보이기 때문이며, 가시성 바인딩이 끊기면 "OK 뒤 사라짐" 단언이 이를 잡는다 (`falsify_visibility_binding_broken.txt`).

## 한계
- 전체 스위트는 돌리지 않았다(카드 지시). 배너의 다른 배너와의 겹침은 눈으로 보지 않았다 — 시험은 IsOffscreen 과 UIA 로 읽히는지만 본다.
- 앱 종료 시 "자동 저장" 경로는 이 시나리오에 없다(픽스처가 프로세스를 끊는다); 그 경로는 SelfCheck 19 ③이 다룬다.
