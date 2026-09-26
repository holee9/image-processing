# QA-B-17 — multiscale levels 를 이미지 크기로 제한 (#121 [safety])

**레인**: Lane B (`dev/postprocess`)
**관측 도구**: MSVC ASan (`/fsanitize=address`), 스크래치 빌드 `build/asan-adv`.
**결과**: ASan RED → 수정 → ASan GREEN. 정식 빌드 스위트 **186/186**, `ci-post` **402/402**.

---

## 1. 주장 (Claim)

1. ASan 이 결함을 **위치까지** 특정했다 — `mfp_scalar.cpp` `upsample()`, **heap-buffer-overflow WRITE**(§2).
2. 원인은 `width >> level` 에만 하한 처리가 없다는 것이다 — 근거를 코드로 확정했다(§3).
3. SPEC 은 `levels 2-8` 만 정하고 **이미지 크기와의 관계를 규정하지 않는다** → 카드 지침대로 (a) 클램프(§4).
4. 상한은 추측이 아니라 유도했다: `numLevels ≤ floor(log2(min(w,h))) + 1`. 관측된 실패 크기와 일치한다(§4.2).
5. GREEN: ASan 클린 + 경계 32/64/128/256 + 극소 1/2/3/5 통과(§5).
6. **위험 분석 문서에 이 항목이 없다**(§7) — grep 결과만 보고, 문서 수정 안 함.

---

## 2. 1단계 — ASan RED

### 2.1 ASan 가용성 (카드: 안 되면 먼저 보고하고 멈출 것)

```
VC/Tools/MSVC/14.44.35207/lib/x64/clang_rt.asan_dynamic-x86_64.lib        (존재)
VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll (존재)
```
가용 확인 → 진행. 스크래치 디렉터리 `build/asan-adv`(새 이름), enhance_advanced 만 ON,
`/fsanitize=address /Zi`, `XPE_WARNINGS_AS_ERRORS=OFF`(계측 코드가 경고를 낼 수 있어 분리).

### 2.2 RED 보고

`DISABLED_` 를 제거하고 32×32 + 64×64 두 케이스를 활성화해 실행:

```
==22680==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x11c7c3ba3734 ...
WRITE of size 4 at 0x11c7c3ba3734 thread T0
SUMMARY: AddressSanitizer: heap-buffer-overflow
  D:\...\modules\enhance_advanced\src\mfp_scalar.cpp:261
  in xpe::enhance_advanced::LaplacianPyramid::upsample(float const*, float*, int, int)
===RUN_EXIT=1===
```
로그: `_red_asan.log`

**읽기가 아니라 쓰기(WRITE)다.** QA-B-16 에서 64×64 가 `XPE_OK` 를 반환하고도 나중에
죽었던 이유가 여기서 확정된다 — 힙 메타데이터를 넘어 쓰고 있었다.

---

## 3. 원인 (코드로 확정)

`LaplacianPyramid` 생성자(`mfp_scalar.cpp:62-`)에서:

```cpp
int currentW = width >> level;          // 하한 없음  <-- 문제
int currentH = height >> level;
int nextW = std::max(1, currentW / 2);  // 하한 1
int nextH = std::max(1, currentH / 2);

levels_[level].resize(currentW * currentH);        // level 이 깊으면 0
std::vector<float> upsampled(currentW * currentH); // 0 크기
upsample(gaussianPyramid[level + 1].data(), upsampled.data(), nextW, nextH);
```

`width >> level` 이 0 이 되는 순간 `upsampled` 는 **크기 0**인데, `nextW/nextH` 는
`max(1, …)` 로 **1 이상**이라 `upsample()` 이 2×2 = 4 float 을 써 넣는다.
ASan 의 "WRITE of size 4" 와 정확히 일치한다.

즉 결함은 `upsample()` 자체가 아니라 **호출자가 넘긴 크기 조합**이며, 그 조합은
`numLevels` 가 이미지 크기와 무관하게 8까지 허용되기 때문에 생긴다.

---

## 4. 2단계 — 수정

### 4.1 SPEC 확인 (카드: SPEC 이 정하면 그대로)

| 출처 | 내용 |
|---|---|
| `docs/project/sdd_adv.md:196` | `\| levels \| int \| 2-8 \| 4 \|` |
| `docs/project/srs_adv.md:436` | "Laplacian pyramid decomposition with 4 levels (configurable 2-8)" |
| `docs/project/srs_adv.md:78` | 기본 4단계 분해 요구 |

