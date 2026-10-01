# QA-A-191 (#216) — 요구 없음 5개의 현재 동작에서 뽑은 SPEC 요구 문안 초안 (문서 초안만)

시작 HEAD `7004b93f` (`evidence/00_head.txt`). 코드·`spec.md` 변경 없음. 이 카드가 추가한 것은 이 보고서와 증거 파일뿐이다.

## 먼저 — 리더가 결정할 것 셋 (관측으로 나온 것)

초안을 쓰려고 코드를 읽다가, 요구로 적으면 **그 동작을 보증하게 되는** 항목 셋이 나왔다. 아래 문안에서는 해당 줄에 `⚠` 를 달았고, 굳히기 전에 리더가 정해야 한다.

| # | 관측 | 근거 | 왜 결정이 필요한가 |
|---|---|---|---|
| ⚠1 | **캐시 적중은 모듈 전역 보정 저장소를 채우지 않는다.** 한 번 캐시된 경로는 `xpe_preprocess_shutdown()` + `init()` 뒤에 `*_cached` 가 `XPE_OK` 를 돌려주는데, 그 직후 `xpe_offset_correct` 는 `XPE_ERR_CALIB_NOT_LOADED`(-16) 를 돌려준다 | 코드 `calibration_cache.cpp:263-265`(적중 시 바로 반환, 로더 호출 없음). 관측 `evidence/02_cached_loader_observed.txt`(적중 뒤 -16) | 호출자가 `*_cached` 의 `OK` 를 "보정에 필요한 맵이 적재됐다"로 읽으면 틀린다. 요구로 적으면 이 동작이 계약이 되고, 결함으로 보면 코드를 고쳐야 한다 |
| ⚠2 | **shutdown 이 캐시를 비우지 않는다.** 캐시 항목의 포인터가 shutdown+init 뒤에도 같다 | 코드 `preprocess.cpp:71-83`(전역 저장소·모드만 초기화, 캐시 접근 없음). 관측 `02_…`("cache survived shutdown") | `api-spec.md:499` 는 캐시 소유 뷰가 "`xpe_calib_cache_clear()`, 퇴출, **모듈 종료**" 까지 유효하다고 적는다. 코드는 종료로 무효화하지 않으므로 문서와 코드가 어긋난다 |
| ⚠3 | **캐시는 경로 문자열로만 키잉하며 파일 변경을 보지 않는다.** 파일을 덮어쓴 뒤 같은 경로로 부르면 옛 맵(100.0)이 나오고, `xpe_calib_cache_clear()` 뒤에야 새 맵(300.0)이 나온다 | 코드 `calibration_cache.cpp:65-68`(경로 문자열 키). 관측 `02_…` | 요구에 "경로 문자열 키, 변경 감지 없음"을 적을지, 캐시 무효화를 별건으로 세울지 |

## 형식과 번호

`spec.md` 의 정의는 `REQ-P1A-101` 이 마지막이고 `102` 이후는 spec·docs·소스 어디에도 쓰이지 않는다 (`grep REQ-P1A-10[2-9]|REQ-P1A-1[1-9][0-9]` 결과 0건). 아래에 **REQ-P1A-102 ~ 106** 을 제안한다. 인용 도구(`tools/docs/check_req_citations.py`)는 코드가 정의 없는 번호를 인용하면 막으므로, 소스 주석에 이 번호를 쓰기 전에 `spec.md` 에 정의가 먼저 들어가야 한다.

문안은 `REQ-P1A-016a`·`REQ-P1A-101` 의 형태를 따랐다: EARS 문장 → 줄 단위 "측정된 계약"(코드 근거 `파일:줄`) → `SRS`·`Traceability`. 아래 줄 번호는 모두 HEAD `7004b93f` 기준이다.

---

## 제안 문안 (복붙용)

### REQ-P1A-102: Cached Offset Map Loader

**When** `xpe_calib_load_offset_cached(filePath, offsetMapOut)` is called with non-NULL arguments, the module **shall** return in `offsetMapOut` a cache-owned view of the offset map of `filePath`: when the calibration cache already holds an entry keyed by that path string it **shall** return that entry without reading the file; otherwise it **shall** load the file through `xpe_calib_load_offset(filePath)`, return that call's error code unchanged if it fails, copy the loaded map into the cache and return the cache's view of the copy.

