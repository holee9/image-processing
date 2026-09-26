# QA-B-21 게이트 보고서 — dataSize 가드 발화 증명 + ai 계약 정합 + 헬퍼 dataSize 채움

**카드**: QA-B-21 (#123, QA-B-20 조건부 PASS 후속)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-21/`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | `ai::validateImageBuffer` 가 `dataSize == 0` 을 계약대로 허용한다 | PASS |
| C2 | C1 로 깨진 기존 테스트는 없다 (영향 테스트 0건) | PASS |
| C3 | B-20 에서 값 초기화만 했던 헬퍼·테스트 지점에 실제 `dataSize` 를 채웠다 (22곳) | PASS |
| C4 | enhance_basic 7 / display 3 / ai 5 / dicom 2 — 진입점 **전부**에 "부족 → INVALID_INPUT" + "0 → 통과" 회귀 케이스가 있다 (17 진입점 × 2 = 34건 + 경계 1건) | PASS |
| C5 | 각 진입점이 공용 검증기를 실제로 경유함을 **RED 로 증명**했다 (가드 무력화 시 17건 전부 실패) | PASS |
| C6 | 부족 버퍼가 가드 전 실제 heap READ 오버플로를 낸다 — enhance_basic·display ASan 관측, 가드 후 0건 | PASS (ai·dicom 은 §4 Gap) |
| C7 | 재실측 ci-post 439 / ci-ai 128 / ci-dicom 47 전부 통과 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 C1·C2 — ai 계약 정합

**변경 (`modules/ai/src/ai.cpp` `validateImageBuffer`)** — 두 곳:

```diff
-    if (img->dataSize == 0) return XPE_ERR_INVALID_INPUT;
+
+    // api-spec "XpeImageBuffer.dataSize on input" (#123): dataSize == 0 means
+    // unspecified and is accepted. This validator used to reject it (#123 B-20),
+    // which contradicted the contract; both checks below are no-ops when it is 0.
...
-        if (bpp != 0u) {
+        if (bpp != 0u && img->dataSize != 0u) {
```

**두 번째 줄이 이 카드의 핵심 발견이다.** 거절 한 줄만 지우면 계약이 지켜지지 않는다 —
B-20 이 넣은 크기 정합 검사 자체가 `dataSize == 0` 을 "필요량보다 작음" 으로 판정해
같은 `INVALID_INPUT` 을 되돌려준다. 실제로 첫 수정 직후 `*_ZeroAccepted` 5건이 전부 실패했고
(`_green_all.log` 초판, `actual: -1 vs -1`), 두 번째 줄을 넣은 뒤 통과했다.
**B-20 의 ai 가드는 `dataSize == 0` 경로에서 한 번도 옳게 동작한 적이 없다.**

또한 B-20 이 남긴 주석
*"(This validator additionally rejects dataSize == 0 … that pre-existing divergence is reported, not changed here.)"*
는 이 수정으로 거짓이 되므로 함께 정정했다.

**영향 테스트 목록**: **0건.** 계약 위반(거절)을 단언하던 기존 케이스는 없었다.

```
build\ci-ai-b20\bin\xpe_ai_tests.exe
[==========] 118 tests from 7 test suites ran. (13 ms total)
[  PASSED  ] 118 tests.
```
(`_ai_after.log` — 수정 직후, 신규 케이스 추가 전)

### 2.2 C3 — 헬퍼 `dataSize` 채움

전수 스캔 스크립트: 테스트 디렉터리 8곳에서 `XpeImageBuffer` 선언을 찾고,
이후 30줄 안에 `.dataSize =` 가 없으면서 `.width`/`.height` 를 **선언하는** 지점만 골랐다.
목록: `_fill_sites.txt` (24건). 제외 근거는 §3.2.

| 파일 | 채운 곳 |
|---|---|
| `enhance_advanced/tests/test_collimation_detect.cpp` | 1 (`createFloatImage` 헬퍼) |
| `enhance_advanced/tests/test_edge_enhancement.cpp` | 4 (헬퍼 1 + `beforeImg` 3) |
| `enhance_advanced/tests/test_exposure_index.cpp` | 1 (헬퍼) |
| `enhance_advanced/tests/test_integration.cpp` | 10 |
| `enhance_advanced/tests/test_integration_ext.cpp` | 2 |
| `enhance_advanced/tests/test_mfp_scalar.cpp` | 4 |
| **합계** | **22** |

형태 두 가지:
```cpp
img.dataSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;   // 치수 유래
beforeImg.dataSize = img.dataSize;                                              // 원본 복제
```

### 2.3 C4·C5 — 진입점 × 케이스 표 (RED 근거 포함)

가드 배치는 B-20 §4 표 그대로. RED 는 **호출부 조건을 `if (false)` 로 무력화**해 얻었다
(헬퍼 본문 조기 반환은 `/W4 /WX` 에서 C4702 로 빌드가 깨져 쓸 수 없었다 — §3.1).

| 모듈 | 진입점 | 부족 → INVALID_INPUT | 0 → 통과 | RED(가드 무력화) |
|---|---|---|---|---|
| enhance_basic | `xpe_log_transform` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_log_inverse` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_noise_reduce` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_noise_estimate_sigma` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_contrast_enhance` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_edge_enhance` | ✔ | ✔ | ***Failed |
| enhance_basic | `xpe_calc_exposure_index` | ✔ | ✔ | ***Failed |
| display | `xpe_apply_modality_lut` | ✔ | ✔ | ***Failed |
| display | `xpe_apply_voi_lut` | ✔ | ✔ | ***Failed |
| display | `xpe_apply_presentation_lut` | ✔ | ✔ | ***Failed |
| ai | `xpe_bodypart_recognize` | ✔ | ✔ | FAILED |
| ai | `xpe_stitch_images` | ✔ | ✔ | FAILED |
| ai | `xpe_stitch_estimate_size` | ✔ | ✔ | FAILED |
| ai | `xpe_bone_suppress` | ✔ | ✔ | FAILED |
| ai | `xpe_dl_denoise` | ✔ | ✔ | FAILED |
| dicom | `xpe_dicom_write` | ✔ | ✔ | ***Failed |
| dicom | `xpe_dicom_write_j2k` | ✔ | ✔ | ***Failed |
| **합계** | **17 진입점** | **17** | **17** | **17 전부 RED** |

경계 1건 추가: `LogTransform_OversizedDataSize_Accepted` (과대 `dataSize` 허용).

**RED 로그** (`_red_all.log`, 가드 4곳 무력화):
```
 1/21 Test #136: DataSizeGuardTest.LogTransform_ShortDataSize_ReturnsInvalidInput .........***Failed
 ...
 7/21 Test #142: DataSizeGuardTest.CalcExposureIndex_ShortDataSize_ReturnsInvalidInput ....***Failed
16/21 Test #401: DisplayDataSizeGuard.ModalityLut_ShortDataSize_ReturnsInvalidInput .......***Failed
17/21 Test #402: DisplayDataSizeGuard.VoiLut_ShortDataSize_ReturnsInvalidInput ............***Failed
18/21 Test #403: DisplayDataSizeGuard.PresentationLut_ShortDataSize_ReturnsInvalidInput ...***Failed
52% tests passed, 10 tests failed out of 21
...
[  FAILED  ] 5 tests (ai: BodypartRecognize / StitchImages / StitchEstimateSize / BoneSuppress / DlDenoise)
...
1/4 Test #87: DicomWriterTest.DataSizeGuard_Write_ShortDataSize_ReturnsInvalidInput ......***Failed
2/4 Test #88: DicomWriterTest.DataSizeGuard_WriteJ2K_ShortDataSize_ReturnsInvalidInput ...***Failed
50% tests passed, 2 tests failed out of 4
```
`*_ZeroAccepted` / `*_ZeroDataSize_Accepted` 17건은 RED 에서도 전부 통과했다 —
실패한 것이 정확히 "가드가 판정하던 것" 뿐임을 보인다.

**GREEN 로그** (`_green_all.log`, 가드 복원):
```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 24
===AI_RUN=0===       (10/10)
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 4
```

### 2.4 C6 — ASan

회귀 케이스의 버퍼는 **완전 할당**이다(선언만 부족). RED 실행이 UB 가 되지 않도록 한 의도적 설계다.
그래서 오버플로 관측용으로 **진짜 부족 할당** 케이스를 따로 넣었다
(`DataSizeGuardAsanProbe.*`, `DisplayDataSizeGuardAsanProbe.*` — 16 float 할당에 32×32 FLOAT32 선언).

빌드: `build/asan-b21` (`/fsanitize=address /Zi`, RelWithDebInfo, `XPE_WARNINGS_AS_ERRORS=OFF`).

**RED** (`_red_asan.log`, 가드 무력화):
```
==24724==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 32   (enhance_basic)
===EB_EXIT=1===
==9616==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 32    (display)
===DISP_EXIT=1===
```

**GREEN** (`_green_asan.log`, 가드 복원):
```
[  PASSED  ] 2 tests.   ===EB_EXIT=0===
[  PASSED  ] 1 test.    ===DISP_EXIT=0===
```
ASan 진단 0건.

### 2.5 C7 — 재실측

`_verify.log`:
```
===CI_POST===    100% tests passed, 0 tests failed out of 439   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 128   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 47    ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | B-20 실측 | QA-B-21 실측 | 차 |
|---|---|---|---|
| ci-post | 415 | 439 | +24 (enhance_basic 16 + display 7 + ASan 프로브 1) |
| ci-ai | 118 | 128 | +10 |
| ci-dicom | 43 | 47 | +4 |

B-20 수치는 `_verify.log` (QA-B-20) 에서 읽었고, 이번 수치는 위 §2.5 의 이번 실행 출력이다.
빌드 디렉터리는 동일 트리의 `build/ci-post`, `build/ci-ai-b20`, `build/ci-dicom`.

### 3.1 RED 기법을 바꾼 이유 (실측 근거)

처음엔 `xpe_data_size_is_consistent()` 본문 첫 줄에 `return 1;` 을 넣어 무력화하려 했다.
`/W4 /WX` 에서 C4702(도달 불가 코드)가 C2220 으로 승격되어 `ci-post` 빌드가 깨졌다:
```
enhance_basic_internal.h(26) : error C2220 / warning C4702
display_internal.h(88) : error C2220 / warning C4702
```
그래서 **호출부**를 `if (false)` 로 바꾸는 방식으로 전환했다. 헬퍼 본문은 손대지 않는다.

### 3.2 스캔에서 제외한 것과 이유

`_scan_sites.txt` 1차(51건) → `_fill_sites.txt` 최종(24건) → 실제 수정(22건).

| 제외 유형 | 건수 | 이유 |
|---|---|---|
| 헬퍼 반환값 대입 (`= makeTestBuffer(...)`, `= createFloatImage(...)`) | 다수 | 헬퍼가 이미 `dataSize` 를 채운다 (`test_ai_abi.cpp:27`, `test_display_integration.cpp:20`) |
| `xpe_alloc_image(&img)` | 다수 | `xpe_memory.cpp:59` 가 `out->dataSize` 를 설정한다 |
| 치수를 선언하지 않는 빈 버퍼 | 다수 | `NOT_INITIALIZED` / 널 검사 케이스 — `dataSize == 0` 이 계약상 옳은 값 |
| 0×0 선언 (`test_exposure_index_ext.cpp:209`, `test_ai_fallback.cpp:293`) | 2 | 0 치수 거절을 단언하는 케이스. `0*0*bpp == 0` 이라 채워도 의미가 없다 |
| `test_api_header_ext.cpp:34` | 1 | 기본 생성값(`dataSize == 0`)을 **단언하는** 케이스 |

1차 스캔이 27건을 놓친 원인은 두 가지였다: (a) 30줄 창 안의 **다른** 선언에 걸린 과잉 제외,
(b) 헬퍼 채움 여부를 확인하지 않고 "대입되었으니 제외" 로 판정한 것. 둘 다 정규식 폭을
좁히고 헬퍼 본문을 직접 읽어 해소했다 — B-20 의 "정규식 하나의 재현율이 전수성의 상한" 과 같은 계열이다.

---

## 4. 미검증 (Gaps)

- **ai·dicom 의 ASan 관측은 없다.**
  - `ai`: 5개 진입점은 검증 뒤 곧바로 `XPE_ERR_PROCESSING_FAILED` 를 반환하는 stub 이다
    (`ai.cpp` `// --- Stub implementation ---`). 픽셀을 읽는 코드가 없으므로 가드를 빼도
    오버플로가 날 수 없다. **소스 판독 근거이며 ASan 실행으로 확인한 것이 아니다.**
  - `dicom`: ASan 스크래치 구성에 DCMTK 를 붙이지 못했다. 실측:
    `-DBUILD_DICOM=ON` 재구성이 `Please set DCMTK_DIR ... (missing: DCMTK_config_INCLUDE_DIR ...)`
    로 실패했다 (`_asan_dicom.log`). vcpkg 의 DCMTK 는 ASan 없이 빌드된 바이너리다.
    dicom 의 RED 근거는 **반환값 회귀 2건**뿐이다.
- **`gsvg` 는 이 카드 대상이 아니다** (생 포인터 API, `XpeImageBuffer` 를 받지 않는다).
- **`enhance_advanced` 회귀 케이스는 이 카드에서 추가하지 않았다** — B-20 에서 이미 RED→GREEN 을 거쳤다.
- **ONNX 실경로 미검증** — ai 는 stub 빌드다. 이 카드의 ai GREEN 을 "AI 동작 검증"으로 읽으면 안 된다.
- **`xpe_stitch_images` 의 출력 버퍼 검사** (`stitchedOut->dataSize == 0` → `BUFFER_TOO_SMALL`) 는
  입력 계약과 별개 규칙이다. 이 카드에서 건드리지 않았고 회귀 케이스도 없다.

---

## 5. 잔여 위험 (Residual-risk)

- **`build/asan-b21` 은 `BUILD_DICOM=OFF` 로 되돌려 두었다.** dicom 실험 중 캐시가 깨졌고
  복구했다(`_asan_restore.log`). 다음에 이 디렉터리를 쓸 때 구성 플래그를 먼저 확인해야 한다.
- **회귀 케이스의 버퍼가 완전 할당인 것은 의도이자 한계다.** 가드가 사라지면 테스트는
  반환값으로 실패하지만 메모리는 안전하다 — 즉 이 34건은 "계약 준수"를 지키지 "메모리 안전"을
  직접 지키지는 않는다. 그 역할은 `*AsanProbe*` 3건이 맡는다.
- **`ci-dicom` 은 vcpkg DLL 경로가 PATH 에 있어야 빌드·실행된다.** 없으면 gtest 탐색이
  `0xc0000135` 로 죽는다. `_bld.bat` / `_verify.bat` 에 경로가 박혀 있다. CI 와 다른 환경 의존이다.
- **`build/ci-ai` (스테일 캐시) 는 그대로 둔다** — B-20 판정대로 레인 워크트리에서 `git clean` 금지.
- 커밋은 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_scan_sites.txt` / `_fill_sites.txt` | 1차 스캔 51건 / 최종 대상 24건 |
| `_ai_after.log` | ai 계약 수정 직후 118/118 (영향 테스트 0건 근거) |
| `_bld.log` / `_bld_dicom.log` | 빌드 오류 진단 (C4702, DCMTK/0xc0000135) |
| `_red_all.log` | 가드 무력화 회귀 RED (17건 실패) |
| `_green_all.log` | 가드 복원 GREEN (38건 통과) |
| `_red_asan.log` / `_green_asan.log` | ASan heap-buffer-overflow READ → 0건 |
| `_asan_cfg.log` / `_asan_dicom.log` / `_asan_restore.log` | ASan 구성·dicom 실패·복구 |
| `_verify.log` | 재실측 439 / 128 / 47 |
