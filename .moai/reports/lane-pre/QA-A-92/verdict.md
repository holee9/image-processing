# QA-A-92 — GenerateOffsetConfigTest 가 스스로 깨끗한 상태에서 시작하기 (#176)

**카드**: `.moai/lanes/pre/inbox/QA-A-92.md` · **브랜치**: `dev/preprocess` · **커밋**: `1fa8902` (미푸시) · **병합 기준**: `af59840` · **기계**: Intel Core i7-12700

바뀐 파일은 시험 1개(`modules/preprocess/tests/test_calib_generate_offset_config.cpp`, +14)뿐이다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `GenerateOffsetConfigTest::SetUp` 이 `init`(결과 무시) → `shutdown` → `init` 으로 보정 맵을 해제한 뒤 시작한다 |
| C2 | 이 시험은 `shutdown` 이 되돌리지 않는 상태(보정 모드·품질 메타)를 읽지 않는다 — 오프셋 생성·저장 소스에 해당 참조가 없다 |
| C3 | **반증**: SetUp 정리만 있고 Load/Save 정리를 빼면 두 쌍 모두 초록, 둘 다 빼면 두 쌍 모두 빨강 |
| C4 | 카드 항목 3: 같은 파일에 다른 픽스처는 **없다** (픽스처 1개, 시험 6건) |
| C5 | 시드 1~32 전부 실패 0(매번 580 실행), 기본 순서 실패 0, ci-preprocess 649/649, ci-common 69/69, 빌드 경고 0 |
| C6 | (2) −4 판독: 코드 주석 한 곳만 이 경우를 의도로 적고 있고, SPEC·SRS·헤더 `@return`·오류 코드 설명·api-spec 은 이 경우를 적지 않는다 (§2 표) |

---

## 2. 증거 (Evidence)

### C1 — 수정

```cpp
void SetUp() override {
    // (QA-A-92, #176 설명 주석)
    (void)xpe_preprocess_init(nullptr);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    tmpDir = ...
```

기존 `TearDown` 의 `shutdown` 은 그대로 두었다.

### C2 — 모드·품질 메타 의존 여부

```
$ grep -cE 'get_mode|quality_meta|record_quality' \
    modules/preprocess/src/xpe_calib_generate_offset.cpp \
    modules/preprocess/src/xpe_calib_generate_offset_methods.cpp \
    modules/preprocess/src/xpe_calib_generate_offset_methods.hpp \
    modules/preprocess/src/xpe_calib_save.cpp
→ 네 파일 모두 0
```

검색 범위는 위 네 파일이다(생성 진입점, 그 도우미 두 파일, 저장 진입점). 시험 6건이 부르는 공개 함수는 `xpe_preprocess_init/shutdown`, `xpe_calib_generate_offset`, `xpe_calib_save` 뿐이다.

### C3 — 반증

도구: 스크래치패드 `a92_toggle.py` — 수정본 3파일을 보관한 뒤, `load`/`save` 는 A-91 이 넣은 `TearDown` 블록만 잘라내고 `dep` 는 설정 파일을 `HEAD` 판으로 되돌린다. 매 상태마다 `git diff --stat` 으로 바뀐 파일을 확인하고 재빌드했다.

대상 = `GenerateOffsetConfigTest.SigmaClipProducesADifferentMapThanMean`

| 상태 | SetUp 정리 | Load/Save 정리 | 재컴파일 | `CalibLoadTest.*`+대상 | `CalibSaveTest.*`+대상 | 시드 2 |
|---|---|---|---|---|---|---|
| 커밋본 | 있음 | 있음 | 3 `.obj`, `BUILD_EXIT=0` | 20 실행 / 20 통과 | 10 실행 / 9 통과 / 실패 0 | 580 / 572 / 실패 0 |
| A | 있음 | **없음** | 3 `.obj`, `BUILD_EXIT=0` | 20 / **20 통과** | 10 / 9 / **실패 0** | 580 / 572 / 실패 0 |
| B | **없음** | **없음** | 3 `.obj`, `BUILD_EXIT=0` | 20 / 19 / **대상 실패** | 10 / 8 / **대상 실패** | 580 / 572 / 실패 0 |

- 카드가 요구한 두 팔(A 초록, B 빨강)이 둘 다 보인다. A-91 은 Load 쪽만 빼는 팔이었는데, 여기서는 Load·Save 를 **함께** 뺐다 — 쌍 두 개를 한 번에 보기 위해서다.
- 종료 코드: A 의 두 쌍 0, B 의 두 쌍 1.

