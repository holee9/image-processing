# GUI-C-219 — 레거시 앱 시작 때 UI 가 멈추던 회귀 수정 (#249)

기준: `dev/gui` (GUI-C-220b `1097265f` 위). 원인은 GUI-C-218 에서 규명한 것 — 212b 가 오라클을 자식 프로세스로 옮기면서 `MainWindow.Window_Loaded` 가 그것을 UI 스레드에서 동기로 3번 불렀다.

## 변경
1. **오라클을 UI 스레드 밖에서**: `PreprocessOracleVerdicts`(신규) 가 DLL 파일(경로+쓰기 시각)별로 판정을 스레드 풀에서 만들고 보관한다. `TryGet` 은 기다리지 않는다(처음 부르면 실행을 시작하고, 끝나기 전엔 null). `Wait` 는 창이 없는 호출자(헤드리스 프로브, 픽스처 E2E 서비스)용. 끝나면 `Completed` 가 불리고 창이 디스패처로 한 번 갱신한다.
2. **"확인 중" 상태**: 판정이 오기 전 준비도는 "확인 중"이다. 통과도 실패도 아니다. 처리는 기존 "준비도 미확인 = 처리 막힘" 과 같이 막힌다(`ProcessingEnabled: false`, `IsSyntheticOracleReady: false`). 진단 탭은 `Preprocess smoke: checking in the background`(그 동안 `pass=` 없음), 매트릭스 행은 `Synthetic oracle checking`, 평가 탭은 "being checked in the background" 문장과 꺼진 스위치, 보정 탭은 `NATIVE-CHECKING`(Hard, 여전히 막음)이고 `NATIVE-NOT-READY` 는 없다.
3. **3번 → 1번**: 진단 보고서·모듈 준비도 두 번 모두 같은 보관소에서 읽는다. 사용자의 새로고침(Refresh 버튼 2곳, 메뉴 2곳, Refresh Modules)만 `Invalidate` 로 재확인한다. 실행 중인 판정은 건드리지 않는다.
4. **ApplicationFixture 생성자 예외 안전**: 생성자 본문을 try/catch 로 감싸 실패 시 앱 프로세스 트리·감시 핸들·타이밍 기록기·자동화 객체를 정리하고 다시 던진다(218 이 `LegacyApp.LaunchOrSkip` 에서 한 것과 같은 모양).
5. FlaUI `ConnectionTimeout` 은 올리지 않았다(지시대로).

시험 훅(정상 사용에서는 설정되지 않음): `XPE_ORACLE_TEST_GATE`(워커가 그 파일이 생길 때까지 기다림), `XPE_ORACLE_TEST_LOG`(워커가 시작할 때 한 줄 기록). 첫 버전은 고정 지연(`XPE_ORACLE_TEST_DELAY_MS`)이었으나 느린 UIA 읽기와 경주해 끝까지 돈 22회 중 7회 R03 이 "확인 중" 을 놓쳤다 — 게이트 파일로 바꿨다(아래 4).

## 1. 시작 응답 측정 (218 과 같은 방법, 교대)
`c218_block.py`: 앱을 띄우고 `WM_NULL` 에 응답하지 않는 가장 긴 구간을 잰다. A=214(`0ac0db5a`) 트리, B=218 HEAD(`21c9f612`), C=수정 후. 10 라운드를 A→B→C 순으로 교대. 회차별 표: `start_ab_c.txt`.

| 팔 | 최장 UI 멈춤 중앙값 | 최대 | 1 s 초과 | 2 s 초과 |
|---|---|---|---|---|
| A (214) | 218 ms | 237 | 0/10 | 0/10 |
| B (218 HEAD, 동기 3회) | 1296 ms | 2171 | 6/10 | 1/10 |
| C (수정) | 162 ms | 193 | 0/10 | 0/10 |

(B 의 10개 값: 882 889 890 902 1209 1384 1546 1617 1630 2171.) 목표 ≈224 ms 대비 C 는 162 ms 로 A 보다도 짧다(A 는 시작에서 오라클 대신 인프로세스 경로를 동기로 돌렸다). 자식 프로세스 수(`c218_children.py`, 4회): 시작마다 1개(수정 전 3개), 수명 243~267 ms.

