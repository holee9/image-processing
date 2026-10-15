# GUI-C-204 — 행동을 단언하는 시험이 없던 메뉴 항목에 시험 (#225)

증거(이 폴더): `after_fix.txt`(Mock·Native 실행 결과), `falsification_arms.txt`(팔 11개), `falsification_arm_z3b.txt`.
새 시험: `MenuBehaviorScenarios`(Z01·D01·A01·H01×3), `MenuExitScenarios`(X01), 고정 장치 `ExitApplicationFixture`, `ApplicationFixture.WatchForExit/WaitForExitCode`(덧붙임만). 앱 코드 변경 없음. E2E 는 전부 UIA 패턴만 쓴다(키보드·마우스 없음).

## 1. 항목마다 코드가 하는 일 → 시험이 보는 것

| 항목 | 핸들러(감싼 조건까지) | 시험 | 단언 |
|---|---|---|---|
| Zoom In / Out | `ZoomIn`: `current = scale <= 0 ? 1.0 : scale; scale = Math.Min(16.0, current*1.25)`; `ZoomOut`: `Math.Max(0.05, current/1.25)`. 끝에 로그 "Comparison viewport zoomed in/out. … Zoom=NNN%" | Z01 | Fit→125%→156%→125%→100%, 13단계 뒤 1600%·한 번 더 해도 1600%, Fit 에서 14단계 뒤 5%·한 번 더 해도 5%, Fit 으로 복구 |
| Native DLL Diagnostics | `ShowNativeDiagnostics`: `DisplayDllDetected` 이면 상태줄 "Display DLL detected at '<path>' (<ver>)." 아니면 "…not detected; mock display pipeline remains active."; 로그 "Native diagnostics: backend=…" | D01 | 로그의 backend 가 실제 구동 중인 백엔드, 감지 시 경로가 디스크에 있고 로그 displayPath 와 같음, Native 는 mock 아닌 semver·`XPE_NATIVE_DIR` 안의 파일·상태줄 런타임 요약에 같은 버전, 미감지 시 정확한 문구와 `displayDetected=False`(Native 에서 미감지는 실패) |
| About | `AboutBuildInfoMenuItem_OnClick`: 모달 메시지 상자 "ImageProcTest GUI-S0 / Backend: <RuntimeInfo.Version> / Help bundle: packaged offline HTML" | A01 | 상자가 뜸, 세 줄 내용, Backend 값이 "unknown" 아님·상태줄 요약에 들어 있음·Native 는 semver·Mock 은 mock 표시, 창 제목에 GUI-S0, OK 로 닫힘 |
| Exit | `Close()` → `OnClosing` 이 첫 닫기를 취소하고 백엔드 종료를 백그라운드로 돌린 뒤 스스로 닫음. 저장 안 된 설정에 대한 물음·저장 없음 | X01 | 저장 안 된 변경(Zoom In)을 둔 채 Exit → 60초 안에 프로세스 종료, 종료 코드 0(측정: 약 1초, 0) |
| Quick Start / Scope / Current Workflow | `OpenHelpPage`: 쪽 파일이 있으면 소유 도움말 창(제목 "ImageProcTest Help - …")을 띄움, 없으면 경고 상자 | H01 ×3 | 해당 제목의 창이 뜸, 창이 가리키는 경로가 앱 `help` 폴더의 기대 파일이고 존재·`<h1>` 내용 있음, 오류 덮개 비어 있음 |

## 2. 새로 알게 된 것