**이미지 크기와의 관계는 어디에도 없다.** 따라서 카드 지침대로 **(a) 클램프**를 택했다.

(b) `CONFIG_INVALID` 거절을 택하지 않은 이유: SPEC 은 `levels` 를 품질 노브로 다루지
정확성 전제조건으로 규정하지 않는다. 처리 파이프라인이 자기가 고르지도 않은 설정값
때문에 멈추는 것보다, 이미지가 감당 가능한 깊이로 낮추는 편이 SPEC 의 취지에 가깝다.
클램프는 debug 로그를 남긴다.

### 4.2 상한 유도

가장 거친 레벨의 인덱스는 `numLevels - 1` 이고, 그 레벨이 최소 1픽셀은 있어야 하므로

```
min(w,h) >> (numLevels - 1) >= 1
  =>  numLevels <= floor(log2(min(w,h))) + 1
```

유도값과 QA-B-16 의 관측이 **정확히 맞는다**:

| 이미지 | 상한 | 설정 8 | QA-B-16 관측 |
|---|---|---|---|
| 32×32 | 6 | 초과 | 호출 중 접근 위반 |
| 64×64 | 7 | 초과 | `XPE_OK` 후 지연 폴트 |
| 128×128 | 8 | 딱 맞음 | 폴트 미관측 |
| 256×256 | 9 | 여유 | 폴트 미관측 |

관측이 유도를 뒷받침한다 — 128 이 경계라는 것도 설명된다.

### 4.3 구현

생성자 본문 선두에서 클램프한다(모든 호출자를 덮는 단일 지점):

```cpp
const int minDim = std::max(1, std::min(width, height));
int affordableLevels = 1;
while ((minDim >> affordableLevels) >= 1) { ++affordableLevels; }
if (numLevels > affordableLevels) { spdlog::debug(...); numLevels = affordableLevels; }
if (numLevels < 1) { numLevels = 1; }
numLevels_ = numLevels;
```

`@MX:WARN` + `@MX:REASON` 으로 사유·ASan 위치·SPEC 근거를 코드에 남겼다.
**levels 기본값(4)과 다른 알고리즘 파라미터는 건드리지 않았다**(카드 금지 사항).

---

## 5. 3단계 — GREEN

### 5.1 ASan 클린 (대상 케이스)

```
[==========] Running 7 tests from 1 test suite.
[       OK ] MultiscaleMaxLevelsOnSmallImage      (32x32)
[       OK ] MultiscaleMaxLevelsOn64x64
[       OK ] MultiscaleMaxLevelsAcrossSizes       (32/64/128/256)
[       OK ] MultiscaleMaxLevelsOnTinyImages      (1/2/3/5)
[  PASSED  ] 7 tests.
===RUN_EXIT=0===
```
로그: `_green_asan.log`

극소 이미지(1×1 포함)를 넣은 이유: 유도식의 하단 경계다. 1×1 은 상한이 1 이므로
`MIN_LEVELS`(2)보다 낮고, 클램프가 그것까지 내려가지 않으면 같은 오버플로가 난다.

### 5.2 ASan 전체 스위트

```
186 tests ran
AddressSanitizer 보고: 0건   (grep -c "AddressSanitizer" -> 0)
FAILED 3건: CollimationDetectTest.LargeImagePerformance
            EdgeEnhancementTest.T308_PerformanceBudget
            IntegrationTest.T608_PerformanceBudgetVerification
```
로그: `_asan_full.log`

**3건 모두 성능 예산 테스트이며 메모리 오류가 아니다.** 실패 내용:
`Expected: (duration.count()) < (100), actual: 534 vs 100` — ASan 계측 오버헤드다.
정식 빌드에서는 셋 다 통과한다(§5.3). 이 3건을 "회귀" 로 읽지 말 것.

### 5.3 정식 빌드(RelWithDebInfo) 무회귀

```
스위트 : 186 tests ran, [  PASSED  ] 186 tests.   ===SUITE_EXIT=0===
ctest  : 100% tests passed, 0 tests failed out of 402   ===ALL_EXIT=0===
```
로그: `_verify.log`

DISABLED 0건 — 결함이 고쳐졌으므로 비활성 케이스를 남길 이유가 없다.

