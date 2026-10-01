# QA-B-173 — 세션 비활성 조회 API, float32 입력 스케일 계약

## 새 수출 (xpe_ai.dll, ai_api.h)

```c
#define XPE_AI_WORKER_NOT_USED  0   /* use_worker 가 꺼져 있음 */
#define XPE_AI_WORKER_ACTIVE    1   /* 워커 경로 사용 중 */
#define XPE_AI_WORKER_DISABLED  2   /* 연속 실패로 이번 세션 비활성 */

XPE_API XpeErrorCode xpe_ai_worker_state(int32_t* stateOut,                 /* 필수 */
                                          uint32_t* consecutiveFailuresOut, /* NULL 허용 */
                                          uint32_t* ceilingOut);            /* NULL 허용, 현재 3 */
```

- 반환: `XPE_OK` / `XPE_ERR_NOT_INITIALIZED`(init 전이거나 shutdown 뒤 — 출력은 건드리지 않음) / `XPE_ERR_INVALID_INPUT`(`stateOut` 이 NULL).
- 읽기 전용: 워커를 띄우지 않고, 알림을 만들지 않고, 호출로 세지 않고, 상태를 바꾸지 않는다.
- **진행 중인 호출을 기다리지 않는다.** 호출은 워커가 멈추면 시간 예산 내내(기본 5 s) 모듈 뮤텍스를 쥐므로, 락을 잡는 조회는 UI 스레드를 그만큼 멈춘다. 진행 중에 부르면 "마지막으로 끝난 호출 기준" 상태를 돌려준다.
- 복구 = `xpe_ai_shutdown()` 뒤 `xpe_ai_init()` (새 세션, 횟수 0). 이 수출은 복구를 대신하지 않는다.
- 수출 수: `xpe_ai` 10 → **11**.

**GUI 쪽 P/Invoke 참고 (clients/ 는 Lane C 소유):** `int32_t*` 하나와 `uint32_t*` 둘, 마지막 둘은 `IntPtr.Zero` 가능. 상태는 호출 사이에만 의미가 있다 (아래 한계 참조).

## 1. 주장

1. `xpe_ai_worker_state` 가 NOT_USED / ACTIVE(0..2) / DISABLED(3) 를 올바르게 진행시키고, 성공이 횟수를 0 으로 되돌리는 것이 보이며, shutdown→init 이 새 상태를 준다.
2. 조회는 호출 중에도 막히지 않는다.
3. 오늘 코드는 float32 입력을 **어디서도 정규화하지 않는다**: 값이 모델에 그대로 들어가고 출력도 그대로 나온다. 헤더에 그렇게 명시했다. 동작은 바꾸지 않았다.

## 2. 증거

### 2.1 새 API (TDD)

- **red:** API 를 만들기 전에 시험부터 추가해 빌드 (`before_b173_build_red.txt`): `BUILD=1`, 원인은 `xpe_ai_worker_state` 와 `XPE_AI_WORKER_*` 가 선언되지 않음(C3861, C2065; 한글 메시지는 cp949 깨짐이라 식별자로 읽음). 같은 실행의 `RUN=0` 은 낡은 바이너리라 증거 아님.
- **green:** 양 빌드에서 `WorkerPathFixture`+`HugeDimensions`+`IpcDeadline` 필터 — `ci-post` 43 통과, `ci-ai` 48 통과 (`after_b173_ci_post.txt`, `after_b173_ci_ai.txt`).
- 시험 (이름이 계약):
  - `WorkerStateBeforeInitIsNotInitializedAndLeavesTheOutputsAlone`
  - `WorkerStateRefusesANullStateAndAcceptsNullOptionalOutputs`
  - `WorkerStateIsNotUsedWhenTheWorkerPathIsOff` (in-process 호출 뒤에도 NOT_USED, 천장은 항상 3)
  - `WorkerStateFollowsTheConsecutiveFailuresAndStaysDisabledAtTheCeiling` — **양 빌드 공통**(모델 없는 디렉터리는 매 호출 결정적으로 실패): ACTIVE 0 → 1 → 2 → DISABLED 3, 이후 4번을 더 불러도 3 에 머문다. 조회만으로 워커가 뜨지 않음(`ChildWorkers()==0`).
  - `AskingForTheWorkerStateRaisesNoAlertAndChangesNothing` (조회 50회: 알림 0, 횟수 불변)
  - `ShutdownThenInitGivesAFreshWorkerState`
  - `WorkerStateAnswersPromptlyWhileACallIsStuckOnASilentWorker` (전체 빌드 전용): 워커를 얼린 채 다른 스레드에서 호출을 걸어 두고 조회 → 250 ms 안에 ACTIVE, 호출은 아직 진행 중, 호출이 끝나면 실패 1 로 보임.
  - 기존 `IntermittentFailuresAlertOnEveryFailureAndAreNeverBlocked` 에 상태 단언 추가: 실패 2 → 횟수 2, 성공 → 횟수 0·ACTIVE.
