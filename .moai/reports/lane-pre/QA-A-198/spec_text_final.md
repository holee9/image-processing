# QA-A-198 (#216) — REQ-P1A-102~106 최종 문안 (spec.md 에 붙일 수 있는 형태)

기준: 코드는 `ab8deba8` (QA-A-197) 의 현재 동작. 이 파일의 `####` 블록 다섯 개가 `spec.md` 의 기존 항목(`#### REQ-P1A-014: …`)과 같은 모양이다. 붙이기 전에 아래 "붙이기 전 확인"을 본다. 이 카드는 `spec.md` 를 건드리지 않았다.

## 붙이기 전 확인 (리더)

1. **자리**: 다섯 항목은 §4.2 의 `REQ-P1A-016a` 뒤(캐시 적재 3개와 `_ex` 가 모두 "적재/실행" 계열), 또는 §4.3b 의 `REQ-P1A-101` 뒤(파이프라인 계열) 중 한 곳에 연속으로 둔다. 번호는 `dev/preprocess` 의 `spec.md` 에서 `REQ-P1A-102~106` 이 비어 있음을 확인했다 (이 브랜치의 최대는 `REQ-P1A-101`). `main` 에서 같은 번호가 이미 쓰였는지는 붙이기 전에 다시 확인한다.
2. **이미 정해진 것** (리더 결정, 2026-10-01): 같은 크기·같은 수정 시각의 파일 변경은 감지하지 못하는 한계를 받아들인다 / 게인 다항식 파일은 `XPE_OK` 와 비운 버퍼를 돌려준다(캐시하지 않음) / `REQ-P1A-106` 은 문자열 값이 아니라 계약만 적는다 (초안 선택, 리더가 바꿀 수 있음).
3. **`Traceability` 의 SWU 번호**: 기존 `REQ-P1A-014~016` 의 `SUP-01` 만 따랐다. SWU 정의 문서와 대조하지 않았다 — 확정은 리더.
4. **`### Phase 1 SUP-01 … Completed` 표** (`spec.md` 의 구현 상태 표)에 넣을 행은 이 파일 맨 아래에 있다.
5. 문안은 코드 줄 번호를 인용하지 않는다 (이름으로 인용, 줄 번호는 편집에 밀린다). 줄 번호가 필요하면 `QA-A-191/verdict.md` 의 "측정된 계약"을 본다.

---

## 붙일 문안

#### REQ-P1A-102: Cached Offset Map Loader

