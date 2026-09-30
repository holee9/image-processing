# 확인 요청 — `ci-preprocess` 에 같은 함정은 **없습니다**

`post` 의 사고(9월 11일자 `ci-post` 디렉터리가 `BUILD_AI=OFF` 로 굳어 AI 시험 190건이 통째로 빠진 채 "705/705 전체 통과" 로 읽힌 것)와 같은 형태가 이 레인에 있는지 확인했습니다. **네 가지를 봤고 넷 다 통과입니다.** 다만 **구조적 위험은 같습니다** — §5 에 적습니다.

---

## 1. 제 헬퍼도 같은 형태입니다 — 재설정 없이 `--build` 만

리더가 물은 (1) 에 대한 답은 **"오래된 디렉터리에 `--build` 만"** 입니다. 제가 모든 카드에서 쓴 스크립트 전문입니다:

```bat
call "...\vcvars64.bat" >nul
cd /d D:\workspace-github\xpe-pre
cmake --build build\ci-preprocess --config RelWithDebInfo     <- --preset 없음
ctest --test-dir build\ci-preprocess -C RelWithDebInfo --output-on-failure
```

`build/ci-preprocess/CMakeCache.txt` 의 작성 시각은 **2026-09-01 10:37** — 약 한 달 전입니다. **형태는 `post` 와 동일합니다.**

## 2. 그런데 캐시가 프리셋과 **전부 일치합니다**

| 변수 | 프리셋 | 캐시 |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | `RelWithDebInfo` |
| `BUILD_SHARED_LIBS` | `ON` | `ON` |
| `BUILD_TESTS` | `ON` | `ON` |
| `BUILD_GSVG` | `OFF` | `OFF` |
| `BUILD_PREPROCESS` | `ON` | `ON` |
| `BUILD_ENHANCE_BASIC` | `OFF` | `OFF` |
| `BUILD_ENHANCE_ADVANCED` | `OFF` | `OFF` |
| **`BUILD_AI`** | **`OFF`** | **`OFF`** |
| `BUILD_DISPLAY` | `OFF` | `OFF` |
| `BUILD_DICOM` | `OFF` | `OFF` |
| `XPE_WARNINGS_AS_ERRORS` | `ON` | `ON` |
| `VCPKG_MANIFEST_DIR` | `${sourceDir}/third_party/common` | `D:/workspace-github/xpe-pre/third_party/common` |

**12개 전부 같습니다.** `post` 와 달리 `BUILD_AI=OFF` 는 **프리셋의 현재 값**이기도 합니다 — 이 레인은 AI 를 짓지 않는 것이 맞습니다.

## 3. 프리셋의 `ci-preprocess` 블록은 9월 1일 이후 **바뀌지 않았습니다**

`CMakePresets.json` 을 만진 커밋은 그 뒤 5건입니다:

| 커밋 | 무엇을 바꿨나 |
|---|---|
| `5215e0f` | `ci-ai` 프리셋 **신설** |
| `adde13d` | **`ci-post`** 의 `BUILD_AI` `OFF → ON` ← **`post` 사고의 원인** |
| `6d7063e` · `cbbf551` · `813e5d2` | `coverage-*` 프리셋 |

**`ci-preprocess` 블록을 건드린 것은 하나도 없습니다.** `inherits` 도 없으므로 다른 프리셋 변경이 흘러들 경로도 없습니다.

## 4. 시험 수 추세 — 올라갔고, 평평한 구간도 설명됩니다

리더가 물은 (2) 입니다. 제 보고서들에서 뽑은 수:

```
QA-A-140  749
      142  752      143  754      144  754      145  754
      146  756      147  757 ... 152  757
      153  758      154  759      155  759      156  761
      158  765      159  769      160  774
      161  774      163  774
      165  775 ... 170  775
```

**줄어든 구간이 없고**, 오른 자리마다 그 카드가 **활성 시험을 더한 카드**입니다(예: `159 → 160` 의 `+5` 는 `test_nonlin_poly_apply.cpp` 의 5건).

`QA-A-165` 이후 **775 로 평평한 것**이 유일한 의심 지점이었습니다 — 그 사이 `test_zz_a166_bounds.cpp`(6건)와 `A169Defect`(3건)를 **추가했는데 수가 안 늘었습니다.** 리더가 말한 그 신호입니다. 확인했고 **설명됩니다**:

```
Total Tests: 802          (ctest -N)
(Disabled)  :  27
802 - 27    = 775         (실행되는 수)
```

