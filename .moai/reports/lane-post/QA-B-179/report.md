# QA-B-179 — post 모듈 수출 C ABI 함수의 예외 탈출 전수 조사 (조사만, 수정 없음)

Refs #233 (#130 과 같은 부류). 제품 코드·시험 코드 변경 없음. 프로브 코드는 저장소에 넣지 않았다(저장소 밖 임시 디렉터리).

## 1. 결론

수출 함수 54개를 조사했다.

| 분류 | 함수 수 | 비고 |
|------|---------|------|
| 입력만으로 예외가 밖으로 나감 | **7** | 실측 5개 + 같은 본문을 공유하는 코드 읽기 2개 |
| 할당 실패 때만 (최외곽 가드 없음) | 6 | 실측 2개(+ clahe 대표 1회), 코드 읽기 4개 |
| 가드 있음 (최외곽 try/catch) | 14 | dicom 10, enhance_advanced 4 |
| 던질 연산이 없음 | 27 | ai 8, enhance_basic 5, enhance_advanced 4, gsvg 4, display 6 |

**잠금을 쥔 채 던져서 잠금이 영구히 남는 것을 실측으로 확인한 경로가 둘이다**: `xpe_bone_suppress`(입력 유발)와 `xpe_ai_get_model_card`(할당 실패). 둘 다 이후 `xpe_ai_shutdown` 이 5초 넘게 돌아오지 않았다.

## 2. 방법

1. **수출 목록 두 축**: 헤더의 `XPE_API` 선언 검색과 `dumpbin /exports` 실행 출력(`dll_exports_*.txt`). 모듈별 개수가 일치했다 (ai 11, enhance_basic 10, enhance_advanced 9, gsvg 8, dicom 10, display 6 = 54). ai 의 DLL 에는 시험 전용 수출 3개(`xpe_ai_test_*`)가 더 있고 이는 `XPE_AI_TEST_HOOKS=OFF` 납품 구성에는 없다(QA-B-178b 에서 확인). 이번 조사에서는 제품 54개만 센다. 빌드: `ci-post`(ai 는 stub 구성), dicom 은 `ci-dicom`.
2. **코드 읽기**: 모듈별 읽기 전용 에이전트 5개가 호출 트리를 따라가며 던질 수 있는 연산, 최외곽 가드, 잠금을 조사했다. **그 보고를 그대로 쓰지 않고** 아래 3 의 실측과 대조했다. 대조에서 에이전트의 주장 둘이 틀린 것으로 드러났다(5절).
3. **입력만으로 던지는 경로 실측**: 함수·시나리오마다 별도 프로세스에서 일회용 프로브 exe 가 DLL 을 `LoadLibrary` 로 읽고 한 번 호출한다. C++ `catch(...)` 로 예외 탈출 여부와 형식을 읽고, 프로세스가 죽으면 종료 코드를 읽는다. 시나리오당 5회(`probe_results_input_paths.txt`). ai 는 호출 뒤 다른 스레드에서 `xpe_ai_shutdown` 을 불러 5초 안에 돌아오는지로 모듈 잠금 상태를 본다.
4. **할당 실패 실측**: 대표 3개만. 프로세스 커밋 한도(작업 객체)를 현재 사용량 + 4 MB 로 두고 여유분을 `malloc` 으로 채운 뒤, 정상 크기의 유효한 입력으로 한 번 호출한다. 시나리오당 10회(`probe_results_alloc_exhaustion.txt`).

## 3. 결과 표

열: 함수 · 분류 · 근거 · 방법(실측/코드 읽기) · 잠금 보유. 분류의 "해당 없음"은 던질 연산이 호출 트리에 없다는 뜻이다. 코드 읽기 근거는 `파일:줄` 이며 줄은 이 커밋 시점이다. "AI_LOG_*" 는 QA-B-177/177b 이후 `noexcept` 라 안전으로 본다.

