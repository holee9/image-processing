# QA-A-145 (#207) — 판정 **(B)**. AVX2 이득이 작고, 진짜 비용은 다른 곳입니다

Lane A (pre), `dev/preprocess`. `origin/main` 병합.

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 754
CTEST_EXIT=0

한 프로세스 네 순서:  ran=685 / 685 / 685 / 685,  실패 0, 위생 위반 0
```

---

## §1 전제 확인

| # | 전제 | 판정 |
|---|---|---|
| 1 | 보정 경로에 AVX2 가 없다 | **참** |
| 2 | 두 `TEST_F` 가 같은 함수를 두 번 부른다 | **참** |
| 3 | `offset`·`gain` parity 는 **진짜** parity 다 | **참 — 그리고 검출까지 셋 다** |

### 1 — `_mm256` 전수와 대조군

| 경로 | `_mm256` |
|---|---|
| `defect_correct.cpp` | **0** |
| `helpers.cpp` | **0** |
| `gain_correct.cpp` | 13 |
| `offset_correct.cpp` | 12 |
| `runtime_detection.h` | 32 |

**0 이 검색 실패가 아닙니다** — 같은 검색이 형제 경로 셋에서 12~32건을 찾습니다.

### 2 — 자기 비교 확인

```cpp
TEST_F(..., MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img2, &output2, &metadata));
    for (...) EXPECT_EQ(outputPixels1[i], outputPixels2[i]);
}
```
**같은 함수, 두 번.** 비교 대상이 하나뿐입니다.

### 3 — **숨은 항목의 답: 넷 중 하나만 가짜입니다**

| 시험 | 무엇과 비교하나 | 판정 |
|---|---|---|
| `test_defect_correct_avx2_parity` | `xpe_defect_correct` **두 번** | **자기 비교** |
| `test_gain_correct_avx2_parity` | 실경로 vs `xpe_gain_apply_scalar_reference` | **진짜** |
| `test_offset_correct_avx2_parity` | 실경로 vs `xpe_offset_apply_scalar_reference` | **진짜** |
| `test_runtime_detection_avx2_parity` | 실경로 vs `ScalarMap`(시험 안의 독립 스칼라 루프) | **진짜** |

**셋 다 독립적으로 유도된 비교 대상을 갖고 있습니다.** 카드가 걱정한 *"넷 다 같은 형태"* 는 아니었습니다 — **문제는 이 하나에 국한됩니다.**

그리고 셋이 진짜인 **이유**가 기록돼 있습니다: `gain`·`offset` 은 AVX2 가 유일 경로가 되면서 스칼라 구현이 *"참조 구현"* 으로 남았고(`SPEC-XPE-P1A` 4.6), **그 참조의 소비자가 이 시험들**입니다. 보정에는 그런 참조가 없습니다 — 스칼라 하나뿐이니까요.

---

## §2 측정 — AVX2 이득 추정. **이것이 판단 근거입니다**

`#204` 가 남긴 출발점: 0.1% 밀도에서 전체 **18.45 ms**, 보정 연산 자체는 **184 ns × 9437 ≈ 1.7 ms (9%)**.

### 성분 측정 (같은 기계, i7-12700, RelWithDebInfo, 3072², 0.1%)

| 성분 | 시간 | AVX2 후보인가 |
|---|---|---|
| 프레임 `memcpy` → `dst` (37.7 MB) | 3.04 ms | **아니오** — CRT 가 이미 벡터화 |
| **두 번째 프레임 복사 `source`** | **12.45 ms** | **아니오** — 할당 + 페이지 폴트 |
| 결함 맵 복사 (9.4 MB) | 0.62 ms | 아니오 |
| 본 루프 (`vector<bool>` 접근 포함) | 3.53 ms | 부분적 |
| ㄴ 바이트 배열로 바꾸면 | 2.77 ms | (비트 패킹이 0.76 ms) |
| 마스크 스캔 스칼라 | 2.03 ms | **예** |
| 마스크 스캔 AVX2 | 0.64 ms | **1.39 ms 절약** |

