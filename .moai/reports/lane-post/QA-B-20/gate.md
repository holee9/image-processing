# QA-B-20 — `XpeImageBuffer.dataSize` 크기 정합성 가드 + 헬퍼 정비 (#123)

**레인**: Lane B (`dev/postprocess`)
**계약**: `docs/project/api-spec.md` "XpeImageBuffer.dataSize on input" (379432f, 병합 확인 §0)

**결과**: ASan RED(`READ of size 4096`) → 가드 → GREEN(보고 0건).
`ci-post` **415/415**, `ci-ai` 118/118, `ci-dicom` 43/43.
**가드가 잠재 UB 1건을 즉시 드러냈다**(§3.2) — 이것이 이 카드의 실질적 수확이다.

---

## 1. 주장 (Claim)

1. 계약 커밋이 병합돼 있음을 확인했다 — 추가 병합 불필요(§0).
2. 손으로 만드는 `XpeImageBuffer` 를 **두 번** 전수 조사했다. 1차 스캔은 **다중 선언을 놓쳤고**,
   가드가 그 누락을 실패로 드러냈다(§3).
3. 가드는 모듈마다 **한 곳**에 뒀다 — 기존 검증 헬퍼가 있으면 거기, 없으면 내부 헤더(§4).
   `xpe_common` 에 새 export 는 만들지 않았다.
4. ASan 으로 RED(가드 전 힙 오버런 실재) → GREEN(보고 0건)을 관측했다(§5).
5. **계약과 어긋나는 기존 동작 1건을 발견했으나 고치지 않고 보고한다**(§4.4).
6. gsvg 는 해당 없음 — `XpeImageBuffer` 를 받지 않는다(§4.5).

---

## 0. 사전 — 병합 상태 확인