- **측정된 계약**:
  - `filePath` 또는 `offsetMapOut` 이 NULL 이면 `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:260`).
  - 적중: 캐시에서 `XpeImageBuffer` 구조체를 값으로 복사해 돌려준다. `data` 포인터는 캐시의 것이며 호출자가 해제하지 않는다 (`:263-265`, 복사 `:65-81`; 소유권 규칙 `api-spec.md:499`).
  - 실패한 적재: `xpe_calib_load_offset` 이 돌려준 코드를 그대로 반환 (`:268-269`). 관측: 없는 파일 → `XPE_ERR_IO_FAILED`(-9).
  - 성공한 적재 뒤 전역 저장소에 오프셋 맵이 없으면(`offset_map` 이 NULL 이거나 `offset_width` 가 0) `XPE_ERR_NOT_INITIALIZED` (`:277`).
  - 반환 버퍼: 형식 `XPE_PIXEL_FLOAT32`, `bitsAllocated = bitsStored = 32`, `width`·`height` 는 적재된 맵의 것, `dataSize = width × height × 4` (`:279-285`).
  - 캐시에 넣을 때 할당 실패 → `XPE_ERR_OUT_OF_MEMORY` (`:233-234`), 넣은 항목을 되읽지 못하면 `XPE_ERR_PROCESSING_FAILED` (`:242-243`).
  - 삽입과 되읽기는 한 번의 잠금 안에서 일어난다 (`:113-126`, `#127`, QA-A-31).
  - 캐시: 키는 호출자가 준 경로 문자열 그대로 (`:65-68`), 기본 용량 4 (`:205`), 가득 차면 가장 오래 쓰지 않은 항목을 퇴출 (`:141-146`), 같은 키로 다시 넣으면 옛 데이터를 해제하고 교체 (`:132-138`). ⚠3
  - ⚠1 적중 경로는 `xpe_calib_load_offset` 을 부르지 않으므로 **모듈 전역 보정 저장소를 바꾸지 않는다** (`:263-265`; 관측: shutdown+init 뒤 적중 → `OK`, 이어진 `xpe_offset_correct` → `XPE_ERR_CALIB_NOT_LOADED`).
- **SRS**: SRS-CALIB-NFR-003-CACHE (캐시 목록·색인 변경을 뮤텍스로 보호, `SRS-CALIB-001:470`). 캐시된 적재의 입출력 계약 자체를 정하는 SRS 요구는 없다.
- **Traceability**: SUP-01 (REQ-P1A-014 와 같은 지원 단위; 다른 SWU 번호는 이 초안에서 확인하지 못함)
- **Verification**: Test (`test_calibration_cache.cpp`, `test_calib_cache_ownership.cpp`, `test_calib_cache_concurrency.cpp`)

### REQ-P1A-103: Cached Gain Map Loader

**When** `xpe_calib_load_gain_cached(filePath, gainMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_gain` as the loader and the gain map as the cached map.

- **측정된 계약**:
  - NULL 인자 → `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:300`); 적중 `:302-304`; 적재 위임과 오류 그대로 반환 `:307-308`.
  - 성공한 적재 뒤 전역 저장소의 스칼라 게인 맵이 없으면 (`gain_map` 이 NULL 이거나 `gain_width` 가 0) `XPE_ERR_NOT_INITIALIZED` (`:316`). 게인 **다항식** 파일은 전역 저장소에 `gain_map` 이 아니라 다항식 계수를 싣고(`gain_correct.cpp` 의 `!g_calib.gain_map && g_calib.gain_poly_coeffs` 분기), 이 함수는 `gain_map` 만 읽으므로 다항식 파일에 대해서는 이 코드가 돌려질 것으로 읽힌다 (코드 읽기만; 관측하지 않음 — Gaps).
  - 반환 버퍼: `XPE_PIXEL_FLOAT32`, 32비트, `dataSize = width × height × 4` (`:318-324`).
  - 캐시 규칙(키·용량·퇴출·소유권)과 ⚠1 은 REQ-P1A-102 와 같다 (같은 `g_calibCache`, `:302`).
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

### REQ-P1A-104: Cached Defect Map Loader

**When** `xpe_calib_load_defect_cached(filePath, defectMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_defect_map` as the loader and the defect map as the cached map.

