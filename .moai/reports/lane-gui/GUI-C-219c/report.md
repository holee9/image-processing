# GUI-C-219c — Codex #112 보류 4건 (#249)

기준: `dev/gui`, GUI-C-219b(`d7c5231d`) 위(그 사이에 222 `2ddf3408` 커밋이 있다). 원문: `.moai/state/codex-archive/112.md`(메인 저장소). 219·219b·219c 를 묶어 재검토 대상.

## 발견 1 (높음) — 해시한 바이트와 워커가 로드하는 바이트가 같지 않을 수 있음

### 먼저 재현했다 (219b 의 홀더, `d7c5231d` 워크트리)
같은 A→B→A 순서(실행 중 원본을 B 로 바꾸고, 워커가 읽은 뒤 A 로 되돌림)를 219b API 로 돌렸다. 시험은 "워커가 본 바이트 = A" 를 단언한다. 결과: **실패** — `Expected: [120]  Actual: [66]` (A = `x`(120), 워커가 본 것 = `B`(66)). 즉 Codex 의 관찰 그대로, 워커는 B 를 검사했고 전후 해시는 둘 다 A 였다.

### 수정: 스냅샷 (`PreprocessOracleSnapshot`)
검사마다 (1) 워커가 로드할 DLL 들을 **전용 임시 폴더로 복사**하고, (2) **복사본을 해시**하고, (3) **복사본의 경로를 워커에 넘긴다**(앱 폴더의 원본은 워커가 보지 않는다), (4) 판정을 저장한 뒤 폴더를 지운다. 해시한 바이트와 판정한 바이트가 구조상 같다.
- **무엇을 복사하는가 (실제 로드 목록)**: ① `xpe_preprocess.dll`, ② 앱의 `NativeDependencyLoader` 가 이 모듈에 대해 이름으로 로드하는 DLL(`xpe_preprocess.dll` 은 목록에 없어 기준선 `fmt.dll`·`spdlog.dll`), ③ 그 DLL 들의 **PE 임포트 표를 읽어 재귀적으로** 따라간 DLL 중 같은 탐색 폴더(DLL 폴더, `vcpkg_installed/x64-windows/bin`)에서 찾은 것. 이 저장소의 DLL 로 실측: `xpe_preprocess.dll` → `xpe_common.dll` → `spdlog.dll`(`xpe_common` 의 임포트). 시험 `TheImportTable_NamesTheModulesDependencies…` 가 `xpe_common.dll` 이 임포트로 잡히고 폐쇄에 들어가는지 확인한다(스테이징된 DLL 이 있을 때).
- **정체성(키)** = 복사된 파일 이름 + 각 복사본의 SHA-256 (`fmt.dll=…;xpe_common.dll=…;xpe_preprocess.dll=…`). 의존 DLL 이 바뀌면 다른 대상이다(Codex: 키가 `xpe_preprocess.dll` 하나뿐).
- **"전후 해시 + 재시도"는 필요 없어졌고 지웠다**: `MaxRunsPerAsk`·"unstable" 실패·`Superseded`·`Wait` 의 재시도 루프를 삭제했다. 실행 중에는 아무것도 바뀔 수 없다(워커는 복사본만 본다). → **발견 3 도 함께 사라졌다**(UI 경로에 예산이 없던 문제는 예산 자체가 없어졌다).
- 복사본 폴더 이름은 `xpe-oracle-snap-<pid>-<guid>`. 비정상 종료가 남긴 것은 다음 확인 때 한 번 정리한다(실행 중인 프로세스의 것은 건드리지 않음). 로컬 실측: 12회 연속 E2E 와 시작 측정 뒤 `%TEMP%` 에 남은 스냅샷 폴더 **0개**.

### 시험 (통합 `PreprocessOracleVerdictsTests`, 21건)
- `TheWorkerIsHandedASnapshot_NotTheOriginal…`: 워커가 받은 경로 ≠ 원본, 이름 같음, 바이트 같음, 판정 뒤 폴더 삭제.
- `AFileChangedToBAndBackToADuringTheRun…`(동기화 게이트로 결정적): 원본이 B 인 순간 워커가 읽은 복사본 = A, 저장된 정체성 = `xpe_preprocess.dll=<A 의 해시>`, 이후 검증이 같은 바이트라 재실행 없음.
- `AFileReplacedDuringTheRun…`: 영구 교체 → 실행은 받은 바이트(A)의 판정으로 저장되고, 다음 검증이 다름을 발견해 `Changed`, 새 내용 재판정(run2).
- `ADependencyDll_IsPartOfTheIdentity…`: `fmt.dll` 이 같은 크기·같은 시각으로 바뀌면 새 대상이고 복사본 옆에 들어 있다.
- `AnAskThatComesAfterTheRunningJobsSnapshot_GetsAPassOfItsOwn`: 아래 "시험이 발견한 결함 1".
- `ASnapshotThatCannotBeMade_IsAFailedVerdict_AnnouncedOnce_NotARefreshStorm`: 아래 "시험이 발견한 결함 2".

