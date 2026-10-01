# QA-A-201 (#216) — 수출 `extern "C"` 함수 전수 조사: 가드 없는 할당 + 잠금 (보고만, 코드 변경 없음)

기준 HEAD `da05b8c0` (QA-A-200). 범위: `modules/common` 수출 16개, `modules/preprocess` 의 C 수출 48개 (총 64개, `dumpbin /exports` 로 뽑음: `evidence/01_`, `02_`). `preprocess` 에는 C++ 링크 수출 5개(`read_xcal_file`, `validate_xcal_header`, `write_xcal_file`, `write_xcal_file_ex`, `xcal_bytes_per_pixel`)가 더 있는데 C ABI 가 아니라서 이 표에서 뺐다. 이 카드는 아무것도 고치지 않았다.

## 한눈에

- 64개 중 **예외가 C ABI 밖으로 나갈 수 있는 함수 14개**. 그중 **실측으로 확인한 것 6개**(`xpe_alert_push`, `xpe_offset_correct`, `xpe_defect_correct`, `xpe_preprocess_pipeline_ex`, `xpe_ghost_create`, `xpe_verify_gain`), **코드 읽기만 8개**(`xpe_preprocess_pipeline`, `xpe_preprocess_pipeline_batch` — `_ex` 와 같은 `fromJson`/`pipeline_core` —, `xpe_verify_offset`, `xpe_verify_pipeline`, `xpe_defect_detect_runtime`, `xpe_bpm_generate`, `xpe_calib_generate_nonlin_lut`, `xpe_nonlinearity_correct`).
- **잠금 누수가 실측된 함수 3개**: `xpe_offset_correct`, `xpe_defect_correct`, 그리고 이 둘을 부르는 파이프라인 (`g_calib_mutex` 가 잠긴 채 남아 이후 보정·적재가 모두 막힌다).
- **부분 커밋이 실측된 함수 2개**: `xpe_alert_push` (가득 찬 큐에서 퇴출·드롭 카운트만 오르고 새 알림은 안 들어감), `xpe_init` (오류를 돌려주는데 초기화는 이미 됨). 이 밖에 캐시의 `put_locked` 가 색인과 목록을 어긋나게 둔다는 것을 실측했다.
- **OOM 이 아니라 입력만으로 던지는 경로 둘**: 파이프라인 설정 JSON 의 잘못된 숫자(`std::stof/stoi`), 고스트 설정 JSON 의 잘못된 숫자(`std::stoi/stod`, 이때 핸들 누수). 둘 다 실측했다.
- 가드가 이미 맞게 있는 함수: 적재·저장·생성 계열과 `xpe_gain_correct`, `xpe_preprocess_init`/`shutdown` 등 (표 참조).

## 먼저 알아 둘 것: `extern "C"` 를 지나가는 예외는 정리가 안 된다 (실측 3건)

MSVC `/EHsc` 는 `extern "C"` 함수를 "던지지 않는 함수"로 가정한다 (QA-A-200 에서 처음 관측). 이 조사에서 그 결과를 세 가지로 확인했다.