### 3.1 ai (11)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_ai_version` | 해당 없음 | 문자열 상수 반환 (`ai.cpp`) | 코드 읽기 | 아니오 |
| `xpe_ai_init` | 할당 실패 때만 | 최외곽 가드 없음. `std::string` 복사(`ai.cpp:541`), `loadedModels` 초기화(`:554-559`), `xpe_alert_push`(`:321`). `nothrow new`(`:537`)와 nlohmann 파싱은 예외 없는 형태(`parse(..., false)`) | 코드 읽기 | 모듈 잠금은 없음. `xpe_alert_push` 안에서 common 의 `g_mutex` (`xpe_common.cpp:152`) |
| `xpe_ai_worker_state` | 해당 없음 | 원자 읽기만, 의도적으로 잠금 없음 | 코드 읽기 | 아니오 |
| `xpe_ai_shutdown` | 해당 없음(제품 빌드) | 잠금 범위가 `delete` 전에 끝남(QA-B-178). 시험 전용 `std::thread` 는 TEST_HOOKS 에만 | 코드 읽기 | 잠금 안에서 던질 연산 없음 |
| `xpe_bodypart_recognize` | 해당 없음 | 검증과 `memcpy` 뿐 | 코드 읽기 | 아니오 |
| `xpe_stitch_images` | 해당 없음 | 검증 루프뿐 | 코드 읽기 | 아니오 |
| `xpe_stitch_estimate_size` | 해당 없음 | 산술뿐 | 코드 읽기 | 아니오 |
| **`xpe_bone_suppress`** | **입력만으로 던짐** | 프로세스의 ANSI 코드 페이지(949)에서 변환할 수 없는 바이트가 든 모델 디렉터리(`C:\xpe_probe_` + `C3 28`)로 `xpe_ai_init` 한 뒤 호출하면 `std::system_error`("No mapping for the Unicode character exists in the target multi-byte code page") 가 밖으로 나가고 **모듈 잠금이 남는다**. 던지는 지점은 `OnnxSession::Create` 의 경로 처리(감사: `FileExists`, `ai_onnx_session.cpp:109` 의 `fs::exists(path)`, 예외 없는 `error_code` 형이 아님). 잠금 안의 할당 경로도 다수(`ai.cpp:931-964`) | **실측**(형식·잠금) + 코드 읽기(정확한 줄) | **예** `state->mtx` (`ai.cpp:854` 부근). 실측: `LOCK_STUCK` 5/5. 워커 경로는 도우미의 `catch(...)`(`ai.cpp:411-425`)가 막음 |
| `xpe_dl_denoise` | 해당 없음 | 검증뿐 | 코드 읽기 | 아니오 |
| **`xpe_ai_get_model_card`** | **할당 실패 때만** | `buildStubModelCard` 와 문자열 연결이 잠금 안(`ai.cpp:1055` 이후) | **실측**: 할당 소진 시 `bad_alloc` 이 밖으로, 이후 `xpe_ai_shutdown` 이 5초 넘게 안 돌아옴, 10/10 | **예** `state->mtx` (**잠금 영구 잔류 실측**) |
| `xpe_ai_set_fallback_mode` | 해당 없음 | 원자 저장 + `AI_LOG_INFO`(안전) | 코드 읽기 | 아니오 |

