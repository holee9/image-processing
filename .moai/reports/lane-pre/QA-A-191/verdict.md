# QA-A-191 (#216) — 요구 없음 5개의 현재 동작에서 뽑은 SPEC 요구 문안 초안 (문서 초안만)

> **갱신 (QA-A-193, 2026-10-01).** 사용자가 아래 ⚠1·⚠2 를 "고친 뒤 요구 작성"으로 결정했고 QA-A-193 이 두 가지를 고쳤다. REQ-P1A-102~104 의 문안은 **고친 동작**에 맞게 아래에서 다시 썼다. 처음 초안(고치기 전)에 있던 ⚠1·⚠2 줄은 지웠고, 그 내용은 "해결 내역"에 남겼다. REQ-P1A-105·106 은 바뀌지 않았다. 줄 번호는 QA-A-193 이후의 `calibration_cache.cpp`·`preprocess.cpp` 기준이다.
>
> **재갱신 (QA-A-196, 2026-10-01).** QA-A-195 의 실측(캐시 적중이 만료되었거나 변조된 파일의 맵을 올린다)에 대해 리더가 "적중은 미스와 같은 판정을 내야 한다"를 결정했고 QA-A-196 이 구현했다. REQ-P1A-102~104 의 문안은 다시 썼다: 적중마다 만료를 재검사하고, 파일 크기·수정 시각이 달라지면 적중을 취소해 미스 경로로 재적재하며, 게인 품질 메타데이터를 적중에서도 같게 적용하고, 게인 다항식 파일은 성공을 돌려준다. 같은 크기·같은 수정 시각으로 바뀐 파일은 감지하지 못하는 한계를 문안에 적었다. 줄 번호는 QA-A-196 이후 기준이다.
>
> **QA-A-197 보강.** Codex #18 이 찾은 교차 로더 결함(같은 경로를 다른 종류의 캐시 로더로 부르면 다른 종류의 맵이 저장소에 들어감)을 고쳤고, REQ-P1A-102 에 "종류" 규칙을 더했다. 줄 번호는 QA-A-197 이후 기준이 아니라 QA-A-196 기준이다(이 카드의 변경은 `get_copy` 비교 한 줄과 `MapKind` 선언).
>
> **최종 문안의 위치 (QA-A-198·200).** `spec.md` 에 붙일 REQ-P1A-102~106 의 최종 문안은 `QA-A-198/spec_text_final.md` 이다. 이 보고서의 문안과 다른 곳은 그 파일이 맞다 — QA-A-200 이 적중의 파일 열기 확인, C ABI 밖으로 예외가 나가지 않는 보장, "동시 쓰기 미지원" 계약, 게인 로더의 커밋 전 계산을 더했다.

시작 HEAD `7004b93f` (`evidence/00_head.txt`). 코드·`spec.md` 변경 없음. 이 카드가 추가한 것은 이 보고서와 증거 파일뿐이다.

## 해결 내역 — 처음 초안이 가리킨 세 가지 (QA-A-193)

| # | 관측 (고치기 전) | 결정 | 해결 |
|---|---|---|---|
| ⚠1 | 캐시 적중이 모듈 전역 보정 저장소를 채우지 않았다 (shutdown+init 뒤 적중 → `OK`, 이어진 `xpe_offset_correct` → `CALIB_NOT_LOADED`) | 고친다 | **고침.** 적중도 그 맵을 전역 저장소에 올린다 (`calibration_cache.cpp:382-388`, 설치 함수 `:312-348`). 시험 `CacheGlobalStore.*CacheHitLeavesThatMapInTheGlobalStore` 3건 + `AfterShutdownAndInitACachedLoad…` |
| ⚠2 | `xpe_preprocess_shutdown()` 이 캐시를 비우지 않았다 (`api-spec.md:499` "모듈 종료까지 유효"와 어긋남) | 고친다 | **고침.** shutdown 이 `xpe_calib_cache_clear()` 를 부른다 (`preprocess.cpp:79`). 시험 `CacheGlobalStore.ShutdownEmptiesTheCache` |
| ⚠3 | 캐시 키는 경로 문자열뿐이라 파일을 덮어써도 옛 맵이 나온다 | 코드는 안 바꾸고 명시한다 | **명시함.** 세 `*_cached` 의 헤더 문서와 아래 문안에 "키는 경로 문자열, 파일이 바뀌면 `xpe_calib_cache_clear()` 필요"를 적었다. 시험 `CacheGlobalStore.ACacheKeyIsThePathStringSoAChangedFileNeedsCacheClear` 가 그 계약을 고정 |

