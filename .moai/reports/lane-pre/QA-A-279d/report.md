[2026-10-22 02:35 KST]
# QA-A-279d — Reviewer #269 F269-1·F269-2 보완 보고 (cdec2dd2 후속)

Refs #278. 결과 요약: **부분 기록 거부, 예약 뒤 seal 재확인, 산술 출력 삭제를 구현했고, 영향 범위의 비 GUI 시험 14개·guard 5개·새 변이 2개 검출·원복·GUI/E2E 컴파일이 모두 확인되었다. 래퍼 배선과 W15b 실행은 여전히 검증되지 않았다(LOGGING-UNVERIFIED, 래퍼 배선).** 이 보고서는 컴파일·비 GUI 시험의 사실만 적으며 #266 HOLD 해제나 변동 원인 확정, 리더·Reviewer 판정과 무관하다.

## 1. 기준과 실행 승인

- 기준: dev/preprocess `cdec2dd2`(부모 `3718666f`). 사본 `build/qa279d/src`(git 무시 경로)는 `git archive cdec2dd2 gui clients` 로 푼 깨끗한 사본이며, 바뀐 3개 파일만 작업 트리에서 올렸다. 작업 트리의 미커밋 gui 변경 8개는 읽지도 쓰지도 않았다.
- 실행 승인: 사용자가 스크립트 전문(78줄, 4809바이트)을 읽은 뒤 sha256 `7da5f488e3b2d632a2d20751474a7fe76497df2675b342442b8b4f56eb0f70ec` 에 한해 1회 승인했다. 승인 메시지에 해시 오타(65자)가 있어 한 번 멈추고, 정정된 64자 해시를 받은 뒤 실행했다. 실행 직전에 해시 일치, 78줄/4809바이트, HEAD `cdec2dd2`, 3개 파일 해시, `build/qa279d` 부재, 입력시험 프로세스 0 을 다시 확인했다.
- GUI 앱, W15b, 삭제, 외부 전송은 없었다. NuGet 복원 외 네트워크 사용은 없고 사용 통계 전송은 껐다.
- 증거: `evidence/qa279d_run_evidence.tar.gz.b64.txt`(복원 `base64 -d … | tar -xz`, tar.gz sha256 `aa9f53a38e2ca7b82fc6273b6bef3a06fb20c80f4e181dc4c014ebb5c1afd6ed`). 스크립트, 출력, 로그 7개, 변경 diff 가 들어 있다. 디스크 원본은 `build/qa279d/` 에 있다.