### 3.2 enhance_basic (10)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_enhance_basic_version`, `_set_max_threads`, `_get_max_threads` | 해당 없음 | 상수 / 원자 | 코드 읽기 | 아니오 |
| `xpe_log_transform`, `xpe_log_inverse` | 해당 없음 | 제자리 루프, 할당 없음 | 코드 읽기 | 아니오 |
| **`xpe_noise_reduce`** | **입력만으로 던짐** | NLM: `width=height=1<<20`, `dataSize=0`(검증이 통과시킴) → `output(n)` 에서 `std::bad_alloc` (`noise_reduce.cpp:236`). 쌍방향 필터: `width=0x80000000`, `height=1` → `int` 부호 전환으로 `ksize*w` 가 거대해져 `std::length_error` (`noise_reduce.cpp:105`). 추가(코드 읽기): 작업 스레드 안의 할당 실패와 `std::thread` 생성 실패는 `std::terminate`(`parallel_rows.h:62-69`) | **실측** (NLM·쌍방향 각 5/5) + 코드 읽기(스레드) | 아니오 |
| `xpe_noise_estimate_sigma` | 할당 실패 때만 | ROI 벡터 `reserve/push_back` (`noise_reduce.cpp:352-370`), ROI 는 한 변의 10% | 코드 읽기 | 아니오 |
| **`xpe_contrast_enhance`** | **입력만으로 던짐** | `tile_width=0x40000000`, `tile_height=2`, 8×8 비평탄 float → `int` 곱 오버플로로 `std::vector<TileLut> tiles` 가 거대해져 `std::length_error` (`contrast_enhance.cpp:160-161`). 검증(`:126`, `:135`)은 곱을 제한하지 않음. 정상 크기(256×256) 입력도 할당 소진 시 `bad_alloc` 이 탈출 | **실측**: 오버플로 5/5, 소진 10/10 | 아니오 |
| **`xpe_edge_enhance`** | **입력만으로 던짐** | `width=0x80000000`, `height=1`, `dataSize=0` → `static_cast<int>(width)` 가 음수가 되어 `ring(ksize*w)` 가 거대해져 `std::length_error` (`edge_enhance.cpp:73`, `:117`) | **실측** 5/5 (에이전트는 "의심" 으로 적었으나 실측으로 확정) | 아니오 |
| `xpe_calc_exposure_index` | 할당 실패 때만 | 모듈 자신의 코드는 던지지 않음. `|DI|>3` 일 때 `xpe_alert_push`(`exposure_index.cpp:146`)가 common 의 `g_mutex` 를 잡은 채 `std::string` 대입과 `push_back` (`xpe_common.cpp:152-169`) | 코드 읽기 | **예, 간접** common `g_mutex` |

### 3.3 enhance_advanced (9)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_enhance_advanced_version`, `_set_max_threads`, `_get_max_threads`, `_shutdown` | 해당 없음 | 상수/원자/`bool` 저장(`shutdown` 은 `lock_guard` 뿐) | 코드 읽기 | `shutdown` 만 (`xpe_enhance_advanced.cpp:77`), 안에서 던질 연산 없음 |
| `xpe_enhance_advanced_init` | 할당 실패 때만 (부분 가드) | `try` 가 있으나 `nlohmann::json::exception` 만 받음(`:57-61`) → `bad_alloc` 은 그대로 나감. **실측: 할당 소진 시 `bad_alloc` 탈출 10/10, 그러나 이후 `init` 재호출은 돌아옴 (`LOCK_RELEASED` 10/10)** | **실측** | 예 `g_initMutex` (`:48`), **그러나 잠금은 풀렸다(감사의 "영구 잔류" 주장은 실측으로 기각)** |
| `xpe_multiscale_process` | 가드 있음 | `try` `multiscale_process.cpp:76`, `catch(std::exception)` `:143`, `catch(...)` `:147` | 코드 읽기 | 잠금은 `g_initialized` 읽기 구간뿐 |
| `xpe_fractional_process` | 가드 있음(약한 곳 있음) | `try` `:83`, `catch(...)` `:171`. 단 `catch(runtime_error)` 본문의 `std::string msg = e.what()` (`:160`)가 핸들러 안에서 할당, `ForRows` 의 스레드 실패는 `terminate`(`parallel_rows.h:62-69`) | 코드 읽기 | 아니오 |
| `xpe_detect_collimation` | 가드 있음 | `try` `collimation_detect.cpp:89`, `catch(...)` `:257` | 코드 읽기 | 아니오 |
| `xpe_adv_calc_exposure_index` | 가드 있음 | `try` `:166`, `catch(...)` `:172`, 잠금은 같은 함수 안에서 해제됨 | 코드 읽기 | 예 `:114`, 정상 해제 |

