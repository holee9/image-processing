# QA-B-178b — 수명 계약의 적용 대상 (Codex #28 B)

Refs #130

## 1. 주장

| # | 주장 |
|---|------|
| C1 | LIFECYCLE CONTRACT 는 접두사 `xpe_ai_*` 가 아니라 **이 헤더가 선언하는 모든 함수**를 대상으로 한다. |
| C2 | "Reentrant"/"Thread-safe" 가 적힌 각 함수 문서에 init/shutdown 과의 동시성은 해당 라벨이 덮지 않는다는 연결 한 줄이 있다. |
| C3 | 수출 함수 전수는 헤더 선언과 실제 DLL 수출 목록 두 방법이 같은 11개를 낸다. |
| C4 | 시험 전용 수출 3개는 `XPE_AI_TEST_HOOKS=OFF` DLL 의 수출 목록에 없다 (실행 출력). |

## 2. 정정: 내 계약 문구가 틀렸다

QA-B-178 에서 "`xpe_ai_*` 호출과 동시에 실행될 수 없다" 고 썼다. Codex 의 지적이 맞다. `xpe_bone_suppress`, `xpe_dl_denoise`, `xpe_bodypart_recognize`, `xpe_stitch_*` 는 접두사가 다르고, 접두사로 읽은 호출자는 그것들을 직렬화 밖에 둔다. 특히 `xpe_bone_suppress` 는 "Reentrant" 라 적혀 있었다. **같은 보고서의 3.5 표에 그 함수들이 이미 적혀 있었는데도** 계약 문구는 그 표를 반영하지 않았다 — 표를 만든 사람이 문구를 쓴 사람이었다는 점이 이 실수를 더 나쁘게 만든다. 문구를 쓴 뒤 표와 한 줄씩 대조하지 않았다.

## 3. 전수 (두 방법)

### 3.1 방법 A: 헤더 선언

`ai_api.h` 에서 `XPE_API` 로 선언된 함수 11개:
`xpe_ai_version`, `xpe_ai_init`, `xpe_ai_shutdown`, `xpe_bodypart_recognize`, `xpe_stitch_images`, `xpe_stitch_estimate_size`, `xpe_bone_suppress`, `xpe_dl_denoise`, `xpe_ai_get_model_card`, `xpe_ai_set_fallback_mode`, `xpe_ai_worker_state`.

### 3.2 방법 B: DLL 수출 목록 (`dumpbin /exports`, 실행 출력)

| 빌드 | 수출 개수 | 증거 |
|------|-----------|------|
| 기본(`XPE_AI_TEST_HOOKS` ON, ci-post) | 14 | `dll_exports_on.txt` |
| `-DXPE_AI_TEST_HOOKS=OFF` (별도 빌드 디렉터리, `xpe_ai` 만 빌드) | 11 | `dll_exports_off.txt` |

대조(스크립트): **헤더 선언 집합 == OFF 수출 집합** (참). ON − OFF = `xpe_ai_test_set_before_state_delete_hook`, `xpe_ai_test_set_log_capture`, `xpe_ai_test_set_mutex_held_hook` (정확히 시험 전용 3개), OFF − ON = 공집합. 즉 두 방법이 같은 집합이고, 납품 구성(OFF)에는 시험 전용 수출이 없다.

### 3.3 모듈 상태 접근 (`ai.cpp` 함수 본문, 중괄호 대응으로 추출)

