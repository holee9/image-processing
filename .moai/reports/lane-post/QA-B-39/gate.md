# QA-B-39 게이트 보고서 — post 6모듈 공개 헤더 Doxygen ↔ 구현 대조

**카드**: QA-B-39 (#133, A-30 유형)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-39/`
**커밋 6건 (모듈당 1)**: `e527374` enhance_basic · `8f0587c` enhance_advanced · `fe4c861` ai · `543e455` display · `5c94ece` dicom · `2aeeb35` gsvg
**코드 변경 0 — 주석만.** 발견한 코드 결함은 §5 에 보고만 한다.

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 6개 공개 헤더의 export 함수 전수를 구현 줄과 대조했다 | PASS |
| C2 | 불일치 **38건**을 찾아 주석을 구현에 맞췄다 | PASS |
| C3 | 코드 결함 **5건**은 고치지 않고 보고만 했다 | PASS |
| C4 | api-spec 불일치 **7건** 목록을 냈다 (반영은 leader) | PASS |
| C5 | 빌드 경고 0, ctest 무회귀 451 / 198 / 144 | PASS |

---

## 2. 모듈별 대조표

판정 기준: 주석이 말하는 것과 구현이 하는 것이 다르면 **불일치**, 구현이 하는데
주석에 없으면 **누락**. 둘 다 "주석을 고친다" 로 처리했다.

### 2.1 `enhance_basic` — 불일치 8건 (`e527374`)

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| (파일) | export 수 | "7 exported" | 8개 | 불일치 |
| (파일) | "All functions operate in-place on float32" | 전부 in-place | `version` 은 이미지 없음, `noise_estimate_sigma` 는 읽기만 | 불일치 |
| `log_transform` | @return | OK / INVALID_INPUT | + **UNSUPPORTED_FORMAT** (`validate_float32_image`) | 누락 |
| `log_inverse` | @return | 동일 | 동일 | 누락 |
| `noise_reduce` | @return | "XPE_OK on success" 뿐 | mode·파라미터별 INVALID_INPUT, UNSUPPORTED_FORMAT | 누락 |
| `noise_estimate_sigma` | @param | NULL 언급 없음 | `img`·`outSigma` 둘 다 NULL 거부, 빈 이미지도 오류 | 누락 |
| `contrast_enhance` | @return | clip/tile 만 | + **이미지가 타일 격자의 2배보다 작으면 INVALID_INPUT** (`:136`) | 누락 |
| `calc_exposure_index` | @return | PROCESSING_FAILED(mean<=0) 만 | + 널·빈 이미지 INVALID_INPUT, 실패 시 `*outEI`/`*outDI` = 0.0 | 누락 |

빈 이미지 처리가 함수마다 갈린다는 사실도 각 `@param` 에 적었다 —
`noise_reduce`·`contrast_enhance`·`edge_enhance` 는 `XPE_OK`,
`noise_estimate_sigma`·`calc_exposure_index` 는 `INVALID_INPUT`.
**구현 사실이므로 코드를 맞추지 않고 문서를 맞췄다**(§5-D1 로 보고).

### 2.2 `enhance_advanced` — 불일치 6건 (`8f0587c`)

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| `init` | 설명 | "with default or **custom configuration**" | JSON 이 파싱되는지만 보고 **값을 저장하지 않는다** (`xpe_enhance_advanced.cpp:46-54` TODO) | **불일치(함정)** |
| `init` | @return | "error code on failure" | 빈 문자열·파싱 실패 = CONFIG_INVALID, `xpe_init` 의 NOT_INITIALIZED 는 성공 취급 | 누락 |
| `shutdown` | 설명 | "release all resources" | 해제할 자원 없음, 플래그만 내림, `xpe_common` 은 그대로 | 불일치 |
| 처리 4함수 | @return | "error code on failure" 한 줄 | NOT_INITIALIZED / UNSUPPORTED_FORMAT / CONFIG_INVALID / INTERNAL / SAFETY_VIOLATION | 누락 |
| `detect_collimation` | @return | 없음 | **저신뢰도 폴백이 오류가 아니라 XPE_OK + 전체 영역** (REQ-ADV-041) | 누락 |
| `calc_exposure_index` | 이름 충돌 | 없음 | `xpe_enhance_basic` 에도 같은 이름, **계약이 다름**(이쪽만 init 필요) | 누락 |

### 2.3 `ai` — 불일치 10건 (`fe4c861`) — **6모듈 중 가장 컸다**

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| (파일) | stub 여부 | **한 마디도 없음** | 기본 빌드가 stub (`XPE_AI_USE_ONNXRUNTIME` 기본 OFF, `ai.cpp:9`) | **불일치(최대)** |
| (파일) | export 수 | "7 functions + 2 extended" | 10개 | 불일치 |
| `init` | @return CONFIG_INVALID | 문서에 있음 | **어떤 입력으로도 반환되지 않음** — 잘못된 JSON 은 경고 후 기본값 | **불일치(A-30 유형)** |
| `init` | @return IO_FAILED | 문서에 있음 | stub 에는 워커 기동이 없음 | 불일치 |
| `init` | 재호출 | 없음 | 이미 초기화면 무시하고 XPE_OK (`:272-275`) | 누락 |
| `bodypart_recognize` | bufLen | "if bufLen is insufficient" | **`< 1` 만 오류**. 짧은 버퍼는 라벨이 잘린 채 성공 형태 | **불일치(함정)** |
| `stitch_estimate_size` | @return PROCESSING_FAILED | 문서에 있음 | **반환 경로 없음**. 또 init 없이 동작하고 4096 으로 잘림 | 불일치 |
| `bone_suppress` | @return | 널만 | + 두 버퍼 크기 불일치 INVALID_INPUT | 누락 |
| `get_model_card` | 실패 동작 | 없음 | 못 찾아도 buf 에 error JSON 을 쓰고 IO_FAILED | 누락 |
| `set_fallback_mode` | 설명 | "AI functions will attempt retries" | **플래그를 저장만 하고 읽는 경로가 없음** (`:595`) | **불일치(함정)** |

추론 4함수(`bodypart_recognize`·`stitch_images`·`bone_suppress`·`dl_denoise`)는
stub 빌드에서 인자 검증 뒤 **무조건 PROCESSING_FAILED** 다. 파일 주석에
BUILD-DEPENDENT BEHAVIOUR 절을 넣고 함수마다 "ONNX 빌드 전용 / stub 에서 도달 불가" 를
표시했다. 이 레인의 상시 제약 "stub GREEN 을 AI 동작 검증으로 읽지 말 것" 이
**헤더에는 적혀 있지 않았다** — 그것이 이 모듈 최대의 불일치다.

### 2.4 `display` — 불일치 7건 (`543e455`)

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| (파일) | export 수 | "5 exported functions" | 6개 | 불일치 |
| (파일) | 공유 검증기 | 없음 | **format 을 dataSize 보다 먼저 본다** — UINT16 버퍼는 dataSize 도 틀렸어도 UNSUPPORTED_FORMAT | 누락 |
| `apply_modality_lut` | @return | TABLE/LINEAR 검증만 | + mode 가 둘 다 아니면 INVALID_INPUT (`:66-67`) | 누락 |
| `apply_voi_lut` | @return | width 만 | + mode 가 셋 중 하나가 아니면 INVALID_INPUT (`:63-64`) | 누락 |
| `voi_preset_create` | 부작용 | center/width | **mode·minOut·maxOut 까지 전부 덮어씀**(LINEAR, 0.0, 255.0) | **불일치** |
| `apply_presentation_lut` | 실패 시 상태 | 없음 | 할당이 free 보다 먼저라 **오류 시 이미지 원상태** | 누락 |
| `gsdf_calibrate` | 비정상 입력 | 없음 | **거부하지 않고 조용히 보정**(0 이하 → 0.01, max<=min → min+1). min/max 만 사용 | **불일치(함정)** |

### 2.5 `dicom` — 불일치 8건 (`5c94ece`)

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| `cfind_mwl` | 쿼리 키 | PatientID / PatientName / **ScheduledStationAETitle** / Modality / **ScheduledProcedureStepStartDate** | PatientID / PatientName / Modality / **AccessionNumber** (`buildFindRequest:324-335`) | **불일치(최대·A-30 유형)** |
| `cfind_mwl` | @return | OK/INVALID/NETWORK/BUFFER | + **PROCESSING_FAILED**(JSON 파싱 실패, association 이후 판정 — QA-B-37 관측) | 누락 |
| (파일) | catch-all | "mapped to XpeErrorCode" | 구체적으로 PROCESSING_FAILED(네트워크는 NETWORK_FAILED) | 누락 |
| `read_image` | @return | 널/OOM/PROCESSING | + **DICOM_INVALID**(PixelData 없음, Rows/Columns 0) | 누락 |
| `get_metadata` | @return | 널만 | + DICOM_INVALID(dataset 없음) | 누락 |
| `write`/`write_j2k` | @return | 널만 | + dataSize 불일치 INVALID_INPUT (#123) | 누락 |
| `validate` | XPE_OK 의미 | "on success" | **"리포트를 만들었다" 이지 "적합하다" 가 아님.** BUFFER_TOO_SMALL 시 버퍼는 JSON 이 아니라 uint32 크기 | 불일치 |
| `cancel` | 동작 | "No-op if no operation is active" | **진입 시 플래그를 지우므로 호출 전 cancel 은 무효**, 관측 지점 2곳뿐 (QA-B-34/37 관측) | **불일치(함정)** |

`open` 에는 "meta 헤더 없는 파일도 열린다 — 적합성 판정은 `validate` 의 몫" 을 추가했고,
`validate` 에는 #139 로 들어온 meta 그룹 검사 범위를 적었다.

### 2.6 `gsvg` — 불일치 4건 (`2aeeb35`) — **6모듈 중 가장 정확했다**

| 함수 | 항목 | 주석 | 구현 | 판정 |
|---|---|---|---|---|
| `init` | "Parses the config JSON" | JSON 파서 | **문자열 스캔**. 키를 찾아 콜론 뒤 `true`/`false` 토큰만 봄 (`gsvg.cpp:56-80`) | **불일치(함정)** |
| `init` | @return | 없음 | 잘못된 config 도 XPE_OK, `handleOut` 은 성공 시에만 기록 | 누락 |
| `process` | `gainMap` | "width*height float32" | **길이를 검증하지 않음** — 짧은 맵은 범위 넘어 읽힘 | 누락 |
| `process` | "exact bitwise copy" | 항상 복사 | 별칭(`src == dst`)이면 복사 자체가 없음 (`:220-223`) | 불일치 |

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 대조 대상 | 헤더 7개(`*_api.h`), `XPE_API` 선언 **46건** (8+1+7+10+6+10+4). `xpe_enhance_advanced_version` 이 두 헤더에 중복 선언되므로 **고유 심볼 45개** | `grep -c "^XPE_API"` 헤더별 |
| 이전 ctest | 451 / 198 / 144 | QA-B-38 `_verify.log` |
| 현재 ctest | **451 / 198 / 144** | `_verify.log` (이번 실행, 세 프리셋 모두 재빌드) |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |
| 코드 변경 | 0 | 6커밋 전부 `include/**/*.h` 주석만 |

---

## 4. 미검증 (Gaps)

- **Doxygen 을 실제로 생성해 보지 않았다.** 문법 오류가 있어도 컴파일은 통과하므로
  이번 검증(빌드 경고 0)이 그것을 보증하지 않는다. 저장소에 Doxygen 실행 경로가
  있는지도 확인하지 않았다.
- **`internal.h` / `*_internal.h` / `ai_onnx_session.h` / `ai_worker_protocol.h` 는 보지 않았다.**
  카드가 `*_api.h` 로 범위를 그었다. `ai_onnx_session.h`(220줄)와
  `ai_worker_protocol.h`(228줄)는 stub 빌드에서 쓰이지 않는 부분이 있을 수 있으나
  **확인하지 않았다**.
- **주석이 이제 맞다는 것을 테스트로 고정하지 않았다.** 문서와 구현이 다시 갈라져도
  CI 는 침묵한다. 이번 대조는 시점 스냅숏이다.
- **`enhance_advanced` 의 `SAFETY_VIOLATION` 조건 두 가지를 코드로만 읽었다.**
  실행해 확인하지 않았다.
- **`display` 의 cross-DLL free 안전성은 판정하지 않았다**(§5-D5).
- **api-spec 은 §11.10·§12.1·DLL 표만 봤다.** 전 절 대조는 하지 않았다.

---

## 5. 코드 결함 — **보고만 한다** (카드 지시)

| # | 위치 | 내용 | 왜 고치지 않았나 |
|---|---|---|---|
| **D1** | `enhance_basic` 6함수 | 빈 이미지가 어떤 함수는 `XPE_OK`, 어떤 함수는 `INVALID_INPUT`. 같은 모듈 안에서 계약이 갈린다 | 어느 쪽이 옳은지는 계약 결정이다 |
| **D2** | `enhance_advanced` 헤더 2개 | `enhance_advanced_api.h` 와 `xpe_enhance_advanced_api.h` 가 **같은 include guard**(`XPE_ENHANCE_ADVANCED_API_H`). 둘 다 include 한 TU 는 나중 것이 통째로 사라진다 | guard 변경은 코드 변경 |
| **D3** | `modules/enhance_advanced/src/` | `xpe_collimation_detect.cpp` 와 `enhance_advanced.cpp` 가 `CMakeLists.txt:11-20` 소스 목록에 **없다**. `xpe_detect_collimation` 의 중복 정의를 담고 있다 | 삭제는 되돌리기 어렵고 B-31 의 교훈이 있다 — 목록만 |
| **D4** | `ai.cpp:595` | `set_fallback_mode` 가 저장한 플래그를 **읽는 경로가 없다**. API 가 아무 효과 없이 성공한다 | ONNX 경로 구현과 함께 결정할 일 |
| **D5** | `presentation_lut.cpp:52` | `xpe_display.dll` 이 `xpe_common.dll` 이 `malloc` 한 버퍼를 `free` 한다. 두 DLL 이 공유 CRT 를 쓰는 동안만 안전하다 | **결함이라고 단정하지 않는다** — 링크 설정을 확인하지 않았다. 확인 대상으로 보고 |

D1·D4 는 헤더에 사실대로 적어 두었으므로, 고치기 전까지는 **최소한 함정이 아니게** 됐다.

## 6. api-spec 불일치 목록 — 반영은 leader (`docs/` 는 main 소유)

| # | api-spec | 실제 |
|---|---|---|
| S1 | §11.10 `xpe_dicom_cfind_mwl(queryJson, remoteAeTitle, remoteHost, remotePort, localAeTitle, resultsJsonOut, resultsBufLen)` | 헤더는 `(host, port, aet, queryJson, outJson, outBufLen, timeoutMs)` — 인자 순서·이름이 다르고 `timeoutMs` 가 spec 에 없다 |
| S2 | §11.10 오류 코드 4종 | `PROCESSING_FAILED` 가 빠져 있다 |
| S3 | §12.1 `gsvg_process(pixels, width, height, config)` → `GsvgErrorCode` | 실제는 `xpe_gsvg_process(handle, src, dst, width, height, gainMap)` → `XpeErrorCode`. 함수명·시그니처·오류형이 모두 다르다 |
| S4 | §12 "does not depend on `xpe_common` types" | `XpeErrorCode`·`XPE_API` 를 쓴다 |
| S5 | DLL 표: `xpe_enhance_advanced.dll` 3 | 헤더 선언 7 |
| S6 | DLL 표: `xpe_ai.dll` 7 | 헤더 선언 10 |
| S7 | DLL 표: `xpe_display.dll` 11 / `gsvg.dll` 8 | 헤더 선언 6 / 4 |

`docs/dicom/README.md:576` 의 `xpe_dicom_cfind_mwl` 은 구조체 기반(`XpeMwlQuery`/
`XpeMwlResult`)으로 적혀 있어 실제 JSON 문자열 API 와 **완전히 다르다.** 같은 문서
`:598` 의 `XPE_ERR_LOSSY_COMPRESSION_NOT_ALLOWED` 도 코드에 없는 오류 코드다.
둘 다 main 소유라 목록만 낸다.

---

## 7. 잔여 위험 (Residual-risk)

- **주석은 시점 스냅숏이라 다시 갈라진다.** 특히 `ai` 는 ONNX 빌드가 켜지는 순간
  "stub 에서 도달 불가" 서술이 전부 낡는다. 그때 이 주석들이 **반대 방향의 함정**이
  된다 — ONNX 전환 카드는 이 헤더를 함께 봐야 한다.
- **`cfind_mwl` 키 목록 정정은 동작을 바꾸지 않는다.** 문서를 믿고
  `ScheduledProcedureStepStartDate` 로 필터하던 호출자는 지금도 전체 워크리스트를
  받는다. 문서가 사실을 말하게 됐을 뿐, 필터가 생긴 것은 아니다.
- **D2(guard 충돌)는 지금 사고를 내지 않는다** — 두 헤더를 함께 include 하는 TU 가
  현재 없기 때문이다. 새 소비자가 생기는 순간 조용히 선언이 사라진다.
- 커밋 6건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_verify.bat` / `_verify.log` | 세 프리셋 재빌드 + ctest 451 / 198 / 144, 경고 0 |
