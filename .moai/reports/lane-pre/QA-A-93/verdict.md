# QA-A-93 — 결함 맵 크기 불일치 −4 를 공개 헤더에 적고 시험으로 단언 (#176)

**카드**: `.moai/lanes/pre/inbox/QA-A-93.md` · **브랜치**: `dev/preprocess` · **커밋**: `577e516` (미푸시) · **병합 기준**: `3533d13` · **기계**: Intel Core i7-12700

바뀐 파일은 2개(`preprocess_api.h` +4/−1, `test_calib_generate_offset_config.cpp` +32)이고, 동작 코드는 바꾸지 않았다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `xpe_calib_generate_offset` 의 `@return` 에 −4 의 조건(결함 맵 크기 불일치)과, 이때 표시를 합치지 않고 맵을 그대로 두며 `output_path` 를 쓰지 않는다는 것을 적었다 |
| C2 | 공개 API 시험 2건을 추가했다: 3x3 맵 → −4·파일 없음·맵 불변, 대조 2x2 맵 → 성공·파일 있음·표시 병합 |
| C3 | 반증: 크기 검사의 `return` 을 빼면 대상 시험만 빨강. **반환 코드, 파일 존재, 맵 내용 세 단언이 모두** 빨강을 낸다 |
| C4 | 기본 순서와 시드 1~32 에서 582 실행 / 574 통과 / 실패 0. ci-preprocess 651/651, ci-common 69/69, 빌드 경고 0 |
| C5 | Doxygen 은 로컬에서 돌리지 못했다 |

---

## 2. 증거 (Evidence)

### C1 — 헤더 (`modules/preprocess/include/xpe/preprocess_api.h`, `xpe_calib_generate_offset`)

```diff
- *         XPE_ERR_CONFIG_INVALID if config_json_or_null is malformed
+ *         XPE_ERR_CONFIG_INVALID if config_json_or_null is malformed, or if a
+ *         "sigma_clip" run marks at least one pixel while a defect map of a
+ *         different width or height is already loaded (the marks are not
+ *         merged; the map is left unchanged and output_path is not written)
```

