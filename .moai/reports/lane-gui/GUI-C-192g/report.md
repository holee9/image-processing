# GUI-C-192g — 완료 콜백 경로에서도 표시된 옛 세대 상태를 철회 (Codex #68)

## 0. 정정 (내 오류)

- **192f 보고서의 "타이머 틱·`Request`·완료마다 비교한다"는 사실과 달랐다.** `WithdrawIfTheSessionMoved()` 는 `Request` 와 `CheckFreshness` 에만 있었고 완료 콜백(`Complete`)에는 없었다. 서술한 대로 구현했다고 코드가 아니라 기억으로 적은 것이다. 이번에 서술이 참이 되도록 고쳤고, 서술(경로 목록)을 **소스를 읽는 시험으로 고정**했다(§2).
- **증거 파일 5개가 git 에 들어가지 않았다.** `git add <폴더>` 는 `.gitignore` 에 걸린 파일을 조용히 건너뛴다(`*_build.ps1`, `.moai/reports/**` 의 `.md`·`.txt` 밖 확장자). 192e·192f·193 보고서가 인용한 `draft_check_shipped_build.ps1`(2곳), `draft_ci_job.diff`, `gui_shipped_build.patch`, `ci_step_check.ps1` 이 저장소에 없었다(`untracked_evidence.txt`). 리더가 이름을 바꿔 넣은 `tools/ci/Test-ShippedBuild.ps1` 이 이제 정본이다. 기록을 보존하려고 네 파일을 추적되는 `.txt` 확장자로 이 폴더에 복사했다(`*.ps1.txt`, `*.diff.txt`, `*.patch.txt`). 새 파일은 `git check-ignore -v` 로 확인했다(위 파일들은 `!` 규칙에 걸려 무시되지 않음, 규칙 번호 포함). **앞으로 커밋 전에 `git status --short --ignored` 로 증거 폴더에 무시된 파일이 없는지 본다.**

## 1. 결함과 고침

**결함.** `Complete` 는 진행 중이던 읽기의 대수가 다르면 그 답을 버리고 다시 읽지만, **이미 화면에 있는** 옛 세대의 `Disabled`/`Active` 는 건드리지 않았다. 새 읽기가 실패하거나 멈추면 다음 타이머 틱(약 1초)까지 옛 답이 남았다.

**고침(한 줄).** `Complete` 가 `_stopped` 를 확인한 직후, 어느 분기로 가기 전에 `WithdrawIfTheSessionMoved()` 를 부른다. 완료 콜백은 어떤 분기에서든(대수·세대·백엔드 불일치, 같은 세대의 추월, 정상 적용) 먼저 "화면의 답이 지금 세션의 것인가"를 확인한다. 정상 적용 분기에서 새 세션의 답이 오면 `Unknown` 다음에 새 답이 이어서 적용된다(점멸이 아니라 정당한 두 번의 변화).

## 2. 시험 (결정적, `Request`·타이머를 부르지 않음)

`TheCompletionOfAnOldRead_WithdrawsTheShownAnswerOfTheReplacedSession` 4케이스 {Disabled, Active} × {새 읽기 실패, 새 읽기 멈춤}: 표시된 답 → 둘째 읽기가 이벤트에 붙들려 시작 → **대수만 올림**(Reset·Request·틱 없음) → 옛 읽기 완료. 철회가 **완료 콜백 안에서** 일어나(`Applied[1] == Unknown`) 새 읽기가 실패·멈춰도 옛 답이 돌아오지 않고, 적용은 정확히 2건이다.

**전이 경로 표**(`EveryTransitionPath_ComparesTheSessionOrRewritesTheScreen`, 소스를 읽는 시험이라 이 트리의 글자만 본다 — 경로가 비교를 잃으면 빨강):

| 경로 | 세대/대수 비교·철회 | 근거 |
|---|---|---|
| `Request` | 있음 | 읽기 전에 `WithdrawIfTheSessionMoved()` |
| `CheckFreshness`(타이머) | 있음 | 맨 앞에서 `WithdrawIfTheSessionMoved()`, 철회하면 `Request()` |
| `Complete`(완료 콜백) | **있음(이번에 추가)** | `_stopped` 직후, 분기 전에 `WithdrawIfTheSessionMoved()` |
| `Reset` | 화면을 직접 다시 씀 | `Show(Unknown)`, 현재 대수로 도장 |
| `Show`(모든 적용) | 대수 도장 | `_shownEpoch = shownUnderEpoch ?? _epoch()`, 답은 읽기 당시 대수로 |
| `Stop` | 화면에 닿지 않음 | `_stopped` 이후 모든 경로가 먼저 반환 |

빠진 칸 0. (`Reset` 은 비교가 아니라 직접 덮어쓰기이고 `Stop` 은 화면을 만지지 않는다고 표에 이유를 적었다.)

## 3. 반증 (`falsification_arms.txt`, 원본 바이트 동일 복원, 복원 뒤 Functional 412/0)

| 지운 것 | 빨강 |
|---|---|
| 완료 콜백의 철회(192f 상태) | 새 이론 시험 4케이스 + 전이 경로 표 시험 |
| 표시된 답의 대수 도장 | 192f 이론 시험 4케이스 + 전이 경로 표 시험 |

## 4. 실행 결과

Functional 412 통과 / 0 실패 / 1 건너뜀, SelfCheck 종료 0, main 병합(`70a1e1be`, dev/gui 에 main 을 받음) 뒤 첫 실행. E2E 는 이번 변경이 refresher 한 줄이라 다시 돌리지 않았다.

## 5. 남은 공백 (Gaps)

- 이전 보고서의 공백이 그대로다: Native 에서 15초·5초·지연 로딩, UI 스레드의 긴 동기 작업, 게이트 밖 자체 답의 동작 시험 부재(#98).
- 전이 경로 표는 소스를 읽는 시험이다: 비교를 부르는 문장이 있는지를 볼 뿐, 호출이 올바른 조건에서 일어나는지는 위 이론 시험(동작)이 본다.
- `tools/ci/Test-ShippedBuild.ps1`·`ci.yml` 의 리더 판본은 이 저장소에서 읽지 못했다(main 이 아직 푸시 전이라 내 받은 main 에 없음). 정본과 내 초안의 차이는 보지 못했다.

## 6. 잔여 위험

- 정상 적용 분기에서 대수가 움직인 직후 새 답이 오면 `Unknown` → 새 답 두 번의 변화가 일어난다(속성 변경 알림 2회). 사용자 화면에서는 한 프레임 안이다.