- **측정된 계약**:
  - NULL 인자 → `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:346`); 적중 `:348-350`; 적재 위임과 오류 그대로 반환 `:353-354`.
  - 성공한 적재 뒤 전역 저장소에 결함 맵이 없으면(`defect_map` 이 NULL 이거나 `defect_width` 가 0) `XPE_ERR_NOT_INITIALIZED` (`:362`).
  - 반환 버퍼: `XPE_PIXEL_UINT8`, `bitsAllocated = bitsStored = 8`, `dataSize = width × height` (`:364-370`).
  - 캐시 규칙과 ⚠1 은 REQ-P1A-102 와 같다 (`:348`).
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

> **세 요구가 공유하는 캐시 규칙 (제안: 별도 요구가 아니라 위 셋에 공통으로 적용되는 주석으로).** 캐시는 세 적재 함수가 **하나**를 공유한다 (`g_calibCache`, `calibration_cache.cpp:209`). 키가 경로 문자열뿐이라 세 종류의 맵이 같은 경로로 호출되면 같은 항목을 가리킨다. 실제로는 다른 종류 파일을 다른 로더로 부르면 적재 단계에서 오류가 나므로 한 경로가 한 종류로만 캐시되지만, 이 점은 관측하지 않았다 (Gaps). `xpe_calib_cache_clear` · `xpe_calib_cache_set_max_size` 의 계약은 SRS-CALIB-FUNC-038 (`SRS-CALIB-001:469`) 이 이름으로 묶는다 — 여기서는 다루지 않는다.

### REQ-P1A-105: Pipeline Execution on the Loaded Calibration

**When** `xpe_preprocess_pipeline_ex(img, meta, calibState, ghostHandle, configJsonOrNull)` is called with non-NULL `img` and `meta`, the module **shall** run the same correction stages as `xpe_preprocess_pipeline` (REQ-P1A-095 through REQ-P1A-101) on the calibration currently held in the module-global calibration store, **without** reading any calibration file and **without** reading `calibState`.

- **측정된 계약**:
  - `img` 또는 `meta` 가 NULL 이면 `XPE_ERR_INVALID_INPUT` (`pipeline.cpp:431`).
  - 같은 단계 실행: 두 진입점이 같은 `pipeline_core` 를 부른다 (`xpe_preprocess_pipeline` `:357`, `xpe_preprocess_pipeline_ex` `:441`). 그래서 단계 순서·플래그·우회·고스트 의존·고스트 버퍼 격리·자료형 전이·결함 단계 가용성(REQ-P1A-095~101)이 그대로 적용된다.
  - 설정 JSON 은 같은 `PipelineConfig::fromJson` 으로 읽는다 (`:434`, 정의 `:48-88`).
  - `calibState` 는 NULL 이어도 되고 읽지 않는다: `(void)calibState` (`:436-439`). 시험: `test_pipeline_ex.cpp:241` 의 `PipelineExNullStateSkipsCalibration` 이 NULL 상태와 **모든 단계 우회** 설정으로 `XPE_OK` 를 확인한다 (단계가 도는 경우에 NULL 상태를 넘기는 시험은 이 파일에 없다 — Gaps).
  - 보정 파일 적재 없음: 단계가 쓰는 맵은 전역 저장소의 것이다. 맵이 없으면 각 단계가 자기 오류(`XPE_ERR_CALIB_NOT_LOADED` 등, REQ-P1A-020a·101)를 돌려주고 이 함수가 따로 확인하지 않는다 (`:441`).
  - 의도된 쓰임: `xpe_calib_state_load()` 를 한 번 부른 뒤 프레임마다 이 함수를 부른다 (`pipeline.cpp:317-319` 주석, 헤더 `preprocess_api.h:780-792`; 헤더가 적은 비용 비교 — 3072×3072 에서 프레임당 약 124 ms 대 약 590 ms, QA-A-105 실측 — 는 이 보고서가 다시 재지 않았다).
- **하지 않는 것(요구에 쓰지 않음)**: `calibState` 의 `offsetLoaded`/`gainLoaded`/`defectLoaded` 플래그를 읽거나 그 값에 따라 단계를 막지 않는다 (`:436-439`).
- **SRS**: SRS-CALIB-PERF-001 (프레임당 500 ms), SRS-CALIB-PERF-003 (보정 파일은 시작 때 한 번 적재; `SRS-CALIB-001:327`, `:339`). 이 함수의 입출력 계약을 정하는 SRS 요구는 없다.
- **Traceability**: REQ-P1A-016a (상태 적재 계약), REQ-P1A-095~101 (파이프라인)
- **Verification**: Test (`test_pipeline_ex.cpp`)