- **반증 B1** — 조회가 모듈 뮤텍스를 잡게 함 (`arm_B1LockedQuery.txt`, 전체 빌드): `WorkerStateAnswersPromptly…` 빨강, 그 외 통과.

### 2.2 스케일 (읽은 것)

| 위치 | 하는 일 |
|---|---|
| `ai.cpp` `xpe_bone_suppress` (in-process) | `img->data` 를 `std::vector<float>` 로 복사해 `OnnxSession::Run` 에 그대로 넘김 |
| `ai_worker_main.cpp` `HandleBoneSuppress` (워커) | payload 의 픽셀을 `std::vector<float>` 로 복사해 `Run` 에 그대로 넘김 |
| `ai_onnx_session.cpp` `Run` | 길이만 검사(모델이 고정한 경우), 텐서 생성 후 실행, 출력 그대로 복사 |
| `validateImageBuffer` | 치수·크기·포맷만 봄, **값은 안 봄**(NaN 포함) |

`normaliz*`·`scale`·클램프 계열 검색(`ai.cpp`, `ai_onnx_session.cpp`, `ai_worker_main.cpp`, `ai_ipc_bridge.cpp`, 헤더)에서 값을 바꾸는 코드는 없었다. XPE-SDD-002:862 의 "Preprocess: normalize input to [0, 1]" 은 **구현되어 있지 않다**.

- 헤더(`xpe_bone_suppress` 설명의 "PIXEL SCALE")에 명시: 호출자가 모델이 기대하는 스케일로 넣는다. 모듈은 정규화·클램프·이동·범위 검사를 하지 않고, 출력은 모델이 내는 스케일 그대로다. 모델이 어떤 스케일을 기대하는지는 모델의 속성이며 모듈은 모른다.
- **고정 시험** `NoScalingIsAppliedToTheInputOrTheOutputOnEitherPath` (전체 빌드 전용, 양 경로): x2 모델에 -3, 0.5, 65535, ±1e6, 255, 4095 … 를 넣으면 출력이 정확히 2배. **반증 B2** — 모듈이 입력을 [0, 1] 로 클램프하게 함 (`arm_B2ClampsInput.txt`): 이 시험 빨강(같은 값을 쓰는 다른 두 시험도 같이 빨강).

## 3. Baseline 귀속

- 시작점: main 85dc2f4f 병합 뒤 HEAD (`git merge main` → 85dc2f4f, 빠르게 감김, 작업 트리 변경 없음).
- 전체 스위트 (프리셋, /WX, 종료 코드는 파이프 없이): `ci-post` cfg 0·build 0·ctest 0, 헤더 **976 / 976** (skipped 25 — 스텁 빌드 전용 건너뜀, DISABLED 1); `ci-ai` cfg 0·build 0·ctest 0, 헤더 **340 / 340** (skipped 5, DISABLED 0). 이전 968 / 332 → 각 +8 (상태 7 + 스케일 1). 잔여 `xpe_ai_worker.exe` 0. (`g171c-p-post-ctest.txt`, `g171c-p-ai-ctest.txt`, `g171c-p-cache.txt`)
- Doxygen(1.12.0, CI 와 같은 버전) 비-CSS 오류 0 (`doxygen_after_b173.txt`).

