# GUI-C-233f 보고 (Refs #251)

리더 결정(Codex #175 후속): --automation-run 자체 점검 보고는 값을 그대로 두고 키 이름에 Requested 를 붙인다.

- GuiAutomationReport: CalibrationEvaluationSummary / OffsetCorrectionMode / DefectCorrectionMode → Requested* (점검이 직접 Offset=Off, Defect=On 을 세팅해 그것을 판정하는 값). 생산·판정(MainWindow.xaml.cs 5곳)과 README 목록을 같이 바꿈.
- 소비자 전수(git grep, gui/clients/docs/tools/.github): MainWindow.xaml.cs, GuiAutomationReport.cs, README.md, tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1 (2줄). docs/design/reference/* 는 과거 설계 참고 사본이라 건드리지 않음. build/*.json 은 과거 실행 산출물(추적 안 됨).
- tools/ 는 리더 소유 → ps1_patch_draft.txt 로 초안만. 이 스크립트는 CI 가 안 돌리고(README 만 언급) 수동 E2E 이며 실행해 보지 않았다. 적용 전까지 이 스크립트의 두 줄 단언은 빈 값을 읽어 실패한다.
- 시험: 클래스·생산자·README 에 새 키가 있고 옛 키가 없음을 고정(TheSelfCheck…Requested…). 반증: 생산자에 옛 키를 되돌리면 빨강(falsification_old_key_in_producer.txt).
- 실행: Mock 백엔드로 실제 자체 점검 1회 — Passed=True, 새 키 3개가 값과 함께 나오고 옛 키는 없음(self_check_keys.txt).
- 통합 시험 연속 3회 946 통과/0 실패 (-v n 출력엔 요약 줄이 안 찍혀 통과 줄 수로 셈).

## PreprocessHandshakeTests 간헐 실패
- 233e 에서 첫 전체 실행에 1회, 이번 카드 중 별도 실행에서 1회 더 관측(둘 다 PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds [FAIL] 한 줄).
- **원문(오류 메시지·스택)은 보존하지 못했다**: 첫 관측은 한 줄만 남겼고, 두 번째는 리다이렉트 순서 실수로 xUnit 의 실패 줄이 파일이 아니라 화면으로 갔다. 그 뒤 -v n 으로 전체를 파일에 받은 연속 3회는 모두 통과해 재현하지 못했다. 시험 순서 고정 재현은 리더가 맡는다고 해서 하지 않았다.
