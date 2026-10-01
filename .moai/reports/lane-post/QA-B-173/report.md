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
