[2026-10-14 15:57 KST] [xpe-gui] GUI-C-238b 보고 — Codex #193 B-2·B-3: 고정 모드에서 앱 폴더·기본 로더로 다시 빠지는 경로 막기 (U1, Refs #251)

푸시하지 않았다. 기준 `c641e6da`. 가용 메모리 5349 MB 로 시작해 5423 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`). 고정 = `XPE_NATIVE_DIR_EXCLUSIVE=1`.

## 막은 경로 (카드 1~4)

| # | 경로 | 수정 | 증거 |
|---|---|---|---|
| 1 | `XpeBackendFactory.Create` 가 DLL 을 못 찾으면 앱 폴더 경로로 대체해 그 DLL 을 검사·시작 | 고정이면 보고 경로는 **지정 폴더** 안(`reportFolder`), 못 찾으면 "어느 폴더의 어느 DLL 이 없음"으로 Mock(배너·로그) | 시험 `PinnedFolderMissingCommon_…` (앱 폴더에 **작동하는** DLL 이 있어도 Mock, 배너가 지정 폴더 경로를 말함) |
| 2 | `GuiNativeLibraryResolver.Resolve` 가 고정 후보 실패 시 `IntPtr.Zero` 로 기본 .NET 로더에 넘김 | 고정이면 **던진다**(`DllNotFoundException`, 지정 폴더와 "default search was not used"를 말함). 로드는 `LoadLibraryEx(DLL 폴더 + System32)` 로 제한(`OracleModuleConfinement` 재사용): 이 DLL 이 이름으로 임포트하는 DLL 도 앱 폴더·PATH 에서 안 가져온다 | SelfCheck 시나리오 11 |
| 3 | `EXCLUSIVE=1` 인데 `XPE_NATIVE_DIR` 없음이 조용히 기본 탐색 | `NativeSearchPolicy.ExclusiveWithoutDirectory`(새): 네 탐지기가 후보를 **하나도 주지 않고**, 팩토리가 "XPE_NATIVE_DIR does not name it (the application folder is not searched instead)" 로 Mock | 시험 `ExclusiveWithoutAFolder_…` , SelfCheck |
| 4 | 종속 DLL·gsvg/AI/DICOM 이 지정 폴더에서 왔는지 모름 | `NativeModuleAudit`(새): 로드된 모듈 목록(`Process.Modules`)에서 `xpe_*`·`dcm*`·fmt·spdlog·ofstd·oflog·openjp2·gsvg 를 골라 지정 폴더 안/밖으로 분류, 로그에 요약 1줄 + 밖인 모듈마다 1줄 + 밖이면 `NATIVE_MODULE_OUTSIDE_PINNED_FOLDER` 경고 알림. 초기화 직후와 첫 체인 직후(다른 모듈은 첫 사용 때 로드) | 아래 "감사가 찾은 것" |

**제한된 LoadLibraryEx 는 현실적이었다**: 고정 모드의 모든 로드가 이 형태로 실행되며, 지정 폴더에서 Native 로 뜨는 시험(OldDllsBesideTheExe_DoNotWin…, 실데이터 W1·W7·W12·W14)이 통과한다. 프로세스 전역 `SetDefaultDllDirectories` 는 되돌릴 수 없어 쓰지 않았다(219d 와 같은 결정).

## 감사가 찾은 것 (카드 4, 수정 전·후 실측)

- **수정 전**(`real_data_regression.txt`): 지정 폴더 `build/e2e-native-dlls` 로 띄운 앱의 감사 줄 — `5 native module(s) loaded, 4 from the pinned folder, 1 from elsewhere` 와 `fmt.dll was loaded from 'D:\workspace-github\image-processing\vcpkg_installed\x64-windows\bin\fmt.dll'`. **고정 모드인데 `fmt.dll` 이 vcpkg 폴더에서 두 번째로 로드되고 있었다.** 원인: `NativeDependencyLoader.GetDependencyDirectories` 가 지정 폴더 다음에 그 위쪽 폴더의 `vcpkg_installed\…\bin` 을 찾아 거기서도 종속 DLL 을 로드한다.
- **수정**: 고정이면 종속 DLL 도 지정 폴더에서만(`NativeDependencyLoader`, 위 `yield break`). **수정 후**(`audit_after_fix.txt`): `4 native module(s) loaded, 4 from the pinned folder, 0 from elsewhere`.
- 이 수정은 별도 반증 시험이 없다(앞뒤 실측 로그가 증거). 감사 분류 자체는 SelfCheck 에서 입력을 지어 확인한다(밖에 있는 모듈 1건, 대소문자 무시, kernel32 제외).

## 시험 (CI Native 잡에서 도는 `NativeLaunchPathTests`, 6건)

앱 출력 폴더를 임시 폴더로 복사하고 시작 스크립트와 같은 인자·환경으로 띄운다.
1. 낡은 DLL 이 exe 옆 + 지정 폴더 정상 → Native (238, 그대로).
2. 낡은 DLL 이 exe 옆 + 고정 아님 → Mock, 배너가 DLL·경로·문제를 말함 (238).
3. **대조군**: exe 옆에 **작동하는** DLL + 고정 아님·`XPE_NATIVE_DIR` 없음 → Native, `src=app`. 이게 있어야 아래 세 시험의 "Mock"이 앱 폴더 DLL 이 원래 안 되는 탓이 아님이 증명된다.
4. 지정 폴더에서 common 이 **빠짐** + 앱 폴더에 작동하는 DLL → Mock, 배너가 지정 폴더의 `xpe_common.dll` 부재를 말하고 앱 폴더 경로는 말하지 않음.
5. 지정 폴더의 common 이 **손상 파일** + 앱 폴더에 작동하는 DLL → Mock, "could not be loaded".
6. `EXCLUSIVE=1`, `XPE_NATIVE_DIR` 없음 + 앱 폴더에 작동하는 DLL → Mock, "XPE_NATIVE_DIR does not name it".
- 6/6 통과(`native_launch_fixed.txt`; 배너 전문이 출력에 있다). SelfCheck 30 시나리오 통과(`selfcheck_run1.txt`, 새 시나리오 11: 해리 DLL 대조 → 고정이면 던짐 → 고정 폴더 안 DLL 은 거기서 로드·경로 기록 → 폴더 없는 고정은 던짐 → 감사 분류).
- **반증** (`falsify.txt`, 복원 바이트 동일, 마지막에 6/6·SelfCheck 통과):
  | 팔 | 되돌린 것 | 빨강이 된 것 |
  |---|---|---|
  | A | 팩토리의 앱 폴더 대체 | 시험 4 |
  | B | 리졸버가 기본 로더에 넘김 | SelfCheck 11 (2건) |
  | C | 폴더 없는 고정 미인식 | 시험 6, SelfCheck 11 |
  | D | 탐지기의 지정 폴더 한정(238 수정) | 시험 1, 4, 5 |
  시험 5 는 A·B·C 에서는 빨강이 되지 않는다(손상 파일이 지정 폴더에 있으면 그 이유로 거절되는 것이 원래 동작) — D 에서만 빨강. 카드의 "각 시험 되돌리면 빨강"은 팔 D 까지 포함해 충족.
- 영향: 통합 시험 185/185(`integration_native_search.txt`: 탐색 정책·GUI 채택·오라클·저하 모드·준비도 등), 실데이터 W1·W7·W12·W14·W3 통과(`real_data_regression2.txt`, `audit_after_fix.txt`; 생성 세트 + 리더의 CI DLL 폴더).

## 한계
- 감사는 **그 시점에 로드된 모듈**만 본다. 초기화 직후에는 common·display 와 종속 DLL 이고, 나머지(preprocess·enhance_basic·dicom·gsvg·ai)는 첫 사용에 로드된다. 첫 체인 직후 감사 줄은 코드에 있으나 이번 실행 출력에서 그 줄을 읽지 않았다(W3 가 마지막 12줄만 찍음).
- 기본(비고정) 모드는 의도적으로 바꾸지 않았다: exe 폴더 우선 + vcpkg 탐색 그대로(감사는 "고정 아님" 한 줄만).
- `PipelineOrchestrator` 의 `LoadLibrary(dllPath)` 는 호출자가 준 폴더의 DLL 을 직접 부르는 별도 경로라 건드리지 않았다.
- 탐지기·정책·종속 로더는 `clients/` 의 공유 소스라 레거시 앱도 같은 규칙을 받는다(통합 시험 185 통과가 확인한 범위까지).
- CI 러너에서는 실행하지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 팩토리 앱 폴더 대체 금지 | GUI-C-238b |
| 2 리졸버의 기본 로더 폴백 금지 | GUI-C-238b |
| 3 `EXCLUSIVE=1` + `XPE_NATIVE_DIR` 없음 | GUI-C-238b |
| 4 모듈 경로 감사 + 제한된 LoadLibraryEx 검토 | GUI-C-238b (+ vcpkg 중복 로드 수정) |
| 반증 시험 3건 | GUI-C-238b |