---

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| enhance_advanced 스위트 | 182 실행 + 1 DISABLED (`QA-B-16/gate.md` §6) | **186 실행, 0 DISABLED** | +4 활성(경계 2 + 스윕 2), DISABLED 해제 |
| `ci-post` 전체 | 398 (`QA-B-16/gate.md` §6) | **402** | +4 = 동일 |
| 프로세스 종료 코드 | 0 | **0** | 동일 |
| ASan 보고 | RED 1건 (`_red_asan.log`) | **0건** (`_asan_full.log`) | 해소 |

## 7. 4단계 — 위험 분석 문서 grep (수정 안 함)

카드 지시대로 grep 만 했다.

| 문서 | 검색어 | 결과 |
|---|---|---|
| `docs/enhance-advanced/SHA-ENHANCE-ADV-001_Software_Hazard_Analysis.md` | `pyramid`, `levels`, `buffer`, `out-of-bounds`, `overflow`, `memory` | **이 항목 없음.** 유일한 `overflow` 매치는 248행 — bilateral filter 의 σ_r 발산으로 **픽셀 값**이 float32 한계로 튀는 위험이며, 메모리 오버플로가 아니다 |

즉 **피라미드 레벨 수로 인한 버퍼 오버런은 위험 분석에 등재돼 있지 않다.**
IEC 62304 Class B 모듈이므로 SHA 항목 추가가 필요해 보이나, `docs/` 는 main 소유라
보고만 한다.

## 8. 미검증 (Gaps)

- **128×128 이상이 "안전" 함을 ASan 으로 확증한 것은 32/64/128/256 네 크기뿐이다.**
  유도식이 모든 크기를 덮지만, 관측은 그 네 개 + 1/2/3/5 다.
- **비정사각형 이미지 미검증.** 유도는 `min(w,h)` 를 쓰지만 케이스는 전부 정사각형이다.
  극단 종횡비(예: 1024×2)는 확인하지 않았다.
- **`reconstruct()` 경로를 별도로 계측하지 않았다.** 생성자에서 `numLevels_` 를 클램프하므로
  같은 값을 쓰지만, 재구성 루프의 인덱싱을 독립적으로 검증하지는 않았다.
- **다른 알고리즘의 유사 패턴 미조사.** `fractional_derivative` 등 다른 소스에도
  `>>` 기반 크기 계산이 있는지 확인하지 않았다 — 같은 유형이 있을 수 있다.
- **ASan 은 enhance_advanced 만 계측했다.** Lane B 의 다른 모듈은 이번 범위 밖이다.
- **성능 영향 미측정.** 클램프는 O(log n) 루프 한 번이라 무시 가능하다고 보았으나
  측정하지 않았다.
- **SHA 문서 미갱신**(§7) — main 소유.

## 9. 잔여 위험 (Residual risk)

- 클램프는 **조용히** 레벨을 낮춘다(debug 로그만). 8단계를 기대한 호출자는 작은
  이미지에서 다른 결과를 받고도 알아채지 못한다. (b) 거절안을 택했다면 드러났을
  트레이드오프이며, §4.1 의 근거로 클램프를 택했다.
- `XPE_MFP_MIN_LEVELS`(2)보다 낮게 클램프되는 경우(1×1, 2×2 등)가 생긴다. SPEC 의
  "2-8" 범위를 벗어나지만, 그 대안은 오버플로다. **SPEC 범위와 물리적 가능성이
  충돌하는 지점이며 SPEC 쪽 정리가 필요하다.**
- ASan 빌드에서 성능 예산 테스트 3건이 실패한다(§5.2). ASan 을 CI 에 넣는다면
  그 3건은 제외하거나 예산을 분리해야 한다.

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| ASan RED 로그 | PASS | §2.2 `_red_asan.log` — 위치·WRITE 크기 포함 |
| ASan GREEN 로그 | PASS | §5.1 `_green_asan.log`, §5.2 `_asan_full.log`(보고 0건) |
| diff | PASS | `mfp_scalar.cpp`(클램프 + @MX), `test_coverage_ext.cpp`(케이스 4건) |
| 경계 케이스 | PASS | §5.1 — 32/64/128/256 + 1/2/3/5 |
| 재실측 | PASS | §5.3 스위트 186/186, ci-post 402/402 |
| footer `Refs #121` | PASS | 커밋 메시지 |
