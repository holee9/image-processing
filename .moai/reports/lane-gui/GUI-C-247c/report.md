[2026-10-15 22:59 KST] GUI-C-247c 보고서 — W16 건너뜀 사유에 허용 토큰 `XPE-SKIP-ALLOWED:272` 추가 (Refs #251)

## 1. 변경

PR #271 CI(run 38734937688)의 "Assert no unexplained skipped Native E2E cases" 단계가 W16 의 건너뜀을 "사유 없음"으로 세었다. 이 게이트는 사유에 `XPE-SKIP-ALLOWED:<이슈>` 토큰이 있어야 허용으로 센다(W10 은 `:265`). 리더가 이슈 #272 와 `ci.yml` 허용 목록 행을 만들었다.

`QuickStartWalkthroughScenarios.cs` 의 W16 `Skip.If(... "XPE_C247_CALIB/XPE_C247_RAW not set ...")` 사유 뒤에 W10 과 같은 방식으로 한 구절과 토큰을 붙였다.

```
XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository. Last run by hand on 2026-10-15 against calib-cyan-v3 (GUI-C-247 report). XPE-SKIP-ALLOWED:272
```

그 외 동작은 바꾸지 않았다. 설정됐는데 폴더·파일이 없으면 여전히 `Assert.True` 로 실패한다(코드 변경은 사유 문자열뿐).

## 2. 검증 (카드대로 1회)

명령: `XPE_C231_NATIVE_DIR=D:\workspace-github\xpe-data\gui-247-dlls` (개인 폴더), `XPE_C247_*` 미설정, `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj --no-build --filter "FullyQualifiedName~W16_ARealFrame"` (빌드는 직전에 별도로 오류 0). 출력 전문: `run1_unset.txt`.

```
W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings [SKIP]
  XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository. Last run by hand on 2026-10-15 against calib-cyan-v3 (GUI-C-247 report). XPE-SKIP-ALLOWED:272
총 테스트 수: 1 / 건너뜀: 1
```

- 가용 메모리 전→후: 6564 → 6229 MB. 실행 뒤 남은 ImageProcTest/testhost/xpe_ai_worker 0.
- 실행 중 소스 편집 없음(편집 → 빌드 → 실행 순서).

## 3. 한계

- 게이트가 이 사유를 실제로 허용으로 세는지는 CI 의 "Assert no unexplained skipped Native E2E cases" 단계에서만 확인된다. 이 세션은 사유에 토큰이 찍히는 것까지만 봤다.
- 247b 의 통과·실패 경로는 이번에 다시 돌리지 않았다(카드 지시, 247b 보고서에 증거 있음).
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. W16 사유에 `XPE-SKIP-ALLOWED:272` 추가(+수동 실행 기록 한 구절) | GUI-C-247c (1절) |
| 2. 그 외 동작 불변 | GUI-C-247c (1절) |
| 검증: 미설정 1회 → Skipped + 토큰 노출 | GUI-C-247c (2절) |
