# GUI-C-235 보고 — LegacyPreprocessReadiness 묶음의 반복 UIA Timeout (Class B, Refs #251)

푸시하지 않았다. 기준은 dev/gui `ca2d491b`(= main `468a3d67` 의 gui·clients 내용, 레거시 앱 `clients/ImageProcTest` 는 이 카드 이전과 같음). 로컬은 Legacy 묶음 위주로, 시작 전 가용 메모리 7810 MB / 32509 MB (`memory_before.txt`), 끝난 뒤 정리 (`memory_after.txt`).

## 판정: 앱이 실제로 멈춘다 (제품) — 그리고 수정하면서 새로 만든 경합 하나를 같이 막았다

### 1. 시간 초과 지점 (CI 로그 인용)
세 실행 모두 같은 스택이다: `UIA3Automation.FromHandle` → `CUIAutomation8Class.ElementFromHandle` 의 `COMException 0x80131505 (Operation timed out)` → `TimeoutException : UIA Timeout`, 호출자는 `Application.GetMainWindow` ← `LegacyApp.LaunchOrSkip(...)` (`LegacyPreprocessReadinessScenarios.cs:479`) ← 시험 본문의 **첫 줄**(R05 `RunTheCommandAfter`:381, R08:348, R06 의 launch). 즉 DLL 교체·버튼 동작이 아니라 **시험 본문이 시작하기 전, 창 핸들이 생긴 직후의 UI Automation 연결 호출**이다.

### 2. 앱이 멈추는가, UIA 만 느린가 → 앱이 멈춘다
- 바깥에서 `SendMessageTimeout(WM_NULL, SMTO_ABORTIFHUNG)` 를 25 ms 마다 보내는 프로브(`LegacyStartupResponsivenessProbe`)로 매 기동의 UI 스레드 응답을 쟀다 (`probe_local_*.txt`, 10회씩): **창 핸들이 생긴 뒤에도 UI 스레드가 160 ms(원본 DLL 폴더) / 390 ms(방금 복사한 DLL 폴더, R05·R06·R08 처럼) 동안 메시지에 답하지 않는다**(CPU 1개로 고정하면 470 ms). 응답이 없는 동안은 UIA 도 답할 수 없다(같은 스레드).
- 원인은 코드로 보인다: `MainWindow.Window_Loaded` 가 UI 스레드에서 `RefreshNativeHealth()` 를 동기로 부른다 — `xpe_init`, 메모리 ABI 스모크, display/DICOM/GSVG/preprocess 준비 점검(각각 모듈 로드 + 호출), 준비 보고 파일 쓰기, 모듈 행렬. 단계별 시간(`startup_trace_copy1.txt`, 임시 계측): `backend.CheckHealth` 78 ms, `WriteReport` 284 ms (DICOM 점검 약 120 ms, JSON 쓰기 약 30 ms), 모듈 준비 50 ms → **Window_Loaded 가 약 370 ms 동안 UI 스레드를 점유**. 창 핸들은 그보다 먼저(약 410 ms) 생기므로 FlaUI 의 `GetMainWindow` 는 가장 바쁜 순간에 2 s 예산(COM 연결 시간 제한)으로 UIA 를 호출한다.
- **CI 에서 이 점유가 2 s 를 넘었다는 것은 직접 재지 못했다.** 로컬(20코어)에서는 최대 0.5 s 이다. CPU 점유 경쟁(CPU 1개 고정 + 같은 우선순위 busy 스레드 6개)으로도 실패는 나지 않았다(기동이 1.4~1.9 s 로 느려졌을 뿐 UIA 호출은 실패하지 않았다, 0/8; 이 경쟁 실험용 코드는 효과가 없어 남기지 않았다). 그래서 기전을 **주입으로 재현**했다:
  - 시작 점검에 `XPE_STARTUP_PROBE_DELAY_MS`(앱의 시험용 훅) 만큼 지연을 넣고, FlaUI 의 `GetMainWindow` 만 쓰는 옛 방식으로 기동: **수정 전**(동기 Window_Loaded): 지연 0 → 2/2 성공(UI 응답 없음 152~164 ms), 지연 3000·6000 → **4/4 `TimeoutException: UIA Timeout`, 약 2.4~2.8 s 에서 실패** — CI 로그와 같은 예외·같은 지점 (`flaui_getmainwindow_before_fix_sync_loaded.txt`). **수정 후**: 지연 0·3000·6000 모두 6/6 성공, UI 스레드 무응답 최장 ≤ 68 ms (`flaui_getmainwindow_after_fix.txt`).
  - Legacy 묶음 전체(20건)에 지연 3000 을 주입: **수정 전 10/20 실패**(R01~R08, L01, L02 가 모두 UIA Timeout), 수정 후 20/20 (`legacy_class_runs.txt`).
