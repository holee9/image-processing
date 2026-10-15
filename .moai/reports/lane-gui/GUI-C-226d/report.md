# GUI-C-226d — Codex #123 보류 2건 (#249)

커밋 둘: 앱 `GUI-C-226d`(a)(`MainWindow.xaml.cs`, `PreprocessOracleVerdicts.cs` 의 시험용 상태 설명 함수), 시험·증거 `GUI-C-226d`(b).

## 1. 안내 문구를 현재 검증 상태에서 파생

**원인**: 226b 는 안내를 문자열(거절 문구 / "준비됐다")로 들고 있다가 `preprocessReady` 가 참이 되는 순간 한 번 바꿨다. 거짓 쪽에는 갱신이 없어서, "준비됐다" 뒤에 DLL 이 다시 바뀌어 Refresh Modules 로 차단되어도 화면에는 "ready now; press the command again" 이 남았다.

**변경**: 별도로 들고 있는 문자열을 없앴다. 앱이 들고 있는 것은 "마지막 명령이 거절되었다"(`processingRefused`, 불리언) 하나이고, 문구는 그릴 때마다 현재 상태에서 만든다(`ProcessingNoticeText()`):
- 검사 중(상태 없음 또는 `IsSyntheticOracleChecking`): "…다시 자동으로 검사한다. 준비되면 명령을 다시 눌러라"
- 검사 통과(`IsNativePreviewReady()`): "…다시 검사했고 지금 준비됐다. 명령을 다시 눌러라"
- 검사가 **끝났고 통과하지 못함**: "…다시 검사했고 그 검사는 통과하지 못한 채 FINISHED 됐다(`<상태>`). 전처리 DLL 이 통과할 때까지 명령은 막혀 있다"
- 다음 명령이 확인을 통과하면 `processingRefused=false`, 즉시 다시 그림(226b 와 같음)

준비 상태를 새로 평가하는 모든 갱신(`UpdateNativePreviewControls`)의 끝에서 다시 그리므로, 상태가 어느 방향으로 바뀌어도 다음 갱신에서 문구가 따라간다(메시지가 그대로여도).

**시험(E2E, UIA 패턴만)** — 워커를 게이트 파일(있으면 즉시 실행, 없으면 대기)로 붙잡아 각 상태를 읽는 동안 유지한다:
- `R07`: ① 거절 + 재검사 중 → "자동으로 검사한다"(준비됐다 없음) ② 통과 → "준비됐다" ③ DLL 을 **다시** 재배포하고 Refresh Modules, 검사를 붙잡음 → "준비됐다"가 **남지 않고** 다시 "자동으로 검사한다" ④ 풀어 줌 → "준비됐다" ⑤ DLL 을 깨뜨리고 Refresh Modules(Codex 의 재현) → "FINISHED without passing", "준비됐다"·"자동으로 검사한다" 없음
- `R08`: 첫 재검사 자체가 실패하는 경우(재배포가 DLL 이 아닌 파일을 남김) → 거절 문구에 "준비됐다"가 없고 "FINISHED without passing" 이 나옴. 깨진 DLL 은 내보내기 확인에서 먼저 걸려 "검사 중" 구간 없이 바로 끝난 상태로 읽힌다(그래서 R07 의 "검사 중 → 실패" 구간은 정상 DLL 의 재배포로 만들었다)
- `R05`·`R06` 는 그대로 통과.

**반증**(`falsification_1_notice_not_derived.txt`): 파생을 끊어 검사가 돌고 있지 않으면 항상 "준비됐다"가 나오게 하면 R07(⑤ 단계, 시간 초과)과 R08 이 빨개진다.

## 2. 226c 의 대기와 훅의 어긋남

**고친 것** (`Diagnostics/VerdictTestWaits.cs`, 새 시험 도우미):
- `Gap`: 작업 스레드를 붙잡는 훅은 이제 `HookLimit`(60초)까지만 기다리고, 그래도 풀리지 않으면 `TimedOut` 으로 **기록**한다. 시험은 끝에서 `AssertHeldUntilReleased` 로 "훅이 스스로 풀렸다"를 **실패로** 만든다. 훅의 한도(60초)는 모든 바깥 대기(`OuterWait` 30초)보다 길어서, 바깥 대기가 훅보다 먼저 끝난다. 이전에는 훅 15초 / 바깥 30초라 훅이 스스로 풀린 뒤에 바깥 대기가 성공할 수 있었다.
- `Expect`/`PollAsync`: 대기가 실패하면 **어느 단계**인지, 기다린 시간, 완료 이벤트·실행 횟수, 판정 보유자의 상태(`DescribeState`: `JobQueued`·`SnapshotTaken`·`Rerun`·저장된 결과·정체성·세대)를 메시지에 낸다.
- 적용: 틈 시험 둘(`AnAskJustBefore…`, `AnAskJustAfter…`), 동기 읽기를 붙잡는 시험(`TryGet_DoesNotReadTheFileOnTheCallersThread…`), 바뀐 내용 확인 대기 둘, 처리 직전 확인 시험들의 대기.

**반증**:
- 훅 한도를 0 으로 → 세 시험이 "gate … released itself … the test did not act in the moment it was written for" 로 빨개진다(`falsification_2_gate_releases_itself.txt`). (첫 시도로 1 ms 를 썼으나 시험이 그 안에 풀어 줘서 안 터졌다 — 한도 0 으로 다시 했다.)
- 실제 정지를 흉내 낸 결함(작업이 스스로 풀리지 않음) + 바깥 대기 5초 → 메시지: `step '…' did not happen within 5 s (waited 5.0 s; runs=1, announced=1; holder: JobQueued=False, SnapshotTaken=False, Rerun=False, stored result=run1, stored identity=xpe_preprocess.dll=2D711..., generation=1)`(`falsification_3_wait_message_on_a_stall.txt`).

**지난 간헐 실패(약 16회 중 2회)의 재현 시도**: 진단이 켜진 채 전체 20회, 매번 재빌드로 시작하는 6회(지난 실패가 빌드 직후였으므로) — **0건 재현**(`reproduction_attempt_20_full_runs.txt`). 즉 "판정 보유자가 실제로 느려졌다"와 "러너가 느렸다"를 가르는 데이터는 아직 없다. 다음에 시간 초과가 나면 위 메시지가 단계와 상태를 알려 준다.

## 실행 증거

- 통합 시험 전체: 862 통과 / 0 실패 / 1 건너뜀(파일 링크 권한) — `integration_full_suite.txt`
- 레거시 E2E 전체: 18 통과 — `legacy_e2e_current_tree.txt`

## 미검증과 잔여 위험

- 원인을 모르는 채 226c 에서 늘린 대기(10→30초)는 그대로다. 이번 변경으로 "대기가 길어서 틈을 놓쳤는데 통과"는 불가능해졌지만(훅이 풀리면 실패) 간헐 실패의 원인은 여전히 모른다.
- 깨진 DLL 로는 "검사 중" 구간을 볼 수 없어 R08 은 "검사 중"을 단언하지 않는다.
- `R07` 은 앱을 한 번 띄워 약 10초 걸린다. CI 에서 도는지는 모른다.