## 발견 2 (보통) — SHA-256 파일 읽기가 UI 스레드에서 동기

복사·해시가 전부 작업 스레드로 갔다. `TryGet` 은 잠금을 잠깐 잡고 **저장된 판정을 즉시** 돌려주며(없으면 null = "확인 중"), 그 판정이 지금 디스크의 DLL 과 같은 것인지의 검증(스냅샷 한 번)은 백그라운드에서 한다. 다르면 저장된 판정을 버리고 `Changed` 를 올려 창이 "확인 중"을 보이고, 새 내용을 판정한다. **정직한 비용**: 교체와 검증 사이(복사+해시 한 번 ≈ 수 ms)에는 옛 판정이 보일 수 있다. 그 뒤로는 디스크에 없는 내용의 판정이 남지 않는다(Codex 가 요구한 "UI 는 즉시 확인 중"의 엄격한 해석 — 모든 질문에 확인 중을 보이면 완료 통지→새로고침→질문의 되돌이가 생겨 정착하지 않으므로 이 형태를 택했다).
- 시험 `TryGet_DoesNotReadTheFileOnTheCallersThread_EvenWhenTheReadIsSlow`: 워커를 복사 직후에 붙잡아(느린 디스크) 둔 채 호출자(UI 스레드 대역)가 두 번 물어도 300 ms 안에 답을 받는다.
- 해시 비용(219b 실측): `xpe_preprocess.dll` 855,040 바이트에 중앙값 0.68 ms. 복사 포함 비용은 따로 재지 않았다(작업 스레드이므로 UI 응답에 영향이 없다 — 시작 측정이 간접 증거).

## 발견 4 (낮음) — 동기 호출 방어의 경계가 파일 목록으로 고정 → UI 스레드 단언

`OracleThreadGuard`(창이 `Dispatcher.CheckAccess()` 로 설치)가 `PreprocessOracleVerdicts.Wait` 와 호스트의 `XpePreprocessOracleProcess.Run`(자식 프로세스를 띄우는 모든 경로) 맨 앞에서 `AssertNotUiThread` 를 부른다. UI(디스패처) 스레드에서 부르면 `InvalidOperationException` 이 나고, 헤드리스 모드·시험·자식 프로세스에서는 가드가 없어 아무 일도 없다. 어느 파일에서 어떤 경로로 추가되든 처음 실행될 때 잡힌다.
- 시험 `TheBlockingEntryPoints_ThrowWhenCalledOnTheUiThread_AndTryGetDoesNot`: 가드가 "이 스레드가 UI" 라고 답할 때 `Wait`·호스트 `Run` 이 던지고 `TryGet` 은 안 던진다. 다른 스레드(전용 스레드 — 풀 태스크는 기다리는 스레드가 인라인으로 실행할 수 있어 쓰지 않았다; 처음 시험이 이것으로 틀렸다)에서는 같은 호출이 허용된다.
- 호출 경계 소스 스캔(219b)은 두 번째 방어선으로 남겼다(근거는 단언).
- 한계: 단언은 **실행 시점**에 잡는다. 앱 전체가 아니라 그 호출이 일어나는 경로를 지나야 잡힌다(그래서 스캔을 남겼다).

## 반증

| 팔 | 방법 | 결과 |
|---|---|---|
| 1 | 워커에 원본 경로를 넘김(219b 동작) | `TheWorkerIsHandedASnapshot…`, `AFileChangedToBAndBackToA…` 빨강 |
| 2 | 정체성을 `xpe_preprocess.dll` 하나로 제한 | `ADependencyDll_IsPartOfTheIdentity…` 빨강 |
| 3 | `TryGet` 안에서 복사·해시(호출자 스레드) | `TryGet_DoesNotReadTheFile…` 빨강(30 s: 붙잡힌 읽기를 기다림) |
| 4 | `Wait` 의 UI 스레드 단언 제거 | `TheBlockingEntryPoints_Throw…` 빨강 |
| 5 | 스냅샷 뒤에 온 질문에 자기 몫의 검증을 주지 않음 | `AnAskThatComesAfterTheRunningJobsSnapshot…` 빨강(10 s) |
| 6 | 반복되는 설정 실패의 "한 번만 알림" 제거 | `ASnapshotThatCannotBeMade…` 빨강(`Expected: 1  Actual: 6`) |

모든 팔은 소스를 바꿔 빌드·실행한 뒤 복원했고 `cmp` 로 원본과 같음을 확인했다.