### 3.4 gsvg (8)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_gsvg_version`, `_get_max_threads`, `_set_max_threads`, `_shutdown` | 해당 없음 | 상수/원자/`delete` (소멸자는 벡터 해제뿐) | 코드 읽기 | 아니오 |
| `xpe_gsvg_init` | 할당 실패 때만(+ 입력 크기 의존) | 최외곽 가드 없음(모듈 전체에서 `try/catch` 0건). 설정 문자열 복사, 표 파일 `rdbuf` 읽기(`virtual_grid.cpp:304-310`), `xpe_alert_push`(`gsvg.cpp:137`, `:241`). 예외가 아닌 결함: 표의 `[kernels]` 가 선택 모델과 전부 불일치하면 빈 벡터의 `back()`(`virtual_grid.cpp:291`)으로 충돌 | 코드 읽기 | 간접 common `g_mutex` |
| **`xpe_gsvg_process`** | **입력만으로 던짐** | 격자 억제(`{"grid_suppression":true}`), `src==dst`, `width=height=2147483647`, `count=SIZE_MAX` → `std::length_error` (`grid_dwt.cpp:420`). 가상 격자 설정 → `std::bad_alloc` (`gsvg.cpp:417`). 검증은 개수의 상한이나 실제 버퍼 크기를 보지 않는다(`gsvg.cpp:384-411`) | **실측** (격자 억제 5/5, 가상 격자 5/5, 대조 5/5 정상) | 아니오 (`xpe_alert_push` 경유 시 common `g_mutex`) |
| `xpe_gsvg_process_masked`, `xpe_gsvg_process_ex` | 입력만으로 던짐 | `process_impl` 을 `xpe_gsvg_process` 와 공유 (`gsvg.cpp:367-496`, 호출 `:514-548`) | **코드 읽기만**(개별 실측 안 함) | 위와 같음 |

참고: 표 파일 값에 따른 `GaussTaps` 크기 폭주(`virtual_grid.cpp:422-424`)와 `set_max_threads(INT_MAX)` + 키 큰 영상의 스레드 생성 실패는 코드 읽기로 가능성만 확인했다(6절).

### 3.5 dicom (10)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_dicom_open`, `_read_image`, `_get_metadata`, `_close`, `_write`, `_write_j2k`, `_validate`, `_cstore`, `_cfind_mwl`, `_cancel` | 가드 있음 (10/10) | 각 함수 본문 전체가 `try{…}catch(...)` (`dicom.cpp:42/48, 57/59, 68/70, 78/80, 128/130, 143/145, 160/162, 179/181, 196/199, 207/209`). 모듈 전체에 `mutex`/`lock_guard` 없음 | 코드 읽기 + **실측 1건**: `xpe_dicom_write_j2k` 에 `width=height=0xFFFFFFFF`, `dataSize=0` → 던지지 않고 **-3(PROCESSING_FAILED) 반환** (5/5), 가드가 실제로 동작 | 아니오 |

가드 바깥에 남는 것: 각 함수 첫머리의 `spdlog::debug(...)`(`dicom.cpp:39, 55, 66, 77, 124, 139, 158, 177, 194, 206`)가 `try` 밖이다. 인자는 리터럴과 포인터라 입력이 만들 수 없다. 던질 가능성은 spdlog 내부 구현에 달려 있어 확인하지 않았다. `open` 은 예외 시 핸들이 새는 결함이 있으나 탈출 경로는 아니다.

### 3.6 display (6)

| 함수 | 분류 | 근거 | 방법 | 잠금 보유 |
|------|------|------|------|-----------|
| `xpe_display_version`, `xpe_apply_modality_lut`, `xpe_apply_voi_lut`, `xpe_voi_preset_create`, `xpe_apply_presentation_lut`, `xpe_gsdf_calibrate` | 해당 없음 (6/6) | 소스에 `<vector>`·`<string>`·`new`·`std::thread` 가 없고, 메모리 함수는 확인하는 `std::malloc`(`presentation_lut.cpp:32`) | 코드 읽기 | 아니오 |

예외가 아닌 결함(참고): `xpe_apply_modality_lut` 은 표 모드에서 `lutData` 길이가 `lutLength` 보다 짧은 경우를 검사하지 않아 `lut[idx]` 가 범위를 벗어날 수 있고(`modality_lut.cpp:64`), `xpe_apply_presentation_lut` 는 `lutData` 의 널을 검사하지 않는다(`presentation_lut.cpp:38, 48`). 둘 다 접근 위반이며 C++ 예외가 아니라 이 조사의 범위 밖이다.

## 4. 잠금을 쥔 채 던질 수 있는 함수

