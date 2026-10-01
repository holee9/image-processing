# QA-B-181 — post 모듈 C ABI 예외 탈출 수정 (QA-B-179 의 1~3순위)

Refs #233 (#130 과 같은 부류)

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `xpe_bone_suppress`, `xpe_ai_get_model_card` 는 예외를 밖으로 내보내지 않고, 잠금을 쥔 구간에서 예외가 나도 잠금이 풀린다. 변환할 수 없는 경로는 예외가 아니라 `XPE_ERR_IO_FAILED` 로 걸러진다. |
| C2 | `xpe_noise_reduce`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_gsvg_process`(+`_masked`, `_ex`) 는 QA-B-179 가 실측한 입력에서 예외 대신 오류 코드를 돌려준다. |
| C3 | `xpe_enhance_advanced_init` 은 같은 가드를 갖는다. "잠금이 풀리는 이유" 가설은 실험으로 확인했다(4절). |
| C4 | 각 가드를 빼면 해당 시험만 빨강이 된다. |

## 2. 변경 요약

| 대상 | 변경 | 오류 코드 |
|------|------|-----------|
| `xpe_bone_suppress` | 본문을 `extern "C++"` 함수 `_impl` 로 옮기고, 수출 함수는 `try/catch` 만 (잠금은 `_impl` 안에 남아 예외가 지나갈 때 풀림) | `bad_alloc` → `XPE_ERR_OUT_OF_MEMORY`, 그 외 → `XPE_ERR_PROCESSING_FAILED` |
| `OnnxSession` 의 `FileExists` | 경로 변환과 `fs::exists` 를 `error_code` 형으로, 변환 실패(`std::system_error`)는 `false` → 기존 `kInvalidModelPath` 경로 | `XPE_ERR_IO_FAILED` (모델이 없을 때와 같음) |
| `xpe_ai_get_model_card` | 같은 구조(`_impl` + 가드) | 위와 같음 |
| `enhance_basic` 3개 | 같은 구조, 가드는 헤더 인라인 `xpe_guarded_call`. `validate_float32_image` 에 "너비·높이는 INT32_MAX 이하" 추가. `contrast_enhance` 의 창 검사와 타일 수를 64비트로 | `INVALID_INPUT`, `OUT_OF_MEMORY` |
| `gsvg` 3개 | 가드 `guarded_call`(익명 네임스페이스 C++ 템플릿)로 `process_impl` 호출을 감쌈. 두 배정밀도 작업 영상이 표현 불가능한 크기(`count > PTRDIFF_MAX / sizeof(double)`)는 입력 오류 | `INVALID_INPUT`, `OUT_OF_MEMORY` |
| `xpe_enhance_advanced_init` | 같은 구조(`XpeAdvGuardedCall`, `internal.h`) | 위와 같음 |
| 시험 전용 | `xpe_ai_test_set_mutex_held_hook` 을 `xpe_ai_get_model_card` 에서도 호출(주석 갱신). 납품 구성(`XPE_AI_TEST_HOOKS` OFF)에는 없음 | — |

### 2.1 변환할 수 없는 경로를 왜 `IO_FAILED` 로 보냈나

호출자의 처지는 "모델 파일이 있다고 하는 경로로 갔지만 파일을 쓸 수 없다" 로 같다. 지금도 존재하지 않는 디렉터리는 `XPE_ERR_IO_FAILED`(-9)를 돌려주므로(실측, 5/5), 경로가 아예 이름이 될 수 없는 경우도 같은 코드로 모은다. 호출자가 구분할 필요가 있는 시나리오가 없고, `XPE_ERR_INVALID_INPUT` 은 `xpe_ai_init` 가 경로를 받아들이고 나서 뒤늦게 돌려주면 오히려 `init` 의 계약(경로 존재를 검사하지 않음)과 어긋난다. `xpe_ai_init` 에서 거르는 안은 계약을 바꾸는 일이라 택하지 않았다. `bad_alloc` 은 "없는 파일" 이 아니므로 삼키지 않고 수출 함수의 가드가 `OUT_OF_MEMORY` 로 바꾼다.

## 3. 만든 시험과 반증 (C4)

| 모듈 | 시험 파일 | 개수 | 대표 시험 |
|------|-----------|------|-----------|
| ai | `test_ai_exception_guard.cpp` | 6 | 시험 훅이 **던지는** 예외를 잠금 구간 안에 둠(할당기·코드 페이지와 무관) → 반환 코드, 출력 불변, `xpe_ai_shutdown` 이 5초 안에 돌아옴(잠금 해제). 변환 불가 경로 시험은 이 프로세스의 코드 페이지에 변환 불가 후보가 있을 때만 실행, 없으면 사유를 적고 건너뜀 |
| enhance_basic | `test_exception_guard.cpp` | 11 | 가드 4(예외 종류별), NLM 선언 크기, 쌍방향·edge 너비 2^31, edge 행 버퍼, contrast 타일 오버플로, 경계, 모듈 공통 INT32_MAX |
| gsvg | `test_exception_guard.cpp` | 5 | 3개 진입점 × (표현 불가 크기, 격자 억제 `bad_alloc`, 가상 격자 `bad_alloc`), 이후 정상 호출, 정상 대형 프레임 |
| enhance_advanced | `test_exception_guard.cpp` | 4 | 가드 3 + 일반 입력과 잠금 자유 |

반증 (각 BUILD=0 확인, 해당 시험만 빨강):

| 반증 | 조작 | 빨강이 된 시험 | 파일 |
|------|------|----------------|------|
| ai-1 | `xpe_bone_suppress`·`get_model_card` 의 가드 제거 | GuardFixture 4개 | `arm_ai_noguard.txt` |
| ai-2 | `FileExists` 를 원래의 던지는 형으로 | 변환 불가 경로 2개 | `arm_ai_throwing_fileexists.txt` |
| eb-1 | `xpe_guarded_call` 의 `catch` 와 `noexcept` 제거 | 가드 4 + NLM + edge 행 버퍼 | `arm_eb_noguard.txt` |
| eb-2 | INT32_MAX 검사 제거 | 쌍방향·edge·모듈 공통 3개 | `arm_eb_no_intmax_check.txt` |
| eb-3 | contrast 의 int 곱셈 복원 | 타일 오버플로 1개 | `arm_eb_old_int_window.txt` |
| gs-1 | `guarded_call` 제거 | `bad_alloc` 2개 | `arm_gs_noguard.txt` |
| gs-2 | 표현 가능 크기 검사 제거 | 표현 불가 크기, 이후 정상 호출 | `arm_gs_no_precheck.txt` |
| ea-1 | `XpeAdvGuardedCall` 의 `catch` 제거 | 가드 2개 | `arm_ea_noguard.txt` |

## 4. 측정으로 드러난 두 가지 (설계를 바꾼 것)

### 4.1 `/EHsc` 에서 `extern "C"` 함수의 잠금이 남는 조건 (`eh_experiment_output.txt`)

카드의 "잠금이 풀리는 이유 가설(함수 안 try 유무)을 코드로 확인" 항목. 스크래치 실험(`/O2`, 던지는 일은 호출된 함수가 하고 호출은 함수 포인터로)에서:

| 구성 | `/EHsc` | `/EHs` | `/EHa` |
|------|---------|--------|--------|
| A: 수출 함수에 try 없음 | **잠금 남음** | 풀림 | 풀림 |
| B: 수출 함수 안에 무관한 타입의 try/catch | 풀림 | 풀림 | 풀림 |
| C: 잠금은 C++ 도우미가 쥐고 `catch(...)` 래퍼 | 풀림 | 풀림 | 풀림 |

**가설이 맞았다**: `/EHsc` 에서는 수출(`extern "C"`) 함수에 예외 처리 영역이 하나도 없으면 컴파일러가 "던지지 않는다" 고 가정해 `lock_guard` 의 소멸자를 해제 정보에서 뺀다. `xpe_enhance_advanced_init` 의 잠금이 풀린 것은 그 함수에 (json 예외용) `try` 가 있었기 때문이고, `xpe_ai_get_model_card`/`xpe_bone_suppress` 에는 없었기 때문에 남았다. 따라서 "try 가 하나라도 있으면 안전" 이 아니다: 그 try 가 못 받은 예외도 밖으로 나간다(EA init 은 `bad_alloc` 이 실제로 탈출했다). 해법은 `/EHs` 로 바꾸는 것이 아니라(빌드 전체 변경) 잠금을 C++ 함수가 쥐고 수출 함수는 가드만 두는 구조 C 이다. (스크래치에서 수출 함수 *안에서 직접* `throw` 하면 컴파일러가 `terminate` 로 바꾸므로(C4297), 실험은 호출된 함수가 던지게 했다. 이 실험의 코드는 저장소에 넣지 않았다.)

### 4.2 가드를 `extern "C"` 블록 안의 함수로 만들면 소용이 없다 (`before_extern_cxx_fix_run.txt`)

첫 구현은 `_impl` 을 `extern "C" { … }` 블록 **안에** `static` 함수로 두었다. 결과: 시험이 `C++ exception "bad allocation" thrown in the test body` 로 실패하고, 뒤이어 `resource deadlock would occur`(같은 스레드가 잠금을 다시 잡으려 함 = 잠금이 남음)가 났다. 블록 안에 선언한 함수는 `static` 이어도 C 언어 연결을 갖고 같은 "던지지 않음" 취급을 받아, 호출 쪽의 `catch` 가 제거되고 `_impl` 의 `lock_guard` 도 풀리지 않는다. `_impl` 을 **`extern "C++"`** 로 선언하자 해결됐다. 같은 이유로 `enhance_basic` 은 헤더 인라인 템플릿(블록 밖), `gsvg` 는 익명 네임스페이스 템플릿, `enhance_advanced` 는 `internal.h` 템플릿을 쓴다.

## 5. 결과

### 5.1 프로브 (수정 전 → 후)

한 프로세스에 한 시나리오, 입력 시나리오 5회·소진 시나리오 10회 (`probe_after_fix_*.txt`, 수정 전은 QA-B-179 `probe_results_*.txt` 와 `before_fix_gsvg_probe.txt`):

| 시나리오 | 수정 전 | 수정 후 |
|----------|---------|---------|
| ai: 코드 페이지에서 변환 불가한 모델 경로 → `xpe_bone_suppress` | `std::system_error` 탈출 + `shutdown` 5초 넘게 안 돌아옴 | `-9`(IO_FAILED), `LOCK_RELEASED` |
| enhance_basic: contrast 타일 오버플로 | `length_error` 탈출 | `-1`(INVALID_INPUT) |
| enhance_basic: NLM 선언 크기 `1<<20` 제곱 | `bad_alloc` 탈출 | `-2`(OUT_OF_MEMORY) |
| enhance_basic: edge, 쌍방향 너비 `2^31` | `length_error` 탈출 | `-1` |
| gsvg: 격자 억제 INT_MAX 제곱 (process / masked / ex) | `length_error` 탈출 (세 개 모두, masked·ex 는 이번에 실측) | `-1` |
| gsvg: 가상 격자 INT_MAX 제곱 | `bad_alloc` 탈출 | `-1` |
| gsvg: 격자 억제 `1e9` 제곱 (process / masked / ex) | `bad_alloc` 탈출 | `-2` |
| 소진: ai `get_model_card` | 탈출 + 잠금 영구 잔류 | `-2`, `LOCK_RELEASED` |
| 소진: enhance_advanced `init` | `bad_alloc` 탈출 (잠금은 풀림) | `-2`, `LOCK_RELEASED` |
| 소진: enhance_basic `contrast_enhance` (정상 크기 입력) | `bad_alloc` 탈출 | `-2` |
| 대조(접근 거부·초과 길이 디렉터리, 정상 contrast·gsvg, dicom) | 던지지 않음 | 같음 |

위 소진 3건은 DLL 안에서 **실제로 할당이 실패**한 end-to-end 결과이다. 저장소의 시험이 아니라 일회용 프로브(저장소 밖)이다.

### 5.2 전체 스위트

| 구성 | 결과 |
|------|------|
| ci-post | `100% tests passed, 0 tests failed out of 1040` (이전 1014 + 신규 26) (`after_full_ci_post_ctest.txt`) |
| ci-ai | `100% tests passed, 0 tests failed out of 374` (`after_full_ci_ai_ctest.txt`) |
| 경고 | 두 빌드 로그 `warning C` 0건 |

## 6. 동작 변화 (호출자가 보는 것)

- 너비·높이가 INT32_MAX 를 넘는 영상은 `enhance_basic` 전 함수에서 `XPE_ERR_INVALID_INPUT` (이전: 정의되지 않은 동작 또는 예외).
- `xpe_contrast_enhance`: 창 검사가 64비트라 `tile_width >= 2^30` 도 정상적으로 거절된다. 경계(`tile * 2 == 크기`)는 그대로 유효이고 시험이 고정한다.
- `gsvg`: 픽셀 수가 `PTRDIFF_MAX/8` 를 넘으면 `INVALID_INPUT`. 그 아래에서 할당이 안 되면 `OUT_OF_MEMORY` (이전: 예외).
- `xpe_bone_suppress`: 변환할 수 없는 모델 경로는 `XPE_ERR_IO_FAILED`.
- 가드 안에서 예외가 나면 `dst`(gsvg)는 첫 패스 이후 복사·보정된 화소를 이미 담고 있을 수 있다(다른 중간 오류와 같다).

## 7. Gaps (미검증)

- `xpe_contrast_enhance` 의 가드 연결은 DLL 크기에서 결정적으로 실패시킬 입력이 없어(출력·타일 할당 전에 영상 전체를 읽음) **가드 헬퍼를 직접 시험**했고(4개), 수출 함수가 그 헬퍼에 연결되어 있다는 것은 같은 헬퍼를 쓰는 `noise_reduce`·`edge_enhance` 의 end-to-end 시험과 소진 프로브(`alloc_enh_clahe`, 정상 크기 입력에서 `-2`)로만 뒷받침된다.
- `xpe_enhance_advanced_init` 안의 할당 실패는 시험으로 주입할 수 없어(JSON 파서 안, DLL 자체 할당기) 가드 헬퍼 시험 + 일회용 소진 프로브 10/10 으로 뒷받침한다. 저장소 시험은 일반 입력 처리와 잠금 자유만 확인한다.
- QA-B-177 의 `operator new` 교체 방식은 시험 exe 가 컴파일한 코드에만 닿는다. 이번 대상은 DLL 안이라 그 방식을 쓰지 않았고, 잠금 구간 안의 예외는 훅이 던지게 했다(ai), 크기 기반 실패는 실제 할당 실패를 썼다(나머지).
- `ai_ipc_bridge`, 워커 경로, ONNX 전체 구성(`Ort::` 예외)의 새 가드 효과는 확인하지 않았다(ci-ai 는 통과했으나 이 경로를 겨냥한 시험은 아님).
- 변환 불가 경로 시험은 ACP 가 변환 불가 후보를 가진 기계에서만 실행된다. CI(영문 코드 페이지일 가능성)에서는 건너뛰고, 그 경우 같은 가드의 증거는 훅 기반 시험이다. 건너뛰었다는 것이 시험 목록에 `SKIPPED` 로 남는다.
- 범위 밖으로 남긴 것: `ForRows` 스레드 생성 실패의 `std::terminate`(5순위), `xpe_noise_estimate_sigma`·`xpe_ai_init`·`xpe_gsvg_init` 등 나머지 할당 실패(6순위), `enqueue_alert`(pre 레인).

## 8. 잔여 위험

- 가드는 예외를 오류 코드로 바꿀 뿐 중간 상태를 되돌리지 않는다. `xpe_bone_suppress` 의 예외가 워커 경로 상태 갱신 중에 났다면 연속 실패 카운터 등이 갱신되지 않은 채 남는다(이 카드에서 확인하지 않음).
- `XPE_ERR_OUT_OF_MEMORY` 와 `XPE_ERR_PROCESSING_FAILED` 를 호출자(GUI)가 구분해 처리하는지는 확인하지 않았다.