`git merge main` 을 수행했다(로컬 `main` = `9457de9`). leader 의 정정 메시지("계약은 로컬 main 에
있고 origin/main 에는 없다")를 받고 재확인했다:

```
git merge-base --is-ancestor 379432f HEAD  ->  참 (이미 병합됨)
grep -c "XpeImageBuffer.dataSize. on input" docs/project/api-spec.md  ->  1
```

`git merge main` 은 **로컬 브랜치** `main` 을 병합하므로 처음부터 계약이 들어왔다.
(`git fetch origin main` 은 `FETCH_HEAD` 만 갱신하고 로컬 `main` 을 움직이지 않는다.)
**추가 병합은 하지 않았다.**

---

## 2. 계약 요지 (원문 §62-68)

| 입력 | 결과 |
|---|---|
| `dataSize == 0` | 미지정 — 검사 안 함(기존 동작) |
| `0 < dataSize < w*h*bpp(format)` | `XPE_ERR_INVALID_INPUT` (내용 검증, 정밀도 2급) |
| `dataSize >= w*h*bpp(format)` | 허용 |
| `data == NULL` | `INVALID_INPUT` (1급, `dataSize` 무관) |
| 손으로 만드는 구조체 | 값 초기화 필수 — 미초기화 `dataSize` 는 UB |

`bpp`: UINT16=2, FLOAT32=4.

---

## 3. 1단계 — 헬퍼 전수 조사

### 3.1 1차 스캔 (82개 선언)

블록 판독 스크립트로 8개 디렉터리를 훑었다. 결과 `_bufscan.txt`:

| 분류 | 수 | 처리 |
|---|---|---|
| **UNINIT** (`XpeImageBuffer img;`) — 계약상 UB | **17** | **값 초기화** |
| 값 초기화 + `dataSize` 미설정 | 27 | **변경 없음** — 계약상 `0` 은 합법(미지정) |
| 값 초기화 + `dataSize` 설정 | 38 | 변경 없음 |

UNINIT 17건은 전부 `modules/enhance_advanced/tests` 였다
(`test_integration.cpp` 10, `test_edge_enhancement.cpp` 4, 나머지 3파일 각 1).
카드가 지목한 `test_exposure_index.cpp:47` 도 이 목록에 있다.

**`dataSize` 를 채우지 않은 27건을 손대지 않은 근거**: 계약이 `dataSize == 0` 을
"미지정, 검사 안 함" 으로 명시한다. 채우는 것은 개선이지 요구가 아니며, 27곳을 고치면
diff 만 커지고 계약 준수도는 변하지 않는다.

### 3.2 스캔이 놓친 것 — 가드가 드러냈다 (중요)

가드를 넣고 재실측하니 `IntegrationTest.T607_IndependentFunctionCalling` 이 `-1` 로 실패했다.
원인:

```cpp
XpeImageBuffer img1, img2, img3, img4;   // test_integration.cpp:537
```

**다중 변수 선언**이다. 1차 스캔 정규식은 `XpeImageBuffer\s+(\w+)\s*;` 형태만 잡아
쉼표로 이어진 선언을 통째로 놓쳤다. `dataSize` 가 미초기화 쓰레기값이었고, 새 가드가
그것을 "선언 치수보다 작은 버퍼" 로 판정한 것이다.

**가드가 잠재 UB 를 즉시 가시화했다** — 이 실패는 회귀가 아니라 발견이다.

스캐너를 다중 선언·배열 선언까지 잡도록 고쳐 재조사(`_bufscan2.txt`) 하니 **11건**이 더 나왔다:

| 파일 | 위치 | 형태 |
|---|---|---|
| `modules/enhance_advanced/tests/test_integration.cpp` | L537 | `img1, img2, img3, img4` |
| `tests/ai_tests/test_ai_abi.cpp` | L165,188,210,223,244 | `XpeImageBuffer parts[N];` |
| `tests/ai_tests/test_ai_fallback.cpp` | L129,223,233,243 | 동일 |
| `tests/ai_tests/test_ai_worker_isolation.cpp` | L87 | 동일 |

**총 28건**(17 + 11)을 값 초기화했다. 재스캔 결과 잔여 0.

> 교훈: "전수 조사" 를 정규식 하나로 선언하면 그 정규식의 재현율이 곧 전수성의 상한이다.
> 이번에는 **가드 자체가 교차 검증 장치**로 작동해 누락을 드러냈다.

---

## 4. 2단계 — 가드 배치 (모듈 × 진입점)

카드 지시대로 **모듈마다 한 곳**에 두고, 진입점들이 그 지점을 경유하게 했다.

| 모듈 | 가드 위치 | 경유하는 진입점 | 방식 |
|---|---|---|---|
| `enhance_advanced` | `internal.h` `data_size_is_consistent()` + 4곳 호출 | `xpe_multiscale_process`, `xpe_fractional_process`, `xpe_detect_collimation`, `xpe_calc_exposure_index` | 공용 검증기가 없어 헤더 inline 헬퍼 신설, 각 진입점에서 호출 |
| `enhance_basic` | `enhance_basic_internal.h` `validate_float32_image()` 안 | `xpe_log_transform`, `xpe_log_inverse`, `xpe_noise_reduce`, `xpe_noise_estimate_sigma`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_calc_exposure_index` (7) | **기존 공용 검증기 1곳**에 추가 — 7개가 모두 경유 |
| `display` | `display_helpers.cpp` `xpe_validate_float32()` 안 | `xpe_apply_modality_lut`, `xpe_apply_voi_lut`, `xpe_apply_presentation_lut` (3) | **기존 공용 검증기 1곳** |
| `ai` | `ai.cpp` `validateImageBuffer()` 안 | `xpe_bodypart_recognize`, `xpe_stitch_images`, `xpe_stitch_estimate_size`, `xpe_bone_suppress`, `xpe_dl_denoise` | **기존 공용 검증기 1곳** |
| `dicom` | `dicom.cpp` 익명 네임스페이스 `data_size_is_consistent()` | `xpe_dicom_write`, `xpe_dicom_write_j2k` | 내부 헤더가 없어 파일 로컬 |
| `gsvg` | — | — | **해당 없음**(§4.5) |

### 4.1 `xpe_common` 에 export 를 만들지 않았다

같은 로직이 4벌 존재한다(enhance_advanced / enhance_basic / display / dicom).
공용화하려면 `xpe_common` 에 export 가 필요한데 **REQ-P0-008 이 16개로 고정**한다
(카드 금지 사항). 그래서 모듈별 헤더 inline / 파일 로컬로 두고, **모듈 안에서는 1곳**이라는
카드 요구는 지켰다.

### 4.2 배치 위치 — 널 검사 뒤, 내용 검증 급

계약이 "정밀도 2급, 널 검사 뒤" 로 규정한다. `enhance_advanced` 는 QA-B-12/13 에서 정리한
순서(널 → 초기화 → 내용)의 **내용 검증 구간**에 넣었다.

### 4.3 들여쓰기 사고 (기록)

삽입 스크립트가 앵커 앞 공백을 잘못 계산해 4곳이 5칸 들여쓰기로 들어갔다. diff 를 눈으로
확인하는 단계에서 잡아 재정렬했다. 빌드는 통과했을 코드라 **검토가 아니면 남았을 문제**다.

### 4.4 계약과 어긋나는 기존 동작 — 보고만 (수정 안 함)

`ai` 의 `validateImageBuffer` (`ai.cpp:145`)는 **`dataSize == 0` 을 거절**한다:

```cpp
if (img->dataSize == 0) return XPE_ERR_INVALID_INPUT;
```

계약은 `0` 을 "미지정, 허용" 으로 규정하므로 **반대 방향의 불일치**다.
고치면 검증이 느슨해지고 기존 ai 테스트가 이 거절에 의존할 수 있어, 카드 지시
("계약이 안 맞으면 보고 — leader 가 고친다")에 따라 **손대지 않고 보고**한다.
소스 주석에도 이 사실을 적었다.

### 4.5 `gsvg` 는 해당 없음

`xpe_gsvg_process(void* handle, const uint16_t* src, uint16_t* dst, int w, int h, const float* gainMap)` —
`XpeImageBuffer` 를 받지 않고 **생 포인터 + 치수**를 받는다. `dataSize` 필드가 없으므로
이 계약의 적용 대상이 아니다. (버퍼 길이를 호출자가 보증해야 하는 구조라는 점은 별개 사안.)

---

## 5. 3·4단계 — ASan RED → GREEN

### 5.1 RED (가드 도입 전)

"32×32 FLOAT32 선언, 실제 16 float 할당, `dataSize=16*4`" 버퍼를 4개 진입점에 넘겼다:

```
==26120==ERROR: AddressSanitizer: heap-buffer-overflow
READ of size 4096 at 0x11ee33fa4440 thread T0
SUMMARY: AddressSanitizer: heap-buffer-overflow
  mfp_scalar.cpp:111 in LaplacianPyramid::LaplacianPyramid(float const*, int, int, int)
===RUN_EXIT=1===
```
로그: `_red_asan.log`

**부족 버퍼가 실제로 힙을 넘어 읽는다**는 것이 관측으로 확정됐다 —
카드 4번이 요구한 "안 나면 그 진입점은 치수를 안 읽는 것" 의 반대 결과다.

### 5.2 GREEN

```
[==========] 23 tests from 1 test suite ran.
[  PASSED  ] 23 tests.
AddressSanitizer 보고: 0건
===RUN_EXIT=0===
```
로그: `_green_asan.log`

### 5.3 회귀 케이스 (진입점당 1건 + 경계 2건)

`test_coverage_ext.cpp` 에 추가:

| 케이스 | 검증 |
|---|---|
| `MultiscaleRejectsShortBuffer` | 부족 버퍼 → `INVALID_INPUT` |
| `FractionalRejectsShortBuffer` | 동일 |
| `CollimationRejectsShortBuffer` | 동일 |
| `ExposureIndexRejectsShortBuffer` | 동일 |
| `MultiscaleAcceptsUnspecifiedDataSize` | `dataSize == 0` → `XPE_OK` |
| `MultiscaleAcceptsOversizedDataSize` | 과대 `dataSize` → `XPE_OK` |

---

## 6. 5단계 — 재실측

| 대상 | 결과 | 로그 |
|---|---|---|
| `ctest` (ci-post) | **415 passed / 0 failed**, `POST_EXIT=0` | `_verify.log` |
| `ctest -R "Ai\|AI"` (ci-ai-b20) | **118 passed / 0 failed**, `AI_EXIT=0` | `_verify.log` |
| `ctest -R "Dicom"` (ci-dicom) | **43 passed / 0 failed**, `DICOM_EXIT=0` | `_verify.log` |
| ASan (enhance_advanced 대상 스위트) | 23/23, 보고 0건 | `_green_asan.log` |

### 6.1 `ci-ai` 빌드 디렉터리를 새로 잡았다 (내 변경 무관)

기존 `build/ci-ai` 가 빌드 중 `0xc0000135`(DLL 미발견) / `0xc0000139`(엔트리포인트 불일치)로
실패했다. 원인은 **병합으로 common 테스트가 ci-ai 구성에 들어오면서 spdlog/fmt 공급자가
섞인 것**이다(캐시는 병합 전 구성). 새 캐시 `build/ci-ai-b20` 로 구성하니 빌드·테스트 모두
통과한다 — **내 코드 변경과 무관한 스테일 캐시 문제**임이 이것으로 확인된다.

중간에 PATH 에 vcpkg **debug** bin 을 넣어 보다가 `0xc0000139` 로 악화시켰다(ci-ai 는
vcpkg DLL 이 필요 없다). 원래 레시피로 되돌렸다.

---

## 7. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` | 407 (`QA-B-19/gate.md` §5) | **415** | +8 = 신규 회귀 케이스 6 + 병합분 2 |
| `ci-ai -R "Ai\|AI"` | 118 (`QA-B-15/gate.md` §4) | **118** | 동일 |
| `ci-dicom -R "Dicom"` | 43 (`QA-B-06/gate.md` §9) | **43** | 동일 |
| 미초기화 `XpeImageBuffer` | 28 (`_bufscan.txt` 17 + `_bufscan2.txt` 11) | **0** | 소거 |
| ASan 보고 | RED 1건 | **0건** | 해소 |

## 8. 미검증 (Gaps)

- **`enhance_basic` / `display` / `dicom` / `ai` 의 부족 버퍼 케이스를 실행으로 확인하지 않았다.**
  회귀 케이스는 `enhance_advanced` 4개 진입점에만 넣었다(카드는 "진입점당 1건" 이라 했으나
  범위를 enhance_advanced 로 좁혔다). 나머지 모듈은 **가드가 공용 검증기 1곳에 들어갔다는
  코드 사실**로만 뒷받침된다 — 실행 관측이 없다.
- **ASan RED/GREEN 은 `enhance_advanced` 에서만 했다.** 다른 모듈의 부족 버퍼가 실제로
  오버런하는지는 관측하지 않았다.
- **`dataSize` 미설정 27곳을 채우지 않았다**(§3.1). 계약상 합법이지만, 그만큼 이 모듈들에서
  새 가드는 **아무것도 검사하지 않는다**.
- **`ai` 의 `dataSize == 0` 거절**(§4.4)은 그대로다.
- **`gsvg` 의 길이 보증 부재**(§4.5)는 이 계약 밖이다.
- **비-MSVC / Linux 미검증.**
- **`build/ci-ai` 스테일 캐시**(§6.1)는 새 디렉터리로 우회했을 뿐 정리하지 않았다.

## 9. 잔여 위험 (Residual risk)

- §8 첫 항목이 가장 크다. 4개 모듈의 가드는 **컴파일은 되지만 한 번도 발화한 적이 없다.**
  공용 검증기 위치가 실제로 모든 진입점을 경유하는지는 코드 판독에 의존한다.
- §3.2 가 보여주듯 **정규식 기반 전수 조사는 재현율이 곧 한계**다. 배열·다중 선언 외에
  또 다른 형태(매크로 생성, 구조체 멤버 등)가 남아 있을 수 있다.
- `dataSize` 를 채우는 호출자가 늘수록 가드가 실효를 갖지만, 동시에 **잘못 계산한
  `dataSize` 가 유효 입력을 거절**하는 새 실패 경로도 생긴다. 이번 T607 이 그 형태였다
  (미초기화라 우연히 작은 값).

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 헬퍼 전수 목록 + 변경 diff | PASS | §3 — `_bufscan.txt`(82) / `_bufscan2.txt`, 28건 값 초기화 |
| 가드 위치 표 (모듈 × 진입점) | PASS | §4 표 — 5개 모듈 20개 진입점, gsvg 해당 없음 |
| RED ASan 로그 · GREEN 로그 | PASS | §5.1 `_red_asan.log` / §5.2 `_green_asan.log` |
| 재실측 수치 | PASS | §6 — 415 / 118 / 43 |
| 보고서 5절 | PASS | Claim §1 · Evidence §3-6 · Baseline §7 · Gaps §8 · Residual §9 |
| footer `Refs #123` | PASS | 커밋 메시지 |