| 함수 | 잠금 | 확인 |
|------|------|------|
| `xpe_bone_suppress` | ai `state->mtx` | **실측: 입력 유발, 잠금 영구 잔류** |
| `xpe_ai_get_model_card` | ai `state->mtx` | **실측: 할당 실패, 잠금 영구 잔류** |
| `xpe_enhance_advanced_init` | `g_initMutex` | 실측: 던지지만 **잠금은 풀림** |
| `xpe_ai_init`, `xpe_calc_exposure_index`, `xpe_gsvg_init` (그리고 ai 의 `xpe_bone_suppress` 의 알림 호출들) | common `g_mutex` (`xpe_alert_push` → `enqueue_alert`, `xpe_common.cpp:152-169`) | 코드 읽기만. 던지면 알림 큐 전체가 막힌다. 이 함수 자체는 common 소관(#233 의 pre 조사 범위) |

**잠금이 풀리는 경우와 남는 경우의 차이는 측정으로 드러났다**: `xpe_enhance_advanced_init` 은 같은 함수 안에 `try` 가 있고 `xpe_ai_get_model_card`/`xpe_bone_suppress` 는 없다. 가설: 함수에 예외 처리 영역이 있으면 컴파일러가 해제(unwind) 정보를 내므로 `lock_guard` 가 풀리고, 없으면(`extern "C"` 는 던지지 않는다고 가정하는 `/EHsc`) 풀리지 않는다. **이 가설은 확인하지 않았다**(해제 정보를 직접 보지 않음). 관찰만 사실이다. 그러므로 "try 가 하나라도 있으면 안전" 이라는 결론은 내릴 수 없다.

## 5. 에이전트 보고 중 실측이 반박한 것

1. **ai 의 상위 후보 두 개가 틀렸다.** 에이전트는 접근 거부 디렉터리(`C:\System Volume Information\x`)와 32 767 자를 넘는 디렉터리를 1·2순위로 들었다. 실측에서 둘 다 던지지 않고 `-9` 를 반환했다(5/5, `LOCK_RELEASED`). 실제로 던지는 입력은 3순위(낮은 확신)로 적었던 "활성 코드 페이지에서 변환 불가능한 바이트" 였다. 이 입력은 **코드 페이지(여기서는 949)에 의존한다**. UTF-8 코드 페이지로 실행하면 같은 입력의 결과가 다를 수 있고 측정하지 않았다.
2. **`xpe_enhance_advanced_init` 의 "잠금 영구 잔류" 주장이 틀렸다.** 에이전트는 `g_initMutex` 가 풀리지 않는다고 적었다. 실측에서 `bad_alloc` 은 탈출하지만 이후 `init` 재호출이 돌아왔다(10/10).
3. 에이전트가 "의심" 이라고 쓴 `xpe_edge_enhance` 의 `width>=2^31` 은 **실측으로 확정**되었다.

## 6. 우선순위 제안

기준은 카드가 정한 대로 입력 유발 > 잠금 보유 > 할당 실패이다. 수정 방향은 한 줄씩이고 이 카드에서는 고치지 않았다.

| 순위 | 대상 | 이유 | 방향 |
|------|------|------|------|
| 1 | `xpe_bone_suppress` (+ `OnnxSession::Create` 경로 처리) | 입력 유발이면서 모듈 잠금이 영구히 남아 `shutdown`/후속 호출이 멈춤. 경로 문자열은 호출자(GUI)가 정한다 | 최외곽 `try/catch(...)` → 오류 코드(`OUT_OF_MEMORY` 또는 `IO_FAILED`), 또는 경로 처리를 `std::error_code` 형으로 |
| 2 | `xpe_noise_reduce`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_gsvg_process`(+`_masked`, `_ex`) | 입력만으로 던짐(선언 크기·타일 수·개수). 잠금은 없으나 C ABI 밖으로 나가고, 입력은 `dataSize=0`/거짓 개수를 검증이 통과시킨다 | 크기·곱의 상한 검증을 할당 앞에 + 최외곽 가드. `contrast_enhance` 는 `int` 곱 오버플로 검사 |
| 3 | `xpe_ai_get_model_card` | 할당 실패이지만 잠금 영구 잔류가 실측됨 | 최외곽 가드 (또는 `std::string` 을 잠금 밖에서 만듦) |
| 4 | common 의 `enqueue_alert` (`xpe_alert_push`) | 호출하는 `xpe_ai_init`/`xpe_calc_exposure_index`/`xpe_gsvg_init` 등에 잠금 잔류 위험을 한꺼번에 전파. common 소관이므로 pre 레인과 조율 | 한 곳(`enqueue_alert`)에 가드 |
| 5 | `ForRows`(`parallel_rows.h`, enhance_basic·enhance_advanced·gsvg 에 같은 형태) | 스레드 생성 실패나 작업 스레드 안의 예외는 최외곽 `catch` 로 못 잡고 `std::terminate` | 스레드 생성 `try` 안에서 이미 만든 스레드를 `join` 한 뒤 재던지기, 작업 스레드 본문에 가드 |
| 6 | `xpe_ai_init`, `xpe_enhance_advanced_init`, `xpe_gsvg_init`, `xpe_noise_estimate_sigma` | 할당 실패 때만, 잠금 잔류는 없거나 간접 | 최외곽 가드 |

시험 전용 관점: 1~3 은 이번처럼 "할당 소진 + 잠금 확인" 프로브를 상시 시험으로 만들면 회귀를 막을 수 있다(QA-B-177b 의 `operator new` 교체 방식은 DLL 자체 할당기에는 쓸 수 없어 작업 객체 한도 방식이 필요).

## 7. 5절식 보고

**주장**: 위 1절의 표.
**증거**: `dll_exports_*.txt`(수출 목록 실행 출력), `probe_results_input_paths.txt`(13개 시나리오 × 5회), `probe_results_alloc_exhaustion.txt`(3개 시나리오 × 10회).
**귀속(baseline)**: 현재 HEAD 에서 빌드한 `build/ci-post/bin` DLL(ai 는 stub 구성), dicom 은 `build/ci-dicom/bin`. 실행 환경 ACP 949.
**Gaps (미검증)**:
- ai 는 stub 구성만 실측했다. ONNX 전체 구성(`ci-ai`)의 `Ort::` 예외 경로는 코드 읽기뿐이다.
- 입력 시나리오는 함수당 대표 입력 1~2개이다. 같은 함수에 다른 입력 경로가 있을 수 있다. 특히 enhance_advanced 의 설정 JSON, dicom 의 `validate`/`cfind_mwl` 의 입력(비-UTF-8 값의 `dump()`, 긴 UID 의 `regex`)은 **가드 안**이라는 코드 읽기 결론만 있고 개별 실측하지 않았다.
- `xpe_gsvg_process_masked`/`_ex`, `xpe_noise_estimate_sigma`, `xpe_ai_init`, `xpe_gsvg_init`, `xpe_calc_exposure_index` 의 분류는 코드 읽기이다.
- 스레드 생성 실패(`ForRows`)는 실측하지 않았다. 수만 개의 스레드를 만들어 머신에 부담을 주는 자원 고갈 부류라 의도적으로 피했다.
- 예외가 C++ `catch` 로 잡히는 것까지만 확인했다. 실제 호출자(C# P/Invoke 등)에서 어떻게 보이는지(보통 SEH 예외 `0xE06D7363`)는 측정하지 않았다.
- 할당 소진 프로브는 커밋 한도를 쓰므로 스택 확장까지 막힌다. 첫 시도에서 약 절반이 `0xC00000FD`(STATUS_STACK_OVERFLOW)로 죽었고, 그 실행들은 버렸다. 스택을 먼저 커밋하도록 고친 뒤 시나리오당 10회 모두 결과가 같았다(`probe_results_alloc_exhaustion.txt`).
- common 모듈은 범위 밖(pre 레인의 QA-A-201)이라 `enqueue_alert` 는 읽기만 했고 실측하지 않았다.

**잔여 위험**: 표에서 "해당 없음"으로 분류한 27개는 호출 트리에 던질 연산이 없다는 코드 읽기 결론이다. 에이전트가 읽지 않은 헬퍼 파일에 연산이 있을 수 있다(dicom 의 `DicomReader.cpp`/`DicomWriter.cpp`/`DicomValidator.cpp` 는 전부 읽지 않고 던지는 구문을 검색한 범위, enhance_advanced 의 `enhance_advanced_helpers.cpp` 도 전부 읽지 않음).