QA-A-193 의 반증: ⚠1 은 오프셋·게인·결함 각각의 설치를 끈 팔 셋이 각자 자기 시험만 빨갛게 했고, ⚠2 는 shutdown 의 캐시 비우기를 뺀 팔이 `ShutdownEmptiesTheCache` 만 빨갛게 했다 (`QA-A-193/evidence/06_arms_summary.txt`).

QA-A-193 이후 QA-A-195 가 발견하고 QA-A-196 이 고친 것:

| # | 관측 (QA-A-195) | 해결 (QA-A-196) |
|---|---|---|
| ⚠4 | 캐시된 뒤 만료된 파일의 맵을 적중이 올렸다 (미스는 `-5`) | **고침.** 엔트리가 파일의 만료 시각을 들고, 적중마다 다시 검사한다. 만료면 `XPE_ERR_CALIBRATION_EXPIRED`, 저장소는 그대로 |
| ⚠5 | 캐시된 뒤 변조된 파일의 맵을 적중이 올렸다 (미스는 `-4`) | **고침.** 엔트리가 파일 크기·수정 시각을 들고, 다르면 적중을 취소해 미스 경로로 재적재한다(SHA-256 재검사). 같은 크기·같은 수정 시각은 못 잡는다 — 한계로 명시 |
| ⚠6 | 게인 품질 메타가 적중에서 직전 교정의 값으로 남았다 | **고침.** 엔트리가 설정 JSON 을 들고, 적중에서 미스와 같은 방식으로 다시 적용한다 |
| ⚠7 | 게인 다항식 파일에서 `gain_cached` 가 `XPE_ERR_NOT_INITIALIZED` (적재는 성공) | **고침.** `XPE_OK` 와 비운 버퍼를 돌려주고 캐시에는 넣지 않는다 |

## 형식과 번호

`spec.md` 의 정의는 `REQ-P1A-101` 이 마지막이고 `102` 이후는 spec·docs·소스 어디에도 쓰이지 않는다 (`grep REQ-P1A-10[2-9]|REQ-P1A-1[1-9][0-9]` 결과 0건). 아래에 **REQ-P1A-102 ~ 106** 을 제안한다. 인용 도구(`tools/docs/check_req_citations.py`)는 코드가 정의 없는 번호를 인용하면 막으므로, 소스 주석에 이 번호를 쓰기 전에 `spec.md` 에 정의가 먼저 들어가야 한다.

문안은 `REQ-P1A-016a`·`REQ-P1A-101` 의 형태를 따랐다: EARS 문장 → 줄 단위 "측정된 계약"(코드 근거 `파일:줄`) → `SRS`·`Traceability`. 아래 줄 번호는 모두 HEAD `7004b93f` 기준이다.

---

## 제안 문안 (복붙용)

### REQ-P1A-102: Cached Offset Map Loader

**When** `xpe_calib_load_offset_cached(filePath, offsetMapOut)` is called with non-NULL arguments, the module **shall** return in `offsetMapOut` a cache-owned view of the offset map of `filePath`, leave the module-global calibration store holding that map, and reach the verdict that loading the file through `xpe_calib_load_offset(filePath)` would reach, as follows: when the calibration cache holds an entry keyed by that path string whose recorded file size and last-write time equal the file's current ones, the module **shall** refuse the call with `XPE_ERR_CALIBRATION_EXPIRED`, leaving the store unchanged, if the entry's recorded expiry has passed, and otherwise **shall** return that entry without reading the file and install its map (with the file's timestamp and session id) into the store; in every other case (no entry, or an entry whose recorded size or last-write time differs from the file's, or a file whose size or last-write time cannot be read) it **shall** drop any such entry, load the file through `xpe_calib_load_offset(filePath)`, return that call's error code unchanged if it fails, and otherwise copy the loaded map into the cache and return the cache's view of the copy. The module **shall not** re-hash the file on a cache hit. When the entry was made by a cached loader of another kind (offset, gain, defect) than the one called, the module **shall** treat the call as a miss for this loader without dropping the entry: it loads through this loader's plain loader, which refuses the file (wrong XCal type), returns that code and leaves the store unchanged.

