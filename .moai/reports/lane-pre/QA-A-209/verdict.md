# QA-A-209 — 설정 JSON 은 최상위 키만 읽는다 (파이프라인·고스트·비선형성·오프셋 생성·XCal 설정 블록)

기준 커밋 `967fb146`(QA-A-208e) 위. 출처: QA-A-208 evidence/09(앞에 `{"nested":{"bypassGain":true}}` 를 두면 중첩 값이 먼저 읽혀 게인 단계가 꺼진다). 이슈 #233. 리더의 추가 요청(`dose_min`/`dose_max` 의 첫 출현 검색)을 포함한다.

## 1. 계약 (api-spec 으로 옮길 한 단락)

모듈의 모든 설정 텍스트(파이프라인, 고스트 보정기, 비선형성 단계, 오프셋 생성의 설정, 그리고 다항식 게인·비선형성 LUT 파일의 설정 블록)는 **하나의 유효한 JSON 객체**이며 키는 **최상위**에서만 읽는다. 중첩된 객체나 배열 안에만 있는 키는 주어지지 않은 것(기본값)이고, 같은 이름의 최상위 키가 중첩 키 옆에 있으면 최상위 키가 이긴다. 최상위 키가 두 번 나오거나 텍스트가 하나의 JSON 객체가 아니면(안 닫힘, 최상위 배열, 객체 뒤의 글자, 문자열 속 날 제어 문자, 작은따옴표, 끝 쉼표) `XPE_ERR_CONFIG_INVALID` 이고 영상·메타데이터·교정 저장소·핸들·파일은 호출 전 그대로다. 빈 문자열 값(`""`)과 객체·배열인 값은 주어지지 않은 것으로 읽고(GUI 가 미설정 옵션을 `""` 로 보내는 규칙, QA-A-202b), 숫자 읽기가 필요한 키는 따옴표 없는 JSON 숫자만 숫자로 인정한다(따옴표 안의 숫자는 이전처럼 주어지지 않은 것). NULL 텍스트 포인터는 모든 기본값이다. 파이프라인은 비선형성 단계가 나중에 읽을 키(`panel.*`)도 첫 단계 전에 같은 규칙으로 확인하므로, 그 키의 중복이 실행 도중에 호출을 실패시키지 않는다.

## 2. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 아래 3절 표의 모든 읽기 경로가 `xpe_config_get_string` / `xpe_config_get_double`(208b 의 nlohmann 기반 `xpe_json_top_level_scalar` 위의 얇은 오류 대응)을 통한다. 새 파서는 만들지 않았고, `strstr` 로 첫 출현을 찾던 코드(`xpe_json_find_scalar`, `xpe_json_get_string`, `xpe_json_get_double`, 오프셋 생성의 사본 `json_get_string`/`json_get_double`)는 삭제됐다. | 코드 diff |
| 2 | (진입점, 키) 조합 25개가 같은 14행 표를 거친다(중첩에만 있음 / 중첩+최상위 / 최상위 중복 / 깨진 JSON / 빈 값 / 정상 …). 거부 행은 `CONFIG_INVALID` 이고 진입점별 "불변"을 확인한다. | 새 시험 8건 |
| 3 | GUI·clients 가 보내는 설정 JSON 은 없거나 평평한 한 키라서 이 변경이 그들을 깨뜨리지 않는다. | 5절, `evidence/07` |
| 4 | 기존 시험 전부 통과. 두 표 시험(`TheAcceptedNotationOf…`)만 입력 구성 방식이 새 규칙에 맞게 고쳐졌다(6절). | 전체 실행 |

## 3. 호출자 전수 (modules/preprocess, `xpe_json_get_*` 와 같은 일을 하던 모든 곳)

