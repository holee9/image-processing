# QA-B-163 (`#229`) — gsvg 도 같은 습관. **4곳이 아니라 13곳, 두 종류였습니다**

## 1. 주장

1. **§2 답: 빨개집니다.** 조용히 통과하는 갈래가 아닙니다 — 저장소 루트에서
   **164건 중 63건**이 죽습니다. 다만 **단언 하나는 구별을 못 합니다.** §2
2. **전수는 4곳이 아니라 13곳이고 두 종류입니다.** 리더 목록의 4곳 외에
   MC 시험 3자리와 **제품 표(`data/`) 5자리**가 더 있었습니다. §3
3. **한 번에 다 못 찾았습니다.** 1차 전수로 8자리를 고쳤더니 실패가 63 → 10 으로
   줄었을 뿐이었고, **남은 10건이 두 번째 종류를 드러냈습니다.** §3
4. **고친 뒤 cwd 네 곳 전부 0**, ctest 도 154/154. `#error` 가 실제로 컴파일을
   멈추는 것까지 확인했습니다. §4
5. `modules/ai` 와 **같은 모양**으로 고쳤고, 공용 헤더로 묶어 되돌아갈 자리를
   없앴습니다. §3

---

## 2. §2 — **빨개지는가, 조용히 통과하는가**

고치기 전, 같은 바이너리로 cwd 만 바꿔서:

| cwd | 결과 |
|---|---|
| `modules/gsvg` (오늘 동작하는 곳) | **0** |
| 저장소 루트 | **1** — 164건 중 **63건 실패**(101 통과) |
| 저장소 밖 `C:\Windows` | **1** |

**시끄럽게 빨개집니다.** `test_bone_suppress_abi.cpp` 에서 봤던 *"표 없음 갈래로
조용히 통과"* 는 **테스트 단위에서는 일어나지 않습니다.**

### 다만 **단언 하나는 구별을 못 합니다**

`test_virtual_grid.cpp:756`:

```cpp
missingFile.replace(..., "tests/data/no_such_table.csv");
EXPECT_EQ(xpe_gsvg_init(&h, missingFile.c_str()), XPE_ERR_CONFIG_INVALID);
```

**표가 없기를 기대하는 음성 시험**입니다. 틀린 cwd 에서는 "없는 표"도 "있는 표"도
똑같이 없으므로 **이 단언은 참입니다.** 그것을 담은 시험
`GsvgVirtualGridApi.InitNeedsEveryValue` 는 **다른 단언 때문에** 빨개지므로 테스트
단위로는 드러나지만, **단언 수준에서는 판별력이 0** 입니다.

> 카드가 물은 두 갈래 중 첫째(빨개진다)가 맞고, 둘째의 씨앗이 **한 단언에** 있습니다.
> 지금은 같은 시험의 다른 단언이 가려 주고 있을 뿐입니다.

---

## 3. §3 전수 — **13자리, 두 종류**. 한 번에 못 찾았습니다

### 1차 전수 (8자리) — `tests/data` 와 MC

| 파일 | 자리 |
|---|---|
| `test_gsvg_result.cpp:26` | `tests/data/…synthetic_table.csv` |
| `test_vg_product_table.cpp:28` | 같음 |
| `test_virtual_grid.cpp:35` | 같음 |
| `test_virtual_grid.cpp:755` | `tests/data/no_such_table.csv` (음성 시험) |
| `test_thread_determinism.cpp:160` | JSON 문자열 안 |
| **`test_virtual_grid_mc.cpp:38`** | `tests/data/mc/` — **리더 목록에 없던 것** |
| **`test_virtual_grid_mc512.cpp:45`** | `tests/data/mc/` — **같음** |
| **`test_virtual_grid_mc512.cpp:46`** | **`../../tools/mcsim/phantoms/`** — `../..` 라 cwd 가 한 칸만 어긋나도 **다른 트리**를 읽습니다 |

고친 뒤 재니 **63 → 10 실패**. 줄었지만 초록이 아니었습니다.

### 2차 — 남은 10건이 **두 번째 종류**를 드러냈습니다

```
GsvgVgProductTable.PublicApiRunsTheProductTable
  test_vg_product_table.cpp(157): xpe_gsvg_init(...) Which is: -4   ← CONFIG_INVALID
```

`"data/vg_table_water_csi600_victre.csv"` — `tests/data` 가 **아니라**
`modules/gsvg/data/` 의 **제품 표**입니다. 제 1차 검색이 `tests/data` 에 걸려
놓쳤습니다.

| 파일 | 자리 |
|---|---|
| `test_mask_outside.cpp:60`, `:107` | 2 |
| `test_thickness_range.cpp:49` | 1 |
| `test_vg_product_table.cpp:29` | 1 |
| `test_thread_determinism.cpp:62` | 1 (JSON 안) |

**합계 13자리 / 9파일.** 보고서에 *"4곳 이상"* 이라 적었던 것이 확정됐습니다.

> **한 번의 전수로 끝나지 않았다**는 것이 이 카드의 관측입니다. 첫 검색어가
> `tests/data` 였고, 그 이름이 **같은 결함의 다른 절반을 가렸습니다.** 실패
> 건수가 0 이 아니라 10 으로 남은 것이 그것을 알려 줬습니다 — **"줄었다" 를
> "고쳤다" 로 읽지 않은 것이 두 번째 종류를 찾은 이유입니다.**