- **측정된 계약**:
  - `filePath` 또는 `offsetMapOut` 이 NULL 이면 `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:435`).
  - 호출 때마다 파일의 크기와 수정 시각을 **캐시를 보기 전에** 읽는다 (`stamp_of` `:60`, 호출 `:439`). 엔트리에는 평범한 로더가 파일을 읽기 **전에** 잰 값이 저장된다 — 읽는 도중 파일이 바뀌면 엔트리는 옛 값을 들고, 다음 호출이 차이를 보고 재적재한다.
  - 적중 판정 (`get_copy`, `:162`): 엔트리의 크기·수정 시각이 지금 값과 같지 않거나 둘 중 하나를 읽지 못했으면 엔트리를 지우고 미스로 처리 (`:171`); 같으면 엔트리의 만료 시각 `!= 0 && 지금 > 만료` 일 때 엔트리를 지우고 만료로 처리 (`:174`, 시각 비교는 `xcal_reader.cpp` 의 만료 검사와 같은 식과 시계 `system_clock`); 아니면 적중.
  - 만료: `XPE_ERR_CALIBRATION_EXPIRED`(-5), 저장소는 건드리지 않는다 (`:451`; 시험 `AMapCachedBeforeItsFileExpiredIsRefusedLikeAMissAndNotInstalled`).
  - 변조: 크기나 수정 시각이 달라지면 미스 경로가 평범한 로더를 불러 SHA-256 을 다시 검사하고 로더의 코드를 그대로 돌려준다 (관측: `-4`; 시험 `AMapCachedBeforeItsFileWasTamperedWithIsRefusedLikeAMiss`, 반증 팔: 비교를 빼면 이 시험과 두 재적재 시험이 빨강).
  - **종류**: 엔트리는 어느 캐시 로더가 만들었는지(오프셋·게인·결함)를 기록한다 (`MapKind`, `get_copy` 의 종류 비교). 같은 경로를 다른 종류의 로더로 부르면 엔트리를 두고 미스로 취급해, 그 종류의 평범한 로더가 파일을 거절하는 코드(관측: `-4`)를 돌려주고 저장소는 그대로다. 이 검사가 없던 시기(QA-A-196 까지)에는 오프셋으로 캐시한 경로를 게인 캐시 로더로 부르면 `OK` 와 함께 오프셋 화소가 게인 저장소에 들어갔고(게인 범위 [0.1, 10.0] 검사도 건너뜀), 반대 방향은 게인 맵이 오프셋 저장소에 들어갔다. 시험 `AMapCachedByOneLoaderIsRefusedByEveryOtherLoaderLikeAMiss` 가 6개 조합 전부와 "거절이 엔트리를 지우지 않는다"를 고정한다.
  - **한계**: 적중은 파일을 다시 해시하지 않는다. 같은 크기·같은 수정 시각으로 바뀐 파일은 감지하지 못하고, `xpe_calib_cache_clear()` 또는 shutdown 뒤에야 새 파일을 읽는다 (시험 `AChangeThatKeepsBothSizeAndWriteTimeIsNotNoticedUntilCacheClear`가 이 한계를 고정한다). 수정 시각의 해상도 안에서 일어난 쓰기도 같은 한계에 든다.
  - 적중: 같은 잠금 안에서 픽셀을 곧바로 전역 저장소가 소유할 배열로 **한 번** 복사하고 메타데이터를 받은 뒤 잠금 밖에서 전역 저장소에 설치한다 (`install_offset` `:388`, 호출 `:452`). 캐시 잠금과 `g_calib_mutex` 는 함께 잡지 않는다. `data` 포인터는 캐시의 것이며 호출자가 해제하지 않는다 (소유권 규칙 `api-spec.md:499`).
  - 설치되는 것: 오프셋 맵, 너비·높이, 파일의 `created_epoch_ms`, 세션 id — 평범한 로더가 쓰는 같은 필드 (`xpe_calib_load_offset.cpp:55-67`).
  - 적중에서 **반복하지 않는 것**: SHA-256·세션 검사.
  - 적중 비용 (3072×3072 오프셋 맵, FLOAT32 37.7 MB, 30회 중앙값, 크기·수정 시각 `stat` 포함): 적중 약 6.8 ms, 미스 약 70 ms (`QA-A-196/evidence/08_hit_cost_with_stat.txt`; `stat` 추가분은 약 0.2 ms, 직전 6.6 ms).
  - 실패한 적재: `xpe_calib_load_offset` 이 돌려준 코드를 그대로 반환 (`:460`). 관측: 없는 파일 → `XPE_ERR_IO_FAILED`(-9).
  - 성공한 적재 뒤 전역 저장소에 오프셋 맵이 없으면(`offset_map` 이 NULL 이거나 `offset_width` 가 0) `XPE_ERR_NOT_INITIALIZED` (`:471`).
  - 반환 버퍼: 형식 `XPE_PIXEL_FLOAT32`, `bitsAllocated = bitsStored = 32`, `width`·`height` 는 적재된 맵의 것, `dataSize = width × height × 4` (`:475-479`).
  - 캐시에 넣을 때 할당 실패 → `XPE_ERR_OUT_OF_MEMORY` (`:359`), 넣은 항목을 되읽지 못하면 `XPE_ERR_PROCESSING_FAILED` (`:368`). 적중의 복사에서 할당 실패는 `XPE_ERR_OUT_OF_MEMORY`, 엔트리 크기가 `width × height × 4` 와 다르면 `XPE_ERR_PROCESSING_FAILED` (`:187-189`).
  - 삽입과 되읽기는 한 번의 잠금 안에서 일어난다 (`put_and_get`, `#127`, QA-A-31).
  - 캐시 키는 호출자가 준 경로 문자열 그대로. 기본 용량 4 (`:329`), 가득 차면 가장 오래 쓰지 않은 항목을 퇴출, 같은 키로 다시 넣으면 옛 데이터를 해제하고 교체. 엔트리가 퇴출·교체·만료·파일 변경으로 지워지면 이전에 돌려준 뷰 포인터는 무효가 된다.
  - `xpe_preprocess_shutdown()` 은 캐시를 비운다 (`preprocess.cpp:79`; 시험 `ShutdownEmptiesTheCache`). 캐시 용량 설정은 shutdown 이 되돌리지 않는다.
