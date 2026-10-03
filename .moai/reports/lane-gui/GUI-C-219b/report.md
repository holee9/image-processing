# GUI-C-219b — Codex #109 보류 3건 (#249)

기준: `dev/gui`, GUI-C-219(`4dfd820e`) 위. 원문: `.moai/state/codex-archive/109.md`(메인 저장소). 219 와 묶어 재검토 대상.

## 발견 1 (높음) — 판정 캐시 키가 경로 + 수정 시각뿐

### 수정
`PreprocessOracleVerdicts` 의 정체성을 **내용의 SHA-256**(+ 경로)으로 바꿨다. 질문할 때마다(`TryGet`·`Wait`) 파일을 읽어 해시를 구한다. 크기와 시각은 **아예 보지 않는다**: 카드는 "해시를 다시 계산할지 고르는 데만" 쓰라고 했지만, 시각·크기로 해시 재계산을 건너뛰면 그 건너뜀 자체가 이 저장소에서 여섯 번 반복된 빠른 경로 결함(시각을 보존한 교체를 놓침)이 된다. 해시가 곧 느린 경로이고 비용이 작아서(아래) 빠른 경로를 남길 이유가 없었다.
- 비용 실측(`xpe_preprocess.dll`, 855,040 바이트, 200회, 워밍 후): 중앙값 **0.68 ms**, p95 0.76 ms, 최대 1.26 ms. 한 번의 새로고침이 질문을 3~6번 하므로 합쳐서 수 ms. (한계: 파일 캐시가 데워진 상태의 측정이다. 첫 읽기는 디스크 속도를 탄다.)
- **실행 중 파일이 바뀌면**: 실행 뒤 해시를 다시 구해 실행 앞과 다르면 그 결과는 어느 내용의 판정도 아니라서 저장하지 않고, 지금 디스크에 있는 내용을 즉시 새로 확인한다(`Wait` 는 최대 3번까지, 그 뒤엔 "unstable" 실패 판정). 되돌려 놓은 내용이 이미 판정을 가진 것이면 새 실행 없이 완료 통지만 올려 창이 "확인 중"에 갇히지 않게 한다.

### 시험 (통합 `PreprocessOracleVerdictsTests`)
- `ATimestampChangeAlone_IsNotANewSubject`: 시각만 바꾸면 새 대상이 아니다(실행 1회).
- `ADifferentFileWithTheSameSizeAndTimestamp_IsANewSubject`: **같은 크기·같은 시각**의 다른 내용 → 재판정(실행 2회, `run2`).
- `AFileThatChangesWhileTheOracleRuns_IsNotJudgedByTheRunThatStartedBeforeTheChange`: 실행 중 교체 → 옛 실행의 결과가 새 내용의 판정으로 저장되지 않고, 새 내용이 확인되며(`run2`), 통지는 한 번.
- `AFilePutBackDuringARun_StillAnnouncesTheKnownAnswer`: 실행 중 이미 판정이 있는 내용으로 되돌려 놓으면 옛 판정이 통지된다.

### 반증 (소스를 바꿔 빌드·실행 후 복원, 파일 동일성 `cmp` 확인)
| 팔 | 방법 | 빨강이 된 시험 |
|---|---|---|
| 1 | 키를 옛 방식(경로+쓰기 시각)으로 되돌림 | `ATimestampChangeAlone…`, `ADifferentFile…SameSizeAndTimestamp…`, `AFileThatChanges…` |
| 2 | 실행 뒤 해시 비교를 끔(`if (false && …)`) | `AFileThatChanges…` |
| 4 | 이미 판정 있는 내용으로 돌아온 경우의 통지를 끔 | `AFilePutBackDuringARun…`(10 s 대기 끝에 빨강) |

## 발견 2 (보통) — 'Refresh Modules' 뒤 세 화면이 같은 '확인 중'이 아님

`RefreshModulesButton_Click` 이 `RefreshNativeHealth(recheckOracle: true)`(전체 갱신 경로)를 부른다. 이전에는 판정만 무효화하고 모듈 행렬만 갱신해 `lastPreprocessHealth` 가 옛 통과로 남았다.
E2E `R04`(UIA 패턴만): 통과가 보이는 상태 → 워커를 게이트로 붙잡고 `Refresh Modules` → 세 탭이 모두 "확인 중"(진단 smoke 줄 "checking in the background", 행렬 "Synthetic oracle checking", 보정 탭 `NATIVE-NOT-READY` 0·`NATIVE-CHECKING` 3, 평가 탭 비준비·Offset 스위치 비활성) → 게이트를 열면 세 탭이 모두 "준비"(워커 로그 2줄).
**반증**: 버튼을 옛 동작(`Invalidate` + `RefreshModuleReadiness`)으로 되돌려 빌드·실행 → R04 빨강, 메시지가 Codex 의 관찰 그대로: `Diagnostics smoke line after Refresh Modules: 'Preprocess smoke: Synthetic oracle pass; pass=True; latency=4.265ms' (a stale pass shows here)`. 복원 후 초록.

## 발견 3 (낮음) — 동기 호출 재도입을 시험이 막지 못함

