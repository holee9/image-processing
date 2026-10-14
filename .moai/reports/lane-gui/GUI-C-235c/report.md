[2026-10-14 10:41 KST] [xpe-gui] GUI-C-235c 보고 — R09 를 시간 추정 대신 명시적 장벽으로 (Codex #187, Refs #251)

푸시하지 않았다. 기준 `87ec383a`(= 69bc741a + main 2b9f37d8). 바뀐 것: **시험과 Debug 전용 장벽뿐** — 출하(Release) 코드는 그대로이며 Release 에는 새 코드가 없음을 아래처럼 확인했다. 로컬 시작 전 가용 메모리 5886 MB / 32509 MB (`memory_before.txt`), 끝난 뒤 정리(`memory_after.txt`).

## 문제와 수정

**문제(Codex #187)**: R09 가 `Thread.Sleep(1500)` 으로 "시작 보고서가 이미 `checking` 으로 만들어졌다"고 가정했고, 시작 작업이 실제로 끝났음을 알 신호가 없었으며, 공용 보고 파일을 지우지 않아 이전 실행의 보고서를 읽을 수 있었다.

**수정(Debug 전용 장벽, `#if DEBUG`)** — `XPE_STARTUP_PROBE_BARRIER_DIR` 디렉터리 안의 표지 파일로 순서를 강제한다:
1. 시작 점검이 `BuildReport` 를 끝내면 `built`(보고서의 `timestampUtc`)를 쓰고, `release` 파일이 생길 때까지 붙든다(안전 한도 60 s).
2. 시작 작업이 세대 확인을 마치면 `done` 에 `dropped`/`applied` 를 쓴다.
3. 시험은 이 표지들로만 순서를 잡는다: 시작 전에 공용 보고 파일을 **지운다** → `built` 를 기다린다(그 시점에 보고 파일이 **없음**을 단언: 시작 점검이 스스로 게시하지 않았고 이번 실행의 것이 아님) → 그제야 오라클 게이트를 푼다(따라서 시작 보고서는 확실히 `checking`) → 화면에 `pass=` 가 나타나길 기다린다(그 새로고침이 새 보고를 게시) → `release` 를 쓴다 → `done` 이 `dropped` 임을 기다린다 → 파일을 읽어 **시작 보고서의 timestamp 와 다름**, `IsSyntheticOracleChecking=false`, `SyntheticOracle.Passed=true`, 화면 `pass=True` 를 단언.
`Thread.Sleep` 로 순서를 잡던 줄과 "판정 시각 < 8 s" 단언은 없앴다(남은 20 ms 폴링은 표지 파일을 기다리는 루프 안이며 순서를 잡지 않는다). 지연 훅 `XPE_STARTUP_PROBE_DELAY_MS`(응답성 측정용)는 그대로 남아 있다.

## 확인
- R09 통과: 첫 묶음에서 1회 실패 후 2회 통과, 이어 5/5 통과 (`r09_after_run1..5.txt`). 매 실행의 출력: `1 start-up report built (timestamp …), held, nothing published` → `2 verdict on screen: … pass=True` → `3 start-up probe ended: dropped` → `4 … file timestamp ≠ start-up report timestamp, checking=False, passed=True`.
  - **첫 실패(802 ms, 시험 본문 전)는 원인을 확인하지 못했다.** 직전에 앱을 막 빌드/종료한 직후의 첫 실행이었고 이후 통과를 확인한 실행은 10회(R09 단독 7회 + Legacy 묶음 3회 안의 R09)이고, 그 사이 4회는 출력 파일을 읽지 못해 결과를 확인하지 않았다. 실패 메시지도 읽지 않았다. 후보는 앞선 측정이 남긴 프로세스가 보고 파일/실행 파일을 잡고 있었을 가능성이나, 근거는 없다.
- **반증(세대 게이트 제거) 4/4 빨강** (`falsify_no_generation_gate.txt`): 붙든 시작 점검이 해제 뒤 직접 게시하게 하면 매번 `done=dropped` 인데도 파일은 시작 보고서 그대로다 — 파일 timestamp 가 `built` 의 timestamp 와 **같고** `checking=True, passed=False`, 화면은 `pass=True`. (235b 의 반증은 시간 추정 위에서 2/2 였고, 이번은 장벽이 순서를 고정하므로 4/4 로 빨강이었다.)
- **Release 에서 사라짐** (`release_has_no_barrier_or_delay_hook.txt`): `ImageProcTest.dll` 의 UTF-16 문자열 검색 — Debug 에는 `XPE_STARTUP_PROBE_BARRIER_DIR`·`XPE_STARTUP_PROBE_DELAY_MS` 가 있고 **Release 에는 둘 다 없다**. (`XPE_ORACLE_TEST_GATE` 는 이 카드 이전부터 있던 별도 시험 훅으로 양쪽에 있다 — 건드리지 않았다.)
- Legacy 묶음 3회 반복: 매번 실패 0 (21 통과·2 건너뜀) (`legacy_class_runs.txt`). 통합 962 통과 · 0 실패 · 2 건너뜀 (`integration_tests.txt`).
- 줄 끝 공백: 이 증거 폴더 0건.

## 한계
- 첫 실행 1회 실패(위)는 설명하지 못했다. CI 에서 R09 가 같은 이유로 흔들리면 `built` 가 오지 않는다는 `TimeoutException`(30 s)으로 보인다.
- 앱 코드 변경은 모두 `#if DEBUG` 이지만, Debug 빌드 실행(E2E)은 이 표지 쓰기 때문에 `ProbeNative` 직후 파일 I/O 가 약간 늘어난다 — 환경변수가 없으면 아무것도 하지 않는다.
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Codex #187 발견 1 (R09 순서·완료 신호·이번 실행 확인) | GUI-C-235c |