- **SRS**: SRS-CALIB-NFR-003-CACHE (캐시 목록·색인 변경을 뮤텍스로 보호, `SRS-CALIB-001:470`). 캐시된 적재의 입출력 계약 자체를 정하는 SRS 요구는 없다.
- **Traceability**: SUP-01 (REQ-P1A-014 와 같은 지원 단위; 다른 SWU 번호는 이 초안에서 확인하지 못함)
- **Verification**: Test (`test_calibration_cache.cpp`, `test_calib_cache_ownership.cpp`, `test_calib_cache_concurrency.cpp`, `test_calib_cache_global_store.cpp`, `test_calib_cache_same_verdict.cpp`)

### REQ-P1A-103: Cached Gain Map Loader

**When** `xpe_calib_load_gain_cached(filePath, gainMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_gain` as the loader and the scalar gain map as the cached map, **shall** apply the gain quality metadata of the file (kept in the entry as its config JSON) again on a hit exactly as the load applies it, and, when the file loaded is a gain polynomial file, **shall** return `XPE_OK` with `gainMapOut` zeroed and cache nothing.

- **측정된 계약**:
  - NULL 인자 → `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:497`); 만료·변경 판정과 적중 설치 `:506-513` (`install_gain` `:399`); 적재 위임과 오류 그대로 반환 `:520`.
  - 적중의 설치는 평범한 게인 로더와 같이 스칼라 맵을 올리고 다항식 계수·`gain_poly_num_coeffs`·적합 범위를 지우며(둘은 대안이지 동시에 갖지 않는다), 너비·높이·타임스탬프·세션 id 를 쓴다 (`xpe_calib_load_gain.cpp:97-126`).
  - **품질 메타데이터**: 적중에서 엔트리의 설정 JSON 으로 `xpe_calib_apply_quality_meta_json` 을 다시 부른다 (`:513`). 평범한 로더가 쓰는 것과 같은 함수·같은 입력이므로 `xpe_calib_get_quality_meta` 가 미스 뒤와 같은 값을 낸다(`previous_r_squared` 연쇄 포함). 시험 `AGainHitReportsTheSameQualityMetadataAsAMiss`가 적중 뒤와 미스 뒤의 구조체 전체를 바이트 비교한다 (반증 팔: 적용을 빼면 이 시험만 빨강).
  - 성공한 적재 뒤 전역 저장소에 스칼라 게인 맵도 다항식도 없으면 `XPE_ERR_NOT_INITIALIZED` (`:538`).
  - **게인 다항식 파일**(`XCAL_TYPE_GAIN_POLY`): 적재가 성공하고 다항식이 전역 저장소에 있으면(스칼라 맵 없음) `XPE_OK` 를 돌려주고 `gainMapOut` 은 0 으로 비운다(`data` NULL, `dataSize` 0). 캐시에는 넣지 않으므로 호출마다 파일을 다시 읽고 검사한다 (`:534-537`; 시험 `AGainPolynomialFileLoadsAndTheCachedLoaderReportsSuccess`, 반증 팔: 이 분기를 끄면 그 시험만 빨강). 호출자는 반환 버퍼가 아니라 `xpe_gain_correct` 로 다항식을 쓴다.
  - 반환 버퍼(스칼라 맵): `XPE_PIXEL_FLOAT32`, 32비트, `dataSize = width × height × 4` (`:542-546`).
  - 캐시 규칙(키·만료 재검사·크기·수정 시각 비교·한계·용량·퇴출·소유권·shutdown 이 비움)은 REQ-P1A-102 와 같다 (같은 `g_calibCache`).
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

