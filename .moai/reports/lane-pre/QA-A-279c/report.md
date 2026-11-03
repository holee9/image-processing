[2026-10-22 01:58 KST]
# QA-A-279c — W15b 관찰 계측 구현 보고 (설계 QA-A-279 개정 2판 기준)

Refs #278. 결과 요약: **비 GUI 계측 시험 12개와 변이 5개 검출, E2E Debug 컴파일(수정 후)은 통과했다. W15b 에서 계측이 실제로 도는지는 검증되지 않았다(LOGGING-UNVERIFIED, 래퍼 배선).** 이 보고서는 컴파일·비 GUI 시험의 사실만 적으며 #266 HOLD 해제, 변동 원인 확정, 리더·Reviewer 판정과 무관하다.

## 1. 기준과 실행 승인

- 기준: dev/preprocess `3718666f`. 두 실행 모두 `git archive 3718666f gui clients` 로 푼 깨끗한 사본(`build/qa279c/src`, `build/qa279c-e2e/src`, git 무시 경로)에서 했고, 작업 트리의 미커밋 gui 변경 8개는 읽지도 쓰지도 않았다.
- 실행 승인: 사용자가 스크립트 전문과 해시를 읽은 뒤 각각 1회만 승인했다. 1차 `run_qa279c.sh` sha256 `1bf5d2d66de336eee0e873a5d0e5c0b9751db722c18e5b7809cbf67b96848d08`, 2차 `run_qa279c_e2e.sh` sha256 `a52afa20a4b3d625a2bd9d7e776ad9c7066a15a593a22c6228e0fc2a40d4785f`. 실행 전 해시·HEAD·폴더 부재·프로세스 0 을 다시 확인한 뒤 각 1회 실행했다. GUI 앱과 W15b 는 실행하지 않았다.
- 증거: `evidence/qa279c_run_evidence.tar.gz.b64.txt` (복원 `base64 -d … | tar -xz`; tar.gz sha256 `899a3f9c4e5cb72fbfeeaee8cfda5592fef8f0f7a61008e81bfb15c2e6a1dabf`). 두 스크립트, 두 출력, 모든 로그(`logs-run1/`, `logs-run2/`), 변경 diff 3개가 들어 있다. 디스크 원본은 `build/qa279c/`, `build/qa279c-e2e/` 에 그대로 있다.

## 2. 변경 파일 (6개, 허가 범위)

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/Services/FaultDelayLog.cs` (새) | 256칸 고정 메모리 기록기, 내보내기(header/event/pending/footer/END), 읽기·분류기, 열기 창 귀속. 파일 전체가 `#if XPE_TEST_FAULTS`, WPF 비의존 |
| `gui/ImageProcTest/Services/FaultInjectingBackend.cs` | `_delayLog`, sleep 앞뒤 `Begin`/`End`, `Shutdown()` 에서 1회 내보내기. `Describe()` 와 `_calls` 의미 불변 |
| `clients/ImageProcTest.E2ETests/.../QuickStartWalkthroughScenarios.cs` | `using ImageProcTest.Services;`, `OpenDetail`, `Launch(environment)`, `ProcessId`, `CloseGracefully`, W15b 의 환경 전달·닫기·관찰 표·지연 로그 보고(`#if XPE_TEST_FAULTS`). 원 단언과 순서 불변 |
| `clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj` | `XpeTestFaults.props` Import 1줄, `FaultDelayLog.cs` 링크 1줄(주석 포함 +6줄) |
| `clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj` | `FaultDelayLog.cs` 링크 1줄(+주석, 총 +2줄) |
| `clients/ImageProcTest.IntegrationTests/Functional/FaultDelayLogTests.cs` (새) | 비 GUI 시험 T1~T12 |

## 3. 실행 결과 (로그별, 스크립트 종료 코드 0 은 판정에 쓰지 않았다)

| 항목 | 로그 | 결과 |
|---|---|---|
| 6개 파일 복사 | 출력 | 6줄 모두 `copied identical` |
| GUI Debug 빌드 | `gui-debug.log` | 종료 0, 오류 0, 경고 36 (기준 비교 안 함) |
| GUI Release 빌드 | `gui-release.log` | 종료 0, 오류 0, 경고 22 (기준 비교 안 함) |
| 기존 `FaultSeamCompiledOutTests` | `held.log` | 5/5 통과 |
| 새 `FaultDelayLogTests` | `new.log` | 12/12 통과 |
| 변이 5개 | `m1`~`m5-*.log` | 5개 모두 적용(`MUTATION NOT APPLIED` 없음), 모두 시험 단언으로 실패(12개 모두 실행 = 컴파일 성공). 4절 |
| 원복 | 출력 | 5회 모두 `restored byte-identical` |
| 최종 12 | `final.log` | 12/12 통과 |
| 1차 E2E Debug 빌드 | `e2e-debug.log` | **종료 1, CS0103 9건**: 시나리오 파일에 `using ImageProcTest.Services;` 누락(제 실수) |
| 수정 후 E2E Debug 빌드 | `logs-run2/e2e-debug2.log` | 종료 0, 오류 0, 경고 7(그중 `QuickStartWalkthroughScenarios.cs(489)` CS1717 3건은 이번 변경 줄이 아님, 나머지 4건도 다른 파일; 기준 비교 안 함) |
| 첫 실행 폴더 보존 | 2차 출력 | manifest `69a1474bb07480d0` 실행 전후 동일 |
| 작업 트리 6개 해시 | 두 실행 끝 | 시작 때와 동일 |
| 잔류 프로세스 | 각 시험 뒤, 마지막 | 0 |