## 4. 미검증 (Gaps)

- **실제 모델이 기대하는 스케일은 모른다.** 저장소의 모델은 x2 장난감 모델뿐이다. "정규화 안 함" 은 오늘 코드의 사실이고, 실제 뼈 억제 모델이 [0, 1] 을 기대하는지·화소값 그대로를 기대하는지는 이 카드로 알 수 없다. 모델 카드(`xpe_ai_get_model_card`, `ai.cpp` 가 만드는 JSON)에는 입력 스케일을 나타내는 키가 없다(`input_scale`·`normaliz` 검색 결과 없음) — 호출자가 모델의 기대 스케일을 코드에서 물어볼 방법도 지금은 없다.
- `xpe_dl_denoise`, `xpe_stitch_images`, `xpe_bodypart_recognize` 의 스케일 계약은 쓰지 않았다. 검색에서 정규화 코드가 없음은 봤지만(위), 이들은 `Run` 이나 세션을 부르지 않고 `XPE_ERR_PROCESSING_FAILED` 를 돌려주는 경로(`ai.cpp` 에서 확인)라 "입력이 모델에 그대로 간다" 는 문장이 성립하지 않는다.
- 상태 조회의 락 없는 읽기는 `xpe_ai_shutdown` 과의 경쟁에서 안전하지 않다(다른 모든 함수와 같은 규칙). 이를 겨냥한 시험은 없다.
- 조회 중간에 호출이 천장에 도달하는 순간의 반쪽 상태(횟수 3·disabled false)는 코드 순서(횟수를 먼저, disabled 를 나중에 씀 / 읽기는 disabled 먼저)로 DISABLED 로 보고되게 했지만, 이 경쟁 창을 일부러 만들어 보는 시험은 없다. 코드 추론이지 관측이 아니다.
- `WorkerStateAnswersPromptly…` 의 250 ms 상한은 이 기계에서 정했다. 느린 CI 에서 흔들릴 수 있다(조회 자체는 락 없는 읽기 둘이라 호출 시간과 무관해야 한다).

## 5. 잔여 위험과 리더 소관

- **`docs/` 는 리더 소유라 건드리지 않았다. 갱신이 필요한 곳:**
  - `docs/project/api-spec.md:35` — `xpe_ai` 수출 수 10 → 11, 함수 목록에 `xpe_ai_worker_state` 추가, §9 에 항목 추가.
  - `docs/project/sdd_ai.md` — 함수 표(207행 부근)에 추가, 상한 값 행(176행)은 이전 카드에서 이미 알린 대로.
  - `docs/post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md:862` — "Preprocess: normalize input to [0, 1]" 은 현재 구현과 다르다. 문서를 코드에 맞출지, 정규화를 모듈에 넣을지는 **동작 변경이므로 먼저 보고** 대상으로 남겼다. 이 카드는 동작을 바꾸지 않았다.
- 정규화를 모듈이 하도록 바꾸려면 그 범위(어떤 정규화인지: 백분위, 최소·최대, 고정 나눗셈)를 정해야 하고, 출력 역변환 여부도 정해야 한다. 지금은 정할 근거(실제 모델)가 없다.
- GUI 가 "이번 세션 비활성" 을 영속 표시하려면 호출 뒤마다(또는 주기로) 이 API 를 읽으면 된다. 알림 큐와 달리 읽어도 사라지지 않는다.

---

# Codex #17 — post 몫 두 건 (bd27911 위)

## ① 상태 게시를 호출 완료 시점의 단일 스냅샷으로