| 함수 (파일) | 읽는 키 | 이전 | 이후 |
|---|---|---|---|
| `PipelineConfig::fromJson` (`pipeline.cpp`) — `xpe_preprocess_pipeline`, `_ex`, `_batch` 가 호출 | `bypassReadout`·`bypassTemp`·`bypassOffset`·`bypassNonlinearity`·`bypassGain`·`bypassBinning`·`bypassDefect`·`bypassGhost`(문자열 `"true"` 또는 `true`), `detectorTempC`, `binningMode`; 그리고 비선형성 단계의 9개 키(미리 확인) | 첫 출현 `strstr` | 최상위만; 중복·깨짐 → `CONFIG_INVALID`(교정 파일을 읽기 전, 영상·메타 불변) |
| `xpe_ghost_create` (`ghost_correct.cpp`) | `tier`, `alpha1`, `tau1`, `alpha2`, `tau2`, `tier2Threshold`, `nlcscBeta` | 첫 출현 | 최상위만; 핸들을 안 돌려줌 |
| `xpe_nonlinearity_apply` (`nonlinearity_correct.cpp`; `xpe_nonlinearity_correct` 와 파이프라인 단계가 호출) | `panel.linear`, `panel.nonlinearity_mode`, `panel.target_platform` | 첫 출현(`panel.linear` 가 `true` 이면 나머지는 읽기 전에 반환) | 최상위만, 세 키를 **결정 전에 함께** 읽음; `CONFIG_INVALID` 는 영상 불변 |
| `xpe_nonlinearity_apply_polynomial` | `panel.nonlin_poly_c0`…`c4`, `panel.adc_max` | 첫 출현 `strtod` | 최상위 JSON 숫자만; `CONFIG_INVALID` 는 "거부 후 LUT 로 대체"(`INVALID_CALIB_DATA`)와 구별되어 호출자에게 닿음 |
| `xpe_calib_stage_gain` (`xpe_calib_load_gain.cpp`) | `dose_min`, `dose_max` (다항식 게인 파일의 설정 블록, 저장된 길이) | 같은 `strstr` + "기본값을 달리해 두 번 물어 존재를 가늠" | 최상위 JSON 숫자만(존재 여부를 직접 받음, 두 번 묻는 꼼수 삭제); 중복 → `CONFIG_INVALID`, 맵·품질 불변 |
| `xpe_calib_load_nonlin_lut` | `xcal_nonlin_extension_start` (LUT 파일의 설정 블록, 저장된 길이) | 첫 출현 | 최상위만; 중복·JSON 아님 → `CONFIG_INVALID`, 표 불변 |
| `parse_offset_generation_config` (`xpe_calib_generate_offset_methods.cpp`, `xpe_calib_generate_offset`) | `method`(JSON 문자열일 때만), `sigma`, `max_iter`, `lower_percentile`, `upper_percentile` | 이 파일의 사본 `strstr` 두 함수 | 공용 읽기로 교체(사본 삭제); `noexcept` 함수 안이라 `bad_alloc` 은 `OUT_OF_MEMORY` 로 받음 |
| `xpe_temp_compensate` (`temp_compensate.cpp:58`) | (설정 인자를 받지만 `(void)` 로 버림) | — | 변경 없음 |
| `xpe_preprocess_init` (`preprocess.cpp:57`) | 키를 읽지 않는다. 텍스트를 nlohmann 으로 파싱해 깨졌으면(`is_discarded`) `CONFIG_INVALID` 로 거부하고 아무것도 보관하지 않는다 | 이미 파싱 방식 | 변경 없음(키가 없으니 중복·첫 출현 문제가 없다; nlohmann 의 파싱은 중복 키를 거부하지 않고 마지막 값을 쓰지만 읽는 값이 없다) |
| `modules/gsvg/src/gsvg.cpp` | `vignette_correction`, `grid_suppression`, `virtual_grid`, `vg_table_path` (자체 `json_get_bool`/`json_get_string`) | 자체 구현 | **이 레인 밖(Lane B)** — 위치만 확인했고 건드리지 않음. 같은 검토가 필요하면 별도 카드 |

## 4. 시험 (`test_config_strict_parse.cpp`, 새 8건)

14행의 표(`@` 키, `$` 효과 있는 값, `%` 효과 없는 값, `~` 진입점이 필요로 하는 다른 멤버): 중첩에만(효과 없음) · 중첩 효과 / 최상위 비효과(효과 없음) · 중첩 비효과 / 최상위 효과(효과 있음) · 중첩 배열 속(효과 없음) · 최상위 중복 3종(거부) · 안 닫힘 · 최상위 배열 · 객체 뒤 글자 · JSON 아님 · 깨진 중첩 · 빈 값(효과 없음) · 정상(효과 있음).

