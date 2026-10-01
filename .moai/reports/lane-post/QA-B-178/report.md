# QA-B-178 — xpe_ai_shutdown 수명 결함, 그리고 bone_suppress 와 shutdown 의 경쟁

Refs #130 (위험 배경 #233, 출처 Codex #25 발견 1)

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `xpe_ai_shutdown` 은 `std::lock_guard` 가 살아 있는 범위에서 `delete state` 를 실행해, 함수 끝에서 파괴된 mutex 를 unlock 했다. 수정 후 잠금 범위는 delete 전에 끝난다. |
| C2 | 결함 2(경쟁)는 모듈을 고치지 않고 **헤더 계약으로 닫았다**. 수정 전 헤더의 문구는 모든 수출 함수에 대해 명확하지 않았으므로 명시적으로 적었다. |

## 2. 결함 1 (확정)

변경 전 (`ai.cpp`, `xpe_ai_shutdown`):

```
auto* state = g_aiState;
std::lock_guard<std::mutex> lock(state->mtx);   // 함수 끝까지 산다
...
g_aiState = nullptr;
delete state;                                    // 잠금이 살아 있는 채로 mutex 가 파괴됨
}                                                // 가드 소멸자가 파괴된 mutex 를 unlock
```

변경 후: 잠금을 블록 `{ … }` 으로 감싸 **전역 포인터를 비우는 것까지 잠금 안에서** 하고, 블록을 닫은 뒤에 `delete state`. 순서는 "(잠금 안) 포인터 비움 → 잠금 해제 → 상태 파괴" 이다. 포인터를 먼저 비우는 이유는, 잠금 안에서 비운 뒤에는 전역을 통해 이 상태에 도달할 방법이 없기 때문이다.

### 2.1 증거 (도구에 의존하지 않는 순서 관측)

시험 전용 훅 `xpe_ai_test_set_before_state_delete_hook` (기존 `XPE_AI_TEST_HOOKS` 옵션 아래, 기본 ON 이고 납품 구성은 OFF): `delete` 직전에 "상태 mutex 가 아직 잠겨 있는가" 를 알린다. 잠금 여부는 **다른 스레드에서** `try_lock` 으로 본다(소유 스레드의 `try_lock` 은 `std::mutex` 에서 정의되지 않은 동작).

| 구성 | 명령 | 결과 |
|------|------|------|
| ci-post | `xpe_ai_tests --gtest_filter=ShutdownLifetime*` | 3 passed |
| ci-post 전체 | ctest | `100% tests passed, 0 tests failed out of 1012` (`after_full_ci_post_ctest.txt`) |
| ci-ai 전체 | ctest | `100% tests passed, 0 tests failed out of 366` (`after_full_ci_ai_ctest.txt`) |
| 경고 | 두 빌드 로그 `warning C` | 0건 |

시험 3개: ① delete 직전 mutex 가 잠겨 있지 않음(대조: 훅이 정확히 1번 호출됨), ② 초기화 안 된 모듈의 shutdown 은 no-op(훅 미호출), ③ init → 사용 → shutdown 을 100회 반복하며 매 회 delete 1번·mutex 해제·shutdown 후 `XPE_ERR_NOT_INITIALIZED`, 이어서 한 번 더 init 해 정상 사용.

### 2.2 반증

잠금 범위를 함수 끝까지(원래 순서) 되돌려 같은 시험을 돌림 (`BUILD=0`): **2개 실패** — 훅이 "잠겨 있음" 을 보고. `arm_original_order_run.txt`.

### 2.3 쓰지 않은 도구 (미검증)

카드는 "디버그 CRT/검사 도구로 파괴된 mutex 접근이 사라졌음을 보이라, 못 잡으면 '이 도구로는 판별 불가' 로 적으라" 고 했다. **외부 검사 도구(ASan, 페이지 힙, 디버그 CRT 지연 해제 검사)는 이번에 돌리지 않았다.** 그러므로 "검사 도구가 잡았다/못 잡았다" 는 주장하지 않는다. 판별 근거는 위의 순서 관측뿐이다. 참고로 `std::mutex::unlock` 의 실제 구현은 MSVC 런타임 DLL 안에 있어 ASan 계측이 닿지 않을 가능성이 있으나, 이것은 확인하지 않은 추정이다.

## 3. 결함 2 (경쟁): 계약부터

### 3.1 변경 전 헤더가 말한 것 (`modules/ai/include/xpe/ai/ai_api.h`)

- `xpe_ai_init` (139행): "Thread safety: Not thread-safe; call from single thread at startup."
- `xpe_ai_shutdown` (157행): "Thread safety: Not thread-safe; call from single thread at shutdown."
- 다른 수출 함수: "Reentrant" (`bodypart_recognize`, `stitch_images`, `stitch_estimate_size`, `bone_suppress`, `dl_denoise`), "Thread-safe" (`get_model_card`, `set_fallback_mode`), `xpe_ai_version`("Thread-safe", 68행).
- 유일하게 명시한 곳: `xpe_ai_worker_state` (470~475행): "It MUST NOT run concurrently with xpe_ai_init or xpe_ai_shutdown, which the module documents as not thread-safe (they free or create the state this reads): a call that races them is a use-after-free, not a stale answer."

### 3.2 판단

init/shutdown 의 "Not thread-safe" 는 **그것들 자신의 동시 호출을 막는 것**으로는 분명하지만, "Reentrant"/"Thread-safe" 로 표시된 나머지 함수가 init/shutdown 과 동시에 돌 수 있는지는 **`xpe_ai_worker_state` 한 곳을 빼고 적혀 있지 않았다.** "Reentrant" 는 호출자 입장에서는 "동시에 불러도 된다" 로 읽힐 수 있다. 그래서 카드의 "계약 문구가 모든 수출 함수에 대해 명확한지 확인" 항목에는 **아니오**였다.

