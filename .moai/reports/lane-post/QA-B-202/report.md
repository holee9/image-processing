# QA-B-202 — 워커 샌드박스 시험이 시험 프로세스의 무결성 수준을 고정 가정하지 않게 (#250)

카드: `.moai/lanes/post/inbox/QA-B-202.md`. 관측(리더): main `f557390c` 의 post-build·ai-onnx 두 잡에서 `WorkerSandbox.TheRealWorkerRunsAtLowIntegrityAndStillAnswersItsRequests` 와 `WorkerSandbox.ControlWithTheRestrictionSwitchedOffTheRealWorkerRunsAtMediumIntegrity` 가 `test_ai_worker_sandbox.cpp(209)` 의 `EXPECT_EQ(0x2000u, …(GetCurrentProcessId()))` 에서 실패. CI 러너는 관리자라 시험 프로세스가 0x3000(높음)이다. 제품 쪽 단언(워커 0x1000)은 통과했다 — 시험의 환경 가정이 틀렸다.

## 0. 결과

| 항목 | 내용 |
|---|---|
| 바뀐 것 | 시험만(`modules/ai/tests/test_ai_worker_sandbox.cpp`). 호스트 무결성 수준을 읽어 기록(`[ HOST ] … 0x…` 한 줄)하고 상대 비교한다. 제품 코드 변경 없음. |
| (a) 제한한 워커 | `== 0x1000` 그리고 `워커 < 호스트`, 호스트 프로세스 자신은 낮아지지 않았다(읽기 값 == 처음 읽은 값). 호스트가 이미 낮음(≤ 0x1000)이면 그 사실을 메시지에 담아 `GTEST_SKIP`. |
| (b) 제한을 끈 대조 시험 | `워커 == 호스트` 의 무결성 수준. 시험 이름도 `…AtMediumIntegrity` → `…AtTheHostsIntegrity`. |
| 관측한 것 | 이 세션(보통 0x2000)에서 `WorkerSandbox.*`·`WorkerPipeSecurity.*` 8개 중 7개 통과·1개 건너뜀(스텁 빌드에서 원래 건너뛰는 시험). 낮은(0x1000) 호스트에서는 새 파일이 통과/건너뜀, **옛 파일은 두 시험 모두 빨강**. |
| 관측하지 못한 것 | **높음(0x3000) 호스트에서의 실행.** 이 세션은 관리자 권한이 아니다(`IsUserAnAdmin = 0`)이고 UAC 없이 높은 무결성 프로세스를 만들 수 없어 CI 와 같은 환경을 재현하지 못했다(§3). |

## 1. 바뀐 코드

- `HostIntegrityRid()` — 이 프로세스의 무결성 수준을 `IntegrityRidOfProcess(GetCurrentProcessId())` 로 읽고 출력한다. 읽지 못하면(0) 시험은 그 사실로 실패한다(`ASSERT_NE(0u, host)`).
- 시험 1(`…RunsAtLowIntegrityAndStillAnswersItsRequests`): 호스트 ≤ 0x1000 이면 건너뛴다. 아니면 `워커 == 0x1000`, `워커 < 호스트`, `호스트 값이 바뀌지 않음`을 단언한다.
- 시험 2(대조, `…AtTheHostsIntegrity`): `워커 == 호스트`. 제한을 끈 워커는 호스트의 토큰을 물려받으므로 호스트의 수준이 된다 — 개발자 셸에서는 0x2000, 관리자 러너에서는 0x3000.
- 주석 한 줄(`0x3000 high` 추가)과 `#include <cstdio>`.

## 2. 같은 가정이 다른 시험에 있는가 (item 3)

`git grep` 으로 `modules`·`tests`·`gui`·`clients/…IntegrationTests` 에서 무결성·관리자·권한 관련 단서를 찾았다(`IntegrityLevel|MandatoryLabel|IsUserAnAdmin|TokenElevation|runneradmin|Administrators|ACCESS_DENIED|non-admin`, 그리고 `modules/ai` 에서 `0x1000|0x2000|0x3000|IntegrityRid|elevat`). 시험 쪽 결과는 둘뿐이다:
- `test_ai_worker_sandbox.cpp` 의 위 두 시험(고침).
- `WorkerPipeSecurity.*` 두 시험(익명 열기 거부·DACL 에 사용자와 SYSTEM 만): 사용자 SID 를 **토큰의 사용자(`TokenUser`)** 로 읽어 비교하고 Administrators(`S-1-5-32-544`)가 DACL 에 없음을 단언한다. 관리자 토큰에서도 `TokenUser` 는 같은 사용자 SID 이므로 이 단언은 관리자 여부에 의존하지 않는다. 이 두 시험은 리더의 CI 관측에서 실패 목록에 없었다(실패는 위 두 시험뿐이라고 전달받음 — 로그를 직접 보지는 않았다).
- `modules/preprocess/tests/test_xcal_replace_retry.cpp` 의 `ERROR_ACCESS_DENIED`: 읽기 전용 목적지와 다른 프로세스가 연 파일의 경우이고 파일 속성·공유 모드에서 오는 거부이므로 권한 수준과 무관하다(코드를 읽은 것, 관리자 셸에서 돌리지는 않았다).
그 외에 관리자 러너에서 의미가 달라지는 단언은 이 검색에서 찾지 못했다. 검색은 위 단서 문자열에 한정된다.