문장의 근거는 코드다(`xpe_calib_generate_offset.cpp`).
- `marked == 0` 이면 먼저 `XPE_OK` 를 반환한다(`:51-53`) → "marks at least one pixel".
- 크기 검사는 병합보다 앞에서 반환한다(`:60-65`) → "not merged; the map is left unchanged".
- 병합이 파일 쓰기보다 앞이다(`:125-126`) → "output_path is not written".
- 경로 이름은 기존 `@param` 이름(`config_json_or_null`, `output_path`)만 썼고 `#`, `\`, `<`, `@` 문자는 넣지 않았다.

### C2 — 시험

`fixtures/make_xcal.hpp` 의 `MakeDefectXCal` 로 전부 0 인 결함 맵을 만들고 `xpe_calib_load_defect_map` 으로 로드한다. 프레임은 2x2 5장 `{100,110,105,108,500}`, 설정 `{"method":"sigma_clip","sigma":1.0}` (기존 시험과 같은 입력 — 4픽셀 모두 `|S|=2 < N_min=3` 으로 표시됨).

| 시험 | 맵 | 단언 |
|---|---|---|
| `MarksAgainstADifferentSizedDefectMapAreRefused` | 3x3 | 반환 `XPE_ERR_CONFIG_INVALID` / `refused.xcal` 없음 / `xpe_calib_save("defect")` 로 읽은 맵 = 9바이트 전부 0 |
| `MarksAgainstASameSizedDefectMapAreMerged` (대조) | 2x2 | 반환 `XPE_OK` / `merged.xcal` 있음 / 맵 = 4바이트 전부 1 |

```
BUILD_EXIT_a93fix=0
--gtest_filter=GenerateOffsetConfigTest.*  →  8 tests from 1 test suite ran.  [  PASSED  ] 8 tests.
```

### C3 — 반증

| 시도 | 약화 | BUILD_EXIT | 결과 |
|---|---|---|---|
| 1 | 조건에 `false &&` | **1** — `warning C4127`(조건식이 상수) → `error C2220` | **무효**. 실행은 이전 바이너리라 8 통과로 찍혔고, 버렸다 |
| 2 | 분기 안 `return XPE_ERR_CONFIG_INVALID;` 를 주석으로 교체 | 0, `xpe_calib_generate_offset.cpp.obj` 재컴파일 | 8 실행 / **대상 1건 실패** (종료 1) |

시도 2 의 실패 단언(로그 원문 요약):

```
[xpe] sigma-clip N_min: 4 pixel(s) below the floor, 4 newly marked in the defect map (XPE-ALG-001 9.8.2.1)
...(236): Expected equality ... -4 ... generate(...) Which is: 0
...(237): Value of: fs::exists(tmpDir / "refused.xcal")  Actual: true  Expected: false
...(241): Expected equality ... Which is: { '\0' x9 }  mask Which is: { 1, 1, 1, 1, 0, 0, 0, 0, 0 }
[  FAILED  ] GenerateOffsetConfigTest.MarksAgainstADifferentSizedDefectMapAreRefused
```

**무엇이 빨강을 내는가**: 세 단언 모두 독립적으로 빨강이다.
- 반환 코드: 0(성공)이 나온다.
- 파일 존재: 오프셋 파일이 쓰인다.
- 맵 내용: 크기 3x3 맵의 앞 4바이트(2x2 분량)에 표시가 들어간다. 검사가 없으면 크기가 다른 맵에 픽셀 위치가 어긋난 채로 합쳐진다는 것이 실행으로 보인다.
- 대조 시험(`...SameSized...`)은 이 약화에서 통과했다.

복원: 보관본 복사 → `git diff --stat` 에서 소스 파일 사라짐 확인 → `BUILD_EXIT_a93R2=0`(1 `.obj`) → 8 실행 8 통과.

### C4 — 전체

```
PRE_BUILD_EXIT=0  warning C 0건  PRE_CTEST_EXIT=0  100% tests passed, 0 tests failed out of 651
COM_BUILD_EXIT=0  warning C 0건  COM_CTEST_EXIT=0  100% tests passed, 0 tests failed out of 69
```

명령: `build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_shuffle --gtest_random_seed=N` (N = 1 … 32) 와 인자 없는 1회.

| 실행 | 실행 수 | 통과 | 실패 |
|---|---|---|---|
| 기본 순서 | 582 | 574 | 0 |
| 시드 1 ~ 32 (32회 각각) | 582 | 574 | 0 |

- 33개 로그를 한 줄씩 판독해 (실행 수, 통과 수, 실패 목록)이 모두 같음을 `uniq` 로 확인했다(시드 32줄이 한 묶음, 기본 순서 1줄). 시드 로그 32개 모두 `seed of N` 줄 1개. 요약 줄 없는 로그 0.
- 실행 수가 580 → 582 로 바뀌었으므로 **시드별 실행 순서는 A-92 와 다르다.**

### C5 — Doxygen

A-83 과 같이 로컬에 `doxygen` 실행 파일이 없다(`where doxygen` 결과 없음). 경고 여부는 CI 의 Doxygen 작업 결과로만 확인할 수 있다.

### 코멘트

```
https://github.com/holee9/image-processing/issues/176#issuecomment-5709251020
```

---

## 3. api-spec.md §6.12 에 넣을 문장 (리더 소유, 제안만)

§6.12 **Description** 의 "OR-merged into the module-global defect map (§9.8.2.1, …)" 문장 뒤:

> If at least one pixel is marked and a defect map with a different width or height is already loaded, the call returns `XPE_ERR_CONFIG_INVALID`: the marks are not merged, the loaded map is left unchanged, and `output_path` is not written.

**Error codes** 줄은 이미 `XPE_ERR_CONFIG_INVALID` 를 포함하므로 목록 자체는 바꿀 필요가 없다. 조건 설명을 붙이는 형식이라면: `XPE_ERR_CONFIG_INVALID` (malformed config, or defect-map size mismatch while marking).

---

## 4. baseline 귀속 (Baseline-attribution)

- 시험·시드·ctest 는 `3533d13`(A-92 병합 포함) + 이 카드 2파일로 빌드한 바이너리다. 반증은 같은 트리에서 소스 한 줄만 바꾼 바이너리다.
- 첫 빌드(`a93fix`)는 병합으로 바뀐 소스 때문에 86 단계 재빌드였다.

---

## 5. 미검증 (Gaps)

- **Doxygen 경고**를 실행으로 확인하지 못했다.
- 반증은 `return` 제거 한 가지만 했다. 조건식만 뒤집는 등 다른 약화는 하지 않았다(`false &&` 는 빌드 실패로 무효).
- 결함 맵이 **너비만/높이만** 다른 경우는 따로 시험하지 않았다(3x3 은 둘 다 다름).
- 동시 호출(스레드)에서의 동작은 보지 않았다.
- 시드 33 이상, `ci-post` 는 보지 않았다. **push 하지 않았다.**

---

## 6. 잔여 위험 (Residual-risk)

- 헤더와 코드가 다시 어긋나지 않게 막는 것은 이 시험 하나다. 이 시험은 반환·파일·맵 셋을 모두 보므로, 검사가 사라지면 세 곳에서 동시에 빨강이 난다.
- 시험 수가 582 로 바뀌어 CI 에 고정된 시드(1, 2, 9)의 실행 순서가 바뀌었다. 이번 32개 시드는 모두 실패 0 이었다.
- **`577e516` 은 미푸시다.**

---

Refs #176
