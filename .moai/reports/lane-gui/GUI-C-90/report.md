# GUI-C-90 보고 — 창 재획득 결함의 CI 빈도 (#170, 판독)

코드 변경 없음. 수집·계수 스크립트는 이 디렉터리에 `*.py.txt` 로 두었다.

## 1. 수집

- **대상**: `gh run list --branch main --workflow "XPE CI Pipeline" --limit 100` 으로 고른 main 실행 100건이다. 기간은 2026-09-12T02:31Z ~ 2026-09-17T05:11Z (`runs.json`).
- **잡 로그부터 봤다** (`gh run view --job <id> --log`, `collect.txt`, `count.txt`):
  - 성공한 실행의 로그에는 테스트 요약 한 줄만 있다(`Passed!  - Failed: 0, Passed: 85, …`). 시험별 줄도, 콘솔 출력(`XPE-C56-PROBE`)도 없다.
  - 그래서 100건 전부에서 탐침 줄이 0 이었고, 같은 로그에서 `UnreadableWindow`·`re-located` 도 0 이었다. **대조군도 0 이므로 이 0 은 "안 보인다" 는 뜻이지 결함이 없다는 뜻이 아니다.**
  - 로그 37건은 `log not found` 였다.
- **실제 계수는 trx 산출물로 했다**: 잡이 올리는 `xpe-gui-e2e-native-results` 를 `gh run download` 로 받았다(`trx-collect.txt`).
  - 로컬 trx 로 먼저 확인했다: 탐침 줄이 trx 안에 기록된다(C-89 전체 Native trx 기준 탐침 13, Win32 3).

| 실행 수 | 잡 결과 | trx |
|---|---|---|
| 42 | success | 받음 |
| 4 | failure | 받음 |
| 39 | cancelled | 산출물 없음 (`no artifact matches`) |
| 8 | cancelled | 산출물 없음 (`no valid artifacts found`) |
| 6 | skipped | 산출물 없음 |
| 1 | `gui-e2e-native` 잡 없음 | — |

- **받지 못한 53건의 원인**: 모두 **취소되었거나 건너뛴 잡**이다. 로그 보존 기간 때문에 못 받은 실행은 없다. 산출물 보존 기간은 14일이고, 대상 기간은 그보다 짧다.
- **trx 46건 가운데 탐침 줄이 없는 8건**: 모두 2026-09-16T02:00Z 이전 실행이다. 탐침을 넣은 `b6b9703` 이 main 에 들어가기 전이므로 **로그 형식이 다른 실행**으로 따로 표시했다. 탐침이 처음 나온 실행은 2026-09-16T04:54Z 다.

## 2. 계수 (탐침이 있는 38 실행, `trx-count.txt` · `trx-table.json`)

- **기동 수**: `XPE-C56-PROBE hwnd=…` 줄을 **프로세스 id 로 중복 제거**해 셌다.
- **결함 수**: 그중 `framework=Win32` 인 줄을 셌다. `re-located` 문구로는 세지 않았다.

| 항목 | 값 |
|---|---|
| 실행 | 38 |
| 기동 | **181** — 실행당 4 (28회), 5 (1), 6 (3), 7 (5), 11 (1) |
| `framework=Win32` | **0** |
| framework 분포 | `Wpf` 181 |
| `launched≠ok` (탐침이 읽은 요소 상태) | 0 |
| `XPE-SKIP-ALLOWED:170` 으로 건너뛴 결과 | 0 |
| `ReadableWindow_IsKept_AndLeavesNoNote` | 38회 모두 Passed (건너뜀 0) |
| **대조군** `UnreadableWindow_IsReplaced_AndTheRunSaysSo` | **38회 모두 Passed** |

- **대조군의 뜻**: 탐침은 매 기동에서 실제 값(`Wpf`)을 읽었다(181/181). 모의로 창을 읽을 수 없게 만드는 `UnreadableWindow` 시험도 매 실행 돌았다. 따라서 Win32 0 은 "탐침이 안 돌아서" 가 아니다.
- **탐침 없는 8건**: `UnreadableWindow` 가 아직 없던 4건은 `absent`, 나머지 4건은 Passed 다.

## 3. 발생률

| 구분 | 발생 / 표본 | 비율 | 95% 신뢰구간 (Wilson) |
|---|---|---|---|
| CI 기동당 | 0 / 181 | 0% | 0 ~ **2.08%** |
| CI 실행당 (1건 이상 발생한 실행) | 0 / 38 | 0% | 0 ~ **9.18%** |
| 개발 기계 (#170 기록) | 7 / 100 | 7% | (#170 원 측정) |
| 참고: 이 세션 로컬 전체 Native 실행 (C-89) | 3 / 13 | 23% | 한 번의 실행이라 비교용 기록만 |

- **비교**: 개발 기계의 7% 는 CI 기동당 95% 상한(2.08%)보다 크다. **CI 러너의 기동당 발생률은 개발 기계보다 낮다**고 읽힌다.
- **로컬 사례의 모습**: 로컬 C-89 trx 의 Win32 3건은 모두 `launched=PropertyNotSupportedException fromHandle=ok fromTree=ok` 였다. C-56 에서 좁힌 "요소 생성 시점" 형태와 같다.

## 4. 한계

- **표본 편향**: 계수 대상은 성공·실패로 **끝난** 실행 42 + 4 건이다. 취소된 47건 안에 결함 기동이 있었는지는 알 수 없다(산출물이 없다).
- **기동 수의 범위**: 기동 수는 `ApplicationFixture` 를 거친 기동만 센다. `AutomationReportBackendTests` 처럼 fixture 를 쓰지 않는 기동은 탐침이 없어 표본에서 빠진다.
- **CI 실행 조건**: CI 는 매 실행 깨끗한 러너에서 돌고, 개발 기계는 다른 세션의 창이 떠 있는 데스크톱에서 돈다(C-82 S11 관측). 두 환경의 차이가 원인이라는 것은 **가설**이다. 원인 조사는 이 카드 범위 밖이다.
- **신뢰구간 계산 방식**: Wilson 구간은 기동끼리 서로 독립이라고 가정한다. 같은 실행 안의 기동끼리 상관이 있으면 실제 구간은 더 넓다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| CI 빈도 측정 | GUI-C-90 (이 카드) |
| 개발 기계 대비 CI 가 낮은 원인 | 다음 카드 (lead 판단) |