**When** `xpe_calib_load_offset_cached(filePath, offsetMapOut)` is called with non-NULL arguments, the module **shall** return in `offsetMapOut` a cache-owned view of the offset map of `filePath`, leave the module-global calibration store holding that map, and reach the verdict that loading the file through `xpe_calib_load_offset(filePath)` would reach, as follows. When the calibration cache holds an entry for that path string, made by this loader, whose recorded file size and last-write time equal the file's current ones, the module **shall** refuse the call with `XPE_ERR_CALIBRATION_EXPIRED`, leaving the store unchanged, if the entry's recorded expiry has passed, and otherwise **shall** return that entry without reading the file and install its map (with the file's timestamp and session id) into the store. In every other case — no entry, an entry whose recorded size or last-write time differs from the file's, a file whose size or last-write time cannot be read, or an entry made by a cached loader of another kind — the module **shall** load the file through `xpe_calib_load_offset(filePath)`, return that call's error code unchanged if it fails, and otherwise copy the loaded map into the cache and return the cache's view of the copy; an entry whose size or last-write time differs, or whose expiry has passed, **shall** be dropped, and an entry made by another kind of loader **shall** be kept. The module **shall not** re-hash the file on a cache hit.

- **계약**:
  - `filePath` 또는 `offsetMapOut` 이 NULL 이면 `XPE_ERR_INVALID_INPUT`.
  - 반환 버퍼: `XPE_PIXEL_FLOAT32`, `bitsAllocated = bitsStored = 32`, `dataSize = width × height × 4`. `data` 포인터는 캐시의 것이며 호출자가 해제하지 않는다. 포인터는 `xpe_calib_cache_clear()`, 퇴출(캐시가 가득 찼을 때, 또는 `xpe_calib_cache_set_max_size()`), `xpe_preprocess_shutdown()`, 파일 변경·만료로 엔트리가 지워질 때까지 유효하다.
  - 오류 코드: `XPE_ERR_INVALID_INPUT`; 미스나 취소된 적중에서는 `xpe_calib_load_offset` 의 코드(`XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`(헤더·무결성·XCal 타입 불일치), `XPE_ERR_CALIBRATION_EXPIRED` 등); 만료된 엔트리의 적중에서 `XPE_ERR_CALIBRATION_EXPIRED`; 적재 뒤 저장소에 오프셋 맵이 없으면 `XPE_ERR_NOT_INITIALIZED`; 캐시에 넣지 못하면 `XPE_ERR_OUT_OF_MEMORY` 또는 `XPE_ERR_PROCESSING_FAILED`.
  - 적중의 만료 판정은 파일 읽기와 같은 식(`지금 > 만료 시각`, 만료 시각 0 = 만료 없음)과 같은 시계(`system_clock`)를 쓴다.
  - **알려진 한계**: 적중은 파일을 다시 해시하지 않는다. 같은 크기·같은 수정 시각으로 바뀐 파일(수정 시각의 해상도 안에서의 쓰기, 수정 시각을 보존하는 도구로 바뀐 파일 포함)은 감지하지 못하며, `xpe_calib_cache_clear()` 또는 `xpe_preprocess_shutdown()` 뒤에야 새 파일을 읽는다. 적중은 SHA-256 과 세션 검사를 반복하지 않는다.
  - 캐시는 오프셋·게인·결함 세 캐시 로더가 **하나**를 공유한다. 키는 호출자가 준 경로 문자열 그대로, 기본 용량 4, 가득 차면 가장 오래 쓰지 않은 항목을 퇴출한다. 엔트리는 어느 캐시 로더가 만들었는지(맵 종류)를 기록하며, 다른 종류의 로더가 같은 경로를 부르면 그 종류의 평범한 로더가 파일을 거절하는 코드를 받고 저장소는 그대로다.
  - `xpe_preprocess_shutdown()` 은 캐시를 비운다. 캐시 용량 설정은 shutdown 이 되돌리지 않는다.
  - 캐시의 목록·색인 변경은 내부 뮤텍스로 보호되고, 이 뮤텍스와 전역 저장소의 `g_calib_mutex` 를 함께 잡는 곳은 없다.
  - 적중 비용(3072×3072 FLOAT32 맵, 30회 중앙값): 약 6.8 ms, 미스 약 70 ms (`QA-A-196/evidence/08_hit_cost_with_stat.txt`). 이 값은 요구가 아니라 측정이다.
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재의 입출력 계약 자체를 정하는 SRS 요구는 없다.
- **Traceability**: SUP-01
- **Verification**: Test (`test_calibration_cache.cpp`, `test_calib_cache_ownership.cpp`, `test_calib_cache_concurrency.cpp`, `test_calib_cache_global_store.cpp`, `test_calib_cache_same_verdict.cpp`)

#### REQ-P1A-103: Cached Gain Map Loader

**When** `xpe_calib_load_gain_cached(filePath, gainMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_gain` as the plain loader and the scalar gain map as the cached map; **shall**, on a cache hit, apply the gain quality metadata of the file (kept in the entry as its config JSON) again exactly as the load applies it, so that `xpe_calib_get_quality_meta` reports what it would report after a miss; and, when the file loaded is a gain polynomial file (`XCAL_TYPE_GAIN_POLY`), **shall** return `XPE_OK` with `gainMapOut` zeroed (`data` NULL, `dataSize` 0) and cache nothing, the polynomial being held in the module-global store for `xpe_gain_correct`.

- **계약**:
  - 반환 버퍼(스칼라 맵): `XPE_PIXEL_FLOAT32`, `bitsAllocated = bitsStored = 32`, `dataSize = width × height × 4`. 소유권·유효 범위·오류 코드·한계·캐시 규칙은 REQ-P1A-102 와 같다.
  - 적중의 설치는 평범한 게인 로더와 같이 스칼라 맵을 올리고 다항식 계수·적합 범위를 지운다(둘은 택일이다). 게인 값 범위 [0.1, 10.0] 검사는 적재 때 한 번 일어나고 캐시에는 그 검사를 통과한 맵만 들어간다.
  - 다항식 파일은 캐시하지 않으므로 호출마다 파일을 다시 읽고 검사하며(만료·무결성 포함), 호출자는 반환 버퍼가 아니라 `xpe_gain_correct` 로 다항식을 쓴다. 적재 뒤 저장소에 스칼라 맵도 다항식도 없으면 `XPE_ERR_NOT_INITIALIZED`.
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

#### REQ-P1A-104: Cached Defect Map Loader

**When** `xpe_calib_load_defect_cached(filePath, defectMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_defect_map` as the plain loader and the defect map as the cached map.

- **계약**:
  - 반환 버퍼: `XPE_PIXEL_UINT8`, `bitsAllocated = bitsStored = 8`, `dataSize = width × height`. 소유권·유효 범위·오류 코드(저장소에 결함 맵이 없을 때의 `XPE_ERR_NOT_INITIALIZED` 포함)·한계·캐시 규칙은 REQ-P1A-102 와 같다.
  - 결함 맵에는 파일의 타임스탬프·세션 id 를 저장소에 쓰지 않는다 (평범한 결함 로더도 쓰지 않는다).
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

> `xpe_calib_cache_clear` · `xpe_calib_cache_set_max_size` 의 계약은 SRS-CALIB-FUNC-038 이 이름으로 묶는다. REQ-P1A-102~104 는 이 두 함수를 다루지 않는다.

#### REQ-P1A-105: Pipeline Execution on the Loaded Calibration

**When** `xpe_preprocess_pipeline_ex(img, meta, calibState, ghostHandle, configJsonOrNull)` is called with non-NULL `img` and `meta`, the module **shall** run the same correction stages as `xpe_preprocess_pipeline` (REQ-P1A-095 through REQ-P1A-101) on the calibration currently held in the module-global calibration store, **without** reading any calibration file and **without** reading `calibState`.

- **계약**:
  - `img` 또는 `meta` 가 NULL 이면 `XPE_ERR_INVALID_INPUT`.
  - 두 진입점이 같은 `pipeline_core` 를 부른다. 그래서 단계 순서·플래그·우회·고스트 의존·고스트 버퍼 격리·자료형 전이·결함 단계 가용성(REQ-P1A-095~101)이 그대로 적용된다. 설정 JSON 은 같은 `PipelineConfig::fromJson` 으로 읽는다.
  - `calibState` 는 NULL 이어도 되고 읽지 않는다. 시험(`test_pipeline_ex.cpp` 의 `PipelineExNullStateSkipsCalibration`)은 NULL 상태와 **모든 단계 우회** 설정만 확인한다 — 단계가 도는 경우에 NULL 상태를 넘기는 시험은 없다.
  - 보정 파일 적재 없음: 단계가 쓰는 맵은 전역 저장소의 것이다. 맵이 없으면 각 단계가 자기 오류(`XPE_ERR_CALIB_NOT_LOADED` 등, REQ-P1A-020a·101)를 돌려주고 이 함수가 따로 확인하지 않는다.
  - 의도된 쓰임: `xpe_calib_state_load()` 를 한 번 부른 뒤 프레임마다 이 함수를 부른다.
- **요구에 쓰지 않은 것**: `calibState` 의 `offsetLoaded`/`gainLoaded`/`defectLoaded` 플래그를 읽거나 그 값에 따라 단계를 막는다고는 쓰지 않는다 (읽지 않는다).
- **SRS**: SRS-CALIB-PERF-001, SRS-CALIB-PERF-003. 이 함수의 입출력 계약을 정하는 SRS 요구는 없다.
- **Traceability**: REQ-P1A-016a (상태 적재 계약), REQ-P1A-095~101 (파이프라인)
- **Verification**: Test (`test_pipeline_ex.cpp`)

#### REQ-P1A-106: Version String

**When** `xpe_preprocess_version()` is called, the module **shall** return a pointer to a NUL-terminated string that is never NULL and stays valid for the lifetime of the process, whether or not the module has been initialized.

- **계약**:
  - 반환 포인터는 정적 저장소를 가리키므로 해제하지 않는다. 함수 본문은 초기화 여부를 읽지 않는다.
  - 문자열의 **값**은 요구에 고정하지 않는다. 지금의 값(`"0.1.0"`)은 소스의 상수이고 같은 모듈의 CMake 프로젝트 버전(`1.0.0`)과 다르다 — 값을 요구에 적으면 둘이 어긋난 채 굳는다. 값을 정하려면 별도 결정과 시험이 필요하다.
- **SRS**: 해당 SRS 요구 없음.
- **Traceability**: 없음 (진단용 수출; `XPE-GUI-NATIVE-INT-READINESS-001` 의 `R1` 행이 사용처)
- **Verification**: 없음 — `xpe_preprocess_version` 을 부르는 시험이 모듈 시험에 없다. 이 항목을 `spec.md` 에 넣으면 시험 한 건(비-NULL·정적·초기화 무관)이 따라와야 한다.

---

## 구현 상태 표에 넣을 행 (`### Phase 1 SUP-01 …` 표 형식)

| Requirement | Status | Implementation Files |
|-------------|--------|----------------------|
| REQ-P1A-102 | Implemented | modules/preprocess/src/calibration_cache.cpp |
| REQ-P1A-103 | Implemented | modules/preprocess/src/calibration_cache.cpp, modules/preprocess/src/xpe_calib_load_gain.cpp |
| REQ-P1A-104 | Implemented | modules/preprocess/src/calibration_cache.cpp |
| REQ-P1A-105 | Implemented | modules/preprocess/src/pipeline.cpp |
| REQ-P1A-106 | Implemented (no test) | modules/preprocess/src/preprocess.cpp |

## 각 주장의 근거 (붙이기 전 대조용)

| 문안의 주장 | 확인한 곳 |
|---|---|
| 적중이 만료를 다시 검사한다 | 시험 `CacheSameVerdict.AMapCachedBeforeItsFileExpiredIsRefusedLikeAMissAndNotInstalled` (반증 팔 QA-A-196 A1) |
| 크기·수정 시각이 다르면 적중 취소 → 재적재 → 변조는 거절 | `…WasTamperedWith…`, `AFileWhoseWriteTimeChangedIsReloaded`, `AFileWhoseSizeChanged…`, `AHeaderCorruptedAfterCaching…` (반증 팔 QA-A-196 A2·A2b·A2c, QA-A-197 B3) |
| 같은 크기·같은 수정 시각은 감지하지 못한다 | `AChangeThatKeepsBothSizeAndWriteTimeIsNotNoticedUntilCacheClear` |
| 다른 종류의 로더는 엔트리를 받지 못하고 엔트리는 남는다 | `AMapCachedByOneLoaderIsRefusedByEveryOtherLoaderLikeAMiss` (6조합, 반증 팔 QA-A-197 B1·B2) |
| 게인 품질 메타가 적중에서 미스와 같다 | `AGainHitReportsTheSameQualityMetadataAsAMiss` (반증 팔 QA-A-196 A3) |
| 다항식 파일은 OK + 비운 버퍼, 캐시 안 함 | `AGainPolynomialFileLoadsAndTheCachedLoaderReportsSuccess` (반증 팔 QA-A-196 A4) |
| 적중이 저장소를 채운다 / shutdown 이 캐시를 비운다 | `CacheGlobalStore.*` (QA-A-193, 반증 4팔) |
| 거절된 적중이 저장소를 건드리지 않는다 | 위 만료·변조·종류 시험이 저장소 불변을 단언 |
| 삭제된 파일은 미스와 같은 코드 | `AFileDeletedAfterCachingIsRefusedLikeAMiss` (도착 때 초록, 시험이 판별하는 반증 팔은 없음) |
| 적중이 다항식 필드를 지운다 | **시험이 가르지 못한다** (QA-A-197 B4/B4b). 문안은 "지운다"고 쓰지만 근거는 코드 읽기(`install_gain`)뿐 |
| 적중이 타임스탬프·세션 id 를 설치한다 | 코드 읽기만 (읽는 공개 API 없음) |
| 압축(RLE) 파일에서도 같다 | **미관측** — 문안은 압축을 언급하지 않는다 |
| `_ex` 가 `calibState` 를 읽지 않는다 | 코드 읽기 + 시험 `PipelineExNullStateSkipsCalibration` (모든 단계 우회에서만) |
| `xpe_preprocess_version` 이 정적이고 초기화와 무관 | 코드 읽기만 (시험 없음) |

## 문안에 일부러 넣지 않은 것

- 적중 비용 숫자는 계약 항목에 "측정"으로만 적었다 (요구가 아니다). 기계가 바뀌면 달라진다.
- 만료 경계(만료 시각과 같은 밀리초), 시계가 거꾸로 가는 경우, 네트워크 드라이브의 수정 시각 신뢰성, 상대 경로와 작업 디렉터리 변경은 관측하지 않아 쓰지 않았다.
- 종류가 다른 로더가 부른 호출의 오류 경로 비용(파일 전체를 읽고 SHA 를 계산한 뒤 거절)은 재지 않았다.
- `xpe_calib_cache_clear`·`xpe_calib_cache_set_max_size` 는 FUNC-038 영역이라 이 항목들에 적지 않았다.
