[2026-10-14 11:58 KST] [xpe-gui] GUI-C-235d 보고 — R09 장벽 표지의 원자적 게시 + 첫 실패 재현 시도 (Codex #188, Refs #251)

푸시하지 않았다. 기준 `c9e256a5`. 바뀐 것은 `#if DEBUG` 안의 `BarrierSignal` 과 R09 한 줄뿐이다. 로컬 가용 메모리 6420 MB 로 시작해 6536 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`).

## 1. 표지 파일 원자적 게시

- `BarrierSignal`(`clients/ImageProcTest/MainWindow.xaml.cs`)이 `built.tmp`/`done.tmp` 에 다 쓰고 닫은 뒤 `File.Move(tmp, 최종 이름, overwrite)` 로 옮긴다. 시험은 최종 이름만 기다리고 읽는다(변경 없음). `release` 는 시험이 쓰는 쪽이고 앱은 존재만 보므로 대상 아님.
- R09 는 `built` 를 읽은 직후 `Assert.NotEmpty(builtTimestamp)` 를 더했다(빈 값이 조용히 지나가지 않게).
- **반증** (`falsify_atomic_publish.txt`): 최종 이름을 먼저 만들고 400 ms 뒤에 내용을 쓰는 변형(되돌린 형태)은 3/3 빨강 — `System.IO.IOException: The process cannot access the file '...uilt' because it is being used by another process.` 같은 400 ms 지연을 임시 이름 안에 넣은 수정본은 3/3 통과. 주입은 런타임 지연(`Thread.Sleep`)이며 거짓 조건이 아니다. 반증 뒤 원본을 복원하고 다시 빌드했다(`FALSIFY` 문자열 0건, diff 는 이 카드 변경분만).

## 2. 첫 실패(802 ms) 확인

깨끗한 빌드 직후부터 R09 를 10회 돌렸다. 실행마다 콘솔 출력을 `runs/runN.txt` 로 보존했다(trx 도 같은 폴더에 남겼지만 `.gitignore` 로 커밋되지 않는다 — 회차별 결과 표의 값은 trx 에서 읽은 것).

| 회차 | 결과 | 시험 시간 |
|---|---|---|
| 1 | Passed | 00:02.40 |
| 2 | Passed | 00:02.42 |
| 3 | Passed | 00:02.29 |
| 4 | Passed | 00:02.39 |
| 5 | Passed | 00:02.33 |
| 6 | Passed | 00:02.39 |
| 7 | Passed | 00:02.39 |
| 8 | Passed | 00:02.40 |
| 9 | Passed | 00:02.41 |
| 10 | Passed | 00:02.38 |

**재현되지 않았다(0/10 실패).** 수정 전 구조에서 802 ms 안에 나는 실패의 한 형태는 위 반증에서 본 `IOException`(표지가 생겼지만 내용 쓰기가 끝나기 전에 읽음)과 같다. 다만 그때의 메시지를 보존하지 못했으므로 그것이 원인이었다고는 말할 수 없다 — "수정 1 이 설명할 수 있는 형태의 실패였다"까지만 확인했다.

참고: 첫 시도는 `XPE_NATIVE_DIR` 를 빠뜨려 10회 모두 건너뜀(종료 코드 0)이었다. 통과로 세지 않고 버린 뒤 환경을 지정해 다시 돌렸다.

## 검증
- Legacy 묶음 1회: 실패 0 · 통과 21 · 건너뜀 2 (측정 프로브 2개, `legacy_class_run.txt`).
- 줄 끝 공백: 이 증거 폴더(.md/.txt) 0건.

## 한계
- 802 ms 첫 실패의 원인은 여전히 확인하지 못했다(재현 0/10).
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Codex #188 발견 1 (표지 원자적 게시) | GUI-C-235d |
| Codex #188 발견 2 (첫 실패 확인) | GUI-C-235d |