## 2. 시험
- `PreprocessOracleVerdictsTests` 10건(통합): `TryGet` 이 막힌 실행을 기다리지 않음(1 s 안), 실행이 호출 스레드가 아닌 스레드 풀 스레드에서 일어남(스레드 id·`IsThreadPoolThread` 단정), 여러 번 물어도 한 번 실행·같은 객체, 새로고침만 재실행, 실행 중 새로고침은 둘째 실행을 만들지 않음, 바뀐 DLL 은 새 대상, 실행기 예외는 실패 판정, 완료 통지는 판정 저장 뒤 한 번.
- 레거시 E2E `R03`(UIA 패턴만): 워커를 게이트 파일로 붙잡은 동안 세 탭이 모두 "확인 중"(평가 탭 `Preprocess=ready` 아님·Offset 스위치 비활성·보정 탭 `NATIVE-NOT-READY` 0건)이고 UIA 읽기가 응답한다(세 탭 읽기 3.2 s — 이 읽기가 성공한 것이 창이 응답한다는 증거). 게이트를 열면 세 탭 모두 "준비" 로 바뀐다. 워커 로그가 한 줄(시작 시 한 번의 실행이 진단 보고서·세 탭을 모두 서비스), 사용자 Refresh 뒤 두 줄. `R01`/`R02` 는 판정을 먼저 기다린 뒤 파생값을 읽도록 순서를 바꿨다.
- `ApplicationFixtureConstructionTests`(E2E, 사용자 앱): 창 예산 1 ms 로 앱이 시작된 뒤 구성이 실패하게 하고(NullReference/Timeout — 시작 뒤에만 나는 실패만 인정), 앱 프로세스가 남지 않는지 시스템에서 센다.
- `NoAppCode_RunsTheOracleInTheAppsProcess_ExceptTheWorkerEntry`(212 시험)는 프로브가 `XpePreprocessOracleProcess.Run(` 을 직접 부른다고 단정했다. 프로브가 보관소를 부르도록 바뀌었으므로 "프로브는 `PreprocessOracleVerdicts.` 를 부르고 보관소의 실행기는 `= XpePreprocessOracleProcess.Run;`" 로 바꿨다(CRLF 클론 전체 실행에서 잡혔다 — 219 가 만든 회귀).
- 통합 시험 전체(LF 워크트리): 828 통과, 0 실패, 0 건너뜀.

## 3. 연속 32회 (레거시 E2E 전 시나리오)
`Scenarios.Legacy` 필터(11건: R01~R03, 오라클 격리, 미리보기 체인, 시작 실패 정리 등)를 32회 연속. 모든 회차 `exit=0 passed=11 failed=0 skipped=0`, 출력에 timeout 문자열 0줄. 표: `consecutive_32_runs.txt`.
첫 두 번의 시도는 무효였고 결과로 세지 않는다: (a) 첫 시도 32회는 내가 반증 팔에서 소스를 복원해 신선도 가드(`ImageProcTest.dll` 이 소스보다 오래됨)에 전부 걸렸다(UIA 와 무관), (b) 둘째 시도는 R03 의 고정 지연 경주로 끝까지 돈 22회 중 7회 실패 후 중단(마지막 회차는 중단되어 세지 않음).

## 4. 반증
| 팔 | 방법 | 결과 |
|---|---|---|
| 동기 복귀(전체) | B = 218 HEAD 트리 | 위 표: 중앙값 218→1296 ms, 1 s 초과 6/10 |
| 부분 복귀 | C 에서 창의 `waitForOracle: false` 2곳만 `true` (`start_partial_sync_arm.txt`, 교대 8회) | **나빠지지 않았다**: 최장 멈춤 중앙값 74 ms. 이 지표로는 반증이 아니다. 추정(미검증): 첫 `Wait` 가 창이 보이기 전 `Window_Loaded` 안에서 한 번 막고 이후는 보관소 적중이라, 측정 시작 이전에 끝난다. 사용자는 창이 늦게 나타남으로 느낄 수 있으나 이 도구는 그것을 재지 않는다 |
| 인라인 실행 | 보관소가 실행을 호출 스레드에서 돌리게 수정 | 시험 10건 중 2건만 통과, 나머지는 막힌 실행기를 기다리며 멈췄다(300 s 에 중단) — 깨끗한 빨강이 아니라 멈춤 |
| 공유 해제 | 모든 질문이 새 실행이 되게 키를 바꿈 | `ManyAsks`·`Invalidate…`·`Completed…`·`TryGet…`·`Invalidate_Leaves…` 5건 빨강 |
| 생성자 정리 제거 | `CleanUpAfterFailedConstruction()` 호출을 주석 처리 | `a failed construction left 1 app instance(s) running` 빨강, 복원 후 초록 |
| R03 고정 지연 | 위 (b) | 7/22 실패 — 게이트 파일로 교체 후 32/32 통과 |

"연속 실행에서 UIA 타임아웃이 다시 나타난다" 는 B 트리로는 돌리지 않았다(미검증). 218 에서 B 의 간헐 타임아웃을 관측했으나 수정 전 트리로 32회 시리즈를 재현하지는 않았다.

## 한계 / 미검증
- 응답 측정은 `WM_NULL` 무응답 구간이다. 창이 처음 보이는 시각은 모든 팔에서 비슷(약 440~550 ms)했다.
- UI 멈춤→UIA 타임아웃의 인과는 218 에서도 같은 실행 안에서 증명하지 못했다.
- 부분 복귀 팔이 반증이 되지 못한 이유는 추정이다.
- CI 에서의 결과는 아직 없다. CRLF 클론에서는 통합 시험만 돌렸고 E2E 는 돌리지 않았다.
- 통합 시험 828 은 로컬 빌드 DLL 구성이다(CI 가 만든 DLL 이 아님).