| 수출 함수 | 상태 접근 | 헤더 동시성 표기 | 계약 대상 | 연결 한 줄 |
|-----------|-----------|------------------|-----------|------------|
| `xpe_ai_init` | `g_aiState` 쓰기 | Not thread-safe | 이 계약의 주체 | (참조 문구 기존) |
| `xpe_ai_shutdown` | `g_aiState` 읽기·삭제 | Not thread-safe | 이 계약의 주체 | — |
| `xpe_ai_version` | 없음 | Thread-safe | 포함 | 반환 문서에 |
| `xpe_bodypart_recognize` | `checkInitialized()` 가 `g_aiState` 읽음 | Reentrant | 포함 | 추가 |
| `xpe_stitch_images` | `checkInitialized()` | Reentrant | 포함 | 추가 |
| `xpe_stitch_estimate_size` | 없음 (본문에 `g_aiState`·`checkInitialized` 없음) | Reentrant | 포함 | 추가 |
| `xpe_bone_suppress` | `g_aiState` 읽은 뒤 잠금 | Reentrant | 포함 | 추가 |
| `xpe_dl_denoise` | `checkInitialized()` | Reentrant | 포함 | 추가 |
| `xpe_ai_get_model_card` | `g_aiState` 읽은 뒤 잠금 | Thread-safe | 포함 | 추가 |
| `xpe_ai_set_fallback_mode` | `g_aiState` 읽은 뒤 원자 저장 | Thread-safe | 포함 | 추가 |
| `xpe_ai_worker_state` | `g_aiState` 잠금 없이 읽기 | 이미 "MUST NOT run concurrently with init/shutdown" | 포함 | 기존 명시 |

Codex 가 든 목록 중 `xpe_stitch_estimate_size` 는 코드상 지금은 모듈 상태를 읽지 않는다(독립 확인). `xpe_ai_version` 도 읽지 않는다. 그래도 계약 대상에 넣었다: 상태를 읽는지가 구현 세부이면 계약이 구현에 의존하게 되기 때문이다(헤더에 그렇게 적음).

## 4. 변경

- `ai_api.h` 계약 문단: "모듈 상태를 읽는 다른 함수" 와 "이 헤더가 선언하는 어떤 함수든 — 경계는 접두사가 아니라 헤더" 로, 함수 9개를 이름으로 나열.
- "Reentrant"/"Thread-safe" 라벨이 있는 함수 문서 7곳(`bodypart_recognize`, `stitch_images`, `stitch_estimate_size`, `bone_suppress`, `dl_denoise`, `get_model_card`, `set_fallback_mode`)에 "Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the LIFECYCLE CONTRACT at xpe_ai_shutdown()" 두 줄 연결. `xpe_ai_version` 은 반환 문서에, `xpe_ai_worker_state` 는 이미 명시.
- QA-B-178 보고서의 같은 문구에 정정 표시, 3.5 표는 이 보고서로 대체했음을 표시.
- 코드 변경 없음(주석만).

## 5. 결과

| 구성 | 결과 |
|------|------|
| ci-post | `100% tests passed, 0 tests failed out of 1014` (`after_full_ci_post_ctest.txt`) |
| ci-ai | `100% tests passed, 0 tests failed out of 368` (`after_full_ci_ai_ctest.txt`) |
| 경고 | 두 빌드 로그 `warning C` 0건 |

## 6. Gaps (미검증)

- 연결 한 줄이 7곳 모두에 들어갔는지는 삽입 개수(7)로만 확인했다. 헤더 문서를 렌더링해 보지는 못했다(Doxygen 미설치).
- OFF 수출 목록은 `xpe_ai` 타깃만 빌드한 DLL 이다. 납품용 전체 빌드(릴리스 프리셋)를 새로 만들어 보지는 않았다. 릴리스 프리셋이 OFF 를 켜는지는 리더 소관이며 확인하지 않았다.
- 계약이 문서라는 한계는 QA-B-178 과 같다. 호출자(GUI)가 지키는지는 확인하지 않았다.
- 전수는 헤더 선언과 DLL 수출 두 방법으로 했고, 모듈 상태 접근 열은 `ai.cpp` 본문 검색 한 방법이다(`state->`, `g_aiState`, `checkInitialized(` 의 존재). 다른 헬퍼를 거친 간접 접근은 이 방법으로 보이지 않을 수 있다.