1. **함수 안의 지역 객체가 정리되지 않는다.** `xpe_defect_correct` 에 할당 실패를 주입하면 7번 모두 예외가 나가고, 그중 6번에서 지역 벡터가 해제되지 않아 블록이 늘어난 채 남는다 (`growth=6`). `lock_guard`/`unique_lock` 도 같은 이유로 풀리지 않는다 (`lockleak`).
2. **호출자의 `try/catch` 가 못 잡을 수 있다.** 같은 호출(`xpe_preprocess_pipeline_ex` 에 `{"detectorTempC":"abc"}`)을 `try { ... } catch (...)` 안에서 **직접** 부르면 예외가 그 `catch` 를 지나쳐 시험 본문 밖으로 나갔다 (`evidence/09_probe_cfgdirect.txt`, gtest 의 "thrown in the test body"). `std::function` 을 한 번 거치면 같은 예외가 잡힌다 (`09_probe_cfg.txt`). 컴파일러가 그 호출 지점을 던질 수 없는 호출로 보고 `try` 영역에서 빼기 때문으로 보인다.
3. **호출자 쪽 정리가 접근 위반으로 죽을 수 있다.** 프로브 람다가 지역 벡터를 가진 채 `xpe_verify_gain` 을 불렀을 때 예외 전파 중 프로세스가 `0xc0000005` 로 죽었다 (`evidence/06_probe_run.txt` 에 "SEH exception with code 0xc0000005" 가 남아 있다). 접근 위반 순간의 스택을 심볼과 함께 찍어 보니 맨 위가 `std::vector<float>::_Tidy`(소멸자) ← `_CxxFrameHandler4` ← … ← `RaiseException` ← `operator new` ← `std::vector<double>::reserve`(`xpe_verify_gain` 안) 였다. 그 스택 출력은 터미널에서만 확인했고 파일로 남기지 못했다 (핸들러를 단 프로브를 지운 뒤라 다시 뽑을 수 없다). 입력 버퍼를 호출 밖으로 옮기자 죽음이 사라졌다. 호출자의 동작은 호출자의 컴파일 결과에 달려 있다는 뜻이다.

따라서 표의 "C ABI 탈출"은 "시험 하니스에서는 잡히지만 실제 호출자(C# `P/Invoke` 포함)에서는 정리 없는 중단일 수 있음"으로 읽어야 한다. `P/Invoke` 경로는 측정하지 않았다.

## 방법

1. **정적 조사 스크립트** (`evidence/11_survey_script.py.txt`, 결과 `04_survey.txt`·`03_survey.json`): 수출 이름마다 정의를 찾아 함수 try 블록·전체 `try`·`catch (...)` 유무, 자기 프레임의 `lock_guard` 등, 할당 토큰, 같은 모듈 도우미 호출(한 단계)을 뽑았다. 작업 목록일 뿐 판정이 아니다 (`get`/`size` 같은 흔한 이름은 오탐).
2. **읽기**: 가드가 없는 함수와 할당·잠금이 있는 함수의 본문을 전부 읽었다.
3. **실측 프로브** (`evidence/10_probe_*.cpp.txt`, 일회용, 커밋하지 않음): 제품 소스와 `modules/common/src/xpe_common.cpp`(직접 `#include`해서 정적 뮤텍스·큐·플래그를 들여다봄)를 한 실행 파일에 컴파일해 넣고 `operator new` 를 교체했다. K번째 할당만 `bad_alloc` 으로 실패시키는 스윕을 함수별 별도 프로세스로 돌려, 예외 탈출·잠금 누수·부분 커밋·블록 증가를 기록했다 (`08_probe_results.txt`, `09_probe_idx_*.txt`). QA-A-200 의 `xpe_preprocess_oom_tests` 와 같은 방식이다.

실측 열의 뜻: `points` 주입이 닿은 할당 지점 수, `failed` 호출이 실패한(오류 코드 또는 탈출) 지점 수, `escaped` 탈출 수, `lockleak` 잠금이 남은 지점 수, `partial` 실패했는데 함수가 소유한 상태가 바뀐 지점 수, `growth` 실패 직후 살아 있는 블록 증가(누수 **또는** 남겨 둔 상태 — 상태를 갖지 않는 함수에서는 누수).

## 표: 수출 함수 64개

범례: **탈출** = 예외가 C ABI 밖으로 나감 / **잠금** = 잠금 누수 / **부분** = 부분 커밋 / **누수** = 메모리 누수 / **정상** = 가드가 맞음 / **없음** = 던질 수 있는 호출이 없음. 근거 열의 "실측"은 `evidence/08_probe_results.txt` 의 숫자이고 "읽기"는 코드만 본 것이다.

### `modules/common` (16)