### REQ-P1A-104: Cached Defect Map Loader

**When** `xpe_calib_load_defect_cached(filePath, defectMapOut)` is called with non-NULL arguments, the module **shall** behave as REQ-P1A-102 with `xpe_calib_load_defect_map` as the loader and the defect map as the cached map.

- **측정된 계약**:
  - NULL 인자 → `XPE_ERR_INVALID_INPUT` (`calibration_cache.cpp:572`); 만료·변경 판정과 적중 설치 `:581-585` (`install_defect` `:415`: 결함 맵·너비·높이); 적재 위임과 오류 그대로 반환 `:593`.
  - 성공한 적재 뒤 전역 저장소에 결함 맵이 없으면(`defect_map` 이 NULL 이거나 `defect_width` 가 0) `XPE_ERR_NOT_INITIALIZED` (`:604`).
  - 반환 버퍼: `XPE_PIXEL_UINT8`, `bitsAllocated = bitsStored = 8`, `dataSize = width × height` (`:608-612`).
  - 캐시 규칙은 REQ-P1A-102 와 같다. 만료·변조 시험은 결함 맵도 같은 시험에서 확인한다.
- **SRS**: SRS-CALIB-NFR-003-CACHE. 캐시된 적재 계약 자체는 SRS 에 없다.
- **Traceability**: SUP-01
- **Verification**: Test (REQ-P1A-102 와 같은 파일)

> **세 요구가 공유하는 캐시 규칙.** 캐시는 세 적재 함수가 **하나**를 공유한다 (`g_calibCache`, `calibration_cache.cpp:333`). 키가 경로 문자열뿐이라 세 종류의 맵이 같은 경로로 호출되면 같은 항목을 가리키지만, 엔트리가 만든 로더의 종류를 기록하므로 다른 종류의 로더는 그 항목을 받지 못하고 평범한 로더의 거절 코드를 받는다 (QA-A-197). `xpe_calib_cache_clear` · `xpe_calib_cache_set_max_size` 의 계약은 SRS-CALIB-FUNC-038 (`SRS-CALIB-001:469`) 이 이름으로 묶는다 — 여기서는 다루지 않는다. 헤더 문서(`preprocess_api.h`)는 QA-A-196 에서 위 규칙에 맞게 다시 썼다.

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

- 적중이 **파일을 다시 해시한다**고는 쓰지 않았다 (크기·수정 시각 비교로 갈음하며, 같은 크기·같은 수정 시각의 변경은 감지하지 못한다고 적었다).
- 적중이 **세션 검사를 다시 한다**고는 쓰지 않았다.
- 캐시가 **같은 크기·같은 수정 시각의 파일 변경을 감지한다**고는 쓰지 않았다.
- shutdown 이 **캐시 용량 설정을 되돌린다**고는 쓰지 않았다.
- `xpe_preprocess_pipeline_ex` 가 `calibState` 의 적재 플래그를 확인한다고는 쓰지 않았다.
- `xpe_preprocess_version` 이 빌드·CMake 버전에서 파생된다고는 쓰지 않았다 (소스 상수).
- `xpe_calib_load_gain_cached` 가 게인 다항식 파일을 **캐시한다**거나 호출자에게 **버퍼로 돌려준다**고는 쓰지 않았다 (성공을 돌려주고 비운 버퍼를 준다).