### REQ-P1A-106: Version String

**When** `xpe_preprocess_version()` is called, the module **shall** return a pointer to a NUL-terminated string that is never NULL and stays valid for the lifetime of the process, whether or not the module has been initialized.

- **측정된 계약**:
  - 반환값은 소스에 적힌 문자열 상수 `"0.1.0"` 이다 (`preprocess.cpp:39-42`; 관측 `02_…`: `version string: "0.1.0"`). 포인터는 정적 저장소를 가리키므로 해제하지 않는다.
  - 초기화 여부를 보지 않는다: 본문이 `g_initialized` 를 읽지 않는다 (`:39-42`).
  - 헤더 문서: "Null-terminated version string. Lifetime: process. Never NULL." (`preprocess_api.h:50-58`).
  - 소비자: 클라이언트 진단 프로브가 이 값을 버전으로 표시한다 (`clients/ImageProcTest/Diagnostics/XpePreprocessReadinessProbe.cs:74-77`). GUI 준비 문서의 `R1` 행은 "DLL 이 존재하고 버전 함수를 부를 수 있다"만 요구한다 (`XPE-GUI-NATIVE-INT-READINESS-001`, `R1` 행).
- **미정(결정 필요)**: 반환 문자열의 **값**을 요구에 적을지. `"0.1.0"` 은 소스 상수이고, 같은 모듈의 CMake 프로젝트 버전은 `1.0.0` 이다 (`modules/preprocess/CMakeLists.txt:10`). 값을 요구에 고정하면 코드와 CMake 가 어긋난 채 굳는다. 값을 적지 않고 위 문안처럼 **계약만**(비-NULL·정적·초기화 무관) 적는 쪽으로 초안을 썼다.
- **SRS**: 해당 SRS 요구 없음 (`SRS-CALIB-001` 에 이 함수의 이름·능력 없음; FUNC-038 의 열거에도 없음).
- **Traceability**: 없음 (진단용 수출; `XPE-GUI-NATIVE-INT-READINESS-001` 이 사용처)
- **Verification**: Test 없음 — `xpe_preprocess_version` 을 부르는 시험 파일이 모듈 시험에 없다 (아래 확인).

---

## 증거

### 코드 읽기

각 문장의 근거는 위 문안에 `파일:줄` 로 달았다. 줄 번호는 HEAD `7004b93f` 에서 읽은 값이다 (`calibration_cache.cpp`, `pipeline.cpp`, `preprocess.cpp`).

### 관측 (`evidence/02_cached_loader_observed.txt`, 임시 프로브 `03_scratch_probe_source.cpp.txt`)

임시 시험 파일로 `xpe_calib_load_offset_cached` 를 실제로 불러 확인했다 (파일은 지웠고 트리에 남지 않았다; 지운 뒤 재빌드 `BUILD_EXIT=0`).

| 단계 | 반환 |
|---|---|
| 적재 전 `xpe_offset_correct` | -16 (`CALIB_NOT_LOADED`) |
| `*_cached` #1 (미스) | 0, 버퍼 4×4, 형식 1(FLOAT32), 32비트, `dataSize=64` |
| 이어 `xpe_offset_correct` | 0 |
| `*_cached` #2 (적중) | 0, 첫 호출과 같은 포인터 |
| shutdown+init 뒤 `xpe_offset_correct` | -16 |
| shutdown+init 뒤 `*_cached` #3 | **0**, 포인터가 옛 캐시 것과 같음 (캐시가 shutdown 을 견딤) |
| 이어 `xpe_offset_correct` | **-16** (전역 저장소가 안 채워짐) |
| 파일을 덮어쓴 뒤 `*_cached` #4 | 0, 첫 값 100.0 (옛 맵) |
| `xpe_calib_cache_clear()` 뒤 #5 | 0, 첫 값 300.0 (새 맵) |
| NULL 경로 / NULL 출력 / 없는 파일 | -1 / -1 / -9 (`IO_FAILED`) |
| `xpe_preprocess_version()` | `"0.1.0"` |

### 시험 존재 확인

`xpe_preprocess_version` 을 부르는 모듈 시험 파일: 0개 (`grep -rln xpe_preprocess_version modules/preprocess/tests` 결과 없음; 클라이언트 프로브만 부름). `*_cached` 를 부르는 시험은 4개 파일, `xpe_preprocess_pipeline_ex` 는 2개 파일 (`test_pipeline_ex.cpp`, `test_nonlin_lut_apply.cpp`).