1. **Current Workflow Help 는 별도 쪽이 없다.** 핸들러가 `HelpPageKind.QuickStart` 를 연다 — Quick Start 와 같은 파일이다(측정: 둘 다 `quick-start.html`). 시험은 "열리고 내용이 있다"만 단언하고 매핑을 못 박지 않았다. 별도 쪽을 만들지는 결정거리다(리더).
2. **줌 상한은 두 군데에서 막힌다**: `MainWindowViewModel.ZoomIn` 의 `Math.Min(16.0, …)` 와 `AppSettings.ComparisonZoomScale` 의 `Math.Clamp(value, 0.0, 16.0)`. 한쪽만 바꾼 팔 Z3 은 초록(남은 한쪽이 막음)이고 둘 다 바꾼 Z3b 에서 1600% 가 1819% 가 되어 빨강. 하한 0.05 는 VM 한 곳뿐(AppSettings 는 0 까지 허용)이라 팔 Z4 가 바로 빨강.
3. **About 에는 앱 자체의 빌드 식별자(버전·커밋)가 없다** — 상수 "GUI-S0" 와 백엔드 버전만 보인다. 카드의 "버전 문자열이 빌드 정보와 일치"는 "백엔드 버전이 상태줄이 보여 주는 버전과 같다"로 단언했다. 앱 빌드 식별자를 About 에 싣는 것은 새 기능이라 하지 않았다.
4. 로그 목록은 **최신이 맨 위**(`Log` 가 index 0 에 삽입). 첫 시도가 "마지막 줄"을 읽어 Zoom 이 안 변한 것처럼 보였다(시험 오류, 고침).
5. 종료 코드: FlaUI `Application.ExitCode` 도 `Process.ExitCode` 도 이 앱(붙은 프로세스)에 대해 던진다. 앱이 1초 안에 끝나므로 종료 전에 열어 둔 핸들(`OpenProcess`)로 `GetExitCodeProcess` 를 읽는다.

## 3. 반증 (`falsification_arms.txt`, 전부 바이트 동일 복원, 마지막에 앱·시험 재빌드)

| 팔 | 결과 |
|---|---|
| Z1 Zoom In 이 로그만 남기고 값 불변 · Z2 Zoom Out 동일 · Z4 하한 0.05→0.005 | Z01 빨강 |
| Z3 상한 한쪽 16→160 | **초록**(두 번째 막이 있음) → Z3b 둘 다 → Z01 빨강(1600% 기대, 1819%) |
| D1 진단 비움 · D2 backend= 에 버전을 씀 | D01 빨강 |
| A1 About 비움 · A2 Backend 를 "unknown" 으로 | A01 빨강 |
| H1 OpenHelpPage 비움 | H01 세 건 모두 빨강 |
| H2 Scope 가 Quick Start 를 엶 | Scope 만 빨강(나머지 둘 초록) |
| X1 Exit 가 닫지 않음 | X01 빨강 |

## 4. 구성

- Mock·Native(스텁 AI 빌드 모듈 디렉터리 + `xpe_ai.dll` 과 워커) 모두 전부 통과(`after_fix.txt`, 각 6/6 실행 건). 구성에 따라 건너뛴 것은 없다 — 구성에 따라 갈리는 단언(Native 의 semver·경로·고정 디렉터리, Mock 의 mock 표시)은 시험 안에서 분기하며, 느슨하게 둘 다 받아 주지 않는다.

## 미검증·한계

1. D01 의 Mock 갈래는 이 기계에서 "미감지" 쪽만 관측했다. Mock 인데 디스크에 표시 DLL 이 있어 "감지" 로 나오는 경우(mock 버전 문자열)의 분기는 코드에 있으나 실행하지 않았다.
2. 팔은 Mock 백엔드에서만 돌렸다(Native 는 통과만 확인).
3. Exit 의 "저장 안 된 상태" 는 Zoom In 하나로만 만들었다. 설정 파일이 안 바뀌었음은 단언하지 않았다(핸들러·`OnClosing` 코드가 저장하지 않음을 읽은 것).
4. CI(`gui-e2e-native`, Mock 잡)에서의 실행은 아직 모른다. 새 시험이 시작·종료하는 앱은 Exit 시험 하나이고, 이 앱 사본의 `Dispose` 는 이미 끝난 프로세스에 `Kill` 이 던지는 예외를 기존 코드가 삼킨다.
5. 도움말 창의 WebBrowser 본문은 UIA 로 읽을 수 없어 화면 내용이 아니라 창이 가리키는 파일의 내용을 단언했다.