## 3. 반증과 재현 한계

**높음 호스트를 재현하지 못했다.** 카드는 관리자 권한 셸에서 로컬로 한 번 재현하라고 했지만 이 세션은 관리자가 아니다(`IsUserAnAdmin = 0`). UAC 승격 프롬프트를 띄우는 것은 사용자 화면에 대한 동작이라 하지 않았다. 대신 **보통이 아닌 호스트**를 만들었다: 같은 사용자의 토큰을 복제해 무결성 수준을 낮음(0x1000)으로 낮춘 프로세스에서 시험 실행 파일을 띄운다(`low_host_runner.py.txt`; 출력은 낮은 수준 프로세스가 쓸 수 있는 `LocalLow` 에 쓴다). 이 호스트는 "시험 프로세스가 0x2000 이 아니다"라는 CI 와 같은 종류의 환경이다(방향만 반대).

| 실행 | 결과 |
|---|---|
| 새 시험, 호스트 0x2000(이 세션) | 7 통과·1 건너뜀(스텁 빌드) — `run_medium_host.txt` |
| **옛 시험(HEAD), 호스트 0x1000** | **두 시험 모두 빨강**: 209행 `Expected 0x2000 (8192), actual 4096`, 240행 같은 형태(`arm_out_low_host.txt`) — CI 의 실패와 같은 종류 |
| 새 시험, 호스트 0x1000 | 시험 1 건너뜀(메시지: "the test process itself runs at low integrity (0x1000)"), 대조 시험 통과(`워커 == 호스트 0x1000`) |

관리자 셸에서 이 시험을 확인하는 명령(리더 또는 관리자 권한이 있는 환경에서):
`build\ci-post\bin\xpe_ai_tests.exe --gtest_filter=WorkerSandbox.*:WorkerPipeSecurity.*` — 기대 출력: `[ HOST ] … 0x3000`, 시험 1 통과(`워커 0x1000 < 호스트 0x3000`), 대조 시험 통과(`워커 == 0x3000`).

## 4. Gap / 잔여 위험

Gap
- 높음(0x3000) 호스트에서의 실행을 관측하지 않았다(§3). `워커 < 호스트` 와 `워커 == 호스트` 가 0x3000 에서 성립한다는 것은 카드가 전한 CI 관측(워커 0x1000 통과, 제한을 끈 워커는 호스트와 같은 수준)과 제한을 끈 워커가 호스트 토큰을 물려받는다는 코드 읽기에서 온 추론이다.
- 낮은 호스트 실행에서는 나머지 두 시험(`ControlTheProbeSeesEveryCapability…`, `ARestrictedWorkerCannotWrite…`)도 빨갛다(`run_low_host_full_filter.txt`). 관측: 첫 시험은 제한을 끈 가짜 워커의 능력이 기대 287 이 아니라 281(현재 작업 폴더와 레지스트리에 쓰지 못함)이고, 둘째는 제한한 워커가 `%TEMP%` 에 파일을 썼다 — 낮은 호스트 실험을 위해 내가 `TEMP` 를 낮은 수준이 쓸 수 있는 `LocalLow` 로 돌려놓았기 때문이다(실험 장치의 부산물). 이 둘은 이번 변경과 무관하고 개발자 셸(0x2000)·CI(0x3000)에서는 통과하므로 고치지 않았다. 낮은 호스트에서 이 프로브 시험들의 동작은 보장하지 않는다.
- `git grep` 은 단서 문자열에 한정된다(§2).

잔여 위험
- 호스트가 낮음이면 시험 1 을 건너뛰어 제한 동작이 관측되지 않는다(드문 경우, 메시지에 사실을 남김).