| 함수 | 할당 지점 | 잠금 (프레임) | 예외 시 결과 | 근거 |
|---|---|---|---|---|
| `xpe_alert_push` | `enqueue_alert`: 메시지 `std::string` 복사, 큐 `push_back`(deque 노드), 손실 알림 `push_back` 과 `loss->message = buf` | `g_mutex`, **도우미(`enqueue_alert`) 프레임** | **탈출**. 잠금은 도우미 안이라 풀림. **부분**: 가득 찬 큐에서 가장 오래된 것을 퇴출하고 드롭 카운트를 올린 뒤 새 알림 삽입이 실패하면 새 알림도 손실 알림 갱신도 없음 | 실측: 빈 큐 3/3 탈출, 가득 찬 큐 4/4 탈출·**부분 4/4**, 잠금 0 (도우미 프레임 덕) |
| `xpe_init` | `g_configJson = ...`(string 대입), `internal_log` | `g_mutex`, 함수 프레임 안(try 안) | 가드 있음(`catch (...)` → `OUT_OF_MEMORY`). **부분**: `g_initialized = true`, 로그 수준, 큐 초기화를 먼저 한 뒤 설정 대입이 실패하면 오류를 돌려주는데 초기화는 이미 됨 | 실측: 1 지점, 실패 1/1, **부분 1/1** (설정이 기존 용량보다 길 때만 할당 — 용량이 남아 있으면 할당이 없다) |
| `xpe_configure` | `std::string(jsonConfig)`, 파서, `g_configJson = ` | `g_mutex`, try 안 | 가드 있음. 상태 불변(강한 보장). **오류 코드 오분류**: OOM 이 `XPE_ERR_CONFIG_INVALID` 로 나감 | 실측: 19 지점 전부 오류 코드로 반환, 탈출 0, 부분 0 |
| `xpe_shutdown` | 없음(`clear`/`flush`/`close`) | `g_mutex`, try 안 | 정상(전부 삼킴, void) | 실측 4 지점 실패 0 |
| `xpe_log_set_file` | `std::filesystem::path`, `make_shared` 로거·싱크 | `g_logMutex`, 함수 프레임(try 밖에서 잡고 안에서 작업) | 탈출 없음(`catch (...)` → `IO_FAILED`). OOM 이 `IO_FAILED` 로 나가는 **코드 오분류**. **부분(읽기)**: 기존 로거를 먼저 닫고 `reset` 한 뒤 새 싱크를 만들므로, 생성이 실패하면 파일 로깅이 꺼진 채 오류 | 실측: 3 지점, 탈출 0. 부분은 프로브가 상태를 관측하지 못해 **읽기** |
| `xpe_log_set_level` | 없음 | `g_logMutex`, 함수 프레임 | 없음 | 실측 0 지점 |
| `xpe_log_flush` | 없음(spdlog 내부) | `g_logMutex`, 함수 프레임 | 없음(spdlog 의 `flush` 는 자체 오류 처리). 실측 0 지점 | 실측 0 지점, 쓰기 실패는 **미측정** |
| `xpe_get_pending_alert` | 없음 | `g_mutex`, 함수 프레임 | 없음 | 실측 0 지점 |
| `xpe_get_pending_alert_count` | 없음 | `g_mutex`, 함수 프레임 | 없음 | 실측 0 지점 |
| `xpe_clear_alerts` | 없음(`deque::clear`) | `g_mutex`, 함수 프레임 | 없음 | 실측 0 지점 |
| `xpe_get_param_range` | 없음 | `g_mutex`, 안쪽 블록 | 없음 | 읽기 |
| `xpe_alloc_image`, `xpe_copy_image`, `xpe_free_image` | `malloc`/`memcpy` (예외 아님) | 없음 | 없음 | 읽기 |
| `xpe_error_string`, `xpe_version` | 없음 | 없음 | 없음 | 읽기 |

### `modules/preprocess` (C 수출 48)