- 따라서 "간헐"이 아니다: 점유가 FlaUI 예산(2 s)에 가까운 느린 러너에서 확률적으로 넘고, 이 묶음은 매번 방금 복사한 DLL 폴더로 앱을 띄워(디스크 상태·백신 검사에 민감) 더 길어지는 쪽이다. 어떤 시험이 걸리는지는 시험마다 달랐다(R05·R06·R08·R05…) — 서로 다른 시험이 같은 첫 UIA 호출에서 걸렸다.

### 3. 묶음 순서 의존
이 묶음의 DLL 교체가 다음 시험의 기동을 늦추는지 확인하려고 했으나, 모든 시험이 자기 DLL 사본(`xpe_c226*_native_<guid>`)을 만들어 그 폴더로만 앱을 띄운다 — 공유 DLL 을 바꾸지 않는다. 순서를 바꿔 볼 필요가 없었고(코드로 확인), 로컬 4회 전체 실행에서 실패 위치는 시험 순서와 무관했다.

## 수정 (제품)
1. **`MainWindow.Window_Loaded`**: 시작 점검(`CheckHealth` + `NativeReadinessProbe.WriteReport`)을 스레드 풀에서 돌리고(`ProbeNative`), 결과는 UI 스레드에서 적용(`ApplyNativeProbe`). 점검 중에는 상태 줄이 "Checking native common backend..." 이고 오라클 줄은 "checking in the background" 이다. 사용자가 누르는 새로고침·오라클 판정 도착은 기존처럼 동기. 판정 도착 등으로 UI 스레드의 새로고침이 먼저 일어났다면 늦게 온 시작 점검 결과로 덮어쓰지 않는다(세대 번호).
2. **보고 파일 쓰기 직렬화** (`NativeReadinessProbe.WriteReport` 의 `lock`): 1 의 결과로 풀 스레드의 점검과 오라클 판정이 부른 UI 스레드 새로고침이 같은 `native-readiness-report.json` 에 동시에 쓸 수 있게 됐다 — 수정 직후 `R02` 가 4회 중 2회 실패(`'PreprocessSmokeText' … 'Report generation skipped'`, 예외 `IOException … being used by another process` 를 임시 로깅으로 확인, `r02_exception.txt`). 잠금을 넣었다.
3. **시험 도구**: `LegacyApp.LaunchOrSkip` 가 창을 얻은 뒤 "시작이 끝났다"(Diagnostics 탭의 모듈 요약 `Executable modules=`)를 기다린다 — 예전엔 UIA 첫 호출이 시작이 끝날 때까지 막혀 이것을 암묵적으로 보장했다. 재시도·타임아웃 상향은 넣지 않았다(기존 60 s 한도 그대로).
4. **측정 도구**(CI 에서는 건너뜀, 환경변수로만 켜짐): `LegacyStartupResponsivenessProbe` — 기동 시 UI 스레드 응답 측정 / 시작 점검 지연 주입 시 `GetMainWindow` 성공률.

## 반증과 전/후 수치
| 항목 | 전 | 후 |
|---|---|---|
| 지연 3000/6000 주입, `GetMainWindow` | 4/4 실패 `UIA Timeout` | 0/4 (UI 무응답 ≤ 68 ms) |
| 지연 3000 주입, Legacy 묶음 20건 | 10 실패 | 20/20 통과 |
| 지연 0, Legacy 묶음 20건 ×4 (파일 잠금 수정 후) | (수정 전 실행은 실패를 못 쟀음) | 80/80 통과 (`legacy_class_runs.txt`) |
| 보고 파일 잠금 | 잠금 제거: R02 6회 중 5회 실패 | 잠금 있음: 0/6 (`falsify_report_file_lock.txt`) |
| UI 스레드 무응답(기동, 로컬) | 160 ms(원본) / 390 ms(복사본) | 46~113 ms |

## 한계 / 미검증
- **CI 의 실제 점유 시간은 모른다.** 주입으로 같은 실패를 재현했을 뿐이며, 이 수정이 CI 의 세 실패를 없앤다는 증거는 main CI 의 다음 실행이다. 반복 실패가 남으면 원인이 이 점유 하나가 아니다.
- 수정 중 한 번 측정 도구의 백그라운드 스레드가 예외로 시험 호스트를 죽여 앱이 남았다(측정 도구 결함, 고침).
- 레거시 앱의 다른 동기 작업(예: 창 표시 전 `InitializeComponent`)은 건드리지 않았다.
- 전체 Native E2E 는 돌리지 않았다(카드 지시).

## 검증
- Legacy 시험 묶음(`Scenarios.Legacy`, 20건) 5회 연속 20/20 (지연 없음 4회 + 지연 3000 1회), 통합 962 통과 · 0 실패 · 2 건너뜀.
- 로컬 DLL 은 CI 산출물 한 벌.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| R05·R06·R08 UIA Timeout | GUI-C-235 |