## 4. 변이와 예상·실제 (정정 포함)

| 변이 | 바꾼 것 | 실패한 시험 | 메시지 |
|---|---|---|---|
| M1 | `Begin` 이 아무것도 기록하지 않음 | T1, T2, T5, T7, T8, T10, T11 | T9 는 통과(변이 상태가 T9 의 입력 상황) |
| M2 | 끝 틱 = 시작 틱 | T2 | `call 1: recorded 0.0 ms for a real sleep of 60 ms` |
| M3 | 둘째 이후 경과를 첫 항목 값으로 | T2 | `call 2: recorded 69.6 ms for a real sleep of 100 ms` |
| M4 | `END` 줄 없음 | T4, T12 를 뺀 10개 | |
| M5 | `call` 항상 1 | T1, T2 | `Expected: Complete / Actual: Incomplete` |

**정정:** 설계 8.3절은 M4 의 실패 대상을 "T5 와 모든 COMPLETE 기대"로 적었다. 구현된 T5 가 M4 에서 실패한 것은 후반부(정상 export → Complete) 때문이고 전반부(미export → INCOMPLETE)는 M4 로 깨지지 않는다. 앞서의 구두 보고에서 "설계의 'T5 는 변이에 안 걸린다'와 다르다"고 한 표현은 틀렸으며 설계 원문이 맞다(설계 끝 '정정' 절에 기록).

## 5. Reviewer #268 조건 반영 위치

- F268-1: `FaultDelayLogReader.Classify` 의 `Anomaly`(SLEEP-RECORD-ABSENT-VS-REQUEST, "주입 또는 계측 이상") — T9. COMPLETE 문구는 "파일이 온전함"만 뜻한다고 명시.
- F268-2: W15b `ReportDelayLog` 가 `proxy minus sleep sum: N/A: PROXY-INCLUSION-UNPROVEN` 을 항상 출력하고, 모든 선결 조건이 맞을 때만 "ARITHMETIC ONLY, not a cause, not a correction, not an input to the gate" 값을 별도로 출력(`FaultDelayAttribution.Summarize`, T12).
- F268-3: 시험 허용 폭 `SlackMs = 20` 을 실행 전에 코드에 고정(결과를 보고 맞추지 않음). M2·M3 가 독립 기준(시험 자신의 시계)으로 잡힘.
- F268-4: 래퍼별 파일(`multiple-wrappers` → INCOMPLETE, T8), 진행 중 sleep 은 `pending`(T10), 동시 내보내기 1회(T11, 25회 반복), 부분 파일은 `no-footer`(T5), 오류는 삼키고 원 호출·예외를 가리지 않음.

## 6. 검증되지 않은 것

- **래퍼 배선(LOGGING-UNVERIFIED):** `FaultInjectingBackend.DelayIfArmed` 가 기록기를 실제로 부르는지는 비 GUI 에서 검증할 수 없다(WPF 타입 때문에 링크 불가, 리더 결정). 시험의 `Sleep` 보조는 같은 순서의 대역이다.
- W15b 쪽 코드(환경 전달, `CloseGracefully`, 닫기 시 `Shutdown`/`ProcessExit` 내보내기, 시간 창 귀속, 시계 영역)는 컴파일만 확인했고 실행하지 않았다. 실행 증거는 CI 의 `gui-e2e-native` 로그에서만 얻는다.
- 환경 변수가 자식 앱에 전달되는지, 앱 닫기가 backend `Shutdown()` 에 닿는지, 기록 비용이 열기 시간에 주는 영향은 미확인이다.
- E2E Release 구성 컴파일, 두 프로세스 `Stopwatch` 기준 일치(시계 영역 sanity check 는 W15b 출력에만 있음).
- 이 컴파일·시험의 통과는 W15b 계측 실행 검증도, #266 HOLD 해제도 아니다.

## 7. 불변 확인

임계값 1200 ms, 주입 1500 ms, W15b 의 `Median`·`proxyShift`·두 `Assert.True` 줄, 각 군 3회, 추가 GUI 팔 없음, `Describe()` 문자열은 바뀌지 않았다(게이트 줄은 diff 에 없다. 관찰 표 보조 함수 안에 같은 식의 `Median` 지역 함수가 새로 추가되었을 뿐이다). push 안 함.
