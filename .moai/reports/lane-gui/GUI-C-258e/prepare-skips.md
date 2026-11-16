# GUI-C-258e 관리형 준비 복원 후 SKIP

원문: `prepare-results/prepare-native.trx`. Legacy 그룹+A06 선택 실행 총 26건 중 23 PASS / 0 FAIL / 3 SKIP이다. 첫 전체 실행의 25 SKIP과 별도 실행이며 합산하지 않는다.

| 시험 이름 | TRX 사유 원문 |
|---|---|
| ImageProcTest.E2ETests.Scenarios.Legacy.LegacyInputKindScenarios.K1_TheInputKindChoice_ReachesThePreview_AndChoosingAnotherKind_InvalidatesOnlyTheMeasuredRows | this xpe_preprocess.dll does not export xpe_verify_offset_signed_file (older than dev/preprocess 026ea384). XPE-SKIP-ALLOWED:273 |
| ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheLegacyAppsStartUp_MessagePumpAndUiAutomation | XPE_C235_RUNS is not set (this is a measurement, not a regression test). |
| ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheFlaUiWindowCall_WithASlowStartUpProbe | XPE_C235_DELAYS is not set (this is a measurement, not a regression test). |