| 함수 | 할당 지점 | 잠금 (프레임) | 예외 시 결과 | 근거 |
|---|---|---|---|---|
| `xpe_offset_correct` | `offmap.assign(맵 전체)` — **프레임마다** 맵을 복사 (3072² float = 37.7 MB) | `g_calib_mutex`, **함수 프레임, 잠금을 잡은 채 할당** | **탈출 + 잠금 누수**: 이후 적재·보정·저장이 전부 `resource deadlock would occur` 또는 대기. 가드 없음 | 실측: 1 지점, 탈출 1/1, **잠금 1/1** |
| `xpe_defect_correct` | `dm_local`(결함 맵 복사)를 `unique_lock` 아래에서, 잠금 해제 뒤 `std::vector<bool> processed/visited(n)`, `analyzeCluster`·`median_filter_cluster` | `g_calib_mutex`, 함수 프레임(복사 때까지) | **탈출 + 잠금 누수(복사 지점) + 누수(지역 벡터 정리 안 됨)**. **부분(읽기)**: 입력을 출력으로 `memcpy` 한 뒤 실패하면 출력 버퍼가 반쯤 쓰인 상태 | 실측: 7 지점, 탈출 7/7, 잠금 1/7, 증가 6/7 |
| `xpe_gain_correct` | `gainmap`/`poly`/`reciprocal` 벡터 (모두 try 안) | `g_calib_mutex`, try 안 | **정상**(`catch (bad_alloc)` → `OUT_OF_MEMORY`). `bad_alloc` 외 예외는 안 잡지만 크기 검사로 `length_error` 는 닫힘 | 실측: 2 지점, 탈출 0, 잠금 0 |
| `xpe_preprocess_pipeline`, `_ex`, `_batch` | `PipelineConfig::fromJson` (string 들, **`std::stof`/`std::stoi`**), `pipeline_core` 의 단계 버퍼 `resize(pixelCount)` 최대 7개 (float 단계는 3072² 에서 각 37.7 MB) | `pipeline_core` 는 잠금을 잡은 채 할당하지 않음. 잠금은 호출하는 `xpe_offset_correct`/`xpe_defect_correct` 가 가짐 | **탈출**. **잠금 누수**는 위 두 함수에서 전달됨. **입력만으로**: 설정의 숫자가 잘못되면 `invalid_argument`/`out_of_range` 가 탈출. **부분(읽기)**: `_batch` 는 앞 영상이 이미 제자리 처리된 뒤 뒤 영상에서 실패하면 일부만 처리된 채 탈출, `meta->flags` 도 단계별로 이미 켜짐 | 실측(`_ex`, 유효 입력): 14 지점, 탈출 12/14(나머지 2건은 가드 있는 `xpe_gain_correct` 의 코드 반환으로 추정), **잠금 2/14**, 증가 6. **입력 실측**: 4가지 잘못된 설정 전부 예외 (아래) |
| `xpe_ghost_create` | 핸들 `new (nothrow)`, 이력 버퍼 2개(try 안), 설정 파싱 문자열(`xpe_json_get_string`) 과 **`std::stoi`/`std::stod` 7곳 — try 밖** | 없음 | **탈출 + 누수**: 핸들과 이력 버퍼 2개(그리고 부속 블록)가 해제되지 않고 `handleOut` 은 null | **입력 실측**: 잘못된 숫자 3가지 모두 예외, 증가 +4, `handleOut` null. OOM 실측(긴 값 설정): 5 지점, 탈출 2/5, 증가 2. 설정 없는/키 없는 호출은 이력 `assign` 만 닿아 가드로 막힘 (3 지점, 탈출 0) |
| `xpe_ghost_correct`, `xpe_ghost_reset` | 없음 (`std::fill`) | 핸들 `mtx`, 함수 프레임 | 없음 | 실측 0 지점 (`correct`), 읽기 (`reset`) |
| `xpe_ghost_destroy` | 없음 (`delete`) | 없음 | 없음 | 읽기 |
| `xpe_verify_offset`, `xpe_verify_gain`, `xpe_verify_pipeline` | 화소 수만큼의 `std::vector<double>` 복사·정렬 도우미 (8바이트 × 화소 수, 3072² 에서 약 75 MB) | 없음 | **탈출**. `*metrics = {}` 로 먼저 비운 뒤 채우므로 탈출하면 `metrics` 는 비었거나 반쯤 채워진 채 | 실측: `xpe_verify_gain` 2 지점 탈출 2/2(블록 증가 1). `xpe_verify_offset` 은 8×8 입력에서 할당 지점 0 이라 **미측정**(읽기: 할당 있음). `xpe_verify_pipeline` 읽기 |
| `xpe_verify_defect` | 없음 | 없음 | 없음 | 읽기 |
| `xpe_defect_detect_runtime` | `tileSigmas`, `windowValues`, `deviations` 벡터 | 없음 | **탈출**. 출력 맵이 반쯤 쓰일 수 있음(읽기) | 읽기 (입력 구성이 까다로워 미측정) |
| `xpe_bpm_generate` | `dark_mean`, `bright_mean`, `dark_bpm`, `bright_bpm`, `merged_bpm` (영상 크기) | 없음 | **탈출** | 읽기 |
| `xpe_calib_generate_nonlin_lut` | `dose`, `meas`, `xs`, `ys`, `lut` 벡터 + 파일 쓰기(`write_xcal_file`) | 없음 | **탈출**; 파일이 반쯤 쓰일 수 있음(읽기) | 읽기 |
| `xpe_nonlinearity_correct` | `xpe_nonlinearity_apply` 가 설정 문자열을 `std::string` 으로 복사, 알림 푸시 | `g_calib_mutex` 는 **도우미(`xpe_nonlinearity_apply`, C++ 링크) 프레임** | **탈출**. 잠금은 도우미 프레임이라 풀림 | 읽기 |
| `xpe_calib_load_offset`, `xpe_calib_load_defect_map`, `xpe_calib_load_gain`, `xpe_calib_load_nonlin_lut` | 파일 읽기 벡터, 맵 배열, 설정 JSON | `g_calib_mutex`, try 안 | **정상** (커밋 블록은 던지지 않음) | 실측(QA-A-200 스윕): 오프셋·결함·게인(스칼라·다항식); 비선형 LUT 는 읽기 |
| `xpe_calib_load_offset_cached`, `xpe_calib_load_gain_cached`, `xpe_calib_load_defect_cached` | 키 `std::string`, 임시 배열, 엔트리 | `g_calib_mutex`, try 안; 캐시 잠금은 클래스 메서드(도우미) | **정상** (QA-A-200 가드) | 실측(QA-A-200 스윕 7건). 단 `put_locked` 는 아래 |
| `xpe_calib_save` | `make_unique` 복사본(맵 크기) | `g_calib_mutex`, try 안 | **정상**. 파일 쓰기가 반쯤 되는지는 읽기 | 읽기 |
| `xpe_calib_generate_offset`, `xpe_calib_generate_gain`, `xpe_calib_generate_gain_polynomial` | 평균·다항식 계산 벡터 | 없음 (전역 잠금 안 잡음) | **정상** (본문 전체 try) | 읽기 |
| `xpe_calib_check_expiry` | 헤더 읽기 | 없음 | **정상** (본문 전체 try) | 읽기 |
| `xpe_preprocess_init`, `xpe_preprocess_shutdown` | 설정 JSON 해석 | `g_lifecycleMutex`/`g_calib_mutex`, try 안 | **정상** (본문 전체 try) | 읽기 |
| `xpe_calib_cache_clear`, `xpe_calib_cache_set_max_size` | 없음 (해제·퇴출) | 캐시 잠금, 클래스 메서드 | 없음 | 읽기 |
| `xpe_calib_state_load`, `xpe_calib_state_release` | 없음 (스택 버퍼 `snprintf`; 해제) | 없음 | 없음 (호출하는 적재 함수가 가드) | 읽기 |
| `xpe_calib_unload_nonlin_lut` | 없음 (`reset`) | `g_calib_mutex`, 함수 프레임 | 없음 | 읽기 |
| `xpe_binning_correct`, `xpe_temp_compensate`, `xpe_validate_readout_artifact` | 없음 | 없음 | 없음 | 읽기 (조사 스크립트 할당 토큰 0) |
| `xpe_calib_get_quality_meta`, `xpe_calib_get_mode`, `xpe_calib_set_mode`, `xpe_calib_get_max_points`, `xpe_calib_get_poly_degree` | 없음 (`memcpy` 등) | 없음 | 없음 | 읽기 |
| `xpe_preprocess_get_param_range`, `xpe_preprocess_is_initialized`, `xpe_preprocess_version` | 없음 | 없음 | 없음 | 읽기 / `xpe_preprocess_version` 은 QA-A-199 시험 |