## 시험이 발견한 결함 2건 (수정함)

1. **스냅샷 뒤에 온 질문이 삼켜짐** — 첫 통합 시험 묶음이 6번 중 2번 "the new content was never checked" 로 간헐 실패했다. 원인: 실행 중인 작업이 끝나가는 순간(이미 복사를 마친 뒤)에 온 질문이 "작업이 이미 있다"로 합쳐져 자기 몫의 검증을 못 받았다. 그 질문 이후에 바뀐 파일은 다음 질문까지 감지되지 않는다. 수정: 작업이 복사를 마친 뒤 온 질문은 `Rerun` 을 세워 한 번 더 통과한다(같은 바이트면 조용하다). 수정 뒤 같은 시험 묶음 6/6 통과, 그리고 이것을 결정적으로 재현하는 시험(위, 팔 5)을 더했다.
2. **설정 실패의 새로고침 폭풍** — 레거시 E2E `R02`(TEMP 가 파일이라 임시 폴더를 만들 수 없음)가 82 초 뒤 실패했다. 원인: 복사본을 만들 수 없는 실패마다 판정을 저장하고 완료를 알렸고, 창이 그 알림에 새로고침하며 다시 질문하면 또 같은 실패로 알림이 올라 창이 정착하지 못했다. 수정: 설정 실패도 정체성(`setup:<예외 종류>:<메시지>`)을 가진 판정으로 한 번만 저장·알리고, 같은 실패가 반복되면 조용하다(`StoreFailureOnce`).

## 측정 (219b 의 방법과 스크립트, 10 라운드 교대)
측정 스크립트(`measure_start_launch_relative.py.txt`, 창 선택 구현 포함)와 창 목록 확인 도구(`list_app_windows_for_probe.py.txt`), 219 의 WM_NULL 스크립트(`measure_ui_stall_wm_null.py.txt`)를 이 폴더에 넣었다(`.py` 는 저장소가 무시하므로 `.txt`). 창 선택: 앱 프로세스의 최상위 창 중 클래스가 `HwndWrapper[ImageProcTest` 로 시작하는 것(UI 스레드의 창들; 다른 스레드의 보조 창은 제외). 가시 = 그 중 하나가 보임, ready = 가시 이후 `WM_NULL`(타임아웃 30 ms) 10회 연속 응답. 원표: `start_launch_relative_abc.txt`.

| 팔 | 창 가시(중앙값) | ready(중앙값) | ready(최대) | 무응답 합계(중앙값) | (최대) |
|---|---|---|---|---|---|
| A (214) | 492 ms | 760 ms | 805 | 467 ms | 526 |
| B (218 HEAD, 동기 3회) | 476 ms | 1631 ms | 3312 | 1519 ms | 3421 |
| C (219c) | 486 ms | 704 ms | 781 | 415 ms | 450 |

219b 의 C(705 ms)와 같다: 스냅샷 복사·해시가 시작 지연을 더하지 않았다(작업 스레드). (219b 때 같이 잰 "부분 복귀" 팔은 이번에 다시 재지 않았다.)

## 시험 결과
- 통합 시험 전체: 839 통과, 0 실패, 0 건너뜀.
- 레거시 E2E(12건) 12회 연속: 모든 회차 `passed=12 failed=0 skipped=0`, timeout 문자열 0줄(`consecutive_12_runs.txt`). 중간에 R02 가 위 결함 2로 한 번 실패했고 수정 뒤의 결과만 표에 있다.

## 한계 / 미검증
- 스냅샷의 의존 DLL 은 임포트 표에서 찾은 것 중 **같은 탐색 폴더에서 발견된 것**이다. 시스템 DLL(`kernel32` 등)과 탐색 폴더에 없는 DLL 은 포함되지 않는다(OS 가 로드하는 것을 앱이 복사할 수 없다). 지연 로드되는 DLL(임포트 표에 없는 `LoadLibrary` 호출)은 보지 못한다.
- 워커가 실제로 로드하는 목록을 프로세스 안에서 확인(예: 모듈 열거)하지는 않았다. 목록은 로더의 이름 목록 + PE 임포트 폐쇄로 얻었고, `xpe_preprocess.dll` → `xpe_common.dll` → `spdlog.dll` 은 임포트 표를 읽어 확인했다.
- `TryGet` 은 교체 직후 한 번의 검증 동안 옛 판정을 돌려줄 수 있다(위 발견 2). 그것이 허용되지 않으면 "모든 질문이 먼저 확인 중" 모델이 필요하고 그것은 완료 통지 되돌이를 끊는 별도 설계(세대 토큰)를 요구한다.
- CI 에서의 결과는 아직 없다. CRLF 체크아웃에서 이 변경을 돌리지 않았다.
