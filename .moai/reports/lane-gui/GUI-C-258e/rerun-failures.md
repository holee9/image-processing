# GUI-C-258e 전체 Native 재검증 실패

원문: `full-rerun-results/full-native-rerun.trx`, `full-native-rerun.txt`와 같은 내용의 `green-full.txt`. 허용된 evidence 3건 외 A06이 남아 15:22 완료 기준을 충족하지 못했다. 추가 수정·재빌드·시험은 중단했다.

| 시험 이름 | TRX 오류 원문 |
|---|---|
| ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB01_InitializeBackend_StartsANewRunSet | An evidence folder is missing. |
| ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261104-154650-d19855. |
| ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported | The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'. |
| ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.EB02_TwoRunSets_KeepSeparateEvidence | No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261104-154658-a3c83e. |

허용 외 이름·원인 한 줄: **A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported — 내부 WPF E2E failed, runner exit -532462766; 근접 시각 동일 runner 기존 이벤트는 Program.cs:219의 `Preprocessing menu must be disabled until Phase 1a.` 단언 위반(Assert :650)과 정합하지만 특정 test PID 일대일 귀속은 미입증.** 기존 OS Event ID 1026 원문은 `a06-existing-event.txt`에 있다. freshness 거절과 구분한다.
