[2026-10-14 15:14 KST] [xpe-gui] GUI-C-238 보고 — 시작 스크립트로 띄운 앱이 Mock 으로 뜨는 결함 (마일스톤 3 U1, Refs #251)

푸시하지 않았다. 기준 `64c9a89e`. 이 카드의 변경은 236c 의 미커밋 작업과 같은 작업 트리에 있어서, 238 의 파일만 명시적 경로로 올린다(`MainWindowViewModel.cs` 는 236c 의 변경과 같은 파일이라 238 의 한 줄만 따로 스테이징했다).

## 1. 원인 (확정, 재현으로)

**낡은 `xpe_common.dll`·`xpe_display.dll` 이 exe 와 같은 폴더에 있으면, `XPE_NATIVE_DIR_EXCLUSIVE=1` 인데도 그 파일이 이긴다.** 앱은 Mock 으로 뜬다.

- 리더 체크아웃의 앱 폴더(`image-processing/gui/ImageProcTest/bin/Debug/net8.0-windows/`)에 2026-04-16 자 `xpe_common.dll`(39,936 B)·`xpe_display.dll`(9,728 B)가 있었다. 같은 인자·같은 DLL 폴더(`build/e2e-native-dlls`, provenance 6b8477ef)로:
  | 앱 | 결과 | 증거 |
  |---|---|---|
  | 내 워크트리 앱(그 폴더에 `xpe_*.dll` 없음) | `mode=Native … src=e2e-native-dlls` | `probe_my_exe_leader_dlls.txt` |
  | 리더 앱(낡은 두 DLL 있음) | `[MOCK]`, `mode=Mock (requested Native)` | `probe_leader_exe_leader_dlls.txt` |
  | 내 앱 폴더 **사본**에 그 두 DLL 만 복사 | `[MOCK]` — 재현됨 | `probe_my_exe_plus_stale_dlls.txt` |
- 세 조건 중 거짓인 것: `BackendMode=="Native"` 는 참(인자가 반영됨), `XPE_NATIVE_DIR`·`EXCLUSIVE` 도 앱 프로세스에 들어갔다(같은 환경에서 내 앱은 Native). 거짓인 것은 **둘째·셋째가 합쳐진 판정**: 위치 탐지가 낡은 DLL 을 골랐고(`XpeCommonLibraryLocator.TryFindDll` 이 후보 중 처음 존재하는 것을 돌려줌), 그 DLL 이 `CanUseNative` 의 필수 내보내기를 갖지 않았다.
- 코드: 네 탐지기(`XpeCommonLibraryLocator`, `XpePreprocessLibraryLocator`, `XpeEnhanceBasicLibraryLocator`, `NativeModuleLibraryLocator`)의 후보 순서가 **exe 폴더 → 지정 폴더(→ 거기서 중단)** 였다. `EXCLUSIVE=1` 은 지정 폴더 **뒤**에서만 멈췄고 exe 폴더를 건너뛰지 못했다. `NativeSearchPolicy` 의 주석("Set to 1 to search ONLY XPE_NATIVE_DIR")과 어긋나 있었다.
- 왜 숨었나: CI 의 E2E 는 빌드 폴더의 exe 를 그대로 띄우고 그 폴더에는 `xpe_*.dll` 이 없다. 앱 폴더에 DLL 이 남는 경로(수동 빌드 흔적)는 CI 에서 한 번도 돌지 않았다.

## 2. 수정 (제품 코드, 최소)

- `NativeSearchPolicy.ExclusiveDirectory`(새): 고정이면 지정 폴더, 아니면 null. 네 탐지기가 맨 앞에서 먼저 확인해, 고정 상태면 **그 폴더만** 후보로 준다(exe 폴더 제외). 고정하지 않은 기본 동작(exe 폴더 우선)은 그대로다.
- 시작 스크립트(`tools/Start-ImageProcTestNative.ps1`)는 고칠 필요가 없었다 — 이미 `EXCLUSIVE=1` 을 건다. 수정안 `.txt` 초안 없음. 선택 사항으로, 스크립트가 exe 폴더의 `xpe_*.dll` 존재를 경고하면 같은 함정을 사전에 알려 줄 수 있다(제안일 뿐, 하지 않았다).

## 3. "왜 Native 를 못 썼나" 를 배너·로그에 구체적으로

- `RealXpeBackend.WhyNativeUnusable`(새): 어느 DLL, 어느 경로(크기·수정일 포함: 낡은 사본은 이것으로 알아본다), 무엇이 문제인지(없음 / 로드 안 됨 / 처음 빠진 내보내기)를 한 문장으로 돌려준다. `CanUseNative` 는 이것이 null 인지로 정의된다.
- `XpeBackendFactory` 가 이 이유를 Mock 에 넘기고(`MockXpeBackend` 생성자 선택 인자, `BackendRuntimeInfo.NativeRefusal`), 배너가 이유를 이어 붙인다. 로그에도 "Native was requested but not used: …" 한 줄.
- 실제 출력(고정하지 않은 경우의 시험 결과): `MOCK BACKEND — Native was requested but could not be used: xpe_common.dll at 'C:\…\app\xpe_common.dll' (1143808 bytes, written 2026-10-14) could not be loaded. Images and results are synthetic; not for clinical judgement.`

## 4. 회귀 시험 (CI 에서 도는 경로)

`clients/ImageProcTest.E2ETests/Scenarios/Smoke/NativeLaunchPathTests.cs` — 앱 출력 폴더를 임시 폴더로 복사하고 거기에 **이름은 맞고 내용이 틀린** `xpe_common.dll`·`xpe_display.dll` 을 넣은 뒤, 시작 스크립트와 같은 인자(`--automation-backend Native --automation-calib … --automation-width/-height`)와 환경(`XPE_NATIVE_DIR`, `EXCLUSIVE=1`)으로 띄운다(UIA 로 읽기만, 전역 입력 없음).
1. `OldDllsBesideTheExe_DoNotWinOverThePinnedFolder_TheAppStartsOnNative`: 제목에 `[MOCK]` 없음, `mode=Native`, `MOCK BACKEND` 배너 없음, `src=<고정 폴더 이름>`.
2. `OldDllsBesideTheExe_WhenNotPinned_MockStartsAndTheBannerNamesTheDllAndTheProblem`: 고정하지 않으면 exe 폴더가 먼저라 Mock 이 맞고, 배너가 DLL 이름·경로·문제를 말해야 한다.
- 실행: 2/2 통과(`native_launch_fixed.txt`, `.trx` 는 커밋하지 않음). `XPE_NATIVE_DIR` 이 없으면 건너뛴다(Mock 잡은 건너뜀 게이트가 없고, Native 잡은 `XPE_NATIVE_DIR` 를 설정하므로 거기서 돈다 — ci.yml 1246~1248행을 읽었으나 CI 에서 실제로 돌려 보지는 않았다).
- **반증** (`falsify.txt`): 탐지기 수정을 되돌리면(팔 A) 1번이 빨강(`Sub-string found`: 제목에 `[MOCK]`), 배너에서 이유를 빼면(팔 B) 2번이 빨강. 복원은 바이트 비교로 확인했고 복원 뒤 2/2 통과.
- 영향 확인: `NativeSearchPolicy|GuiNativeSearchAdoption|DegradedModeReadiness|EnhanceBasicStage|BaselineDicomExport` 58/58 통과(`integration_native_search.txt`), SelfCheck 전 시나리오 통과(`selfcheck_after238.txt`). 배너 문구를 고정한 기존 시험은 grep 으로 찾지 못했다.

## 한계

- 시험이 쓰는 "낡은 DLL" 은 실물이 아니라 같은 세트의 다른 모듈 DLL(`xpe_preprocess.dll`, `xpe_enhance_basic.dll`)을 이름만 바꿔 넣은 것이다. 실물 낡은 파일로 낸 수정 전 재현은 `probe_my_exe_plus_stale_dlls.txt` 이고, 리더가 그 파일을 옮긴 뒤라 수정 후 재확인은 대용품으로만 했다.
- 정지 상태의 확인이다: Native 로 뜨는 것(`mode=Native`, `src=`)까지 보았고, 영상을 열어 처리하는 흐름은 이 카드에서 다시 돌리지 않았다.
- CI 의 Native E2E 잡에서 새 시험이 도는 것은 확인하지 못했다(리더가 PR CI 로).
- 메모리: 작업 뒤 가용 4356 MB, 남은 `ImageProcTest` 프로세스는 리더의 것 1개뿐(`memory_after.txt`). 시작 전 값은 이 카드용으로 따로 재지 않았다(직전 236c 측정 4707 MB).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 원인 확정 | GUI-C-238 |
| 2 최소 수정 | GUI-C-238 |
| 3 거부 이유를 배너·로그에 | GUI-C-238 |
| 4 시작 스크립트 경로 회귀 시험 | GUI-C-238 |