### 수출은 아니지만 같은 표에 넣으라고 한 것: `put_locked`

| 항목 | 할당 지점 | 잠금 | 예외 시 결과 | 근거 |
|---|---|---|---|---|
| `CalibrationLRUCache::put_locked` (캐시 삽입) | `entry.path = path`(string), `lru_.push_front`, `index_[path] = ...`(해시 노드), 앞서 `publish_and_view` 가 `malloc` 한 맵 버퍼 | 캐시 잠금(클래스 메서드, 정상 해제) | **부분**: `push_front` 뒤에 색인 삽입이 실패하면 목록에만 엔트리가 남고 색인에는 없음(유령 엔트리). **누수(읽기)**: 소유권을 엔트리로 옮기고(`buffer->data = nullptr`) `push_front` 가 던지면 맵 버퍼를 아무도 해제하지 않음; 그 전에 던지면 `publish_and_view` 의 `malloc` 버퍼가 해제되지 않음(RAII 없음). 수출 함수는 QA-A-200 의 가드가 예외를 오류 코드로 바꿈 | **실측**: 9 지점 중 8 지점 실패, 그중 1 지점에서 용량 2 로 A·B·A 를 부르면 A 가 더는 캐시에 없었다 (유령 엔트리가 B 삽입 때 퇴출되며 A 의 색인을 지움). 누수는 읽기 |

