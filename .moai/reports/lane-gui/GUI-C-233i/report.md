# GUI-C-233i 보고 — 수동 실 E2E 스크립트의 단언을 요청한 백엔드의 사실과 비교 (Refs #251)

dev/gui 에서 작업, 233h(`7589e130`) 위. 푸시 안 함. 제품 코드 아님(시험 스크립트·fixture). `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1` 은 리더가 이 한 건에 한해 dev/gui 에서 커밋하도록 지정.

## 바꾼 것
- **manifest** (`gui/ImageProcTest/fixtures/gui-s0/fixture-manifest.json`): 백엔드별로 나눔 — `backendVersionMock = "v0.0.0-mock"`, `backendVersionNative = "read-from-dll: …"`(상수가 아니라 DLL 에서 읽는다는 표시), `initialAlertCountMock = 3`, `initialAlertCountNative = 1`, `initialAlertNative = "REAL_DISPLAY_BACKEND_ACTIVE"`. C# 모델(`GuiFixtureExpectedTelemetry`)과 SelfCheck 의 Mock 단언(`…Mock`)을 같이 바꿨다(옛 키를 읽는 곳은 이 둘뿐 — `git grep` 으로 확인).
- **스크립트**: `-Backend Native|Mock`(기본 Native) 매개변수를 두고 앱 인자와 단언이 모두 그것을 따른다. Native 는 (a) 보고서의 `ActualBackendMode` 가 Native, `MockBackend` 가 false, (b) `BackendVersion` 에 "mock" 이 없음, (c) **DLL 이 스스로 보고하는 버전**과 같음 — 스크립트가 P/Invoke 로 `xpe_display_version()` 을 앱에 준 폴더(`XPE_NATIVE_DIR`, 없으면 앱 폴더)에서 직접 읽어 `xpe_display <버전>` 과 `DisplayVersion` 을 비교. Mock 은 manifest 의 Mock 값.

## 바꾼 단언마다 "왜 원래 단언이 틀렸나"
| 단언 | 원래 | 왜 틀렸나 | 지금 |
|---|---|---|---|
| 백엔드 버전 | `BackendVersion -eq manifest.backendVersion` (`v0.0.0-mock`) | 스크립트는 2026-09-17 이후 Native 를 요청하는데 기대값은 Mock 문자열. Native 로는 통과가 불가능한 단언이었고, 반대로 Mock 이 실수로 실행돼도 이 한 단언만으로는 "요청과 같다" 를 말하지 못했다 | 요청한 백엔드의 사실(위) |
| 초기 알림 수 | `InitialAlertCount -eq manifest.initialAlertCount` (3) | 3 은 Mock 이 시작 때 스스로 올리는 시나리오 알림 3개(`MOCK_BACKEND_ACTIVE` 등)의 수. Native 는 실제로 1개(INFO `REAL_DISPLAY_BACKEND_ACTIVE`)를 올린다 — 앞 단언을 고치자 곧바로 여기서 멈췄다 | 백엔드별 기대 수(Mock 3, Native 1) + Native 는 내보낸 런타임 로그에 그 알림 코드가 있음 |
| 초기 로그 수 | `InitialLogCount -ge 5` | 하한이라 두 백엔드 모두 맞다(Native 실측 9) | 그대로 |
나머지 단언(적재 후 로그 증가, 이미지 요약, 교정 요약, 도움말, 종료 상태 등)은 Native 실행에서 그대로 통과해 바꾸지 않았다.

## 실행 결과
- **Native 1회 끝까지**(`XPE_NATIVE_DIR` = `c233_native`, 앞서 쓴 CI 빌드 `ci-dlls-39f9727a` 7개 + post 의 `xpe_display.dll`, `gsvg.dll`; 출처 미증명): 종료 코드 0, "GUI real automation E2E passed." (`native_run_c233.txt`). 보고서 `ActualBackendMode=Native`, `BackendVersion="xpe_display 1.0.0"`, `DisplayVersion="1.0.0"`, `InitialLogCount=9`, `InitialAlertCount=1`. 첫 시도(233h)에서 멈춘 버전 단언 다음에 "초기 알림 수" 단언이 또 멈췄고 위 표대로 고친 뒤 통과.
- **Mock**(`-Backend Mock`, DLL 폴더 없이): 종료 코드 0, 통과 (`mock_run.txt`).
- SelfCheck 통과(`selfcheck.txt`) — Mock 의 manifest 단언이 새 키(`…Mock`)로 읽는다.

## 반증 (Native 요청인데 Mock 이 실행되는 경우)
`XPE_NATIVE_DIR` 을 빈 폴더로, `XPE_NATIVE_DIR_EXCLUSIVE=1` 로 두고 Native 를 요청하면 스크립트가 **빨강**(종료 코드 1, `falsification_native_requested_dll_folder_empty.txt`). 다만 그때 멈춘 것은 새 단언이 아니라 그 앞의 `Automation report marked failure`(앱 자신이 `BackendMatchesRequest=false` 라서 `Passed=false` 로 쓴 보고서)다. 새 단언이 독립적으로 잡는지는 같은 보고서 값으로 따로 평가했다(`falsification_report_values.txt`): `ActualBackendMode=Mock`, `MockBackend=True`, `BackendVersion="v0.0.0-mock"` 이므로 새 단언 세 개(`ActualBackendMode -eq Native`, `MockBackend -eq false`, `BackendVersion -notlike *mock*`)가 모두 거짓이다. 즉 앞 단언이 없었다 해도 새 단언이 잡는다 — 다만 그 상황을 새 단언만 남기고 스크립트로 다시 돌려 보지는 않았다.

## 한계
- 결합 CI 빌드 한 벌(같은 출처의 DLL)로는 돌리지 않았다. 사용한 DLL 의 출처는 위와 같이 미증명.
- `xpe_display_version()` 만 DLL 사실로 쓴다. 다른 모듈 버전은 이 스크립트가 검사하지 않는다.
- 이 스크립트는 CI 가 돌리지 않는 수동 E2E 다(README 만 언급).
