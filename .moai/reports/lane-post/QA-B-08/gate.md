# QA-B-08 — `xpe_enhance_basic` 라이브러리 경고-오류화 누락 (#106 마무리, Class A)

**레인**: Lane B (`dev/postprocess`)
**대상**: `modules/enhance_basic/CMakeLists.txt` — `xpe_enhance_basic` 라이브러리 타깃
**배경**: QA-B-06 범위 밖 관측. `/WX-` 도 헬퍼 호출도 없어 카드의 13곳 목록에 빠져 있었고,
결과적으로 이 라이브러리만 `XPE_WARNINGS_AS_ERRORS` 통제 밖에 남아 있었다.

---

## 1. 주장 (Claim)

1. `xpe_enhance_basic` 라이브러리 TU 의 경고는 **0건** — 전환 가능하다.
2. `xpe_target_warnings_as_errors(xpe_enhance_basic)` 1줄 추가로 전환했다.
3. 전환이 **실제로 문다** — 경고 주입 시 빌드가 실패한다(관측).
4. `ci-post` ctest 무회귀.

## 2. 증거 (Evidence)

### 2.1 1단계 측정 — 강제 재컴파일

`build/ci-post/modules/enhance_basic/CMakeFiles` 를 지우고 재빌드했다
(증분 빌드는 경고를 다시 출력하지 않으므로 재측정이 되지 않는다).

```
라이브러리 TU 재컴파일: 6건  (xpe_enhance_basic.dir/... — 소스 6개 전부)
warning / error       : 0건
===BUILD_EXIT=0===
```
로그: `_measure.log`

6건은 `CMakeLists.txt` 의 `XPE_ENHANCE_BASIC_SOURCES` 6개(enhance_basic, log_transform,
noise_reduce, contrast_enhance, edge_enhance, exposure_index)와 일치한다 —
일부만 재컴파일된 것이 아니다.

### 2.2 2단계 변경 — 1파일 2줄

```diff
 else()
     target_compile_options(xpe_enhance_basic PRIVATE
         -Wall -Wextra -Wpedantic -mavx2
     )
 endif()
+
+xpe_target_warnings_as_errors(xpe_enhance_basic)
```

위치 관례는 다른 15곳과 동일하다 — `if(MSVC)/else()` 블록의 `endif()` **뒤**.
헬퍼가 비-MSVC 의 `-Werror` 도 처리하므로 `if(MSVC)` 안에 두면 안 된다.

`git diff --stat` → `1 file changed, 2 insertions(+)` (본문 1줄 + 빈 줄 1줄).
기존 `/wd4365`(ring buffer int→size_t)와 `/arch:AVX2 /fp:fast` 는 건드리지 않았다 —
경고-오류화 여부와 독립된 축이다.

### 2.3 3단계 프로브 — 실제로 무는가

QA-B-06 §8 과 같은 프로브(미참조 정적 함수 → C4505, /W4 레벨)를
`src/log_transform.cpp` 에 주입해 재빌드했다.

```
log_transform.cpp(61): warning C4505  ->  error C2220
FAILED x1
===BUILD_EXIT=1===
```
로그: `_probe.log`

전환 전이었다면 경고만 출력되고 `BUILD_EXIT=0` 이었을 자리다.

프로브 원복 확인: `grep -c xpe_wx_probe src/log_transform.cpp` → **0**.
원복 후 재빌드 `BUILD_EXIT=0`, warning/error 0 (`_build_after.log`).

### 2.4 ctest 무회귀 (재실측)

```
100% tests passed, 0 tests failed out of 282
===TEST_EXIT=0===
```
로그: `_ctest.log`

## 3. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` ctest | 282 passed / 0 failed (`QA-B-06/gate.md` §9, `_ctest_all.log`) | **282 passed / 0 failed** | 동일 |
| `xpe_enhance_basic` 라이브러리 경고 | 0 (QA-B-06 §2 ci-post 59스텝 전체 0건) | **0** (본 카드 §2.1, 라이브러리 TU 6건 한정 재측정) | 동일 |
| 워크트리 상태 | — | `M modules/enhance_basic/CMakeLists.txt` 1건 + main 소유 미추적 2건 | 프로브 흔적 0 |

## 4. 미검증 (Gaps)

- **비-MSVC 경로 미실행.** 이 1줄은 GCC/Clang 에서 `-Werror` 를 붙인다. Windows 에서만
  측정했다(QA-B-06 §10 과 동일한 공백이며, leader 판정상 현재 Linux CI 는 없다).
- **`XPE_WARNINGS_AS_ERRORS=OFF` 빌드 미실행.** OFF 에서 헬퍼가 아무것도 하지 않는
  경로는 코드로만 확인했다.
- **`ci-post` 외 구성 미측정.** `xpe_enhance_basic` 은 `ci-fullstack` 에서도 빌드되지만
  그 구성으로는 돌리지 않았다. 컴파일 옵션이 같으므로 결과가 같을 것으로 보나 관측하지 않았다.
- **`/wd4365` 유효성 미확인.** 기존 억제가 아직 필요한지는 이 카드에서 재검증하지 않았다
  (QA-B-06 이 `xpe_ai_tests` 의 `/wd4996` 에 대해 한 종류의 확인을 여기서는 하지 않았다).

## 5. 잔여 위험 (Residual risk)

- 이 라이브러리는 `/arch:AVX2 /fp:fast` 로 SVML 자동 벡터화를 켜 둔다. 컴파일러 업그레이드로
  벡터화 관련 새 경고가 생기면 이제 빌드가 막힌다 — 의도한 동작이지만 툴체인 업그레이드 비용이다.
- `/wd4365` 가 덮고 있는 int→size_t 변환은 여전히 보이지 않는다. 전환은 "억제하지 않은
  경고" 에만 작용하므로, 이 라이브러리가 완전히 경고 없이 빌드된다는 뜻은 아니다.

## 6. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| diff 1파일 1줄(+주석) | PASS | §2.2 — 1파일, 본문 1줄 |
| 프로브 관측 로그 | PASS | §2.3 `_probe.log` (C4505 → C2220, FAILED) |
| ctest ci-post 무회귀 (재실측) | PASS | §2.4 282/282, §3 |