## 입력만으로 던지는 경로 (실측, OOM 아님)

| 호출 | 입력 | 결과 |
|---|---|---|
| `xpe_preprocess_pipeline_ex` | `{"detectorTempC":"abc"}` | `std::invalid_argument: invalid stof argument` 탈출 |
| 〃 | `{"binningMode":"x"}` | `invalid stoi argument` 탈출 |
| 〃 | `{"detectorTempC":"1e999"}` | `std::out_of_range: stof argument out of range` 탈출 |
| 〃 | `{"binningMode":"99999999999999999999"}` | `stoi argument out of range` 탈출 |
| `xpe_ghost_create` | `{"alpha1":"abc"}`, `{"tier":"x"}`, `{"tau2":"1e999"}` | 각각 `invalid stod`/`invalid stoi`/`stod out of range` 탈출, **핸들 누수(블록 +4)**, `handleOut` null |

`xpe_preprocess_pipeline`·`_batch` 는 같은 `fromJson` 을 쓰므로 같다 (읽기, 직접 실측은 `_ex` 만). 위 호출을 `try` 안에서 **직접** 부르면 예외가 호출자의 `catch` 를 지나쳤다 (앞의 관측 2).

## 우선순위 제안 (결정은 리더)

1. **입력으로 던지는 둘** — 파이프라인 `fromJson` 과 `xpe_ghost_create` 의 숫자 파싱: 잘못된 설정 한 줄로 호출자가 중단된다. 예외 없는 변환(`strtof`/`strtod` + 끝 검사 → `XPE_ERR_CONFIG_INVALID`)이 자연스럽다. 고스트는 핸들 누수도 같이 닫힌다.
2. **`xpe_offset_correct`·`xpe_defect_correct`** — 프레임마다 큰 복사를 잠금 아래에서 하고 가드가 없다. 한 번의 OOM 으로 이후 모든 보정·적재가 막힌다. 가드와 함께 복사를 잠금 밖으로 빼는 것(`xpe_defect_correct` 는 이미 해제 후 처리)도 검토 대상이다.
3. **`xpe_alert_push`** — 모든 모듈이 쓰는 경로. 잠금 누수는 없지만 탈출과 부분 상태(손실 계수만 오르고 새 알림 없음)가 있다.
4. **`xpe_init` 의 부분 커밋**, **`xpe_log_set_file`/`xpe_configure` 의 오류 코드 오분류**.
5. **`verify_*`, `bpm`, `runtime`, `nonlin_lut`** 의 탈출 (OOM 에서만, 입력 유발 경로는 못 찾음).
6. **`put_locked`** 의 유령 엔트리와 누수.