**B 의 시드 2 가 초록인 것**을 따로 확인했다. B 에는 A-91 이 캐시 픽스처 2곳(`test_calib_cache_ownership.cpp`, `test_calibration_cache.cpp`)에 넣은 정리가 남아 있다. 오염원 4파일을 모두 `b35c0a3^` 판으로 되돌려 다시 돌렸다.

| 상태 | SetUp 정리 | 오염원 4파일 | 재컴파일 | 시드 2 |
|---|---|---|---|---|
| C | 없음 | `b35c0a3^` | 5 `.obj`, `BUILD_EXIT=0` | 580 / 571 / **대상 실패** (종료 1) |
| D | **있음** | `b35c0a3^` | 1 `.obj`, `BUILD_EXIT=0` | 580 / 572 / **실패 0** (종료 0) |

- C 는 A-91 의 재현(시드 2 대상 실패)과 같은 결과다. D 는 **오염원을 A-91 이전으로 돌려도 SetUp 정리만으로 시드 2 가 초록**이다.
- 따라서 시드 2 전체 실행에서 대상이 실패하려면 캐시 픽스처 쪽 상태도 A-91 이전이어야 한다. 어느 시험이 직접 맵을 남겼는지는 이번에 좁히지 않았다(§4).

복원: `a92_toggle.py restore` + 캐시 2파일 보관본 복사 → `git diff --stat` = 설정 파일 1개 +14 → 재빌드 `BUILD_EXIT_a92rest2=0`. 커밋은 그 뒤에 했다.

### C4 — 같은 파일의 다른 픽스처

`test_calib_generate_offset_config.cpp` (226줄) 의 `class ... : public ::testing::Test` 는 `GenerateOffsetConfigTest` 하나다. `TEST_F` 6건 모두 이 픽스처다. **목록에 올릴 대상 없음.**

### C5 — 전체

```
ci-preprocess: PRE_BUILD_EXIT=0  warning C 0건  PRE_CTEST_EXIT=0
               100% tests passed, 0 tests failed out of 649
ci-common:     COM_BUILD_EXIT=0  warning C 0건  COM_CTEST_EXIT=0
               100% tests passed, 0 tests failed out of 69
```

명령: `build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_shuffle --gtest_random_seed=N`, N = 1 … 32, 그리고 인자 없는 기본 순서 1회.

| 실행 | 실행 수 | 통과 | 실패 |
|---|---|---|---|
| 기본 순서 | 580 | 572 | 0 |
| 시드 1 | 580 | 572 | 0 |
| 시드 2 | 580 | 572 | 0 |
| 시드 3 | 580 | 572 | 0 |
| 시드 4 | 580 | 572 | 0 |
| 시드 5 | 580 | 572 | 0 |
| 시드 6 | 580 | 572 | 0 |
| 시드 7 | 580 | 572 | 0 |
| 시드 8 | 580 | 572 | 0 |
| 시드 9 | 580 | 572 | 0 |
| 시드 10 | 580 | 572 | 0 |
| 시드 11 | 580 | 572 | 0 |
| 시드 12 | 580 | 572 | 0 |
| 시드 13 | 580 | 572 | 0 |
| 시드 14 | 580 | 572 | 0 |
| 시드 15 | 580 | 572 | 0 |
| 시드 16 | 580 | 572 | 0 |
| 시드 17 | 580 | 572 | 0 |
| 시드 18 | 580 | 572 | 0 |
| 시드 19 | 580 | 572 | 0 |
| 시드 20 | 580 | 572 | 0 |
| 시드 21 | 580 | 572 | 0 |
| 시드 22 | 580 | 572 | 0 |
| 시드 23 | 580 | 572 | 0 |
| 시드 24 | 580 | 572 | 0 |
| 시드 25 | 580 | 572 | 0 |
| 시드 26 | 580 | 572 | 0 |
| 시드 27 | 580 | 572 | 0 |
| 시드 28 | 580 | 572 | 0 |
| 시드 29 | 580 | 572 | 0 |
| 시드 30 | 580 | 572 | 0 |
| 시드 31 | 580 | 572 | 0 |
| 시드 32 | 580 | 572 | 0 |