- 소스 스캔 단언을 **호출 경계 검사**로 바꿨다(`TheWindowAndItsViewModels_NeverCallTheOracleOrWaitForIt`): `MainWindow.xaml.cs` 와 `ViewModels/*.cs` 에서 `XpePreprocessOracleProcess.Run(`·`XpePreprocessSyntheticOracle.Run(`·`PreprocessOracleVerdicts.Wait(` 호출이 없고, 준비도 사슬로 들어가는 모든 호출(`WriteReport`·`Check`·`Evaluate`·`moduleReadinessViewModel.Refresh`)이 `waitForOracle: false` 를 말해야 한다(뷰모델은 호출자의 값을 넘겨받음을 확인). 인자 읽기는 괄호 균형으로 한다. 대조군: 사슬로 들어가는 호출을 3개 이상 봐야 한다.
  반증: 창에 `XpePreprocessOracleProcess.Run("x")` 한 줄 추가 → 빨강; `WriteReport(result, waitForOracle: false)` 에서 인자 제거 → 빨강.
  기존 212 의 소스 스캔(`NoAppCode_RunsTheOracleInTheAppsProcess_ExceptTheWorkerEntry`)은 "프로브가 보관소를 부르고 보관소의 실행기가 자식 프로세스 호스트" 확인으로 유지했다.
- `R03` 이 `NATIVE-CHECKING` Hard finding 이 **있음**도 단언한다(`checking.CheckingFindings > 0`; 실행 값 3).
- **시작 완료 시간(창이 보이기 전 구간 포함)**: 측정 스크립트를 새로 썼다 — 프로세스 시작을 0 으로 하여 UI 스레드 창에 `WM_NULL` 을 3 ms 간격으로 보내, 창 핸들 생성·창 가시·"연속 10회 응답"(= ready) 시각과 무응답 합계를 잰다. **먼저 내 스크립트의 결함을 고쳤다**: 첫 판은 다른 스레드의 보조 창을 찔러 모든 팔이 같게 나왔다(B 도 정상으로). 창 목록을 열어 UI 스레드의 `HwndWrapper[ImageProcTest…]` 로 고쳤고, 고친 뒤 B 가 멈춤을 보인다. 원표: `start_launch_relative_abcs.txt`(10 라운드 A→B→S→C 교대).

| 팔 | 창 가시(중앙값) | ready(중앙값) | ready(최대) | 무응답 합계(중앙값) | (최대) |
|---|---|---|---|---|---|
| A (214) | 484 ms | 744 ms | 771 | 455 ms | 486 |
| B (218 HEAD, 동기 3회) | 474 ms | 2046 ms | 3856 | 1644 ms | 3226 |
| S (부분 복귀: 창의 `waitForOracle: false` 두 곳만 `true`) | 468 ms | 584 ms | 600 | 356 ms | 385 |
| C (수정) | 484 ms | 705 ms | 748 | 410 ms | 484 |

**부분 복귀 팔은 이번에도 빨개지지 않는다.** S 의 ready 584 ms 는 C 의 705 ms 보다 **빠르고** 무응답 합계도 작다(356 vs 410). 이 지표로는 "창의 두 호출을 동기로 되돌려도 시작이 나빠지지 않는다". 숫자로 본 이유와 한계:
- 진짜 동기 회귀(B)가 나쁜 것은 오라클을 **세 번**(약 250 ms × 3) 직렬로 기다리고 그 뒤 처리까지 겹치기 때문이다(ready 2046 ms). S 는 보관소가 한 번만 돌리므로 대기가 한 번(≈ 자식 수명 243~267 ms)뿐이다. 즉 이 지표에서 시작을 지키는 것은 "UI 스레드 밖"이 아니라 **"세 번이 한 번"** 이다.
- C 가 S 보다 약 120 ms 늦은 이유는 측정하지 않았다. 가설(미검증): 오라클이 끝나면 완료 통지가 전체 갱신(`RefreshNativeHealth`: 공통 백엔드 상태·보고서·준비도·미리보기 컨트롤)을 UI 스레드에서 한 번 더 돌린다. 이 비용을 줄이는 것은 이번 카드의 범위가 아니다.
- 그러므로 "오라클이 UI 스레드에서 돌지 않는다"는 주장의 근거는 시작 지표가 아니라 **스레드 단언 시험**(`TheRun_HappensOnAnotherThread…`, 인라인 실행으로 바꾸면 멈춤/빨강)과 호출 경계 검사이다. 시작 지표가 막아 주지 못하는 것: 한 번의 대기(≈250 ms)가 되돌아오는 회귀. 대신 호출 경계 검사가 그것을 막는다.
- 219 보고서의 "부분 복귀 팔이 지표를 나쁘게 만들지 않았다"와 그 추정(창이 보이기 전에 끝남)은 위 결과로 **부분적으로만 맞았다**: 새 지표는 창이 보이기 전 구간을 포함하는데도 S 가 나빠지지 않았으므로 "측정이 그 구간을 못 봤다"는 설명은 맞지 않다(이번 지표에서는 보인다). 나빠지지 않은 것은 대기가 한 번뿐이라서다.

## 시험 결과
- 통합 시험 전체(LF 워크트리): 832 통과, 0 실패, 0 건너뜀.
- 레거시 E2E(12건: R01~R04 외) 12회 연속: 12/12 회차가 `passed=12 failed=0 skipped=0`, timeout 문자열 0줄(`consecutive_12_runs.txt`).

## 한계 / 미검증
- 해시는 `xpe_preprocess.dll` 하나의 내용만 정체성에 넣는다. 오라클 자식이 읽는 다른 DLL(공통 모듈 등)이 바뀌어도 판정은 그대로 유지된다(이전과 같은 범위).
- 해시 비용은 워밍된 파일 캐시 기준이다.
- CI 에서의 결과는 아직 없다. CRLF 체크아웃에서 이 변경을 돌리지 않았다.