고치는 모양(제안): `extern "C"` 함수는 본문 전체를 함수 try 블록으로 감싸 `bad_alloc` → `OUT_OF_MEMORY`, 그 밖 → `PROCESSING_FAILED`; 잠금을 잡은 채 할당하지 않도록 도우미로 빼거나 복사를 잠금 밖에서; 커밋 앞에서 던질 수 있는 일을 끝내기. 회귀 시험은 QA-A-200 의 `xpe_preprocess_oom_tests` 에 스윕을 추가하는 방식이 이미 있다. 단 `modules/common` 함수(알림 큐 등)는 그 실행 파일이 `xpe_common` DLL 에 링크하므로 주입이 닿지 않아, 시험하려면 공통 소스를 컴파일해 넣은 실행 파일이 따로 필요하다 (이 카드의 프로브가 그 방식).

## Gaps / Residual-risk

- 프로브 결과는 **8×8 입력**이다. 큰 영상에서 어느 할당이 가장 먼저 실패하는지는 재지 않았다 (위 크기 언급은 곱셈이지 측정이 아니다).
- `xpe_log_set_file` 의 부분 커밋과 `xpe_verify_offset`·`xpe_verify_pipeline`·`xpe_defect_detect_runtime`·`xpe_bpm_generate`·`xpe_calib_generate_nonlin_lut` 는 **읽기만**이다.
- 보정 로더·캐시 로더(QA-A-200)와 생성·저장 함수는 가드가 있다는 것만 읽었고, 생성·저장의 파일 쓰기 중간 실패가 파일을 반쯤 남기는지는 보지 않았다.
- **`P/Invoke`(C# GUI) 경로에서 예외가 어떻게 되는지는 측정하지 않았다.** 네이티브 C++ 예외가 관리 코드로 넘어가면 보통 `SEHException` 이거나 프로세스 중단이다.
- 표의 "탈출"을 모두 같은 심각도로 보면 안 된다: 잠금 누수가 붙은 둘(`offset`, `defect`)은 프로세스가 살아 있어도 이후 호출이 전부 막힌다.
- `growth` 는 누수와 남겨 둔 상태를 구분하지 못한다 (알림 큐·로거). 상태를 갖지 않는 함수(`defect_correct`, `ghost_create`, `verify_gain`)에서만 누수로 읽었다. "초기화 뒤에도 남는 블록"으로 다시 재는 시도(`growthAfterReset`)는 잡음이 커서 쓰지 않았다.
- 이 조사는 `modules/common`·`modules/preprocess` 만 다뤘다. 다른 모듈(`ai`, `enhance_*`, `gsvg`, `dicom`)의 수출 함수는 보지 않았다 (`xpe_alert_push` 를 부르는 곳은 많다).
- 프로브용 `xpe_common.cpp` 는 직접 `#include` 해서 컴파일했으므로 DLL 로 빌드된 실제 `xpe_common` 과 컴파일 옵션이 같다는 것은 확인하지 않았다.

## 증거

`evidence/`: `01_`~`02_` 수출 목록, `03_survey.json`·`04_survey.txt` 정적 조사 결과, `05_probe_build.txt`, `06_probe_run.txt`(초기 단일 실행 — 프로세스 사망 포함), `07_probe_list.txt`, `08_probe_results.txt`(함수별 실측 한 줄씩), `09_probe_idx_*.txt`(원본 출력), `09_probe_cfg.txt`·`09_probe_cfgdirect.txt`·`09_probe_cfgdirect_first_attempt.txt`(설정 입력 실측: 간접 호출 대 직접 호출), `09_probe_cache.txt`(`put_locked`), `10_probe_*.cpp.txt`(프로브 소스), `11_*.py.txt`(조사 스크립트).