| 시험 | 진입점·키 | "효과"를 보는 법 | 거부 시 불변 확인 |
|---|---|---|---|
| `ThePipelineEntryPointsRead…` | `xpe_preprocess_pipeline` / `_ex` / `_batch` × `bypassOffset`; `detectorTempC`, `binningMode`; `panel.*` 6개 키(미리 확인) | 오프셋 단계가 돌면 `CALIB_NOT_LOADED`, 우회되면 `OK` | 영상 바이트·메타데이터 |
| `ThePipelineWithARefusedTopLevelConfig…` | 교정 경로를 준 `pipeline` + `batch`, 중복 `panel.linear` | 교정이 실제로 안 읽힘 | 교정 저장소(오프셋 적재 여부), 영상 2장, 메타 |
| `TheGhostCorrectorReads…` | `alpha1`, `tau2`, `tier`, `nlcscBeta` | `on` 이 잘못된 숫자라 읽히면 거부 | 핸들이 안 돌아옴 |
| `TheNonlinearityStageReads…` | `panel.nonlinearity_mode`, `panel.nonlin_poly_c1`, `panel.target_platform`, `panel.linear` | 다항식 2x 가 프레임 1000 을 2000 으로, `panel.linear` 는 건너뜀 | 화소 |
| `TheOffsetGenerationReads…` | `sigma`, `method`, `max_iter`, `lower_percentile` | 범위 밖 `on` 이 읽히면 거부 | 출력 파일이 안 생김 |
| `TheConfigBlockOfAPolynomialGainFile…` | `dose_min`(`dose_max` 는 최상위에 둠) | 범위가 읽히면 "without a dose range" 알림이 없음 | 이전 게인 맵(250) |
| `TheConfigBlockOfANonlinearityLutFile…` | `xcal_nonlin_extension_start` | 표 끝을 넘는 `on` 이 읽히면 `INVALID_CALIB_DATA` | 이전 LUT(1000→500) |
| `ANullConfigPointerStillMeansTheDefaults` | NULL 텍스트 | 모든 기본값(고스트 `OK`, 파이프라인은 float 결과가 버퍼에 안 맞아 `BUFFER_TOO_SMALL`) | — |

## 5. GUI·clients 가 보내는 설정 (소스 인용)

`evidence/07_gui_clients_config_census.txt` (검색 대조군: `xpe_preprocess_init` 18건, `xpe_nonlinearity_correct` 8건이 잡힘).

- GUI: `gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs:61` `xpe_preprocess_init(null)`, `:143` `xpe_nonlinearity_correct(ref offsetOut, null)` — 설정 텍스트를 보내지 않는다. `xpe_ghost_create`·`xpe_preprocess_pipeline*` 호출은 gui·clients 의 C# 소스에 0건.
- clients: 통합 시험이 `init(IntPtr.Zero)`, `generateOffset(…, offsetPath, null)`, `correct(ref image, null)`, 그리고 `NonlinearityStageWiringTests.cs:139` 의 평평한 한 키 `{"panel.linear":"false"}` 하나만 보낸다.
- 따라서 중첩·중복·깨진 JSON 을 보내는 호출자가 저장소 안에 없고, 규칙이 바뀌어도 깨지는 소스는 없다. **관찰하지 못한 것**: GUI 가 미설정 옵션을 `""` 로 보낸다는 202b 의 서술은 이 소스 어디에서도 확인되지 않았다(GUI 는 설정 자체를 안 보냄). 규칙은 그 관례를 그대로 지원한다.

## 6. 동작 변화와 기존 시험 수정

- 설정은 **유효한 JSON** 이어야 한다. 이전에는 한 글자도 파싱하지 않고 문자열 검색으로 읽어 작은따옴표·끝 쉼표·날 제어 문자 등이 우연히 통과했다.
- 기존 시험 2건(`TheAcceptedNotationOfThePipelineEntryPointsIsFixedByATable`, `…OfGhostCreateIsFixedByATable`)은 값에 **날 TAB 문자**를 JSON 문자열 안에 그대로 넣어 설정을 만들었다. 날 제어 문자는 JSON 이 아니라서 새 규칙에서는 거부됐고, 입력 구성을 `jsonStringBody()`(따옴표·역슬래시·제어 문자를 이스케이프)로 고쳤다. 숫자 변환기가 보는 값(탭이 앞에 붙은 숫자)과 행·기대는 그대로다.
- `xpe_nonlinearity_correct` 는 이제 `XPE_ERR_CONFIG_INVALID` 를 돌려줄 수 있다(키 중복·JSON 아님). 이 코드는 문서화됐다.
- 비선형성 LUT 파일의 설정 블록은 JSON 객체여야 한다(블록이 없거나 공백뿐이면 그대로 통과). 생성기가 쓰는 블록은 JSON 이다.
- 키의 값이 객체·배열이면 "주어지지 않음"(이전과 같다), 따옴표 안의 숫자는 숫자 키에서 "주어지지 않음"(이전과 같다).
- 비용: **파이프라인 호출 한 번의 설정 읽기가 1.36 µs → 24.7 µs**(8×8 프레임, 우회 8개, 아무 단계도 안 도는 호출 20000회 평균; 옛 읽기를 되살린 반증 빌드와 이번 빌드를 같은 프로그램으로 비교, `evidence/08`). 호출당 키 19개를 각각 파싱하기 때문이고(우회 8 + 숫자 2 + 비선형성 9), 큰 프레임을 처리하는 호출에서는 무시할 만하지만 작은 프레임을 빠르게 반복 호출하는 용도에서는 보일 수 있다. 한 번의 파싱으로 여러 키를 읽도록 줄일 수 있으나 이번 카드에서는 하지 않았다.
- 공개 헤더 문서: `xpe_preprocess_pipeline` 에 위 읽기 규칙(한 곳)을 적고 `_ex`·`_batch`·`xpe_ghost_create`·`xpe_nonlinearity_correct`·`xpe_calib_generate_offset`·`xpe_calib_load_nonlin_lut`·`xpe_calib_load_gain`(다항식 `dose_min`/`dose_max`)이 그것을 가리키게 했다.