## 지금 적힌 문서와 어긋난 곳 (참고)

- `api-spec.md:31` 의 `xpe_preprocess | 45` 는 수출 C ABI 48개와 다르다 (`QA-A-189`). 이 카드들에서는 안 고침.
- `api-spec.md:499` 의 "모듈 종료" 시 캐시 소유 뷰 무효화 문구는 **QA-A-193 으로 코드와 맞아졌다**.
- `preprocess_api.h` 의 세 `*_cached` 문서의 오류 코드 목록은 **QA-A-193 에서 코드에 맞게 다시 썼다**.

## 결정 재료 (한 줄씩) — 갱신

1. **REQ-P1A-102~104 (`*_cached`)** — ⚠1·⚠2·⚠4~⚠7 은 고쳤다. 남은 결정: (a) 같은 크기·같은 수정 시각의 변경을 못 잡는 한계를 요구로 굳힐지, 적중에서 해시를 다시 하는 선택(적중 비용이 미스 수준으로 오른다)을 볼지, (b) 게인 다항식 파일이 성공과 비운 버퍼를 돌려주는 계약을 굳힐지(호출자가 NULL 을 역참조하면 호출자의 결함).
2. **캐시 용량 설정이 shutdown 에서 되돌려지지 않는 것** — 결정에 필요한 것: 이것도 shutdown 이 되돌릴 모듈 전역으로 볼지 (`xpe_calib_mode_reset_globals` 가 QA-A-120 에서 "전역 전부"를 되돌리게 한 것과 같은 질문).
3. **REQ-P1A-105 (`_ex`)** — 결정에 필요한 것: `calibState` 가 읽히지 않는 현 동작을 요구로 굳힐지(그러면 인자는 "호환용"으로 남음), 인자를 없애는 ABI 변경을 별건으로 볼지. 제품 호출자는 없다 (QA-A-189).
4. **REQ-P1A-106 (`version`)** — 결정에 필요한 것: 문자열 값(`"0.1.0"` 대 CMake `1.0.0`)을 요구로 고정할지, 계약만 적을지. 시험이 없어 이 요구는 시험도 함께 세워야 한다.

## Gaps / Residual-risk

- (해소) 게인 다항식 파일에 대한 `xpe_calib_load_gain_cached` 반환값은 QA-A-193 에서 관측했다 (`XPE_ERR_NOT_INITIALIZED`, 위 REQ-P1A-103).
- (해소) 적중의 만료 재검사는 QA-A-196 에서 구현되고 시험으로 고정됐다. 남은 것: 만료 경계(만료 시각과 같은 밀리초)의 동작은 관측하지 않았다 (코드는 읽기 검사와 같은 `지금 > 만료`).
- 파일 크기·수정 시각의 해상도, 시계가 거꾸로 가는 경우, 네트워크 드라이브에서의 수정 시각 신뢰성은 관측하지 않았다.
- 다항식 파일과 결함 맵의 변조·만료는 (결함 맵은 시험이 확인, 다항식은 캐시하지 않으므로 해당 없음) — 다항식 파일이 만료·변조일 때의 반환은 평범한 로더의 코드를 그대로 쓴다는 코드 읽기만 있다.
- (해소) 같은 경로를 서로 다른 종류의 `*_cached` 로 부를 때의 동작은 QA-A-197 에서 6개 조합 전부 관측하고 고쳤다.
- NULL `calibState` 로 단계를 실제로 돌리는 경우(우회 없음)는 시험도 관측도 하지 않았다. 코드가 `calibState` 를 읽지 않는다는 것(`pipeline.cpp:439`)에 근거한다.
- `_ex` 가 `xpe_preprocess_pipeline` 과 같은 결과를 내는지는 같은 `pipeline_core` 를 부른다는 코드 읽기로 적었다. 같은 입력으로 두 진입점을 나란히 돌려 비교하지는 않았다.
- `Traceability` 칸의 SWU 번호는 기존 REQ-P1A-014~016a 의 `SUP-01` 을 따랐을 뿐 SWU 정의 문서와 대조하지 않았다.
- 문안은 초안이다. `spec.md` 에 넣기 전에 번호와 `SRS`·`Traceability` 칸을 리더가 확정한다.