- 33개 로그 모두 요약 줄(`tests from … test suites ran`)이 있다. 시드 로그 32개 모두 `seed of N` 줄이 있다. 요약 줄이 없으면 `INVALID` 로 찍도록 판독했고, 무효는 0건이다.
- 580 = 572 통과 + 원래 건너뛰는 8건. 시험 수가 A-91 과 같으므로 시드별 순서도 A-91 과 같다.

### C6 — (2) 크기가 다른 결함 맵이 있을 때 −4 (판독만, 코드 변경 없음)

**동작 위치**: `modules/preprocess/src/xpe_calib_generate_offset.cpp:40` `merge_static_defect_mask`

```cpp
if (g_calib.defect_map && (g_calib.defect_width != width ||
                           g_calib.defect_height != height)) {
    // Resizing silently would either drop existing marks or misplace
    // the new ones; reporting the mismatch is the lesser evil.
    return XPE_ERR_CONFIG_INVALID;
}
```

- 도입 커밋: `74fee8f` (2026-09-11) `feat(preprocess): GREEN — N_min 미달 픽셀을 전역 결함 맵에 OR 병합`.
- 호출 순서(`xpe_calib_generate_offset`, `:125`): 설정 파싱 → `generate_offset_values` → `merge_static_defect_mask` → XCal 파일 쓰기. **−4 가 나면 오프셋 파일은 쓰이지 않는다.**
- 이 분기에 닿는 것은 결함 표시가 1개 이상일 때뿐이다(`marked == 0` 이면 먼저 `XPE_OK` 로 반환, `:51-53`). 즉 `sigma_clip` 에서 N_min 미달 픽셀이 생긴 경우다.

**문서별로 이 경우를 적고 있는가**

| 출처 | 위치 | 이 경우(기존 결함 맵과 크기 불일치)에 대한 기술 |
|---|---|---|
| 코드 주석 | `xpe_calib_generate_offset.cpp:62-63` | **있음** — "Resizing silently … reporting the mismatch is the lesser evil" |
| 헤더 `@param` | `preprocess_api.h:257-261` | 표시 픽셀을 "OR-merged into the global defect map" 한다고만 적음. 크기 불일치 언급 없음 |
| 헤더 `@return` | `preprocess_api.h:262-266` | `XPE_ERR_CONFIG_INVALID if config_json_or_null is malformed` — **크기 불일치로 −4 를 낸다는 줄 없음**. 목록에 `NOT_INITIALIZED`·`INVALID_INPUT`·`IO_FAILED` 가 있고 `OUT_OF_MEMORY`·`PROCESSING_FAILED` 는 없음 |
| 오류 코드 설명 | `modules/common/include/xpe/common/xpe_error.h:51` | `-4 /**< Configuration file missing, malformed, or version mismatch */` — 맵 크기 불일치는 문구에 없음 |
| SPEC | `.moai/specs/SPEC-XPE-P1A/spec.md:363-367` REQ-P1A-017 | "merged into the global defect map (§9.8.2.1)" 까지만. 크기 불일치·오류 코드 언급 없음. SRS 링크 `SRS-CALIB-020` |
| SPEC-XPE-P1A-CALIB | `.moai/specs/` | **해당 디렉터리 없음** — `ls .moai/specs` 21개 중 이름에 `calib` 포함 0개 |
| SRS | `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md` | `SRS-CALIB-020` 이라는 ID 는 이 문서에 **없다**(있는 것은 `SRS-CALIB-FUNC-020` = lag/ghost 지표, `:126`). 저장소 전체 `*.md` 에서 `SRS-CALIB-020` 은 `spec.md`, `api-spec.md` 와 `.moai/backups/` 사본에만 나온다 |
| api-spec | `docs/project/api-spec.md:667-669` §6.12 | "OR-merged into the module-global defect map" 까지만. 오류 코드 목록에 `XPE_ERR_CONFIG_INVALID` 는 있으나 조건 설명 없음 |
| 알고리즘 명세 | `XPE-ALG-001 …Specification.md` §9.8.2.1(`:7060`), §9.8.4(`:7151`), §9.8.5 엣지 케이스 | `:7055-7165` 를 읽음. `|S| < N_min` 이면 정적 결함 마킹, 함수가 `defect_mask` 를 반환, "모든 프레임 클리핑 → defect_mask=True" 는 있음. 이 범위에 전역 결함 맵과의 병합·크기 불일치는 **없음** |

**이 경우를 단언하는 시험**