## 코드가 하지 않아서 쓰지 않은 것

- `*_cached` 가 **전역 보정 저장소를 채운다**고는 쓰지 않았다 (미스일 때만 채워지고 적중일 때는 아니다 — ⚠1 으로 분리).
- `shutdown` 이 캐시를 비운다고는 쓰지 않았다 (⚠2). `api-spec.md:499` 의 "모듈 종료까지 유효"는 코드가 지키지 않는다.
- 캐시가 파일 변경을 감지한다고는 쓰지 않았다 (⚠3).
- `xpe_preprocess_pipeline_ex` 가 `calibState` 의 적재 플래그를 확인한다고는 쓰지 않았다.
- `xpe_preprocess_version` 이 빌드·CMake 버전에서 파생된다고는 쓰지 않았다 (소스 상수).
- 게인 다항식 파일이 `*_gain_cached` 로 정상 서비스된다고는 쓰지 않았다 (코드상 `gain_map` 만 읽음).

## 지금 적힌 문서와 어긋난 곳 (참고, 이 카드에서는 안 고침)

- `api-spec.md:31` 의 `xpe_preprocess | 45` 는 수출 C ABI 48개와 다르다 (`QA-A-189`).
- `api-spec.md:499` 의 "모듈 종료" 시 캐시 소유 뷰 무효화 문구는 ⚠2 와 어긋난다.
- `preprocess_api.h` 의 `xpe_calib_load_offset_cached` 문서에는 오류 코드로 `XPE_ERR_IO_FAILED`·`XPE_ERR_CALIBRATION_EXPIRED` 만 나열되어 있다 (`:836-841` 부근). 코드는 `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_PROCESSING_FAILED` 도 돌려준다.

## 결정 재료 (한 줄씩)

1. **REQ-P1A-102~104 (`*_cached`)** — 결정에 필요한 것: ⚠1(적중이 전역 저장소를 안 채움)을 요구로 굳힐지, 결함으로 보고 코드를 고칠지. 굳히지 않는다면 이 셋은 "캐시 소유 뷰를 돌려준다"까지만 적고 ⚠1 줄을 뺀다.
2. **⚠2 / ⚠3** — 결정에 필요한 것: `api-spec.md:499` 의 "모듈 종료" 문구를 고칠지, `shutdown` 이 캐시를 비우도록 코드를 고칠지; 경로 키잉의 변경 비감지를 요구에 적을지.
3. **REQ-P1A-105 (`_ex`)** — 결정에 필요한 것: `calibState` 가 읽히지 않는 현 동작을 요구로 굳힐지(그러면 인자는 "호환용"으로 남음), 인자를 없애는 ABI 변경을 별건으로 볼지. 제품 호출자는 없다 (QA-A-189).
4. **REQ-P1A-106 (`version`)** — 결정에 필요한 것: 문자열 값(`"0.1.0"` 대 CMake `1.0.0`)을 요구로 고정할지, 계약만 적을지. 시험이 없어 이 요구는 시험도 함께 세워야 한다.

## Gaps / Residual-risk

- 게인 다항식 파일에 대한 `xpe_calib_load_gain_cached` 반환값(`NOT_INITIALIZED` 로 읽힘)은 코드 읽기만이고 관측하지 않았다.
- 같은 경로를 서로 다른 종류의 `*_cached` 로 부를 때(공유 캐시·경로 키)의 동작은 관측하지 않았다.
- NULL `calibState` 로 단계를 실제로 돌리는 경우(우회 없음)는 시험도 관측도 하지 않았다. 코드가 `calibState` 를 읽지 않는다는 것(`pipeline.cpp:439`)에 근거한다.
- `_ex` 가 `xpe_preprocess_pipeline` 과 같은 결과를 내는지는 같은 `pipeline_core` 를 부른다는 코드 읽기로 적었다. 같은 입력으로 두 진입점을 나란히 돌려 비교하지는 않았다.
- `Traceability` 칸의 SWU 번호는 기존 REQ-P1A-014~016a 의 `SUP-01` 을 따랐을 뿐 SWU 정의 문서와 대조하지 않았다.
- 문안은 초안이다. `spec.md` 에 넣기 전에 번호와 `SRS`·`Traceability` 칸을 리더가 확정한다.