### 고친 방법 — `modules/ai` 와 같은 모양

```cmake
target_compile_definitions(gsvg_tests PRIVATE
    XPE_GSVG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    XPE_GSVG_PRODUCT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    XPE_GSVG_MCSIM_PHANTOM_DIR="${_XPE_GSVG_MCSIM_PHANTOMS}")
```

세 종류를 **따로** 둔 이유: `tests/data` 는 지어낸 수, `data/` 는 **제품이 싣는
표**(시험이 그것을 이름으로 부르므로 사본이 아니라 진짜에 닿아야 합니다),
`tools/mcsim/phantoms` 는 저장소에 복사하지 않는 1 MB 영상들입니다.

**공용 헤더 `tests/test_data_paths.h`** 로 묶었습니다 — 상수 네 벌 대신 한 곳.
다음 시험 파일은 include 만 하면 되므로 **상대 경로가 돌아올 자연스러운 자리가
없습니다.** 세 매크로 각각에 `#ifndef → #error` 를 뒀습니다.

JSON 안에 들어가는 두 자리는 `JsonPath()` 로 감쌌습니다. CMake 가 슬래시를
정규화하므로 **오늘은 아무것도 바꾸지 않지만**, 그 전제가 깨질 때의 실패가
조용하기 때문입니다 — 역슬래시 하나면 config 가 파싱은 되고 표만 안 실려, 시험이
그것을 **스레딩 결과로** 읽습니다.

---

## 4. §4 반증

| 검사 | 결과 |
|---|---|
| 직접 실행, 저장소 루트 | **0** (전 1) |
| 직접 실행, `modules/gsvg` | **0** |
| 직접 실행, `modules/ai` | **0** (전 1) |
| 직접 실행, 저장소 밖 `C:\Windows` | **0** (전 1) |
| **ctest** (등록된 환경) | **0** — 154/154 |
| `xpe_ai_tests` 도 저장소 밖 | **0** (QA-B-162 가 유지되는지) |

**둘 다 재는 규칙**을 적용했습니다 — ctest 는 등록된 환경을, 직접 실행은 **그
환경 없이도 서는지**를 봅니다.

### `#error` 가 실제로 멈추는가 — 정의를 지워 확인

`XPE_GSVG_PRODUCT_DATA_DIR` 한 줄을 CMake 에서 빼고 재구성·빌드:

```
test_data_paths.h(30): fatal error C1189: #error:
  "XPE_GSVG_PRODUCT_DATA_DIR must be defined by the build (modules/gsvg/CMakeLists.txt)"
```

**컴파일이 멈춥니다.** 되돌린 뒤 정의 1건을 다시 확인했고 전체 빌드가 섭니다.

---

## 5. 검증

```
빌드:        ===BUILD_ALL=0===
전체 ctest:  ===CTEST_ALL=8===  753/754
gsvg ctest:  154/154
cwd 스윕:    루트 0 · modules/gsvg 0 · modules/ai 0 · C:\Windows 0
             (gsvg_tests, xpe_ai_tests 둘 다 저장소 밖에서 0)
고치기 전:   루트 1 (63/164 실패) · 저장소 밖 1
1차 고침 후: 루트 1 (10 실패) ← 두 번째 종류가 남아 있던 증거
#error 반증: fatal error C1189, 정의를 빼면 컴파일 중단
```

빨강 1건은 **선재**입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`
(`enhance_advanced`) — `QA-B-154`·`156`·`160`·`161`·`162` 에서 같은 구성에 이미
있던 것이고 `ci-post` 프리셋이 아니기 때문입니다.

종료 코드는 전부 파이프 없이 받았습니다.

## 6. 미검증 · 잔여 위험

- **CI 에서 확인하지 못했습니다.** `#162` 단계가 초록이 되는지는 푸시 뒤에야
  압니다. 제가 한 것은 그 단계의 cwd 를 로컬에서 재현한 것입니다
- **`working-directory: modules/gsvg` 가 이제 필요 없는지 판정하지 않았습니다** —
  리더 몫입니다. 제가 보인 것은 *"gsvg 도 ai 도 그 줄 없이 선다"* 까지이고,
  **다른 스위트가 그 줄에 의존하는지는 안 봤습니다**
- **음성 단언(`:756`)의 판별력 0 은 고치지 않았습니다.** 지금은 같은 시험의 다른
  단언이 가려 주지만, **그 단언만 남으면 틀린 이유로 통과합니다.** 고치려면
  "없는 표" 를 실재하는 디렉터리 안의 없는 이름으로 두는 것으로 충분한데,
  **경로 결함의 범위를 넘어서 판단이 필요해** 보고만 합니다
- **다른 모듈은 열지 않았습니다**(카드 §5) — `preprocess`·`enhance_*`·`display`·
  `dicom` 에 같은 형태가 있는지 모릅니다
- **경로가 박혔으므로 시험 바이너리를 다른 기계로 복사해 돌리면 실패합니다**
  (`QA-B-162` 와 같은 교환). 특히 `XPE_GSVG_MCSIM_PHANTOM_DIR` 은 저장소 밖
  `tools/mcsim/phantoms` 를 가리키므로, 그 디렉터리가 없는 체크아웃에서는
  **전과 마찬가지로** MC 시험이 실패합니다 — 이 카드가 바꾼 것은 *어디서 돌리든
  같은 곳을 본다*는 것이지 *파일이 있다*는 보장이 아닙니다

---

Refs #229