## 2. 변경 (허가 3파일)

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/Services/FaultDelayLog.cs` | 읽기: 순수 지연 경로에서 `0 < events < calls` 는 `INCOMPLETE: partial (… no sleep for call number(s) …)`, 범위 밖 call 번호는 `inconsistent`. 기록: `Begin` 을 `IsOpenForBegin()` + `Reserve()` 로 나누고 예약 뒤 seal 을 다시 확인해 late 로 센다(측정 중 잠금 없음, 경로에 훅 없음). footer 의 `calls` 는 슬롯 확인 뒤에 읽는다. 산술 참고치 관련 `ArithmeticEligible`·`Why` 삭제 |
| `clients/ImageProcTest.E2ETests/.../QuickStartWalkthroughScenarios.cs` | W15b 출력부만: `proxy − sleep 합계` 산술 출력 삭제. `N/A: PROXY-INCLUSION-UNPROVEN` 줄, 원자료, 열기 창별 분류, "차감하지 않는 raw 합" 만 남김. 게이트 줄 불변 |
| `clients/ImageProcTest.IntegrationTests/Functional/FaultDelayLogTests.cs` | T12 를 산술 조건 없는 형태로 고침. 새 T13(seal 확인과 예약 사이에 export 를 끼워 결정적 재현), T14(`calls=2, events=1` 과 범위 밖 call 번호) |

## 3. 실행 결과 (로그별, 스크립트 종료 코드 0 은 판정에 쓰지 않았다)

| 항목 | 로그 | 결과 |
|---|---|---|
| 3개 파일 복사 | 출력 | 3줄 모두 `copied identical` |
| GUI Debug 컴파일 | `gui-debug.log` | 종료 0, `error CS` 0, 오류 0, 경고 22 (기준 비교 안 함) |
| E2E Debug 컴파일 | `e2e-debug.log` | 종료 0, `error CS` 0, 오류 0, 경고 7 (기준 비교 안 함; 로그에서 FaultDelayLog 를 가리키는 경고는 0건) |
| 기존 guard `FaultSeamCompiledOutTests` | `held.log` | 5/5 통과 |
| `FaultDelayLogTests` 14개 | `new.log` | 14/14 통과 (T1~T14) |
| 변이 M6 (예약 뒤 seal 재확인 제거) | `m6-no-seal-recheck.log` | 14개 실행(컴파일 성공), **T13 만 실패** `Expected: -1 / Actual: 1` |
| 변이 M7 (partial 규칙 제거) | `m7-no-partial-rule.log` | 14개 실행, **T13, T14 실패** `Expected: Incomplete / Actual: Complete` |
| 원복 | 출력 | M6, M7 모두 `restored byte-identical` |
| 최종 | `final.log` | 14/14 통과 |
| 이전 출력 폴더 보존 | 출력 | `qa279c=69a1474bb07480d0`, `qa279c-e2e=1bdb329868945d40` 실행 전후 동일, `earlier folders unchanged` |
| HEAD, 3개 파일 해시 | 출력 | `cdec2dd2`, 시작 때와 동일 |
| 잔류 프로세스 | 각 시험 뒤, 마지막 | 0 |

변이 적용 여부: 두 `sed` 는 실행 전에 `/tmp` 임시 사본으로 각각 1줄이 바뀌는 것을 확인했고, 실행 중에도 `MUTATION NOT APPLIED` 는 나오지 않았다.

## 4. Reviewer #269 조건 대응

- F269-1: reader 의 partial 규칙(T14, M7 이 검출), 예약 뒤 seal 재확인(T13, M6 이 검출). 결정적 재현은 `IsOpenForBegin()` → `Export()` → `Reserve()` 순서를 시험이 직접 밟아서 한다. 이 경합이 실제 앱에서 얼마나 자주 나는지는 측정하지 않았다.
- F269-2: 산술 참고치 출력과 그 선결 조건 코드를 삭제했다(증거: diff 에 `ArithmeticEligible` 없음, 출력부에 N/A 줄만). "raw 합" 줄은 남아 있으며 어디에서도 차감하지 않는다.
- 결정: 순수 지연 경로(W15b)를 전제로 한다. 호출이 sleep 전에 실패하는 혼합 fault 는 이 로그가 설명하지 않으며, 그런 경우 `events < calls` 는 `partial` 로 읽힌다.

## 5. 검증되지 않은 것

- **래퍼 배선(LOGGING-UNVERIFIED):** `FaultInjectingBackend.DelayIfArmed` 가 기록기를 실제로 부르는지는 비 GUI 에서 검증하지 못한다. 시험의 `Sleep` 은 대역이다.
- W15b 쪽 코드(환경 전달, `CloseGracefully`, 닫기 시 export, 시간 창 귀속, 시계 영역)는 컴파일만 했고 실행하지 않았다. CI `gui-e2e-native` 로그에서만 확인할 수 있다.
- 경고 수(GUI Debug 22, E2E 7)는 기준과 비교하지 않았다. 첫 보고(GUI Debug 36)와 숫자가 다르나 이유는 조사하지 않았다.
- E2E Release 컴파일, 출하 binary 분석, 두 프로세스의 `Stopwatch` 기준 일치.
- 이 PASS 는 W15b 계측 실행 검증도, #266 HOLD 해제도 아니다.

## 6. 불변 확인

임계값 1200 ms, 주입 1500 ms, W15b 의 `Median`·`proxyShift`·두 `Assert.True` 줄, 각 군 3회, 추가 GUI 팔 없음, `Describe()`·`_calls` 의미, 미커밋 gui 변경 8개는 바뀌지 않았다. push 안 함.