이 판단에 대해 반대 해석도 가능하다(카드: "모호하면 결함으로 보고 수정"). 나는 모호함을 모듈 수정이 아니라 **계약의 명시**로 풀었고, 리더가 다른 쪽을 원하면 아래 3.4 의 비용을 근거로 다시 정할 수 있다.

### 3.3 변경

`xpe_ai_shutdown` 문서에 `LIFECYCLE CONTRACT` 문단 추가, `xpe_ai_init` 문서에서 그것을 가리킴: init 과 shutdown 은 **서로, 자기 자신과, 그리고 "Reentrant"/"Thread-safe" 로 표시된 것을 포함한 모든 다른 `xpe_ai_*` 호출과** 동시에 실행될 수 없다. 그 단어들은 그 함수들끼리의 동시성을 말할 뿐 init/shutdown 에 대한 것이 아니다. 어기면 stale 한 답이 아니라 use-after-free 이다. 호출자가 직렬화한다(예: 모든 호출은 공유, init/shutdown 은 독점으로 잡는 읽기/쓰기 잠금).

### 3.4 모듈이 직접 막지 않은 이유

| 선택지 | 비용 |
|--------|------|
| 전역 수명 잠금(읽기/쓰기) 을 모든 수출 함수에 추가 | shutdown 이 진행 중인 가장 긴 호출을 기다린다(워커 호출은 deadline 까지 갈 수 있음). 그리고 `xpe_ai_worker_state` 가 잠금을 잡게 되어 "UI 스레드가 기다리지 않는다"는 문서화된 보증(잠금 없음)이 깨진다. 이를 피하려면 `g_aiState` 를 원자 포인터로 하고 참조 계수를 도입해야 한다 |
| 참조 계수 / 실행 중 카운트 | 호출마다 원자 증감 2회 + shutdown 의 대기 루프. 설계 범위가 이 카드를 넘고, `workerPublished` 의 잠금 없는 읽기 경로와 얽힌다 |
| **계약 명시 (선택)** | 모듈 비용 0. GUI 쪽에서 계약을 지키도록 GUI-C-186b 가 진행 중이라고 리더가 전함 |

호출당 비용은 측정하지 않았다(수정하지 않았으므로).

### 3.5 수출 함수 전수 (같은 형태의 위험)

`ai.cpp` 에서 `g_aiState` 를 읽은 뒤 `state->` 를 사용하는 수출 함수. 이름 축(`g_aiState` 줄 grep) 으로 센 것이고 이 계약이 모두 덮는다:

| 수출 함수 | 상태 읽기 | 헤더의 동시성 표기 | 계약이 덮는가 |
|-----------|-----------|--------------------|---------------|
| `xpe_ai_init` | 쓰기 | Not thread-safe | 예 |
| `xpe_ai_shutdown` | 읽고 삭제 | Not thread-safe | 예 |
| `xpe_ai_worker_state` | 잠금 없이 읽기 (`const AiModuleState* state = g_aiState;`) | (명시적 MUST NOT, 이미 있었음) | 예 |
| `xpe_bone_suppress` | 읽은 뒤 잠금 (`auto* state = g_aiState;` 다음에 `lock_guard`) | Reentrant | 예 (이번에 명시) |
| `xpe_ai_get_model_card` | 읽은 뒤 잠금 (같은 형태) | Thread-safe | 예 (이번에 명시) |
| `xpe_ai_set_fallback_mode` | 읽은 뒤 원자 저장 (`auto* state = g_aiState;` 다음에 `fallbackMode.store`) | Thread-safe | 예 (이번에 명시) |
| `xpe_bodypart_recognize`, `xpe_stitch_images`, `xpe_stitch_estimate_size`, `xpe_dl_denoise` | `checkInitialized()` 가 `g_aiState` 를 읽음 | Reentrant | 예 (이번에 명시) |
| `xpe_ai_version` | 상태 읽지 않음 | Thread-safe | 해당 없음 |

한계: 줄 번호는 같은 파일 편집에 밀리므로 코드 형태로 인용했다. 이름 축(`g_aiState`) 하나로 센 것이다(`state->` 를 쓰는 함수를 호출 축으로 다시 세어 대조하지는 않았다).

## 4. Gaps (미검증)

- 외부 메모리 검사 도구는 돌리지 않았다(2.3).
- 결함 2 의 경쟁을 결정적으로 재현하는 시험은 만들지 않았다(계약으로 닫았으므로). 계약 위반 사용의 실제 결과(use-after-free)는 이 카드에서 실행해 보지 않았다.
- 훅 호출은 `XPE_AI_TEST_HOOKS` 옵션이 OFF 인 DLL 에 없다는 것을 이번 카드에서 `dumpbin` 으로 다시 확인하지는 않았다(QA-B-173 에서 확인한 같은 옵션을 재사용).
- Doxygen 은 이 환경에 설치되어 있지 않아 헤더 주석 변경을 돌려 보지 못했다(`which doxygen` 없음).
- `xpe_ai_init` 쪽 `g_aiState = state` 와 "이미 초기화" 검사의 동시성은 계약으로만 덮었고 시험하지 않았다.

## 5. 잔여 위험

- 계약이 문서일 뿐이라, GUI 가 지키지 않으면 위반은 조용한 메모리 오류로 나타난다(GUI-C-186b 의 결과는 확인하지 않았다).
- 시험이 보는 것은 "delete 직전 mutex 가 잠겨 있지 않다" 는 순서이다. 이후 `delete` 자체의 정상 동작(이중 해제 없음 등)은 반복 시험이 무사히 100회 도는 것으로만 뒷받침된다.