**문제 (리더 지적, 코드로 확인):** `ai.cpp` 의 세 번째 실패 경로가 카운터 3 과 `disabled=true` 를 저장한 **뒤에** 워커 종료(`workerSupervisor.reset()`)·알림·반환을 했다. 그 사이 조회는 DISABLED / 3 을 봤다 — 호출은 끝나지 않았고 워커 프로세스는 아직 살아 있는데. "마지막으로 끝난 호출 기준" 이라는 헤더의 약속이 세 번째 실패 중에 깨졌다. 두 원자값을 따로 읽는 경쟁 창도 있었다.

**수정:** 내부 작업 필드(`workerConsecutiveFailures`, `workerDisabled`)를 일반 변수로 되돌리고(뮤텍스 아래에서만 씀, 클라이언트는 읽지 않음), 조회가 읽는 것은 **packed atomic 한 단어** `workerPublished`(비트 31 = disabled, 하위 비트 = 연속 실패 수) 하나뿐이다. `publishWorkerState()` 는 호출이 모든 일(카운트, 전환, 워커 종료, 알림)을 마친 **마지막에** 한 번 쓴다. 한 단어라 호출 시점이 다른 카운트와 플래그를 섞어 읽을 수 없고, 이전의 "카운트가 천장이면 DISABLED 로 간주" 보정 코드는 필요 없어져 삭제했다. 락은 여전히 잡지 않는다.

**시험** `WorkerStateKeepsTheLastCompletedStateUntilTheCallThatChangesItHasFinished` (양 빌드): 실패 2 까지 만든 뒤 워커를 얼리고 세 번째 호출을 다른 스레드에서 건 채 조회를 계속 폴링한다. 판정 기준은 (a) 첫 DISABLED 가 나오기 전까지 모든 조회는 정확히 (ACTIVE, 2), (b) **첫 DISABLED 가 보이는 순간** 호출의 나머지 효과가 이미 있어야 한다 — "disabled" 알림이 큐에 있고 워커 프로세스가 없다.
- 기준을 한 번 고쳤다: 처음엔 "호출 스레드가 반환 플래그를 세우기 전의 DISABLED = 오류" 로 썼는데, 수정된 코드에서도 39~41건이 걸렸다. 게시가 호출의 **마지막 단계**라 반환 플래그보다 수 마이크로초 앞서는 것이 올바른 동작이므로 시험 기준이 틀린 것이었다. 알림·워커 기준으로 바꿨다.
- **수정 전 (이전 커밋의 `ai.cpp` 비수정):** 빨강 — 폴링 약 1550만 번 중 첫 오관측 "DISABLED with 3 failures, 0 disabled-alerts, 0 live workers" (`before_audit17_prefix_unmodified_ci_post.txt`). 이 비수정 코드가 걸리는 것은 창이 수십 마이크로초라 **확률적**이다(옛 기준으로 같은 코드를 쟀을 때 530 / 1600만). 그래서 신뢰할 증거는 창을 넓힌 팔이다.
- **수정 전 + `Sleep(300)` 을 창에 주입:** 빨강 (`before_audit17_prefix_injected_window_ci_post.txt`).
- **수정 후:** `ci-post` 44, `ci-ai` 49 통과; 폴링 약 1600만 번 모두 ACTIVE / 2, 오관측 0 (`after_audit17_ci_post.txt`, `after_audit17_ci_ai.txt`).
- **반증 E** — 상태 저장 직후에 게시(호출이 끝나기 전) + `Sleep(300)` (`arm_E18PublishEarly.txt`): 빨강, 첫 오관측 "DISABLED … 0 disabled-alerts".
- **반증 F** — 변경마다 게시(카운트 증가 직후에도) + `Sleep(300)` (`arm_F18PublishEveryMutation.txt`): 빨강, 오관측 101건.

## ② 비차단 시험의 견고성

