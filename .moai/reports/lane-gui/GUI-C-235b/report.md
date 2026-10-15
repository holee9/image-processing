[2026-10-14 09:37 KST] [xpe-gui] GUI-C-235b 보고 — Codex #186 보류 2건 (107b4766 위, Refs #251)

푸시하지 않았다. 레거시 앱(`clients/ImageProcTest`) 제품 코드 변경 → Codex 대상. 로컬 시작 전 가용 메모리 6291 MB / 32509 MB (`memory_before.txt`), 끝난 뒤 정리(`memory_after.txt`).

## 1. 오래된 시작 점검이 최신 보고 파일을 덮음 (P1)

**원인(코드)**: 시작 점검이 `ProbeNative()` → `NativeReadinessProbe.WriteReport()` 로 보고 **파일을 풀 스레드에서 세대 비교 전에** 썼다. `lock` 은 동시 쓰기 충돌만 막고 게시 순서는 못 막으므로, 오라클 판정/새로고침이 UI 스레드에서 새 보고를 쓴 뒤 뒤늦게 락을 얻은 낡은 `checking` 보고가 파일을 덮을 수 있었다(UI 는 최신, 파일은 낡음).

**수정**
- `NativeReadinessProbe.BuildReport`(점검 + JSON 텍스트 생성, 파일 안 씀)와 `PublishReport`(임시 파일에 다 쓴 뒤 `File.Move(overwrite)` 로 교체 → 부분 읽기 없음, 같은 `lock` 아래)로 나눴다. `WriteReport` 는 둘을 이어 부르는 기존 진입점(헤드리스 호출자용, 동작 동일).
- 창은 풀 스레드에서 **만들기만** 하고, **세대 확인을 통과한 결과만 UI 스레드(`ApplyNativeProbe`)에서 게시**한다. 오래된 시작 점검은 UI 에도 파일에도 반영되지 않는다. 게시와 화면 적용이 같은 스레드·같은 게이트라 파일과 화면은 항상 같은 점검의 것이다.
- 호출 경계 검사(`PreprocessOracleVerdictsTests.TheWindowAndItsViewModels_NeverCallTheOracleOrWaitForIt`)의 목록에 `NativeReadinessProbe.BuildReport(` 를 더했다(창의 호출은 `waitForOracle: false` 여야 한다).

**결정적 시험(장벽)** `R09_AnOvertakenStartUpProbe_NeverReplacesTheNewerReportFile`: 오라클을 게이트로 잡고(`XPE_ORACLE_TEST_GATE`, 파일 없음) 시작 점검이 끝난 보고(`checking`)를 8 s 붙들어 둔 채(`XPE_STARTUP_PROBE_DELAY_MS`) 게이트를 풀어 판정을 새로고침으로 먼저 도착시키고, 붙든 점검을 놓은 뒤 화면(`pass=True`)과 `native-readiness-report.json`(`preprocess.IsSyntheticOracleChecking`=false, `SyntheticOracle.Passed`=true)을 대조한다. 통과 2/2 (화면 판정 도착 2975/2786 ms < 붙든 8000 ms).
**반증**: 붙든 점검이 세대 확인 없이 직접 게시하게 하면(옛 순서) 2/2 빨강 — 파일 `checking=True, passed=False` 인데 화면은 `pass=True` (`falsify_publish_before_generation_check.txt`).

## 2. 지연 주입 훅이 출하 빌드에도 살아 있음 (P2)

**수정**: `XPE_STARTUP_PROBE_DELAY_MS` 읽기를 `#if DEBUG` 안으로 옮겼다(출하(Release) 빌드에는 이 코드가 없다). 또한 위치를 **시작 경로의 풀 스레드(점검이 끝난 뒤)** 로 옮겨 `ProbeNative()` 에서는 뺐다 — 수동 새로고침·오라클 판정 경로(UI 스레드)는 어떤 구성에서도 이 훅을 읽지 않는다.
**확인(출하 구성)**: 오라클을 잡아 둔 채 `XPE_STARTUP_PROBE_DELAY_MS=3000` 으로 각 구성의 앱을 띄워 시작 보고가 게시되는 시각을 쟀다(`release_vs_debug_delay_hook.txt`, 3회씩 교대):
| 구성 | 시작 보고 게시까지 |
|---|---|
| Release | 541 / 1278 / 539 ms (변수를 무시) |
| Debug | 3554 / 3553 / 3554 ms (훅이 3000 ms 를 붙듦) |

## 3. 줄 끝 공백
GUI-C-235 의 `legacy_class_runs.txt` 줄 끝 공백 10줄을 지웠다. GUI-C-234c~235b 증거 전체를 다시 스캔: 남은 줄 끝 공백 0.

## 검증
- Legacy 묶음(`Scenarios.Legacy`, 신규 R09 포함 23건 중 21 통과·2 건너뜀) **3회 반복 모두 실패 0** (`legacy_class_runs.txt`; 건너뜀 2건은 환경변수 게이트).
- 통합 962 통과 · 0 실패 · 2 건너뜀 (`integration_tests.txt`).
- 앱·통합·E2E 프로젝트 빌드 오류 0.

## 한계
- CI 러너에서 이 수정이 R05/R06/R08 의 UIA Timeout 을 없앤다는 증거는 여전히 CI 의 다음 실행이다(GUI-C-235 와 같음).
- 새 시험(R09)은 Debug 빌드의 시험 훅에 의존한다 — 출하 구성에서는 건너뛰어야 하나, E2E 는 항상 Debug 를 띄우므로 CI 에 영향이 없다.
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Codex #186 발견 1·2, 줄 끝 공백 | GUI-C-235b |