> **`#204` 에서 설명되지 않던 12.5 ms 를 찾았습니다.** `defect_correct.cpp:182` 의 `std::vector<float> source(src, src + n)` 가 **프레임을 한 번 더 복사**하고, `dst` 와 달리 **새로 할당**하므로 페이지 폴트까지 붙어 12.45 ms 입니다. **이 함수에서 가장 비싼 단일 항목이고, 나머지 전부를 합친 것보다 큽니다.**

### 추정 — AVX2 로 옮겨서 얻는 것

| 항목 | 낙관적 이득 |
|---|---|
| 보간 연산 1.7 ms | 4근방이 **흩어진 주소**라 gather 바운드. 2배로 봐도 **~0.85 ms** |
| 마스크 스캔 1.39 ms | **단, 실제 루프는 `dm[idx] && !processed[idx]` 이고 `processed` 가 `std::vector<bool>`** 입니다. 벡터화하려면 자료구조부터 바꿔야 하므로 "AVX2 추가" 가 아니라 재작성입니다 |
| **합계** | **약 1~2 ms / 18.45 ms = 5~11%** |

**대비**: `source` 복사 하나를 없애면 **12.45 ms ≈ 67%** 입니다 — 그리고 **AVX2 와 무관합니다.**

### 그래서 (B)

| | |
|---|---|
| (A) AVX2 구현 | 전체의 **5~11%**, 새 벡터 코드와 그 유지비 |
| 성능 요구 | `#204` 가 세운 목표 **45 ms**, 현재 **18.45 ms** — **압박이 없습니다** |
| 가장 큰 이득 | `source` 제거(**67%**) — AVX2 가 아님 |

**"AC 에 적혀 있으니 구현한다" 는 근거가 아니고**, 측정은 그 반대를 말합니다. **(B) 로 갑니다.**

---

## §3 (B) 실행 — 둘 다 고쳤습니다

### 시험 (제 소유)

| | 전 | 후 |
|---|---|---|
| 파일 | `test_defect_correct_avx2_parity.cpp` | **`test_defect_correct_determinism.cpp`** |
| 클래스 | `DefectCorrectAVX2ParityTest` | `DefectCorrectDeterminismTest` |
| 시험 2 | `ParityWithNonMultipleStride` | `DeterministicOnANonMultipleStride` |
| 임시 파일 | `test_defect_avx2_parity_defect.xcal` | `test_defect_determinism_defect.xcal` |

`git mv` 로 이력을 보존했고, 헤더에 **왜 이름이 바뀌었는지**와 **이 시험이 무엇을 잡고 무엇을 못 잡는지**를 적었습니다 — 특히 *"일관되게 틀린 보정은 두 호출이 같은 오답에 동의하므로 잡지 못한다"* 는 것. 형제 셋이 다른 형태라는 것도 **확인한 사실로** 적었습니다.

CMake 의 `# SPEC-SIMD-001: AVX2 parity tests` 주석이 이제 이 파일을 잘못 묶으므로, 앞 둘의 참조 구현을 명시하고 이 파일은 따로 이유를 달아 분리했습니다.

### AC (리더 소유 — 제안만)

`acceptance.md:447` `AC-SIMD-003` 제안:

| 항목 | 지금 | 제안 |
|---|---|---|
| 이름 | `AC-SIMD-003` (SIMD parity) | **`AC-DEFECT-DET-001`** 또는 SIMD 군에서 분리. SIMD 와 무관합니다 |
| Given | UINT16 프레임 100개 | **FLOAT32** 프레임 (API 가 UINT16 을 거부 — `defect_correct.cpp:124`) |
| When | scalar 와 AVX2 경로로 각각 보정 | **같은 입력으로 두 번 보정** |
| Then | 바이트 일치 | **두 출력이 바이트 일치** (결정성) |
| Test Count | 600 | **2** |
| REQ 매핑 | `REQ-P1A-040` (SIMD Optimization) | **`REQ-P1A-012`** (Defect Correction Execution) — SIMD 요구가 아닙니다 |
| 알고리즘 | `bilinear` | **가중치 없는 4근방 평균 / 3×3 median** (`#125` 정정 반영) |