`WorkerStateAnswersPromptlyWhileACallIsStuckOnASilentWorker` 를 고쳤다.
- **`Sleep(300)` 제거:** 호출이 모듈 **안에서** 대기 중임을 증거로 확인한다. `WaitUntilTheModuleIsHeld` — 같은 뮤텍스를 잡는 `xpe_ai_get_model_card` 를 다른 스레드에서 불러 150 ms 안에 돌아오지 않으면 뮤텍스를 호출이 쥐고 있다는 증거다. 호출이 아직 안 들어갔는데 조회해서 우연히 통과(공허한 통과)하는 길이 막혔다.
- **`std::terminate` 경로 제거:** 스레드를 `JoiningThread`(소멸자에서 join 하는 RAII 래퍼)에 담아 ASSERT 조기 반환에서도 join 된다. 이 프로젝트는 C++17 이라 `std::jthread` 가 없다 — 처음 `std::jthread` 로 썼더니 빌드가 실패했고, 그 실행들의 "통과" 줄은 낡은 바이너리 결과여서 증거로 쓰지 않고 폐기했다(BUILD=1 확인).
- **상한:** 1000 ms, 예산(`timeout_ms`)은 5000 ms. 조회는 0 ms(타이머 분해능 ~16 ms 로 "16 ms 미만"), 락을 잡는 반증 B1 은 새 시험에서 **4985 ms** 기다렸다(`arm_B1LockedQuery_new_test.txt`): 상한의 5배, 실측값의 1000배 이상으로 분리되고 CI 가 1 초 느려도 견딘다. 측정 근거를 시험 주석에 갱신했다.

## 확인 (코드 변경 없음)

`xpe_ai_worker_state` 와 `xpe_ai_shutdown`/`xpe_ai_init` 의 동시 실행 금지: 헤더 주석에 **"MUST NOT run concurrently with xpe_ai_init or xpe_ai_shutdown … a use-after-free, not a stale answer"** 를 명시했다(기존엔 "must not race" 한 줄뿐이었다). 두 쪽 모두 공개 헤더가 이미 "Not thread-safe" 로 적고 있다. 클라이언트가 이를 직렬화해야 한다는 점도 적었다.

## 이번 라운드의 Gaps

- **`ci-post` 첫 전체 실행에서 `IntegrationTest.T608_PerformanceBudgetVerification`(enhance_advanced, 분수 처리 121 ms > 예산 50 ms) 1건이 실패했다** (`first_full_run_T608_failure_ci_post_ctest.txt`). 이 변경과 무관한 모듈의 타이밍 시험이고 이 라운드에서 건드린 곳이 없다. 단독 8회는 모두 통과(약 35 ms)했고 전체 재실행도 977 / 977 로 통과했지만, **원인(부하)은 증명하지 못했다** — 같은 실행의 다른 시험이 만든 부하라는 것은 추정이다. 재실행으로 덮었다고 읽히지 않도록 첫 실행의 실패를 증거로 남긴다. main CI 에서 같은 시험이 흔들리면 별도 카드 대상.
- 비수정 이전 코드가 새 시험에 걸리는 것은 확률적이다(위). 결정적 증거는 창을 넓힌 팔이다.
- 게시 순서(알림·워커 종료 뒤에 게시)는 코드 순서와 시험으로 확인했고, 다른 스레드에서의 메모리 순서(release/acquire)는 코드 추론이다.

## 전체 스위트 (최종 트리)

- `ci-post`: cfg 0, build 0, ctest 0, 헤더 **977 / 977** (skipped 25, DISABLED 1) — 두 번째 실행 (`g171c-r-post-ctest.txt`).
- `ci-ai`: cfg 0, build 0, ctest 0, 헤더 **341 / 341** (skipped 5) (`g171c-r-ai-ctest.txt`).
- 이전 976 / 340 → 각 +1 (세 번째 실패 중 조회 시험). Doxygen 비-CSS 오류 0 (`doxygen_after_audit17.txt`), 잔여 `xpe_ai_worker.exe` 0.