- `test_sigma_clip_nmin.cpp:233` `OrMergeRejectsASizeMismatch` — 내부 함수 `or_merge_defect_bits` 에 길이가 다른 마스크를 넣어 0건 반영을 단언한다. 이것은 **마스크 길이 ≠ n** 경우이고, 전역 맵 크기 불일치(−4)와는 다른 분기다.
- 공개 API 로 "다른 크기의 결함 맵이 있을 때 `xpe_calib_generate_offset` 가 −4" 를 단언하는 시험: 아래 6개 파일에서 `CONFIG_INVALID` 단언을 찾았고, 모두 설정 문자열 오류에 대한 것이었다 — **이 경우를 단언하는 시험은 이 범위에서 0건**.
  - 검색한 파일: `test_calib_generate_offset_config.cpp`, `test_sigma_clip_nmin.cpp`, `test_sigma_clip_conformance.cpp`, `test_calib_generate_offset_multi.cpp`, `test_xpe_calib_generate_offset.cpp`, `test_error_precedence.cpp` (+ 설정 파일의 SetUp 주석).
  - `xpe_calib_generate_offset` 를 부르는 나머지 시험 파일 4개(`test_calibration_manager.cpp`, `test_calibration_roundtrip.cpp`, `test_xpe_preprocess_calibration.cpp`, 빌드 제외된 `test_xpe_preprocess.cpp`)는 이번에 보지 않았다.
- 이번 상태 B 의 `CalibLoadTest` 쌍 로그(`a89-a92BL.log`)에서 대상의 실패 줄은 `generate(series, "clip.xcal", "{\"method\":\"sigma_clip\",\"sigma\":1.0}")` / `Which is: -4` 다 — 두 번째 `generate`(sigma_clip) 호출이 이 분기로 −4 를 받았다.

### 코멘트

```
https://github.com/holee9/image-processing/issues/176#issuecomment-5708422079
check: 본문에 1fa8902 포함 1건
```

(2) 는 카드 지시대로 코멘트에 넣지 않았다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 커밋본·시드 32개·ctest 는 `af59840` + 이 카드 변경 1파일로 빌드한 바이너리다(복원 후 재빌드 `a92rest2`, 이어서 전체 빌드 `a92-full-pre` 는 `Building CXX` 0줄, `PRE_BUILD_EXIT=0`).
- 반증 A·B 는 `af59840` 에서 Load/Save 두 파일의 A-91 블록만 잘라낸 트리, C·D 는 오염원 4파일을 `b35c0a3^` 판으로 바꾼 트리다. 각 상태의 `git diff --stat` 을 실행 전에 확인했다.
- (2) 의 줄 번호는 `af59840`(구현)과 `D:/workspace-github/image-processing` 작업 트리(문서)를 이번에 읽은 값이다.

---

## 4. 미검증 (Gaps)

- **시드 2 에서 대상 직전에 맵을 남기는 시험이 무엇인지**는 좁히지 않았다. C/D 로 "캐시 픽스처 쪽 A-91 정리가 있으면 시드 2 에서 안 드러난다"까지만 보였다.
- **캐시 픽스처 2곳만 되돌린 상태**(Load/Save 정리 유지)는 돌리지 않았다.
- `xpe_calib_generate_offset` 를 부르는 **나머지 시험 파일 4개**는 −4 단언 여부를 보지 않았다.
- 헤더 `@return` 의 `XPE_ERR_NOT_INITIALIZED` 줄이 실제 코드와 맞는지는 이번 카드 범위가 아니어서 보지 않았다.
- **시드 33 이상**, 셔플 소요 시간, `ci-post` 바이너리는 보지 않았다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **시험 수가 바뀌면 같은 시드의 순서가 바뀐다.** 이번 측정은 실행 580건 기준이다.
- CI 의 시드 2 는 이번 상태 B(SetUp 정리 제거 + Load/Save 정리 제거)에서 초록이었다. 이 시험의 SetUp 정리가 빠지는 회귀를 CI 시드 2 만으로 잡으려면 캐시 픽스처 쪽 상태에도 기대게 된다. 쌍 실행 두 개는 B 에서 빨강이었다.
- 이 픽스처의 SetUp 정리는 **맵**만 비운다. 앞으로 이 시험이 모드나 품질 메타를 읽게 되면 그 둘은 따로 맞춰야 한다.
- **`1fa8902` 은 미푸시다.**

---

Refs #176