## 7. 수정 전 / 후, 반증

수정 전 상태는 208 evidence/09 가 보인 대로다(중첩 값이 먼저 읽힘). 반증(한 번에 하나, 전체 빌드, 복원 뒤 `cmp` 동일; `ConfigStrictParse.*` 실행):

| 팔 | 손상 | 빨개진 시험 (새 시험 8건 중 / 기존) |
|---|---|---|
| strstr_readers | 두 읽기 함수를 이 카드가 지운 첫 출현 `strstr` 본으로 되돌림(리더의 반증) | 새 시험 7건(NULL 시험만 그대로 초록) / 표 시험 2건(JSON 이스케이프 입력을 `strstr` 은 풀지 못함) |
| dup_not_detected | 최상위 중복 검사 삭제 | 새 시험 7건 / 208 시험 2건 |
| nested_keys_counted | 깊이 1 이 아닌 키도 셈 | 새 시험 6건(파이프라인-교정경로 시험은 초록) / 208 시험 2건 |
| pipeline_no_upfront_nonlin_keys | 파이프라인의 비선형성 키 사전 확인 삭제 | 파이프라인 시험 2건 |
| lut_error_ignored | LUT 적재가 읽기 오류를 무시 | LUT 시험 |
| dose_error_ignored | 다항식 게인 적재가 읽기 오류를 무시 | 다항식 게인 시험 |

각 팔의 정확한 목록: `evidence/arm_*_dll.txt`.

## 8. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| preprocess DLL 시험 | 788 실행 / 780 통과 / 8 건너뜀, 섞기도 동일 |
| preprocess OOM exe | 45 통과 |
| 할당 실패 스윕 | 전부 통과. 파이프라인 진입점의 지점 수는 14·43·55·74·74 → 464·493·567·524·524 (호출당 설정 파싱 19회가 할당함; 예외가 밖으로 나간 경우 없음, 상태 불변) |
| common OOM / common | 12 / 69 통과 |
| `ctest -N` | 954 (직전 946 + 새 시험 8) |
| 수출 이름 / 헤더 문서 / 프리셋 | 48·16 불변 / 0 findings / 12 of 12 |

## 9. 미검증 (Gaps)

- **`xpe_nonlinearity_correct` 의 할당 실패**: 이 함수에는 아직 `bad_alloc` 가드가 없다(QA-A-204 3/3 의 몫). 이번 변경이 그 안에서 할당(파싱)을 늘렸지만 그 단독 진입점의 할당 실패 스윕은 없다(파이프라인 스윕은 비선형성 단계를 우회해 돈다).
- **gsvg(Lane B)** 의 자체 설정 읽기는 확인하지 않았다(위치만).
- GUI 의 `""` 관례는 소스에서 확인되지 않았다(5절). 현장의 설정 파일·외부 호출자는 점검할 수 없다.
- 파이프라인 호출당 +23 µs 외의 처리량 영향은 재지 않았다.
- `xpe_preprocess_pipeline_ex` 의 교정 경로 + 거부 조합은 확인하지 않았다(이 진입점은 교정 경로 인자를 받지 않는다); `pipeline`·`batch` 만 확인했다.
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만.

## 10. 잔여 위험

- 설정 JSON 을 손으로 쓰는 외부 호출자가 비표준 JSON(작은따옴표, 끝 쉼표)을 보내고 있었다면 이제 `CONFIG_INVALID` 를 받는다.
- 비선형성 LUT 파일을 외부 도구가 JSON 이 아닌 설정 블록으로 쓴다면 적재가 거부된다(이 저장소의 생성기는 JSON 을 쓴다).