`QA-A-165` 이후 제가 더한 것은 **전부 `DISABLED_` 프로브**입니다(카드가 요구한 형태 — CI 를 게이트하지 않도록). 그래서 `802` 는 올랐고 `775` 는 그대로입니다. **두 수를 같이 보면 평평함이 결함이 아닙니다.**

> 제 보고서들이 `775 / 775` 만 적었고 `802` 를 적지 않았습니다. 그 탓에 *"시험을 더했는데 안 늘었다"* 가 제 보고서만으로는 **구별되지 않았습니다.** 앞으로 `N / M (총 T, DISABLED_ D)` 로 적겠습니다.

## 5. 결정적 확인 — 새로 설정해 **같은 수가 나옵니다**

위 넷은 전부 *"어긋날 이유가 없다"* 는 논증입니다. `post` 사고의 교훈은 그 논증이 **실행과 다를 수 있다**는 것이므로, 새 디렉터리에 프리셋으로 처음부터 설정해 비교했습니다(삭제 대신 새 이름 — `build/ci-preprocess-a171`).

```
cmake --preset ci-preprocess -B build\ci-preprocess-a171
```

| | 오래된 `build/ci-preprocess` | **새 `build/ci-preprocess-a171`** |
|---|---|---|
| 설정 | 2026-09-01, `--build` 만 반복 | `CONFIG_EXIT=0` (방금) |
| 빌드 | `BUILD_EXIT=0` | `BUILD_EXIT=0` |
| `ctest -N` 총계 | **802** | **802** |
| 실행/통과 | **775 / 775** | **775 / 775** |
| `CTEST_EXIT` | 0 | 0 |

**두 디렉터리가 같은 수를 냅니다.** 오래된 디렉터리에 숨은 시험은 없습니다.

---

## 6. 그래도 남는 것 — 구조적 위험은 동일합니다

**제 쪽이 안전한 이유는 규율이 아니라 우연입니다.** `ci-preprocess` 프리셋이 한 달간 바뀌지 않았기 때문이고, `cmake --build` 는 여전히 *"이 디렉터리가 무엇으로 설정됐나"* 를 묻지 않습니다. **프리셋이 다음에 바뀌면 저도 같은 함정에 빠집니다** — 그때도 초록이 나오고, 빠진 시험만큼 조용합니다.

`CMakeLists.txt` 변경은 Ninja 가 알아서 재생성하므로 잡힙니다. **프리셋 값 변경만 안 잡힙니다.** 그 비대칭이 이 사고의 본질입니다.

`cmake/`·`CMakePresets.json`·`.github/` 는 제 소유가 아니므로 **관측만** 적습니다. 리더가 고를 수 있는 것으로 보이는 것:

1. **레인 헬퍼를 `--preset` 으로 바꾼다** — 매번 재설정하면 캐시 표류가 원리적으로 불가능해집니다. 대신 증분 빌드 이점을 잃습니다(제 헬퍼는 카드마다 여러 번 돕니다)
2. **캐시–프리셋 대조를 검증에 넣는다** — `CMakeCache.txt` 의 `BUILD_*` 를 프리셋 `cacheVariables` 와 비교해 어긋나면 실패. 재설정 비용 없이 표류만 잡습니다
3. **보고서에 `총계 / DISABLED / 실행` 셋을 다 적게 한다** — `post` 의 190 이 안 보인 것은 수 하나만 적혀 있어서였습니다

**어느 것도 제가 고르지 않았습니다.** 다만 `3` 은 `1`·`2` 의 대체가 아닙니다 — 수를 더 적는 것은 **사람이 대조할 때만** 잡습니다. `post` 에서 190 은 직전 카드 보고서에 이미 있었습니다.

## 7. 미검증

- **다른 레인의 디렉터리는 보지 않았습니다** — `gui` 쪽에 같은 표류가 있는지 확인하지 않았습니다(제 소유 아님)
- **`build/ci-preprocess-a171` 을 남겨 두었습니다**(삭제 금지 규약). 다음 카드는 기존 `build/ci-preprocess` 를 계속 씁니다 — 방금 둘이 같은 결과를 내는 것을 봤으므로
- **`802` 라는 총계를 과거 보고서에 소급 기입하지 않았습니다.** 그때 관측한 수가 아니므로 역산하지 않습니다
- 이 확인은 **현재 트리(`33bb989`)** 기준입니다. `post` 처럼 프리셋이 바뀌는 순간 다시 유효하지 않습니다

🗿 MoAI