---

## §4 반증 — (B) 형태: **이 시험이 무엇을 잡는가**

비결정성을 실제로 주입했습니다 — 정적 카운터로 **두 번째 호출에서만** `dst[0]` 을 1 더하게(컴파일러가 접을 수 없는 조건):

```
BUILD_EXIT=0      DLL 20:00:00 (갱신 확인)

    Which is: 1163.76782
    Which is: 1164.76782
Pixel mismatch at index 0
[  FAILED  ] DefectCorrectDeterminismTest.MultipleCallsAreBitIdentical
```

**잡습니다.** 주입을 원복한 뒤 다시 초록입니다. 카드가 경계한 *"그 시험도 아무것도 안 보는 것"* 은 아닙니다 — **좁을 뿐 비어 있지 않습니다.**

---

## 보고만 합니다 — 손대지 않은 것 셋

### ① `source` 복사는 **앨리어싱 가드일 수 있습니다** — 없애라고 말하지 않습니다

12.45 ms 로 가장 비싸지만, `src` 를 직접 읽지 않는 이유가 **`input->data == output->data` 인 호출**을 견디기 위한 것일 수 있습니다(그 경우 `dst` 에 쓰면 `src` 가 오염됩니다).

**확인하지 못했습니다** — `preprocess_api.h` 의 `xpe_defect_correct` 문서에 in-place 허용/금지가 **적혀 있지 않고**, in-place 를 시험하는 곳도 찾지 못했습니다(검색에서 잡힌 `test_xpe_preprocess_memleak.cpp` 는 다른 맥락).

**근거를 확인하지 못한 가드는 건드리지 않습니다.** 이것을 없애는 것이 이 함수 최대의 성능 항목이지만, 먼저 답해야 할 것은 *"어떤 호출자가 in-place 로 부르는가"* 입니다. 별도 카드가 맞습니다.

### ② 옛 이름이 `.moai/` 두 곳에 남습니다 (리더 소유)

```
.moai/docs/acceptance.md:114     | Defect | `tests/test_defect_correct_avx2_parity.cpp` | 2 |
.moai/project/dev-plan.md:58     | REQ-SIMD-003 Defect | test_defect_correct_avx2_parity.cpp | ✅ |
```

`.moai/reports/lane-pre/*/\*.log` 의 옛 이름은 **과거 실행 기록**이라 그대로 두는 것이 맞습니다.

### ③ `std::vector<bool>` 두 개

`processed`·`visited` 가 비트 패킹이라 9.4M 접근에서 **0.76 ms** 를 씁니다. 바이트 배열이면 그만큼 빠릅니다. 성능 압박이 없으므로 제안만 합니다.

---

## 미검증

- **AVX2 이득은 추정입니다.** 실제로 구현해 재지 않았습니다 — 보간의 2배 가정은 gather 지연에 대한 낙관적 상한이고, 실제로는 더 나쁠 수 있습니다. **추정이 틀려도 결론은 안 바뀝니다**: 최대치인 마스크 스캔 1.39 ms + 보간 1.7 ms 를 **전부** 0 으로 만들어도 18.45 → 15.4 ms 로 목표 45 ms 와 무관합니다.
- **성분 측정은 따로 잰 것**이라 합이 실측 전체와 정확히 맞지 않습니다(캐시 상태가 다릅니다). 성분 합 ≈ 19.6 ms vs `#204` 실측 18.45 ms.
- **`AC-SIMD-001/002/004/005`** 는 보지 않았습니다(범위 밖). 다만 §1-3 확인에서 `004`(검출)의 시험이 **진짜 parity** 인 것은 확인됐습니다.
- **in-place 호출자 유무**를 확인하지 못했습니다 — 위 ①.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **이름을 바꿔도 AC 가 안 바뀌면 절반만 고친 것입니다.** 카드가 짚은 그대로이고, AC 는 리더 소유라 제안만 했습니다.
- **`source` 를 성능 항목으로만 읽으면 위험합니다.** ① 의 이유로, 없애기 전에 in-place 계약부터 정해야 합니다.
