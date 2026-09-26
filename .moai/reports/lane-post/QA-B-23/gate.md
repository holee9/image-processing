# QA-B-23 게이트 보고서 — gsvg 수명주기 누수 게이트(G3)

**카드**: QA-B-23 (#105, QA-B-22 후속)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-23/`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | gsvg 에 warm-up 100 → 기준선 → 1000회 → 증가량 < 1 MB 케이스를 넣었다 | PASS |
| C2 | 측정 사이클이 실제 처리 경로를 지난다 — 두 처리 단계를 config 로 켰다 | PASS |
| C3 | 민감도 프로브(4096 B/사이클)로 FAILED 관측, 5회 재현, 실측 증가량 기록 | PASS |
| C4 | 프로브 원복 후 5회 전부 통과, 소스 잔재 0건 | PASS |
| C5 | 재실측 ci-post 444 / ci-ai 129 / ci-dicom 48 전부 통과 | PASS |
| C6 | 크기를 3072 → 512 로 축소했고 사유를 소스와 보고서에 남겼다 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 작업 전 현황

`modules/gsvg/tests/test_gsvg_abi_smoke.cpp:224` `GsvgAbiSmoke.RepeatedLifecycleDoesNotLeakOrCrash`
— 256×256, **32회**, 힙 계측 없음, warm-up 없음. 소스 주석이 스스로 적고 있다:

```
// REQ-GSVG-021 requires 100 frames in batch mode without leaks; here we
// run a smaller loop on the smoke path and rely on the build's ASan/leak
// sanitiser (when enabled) to catch handle-level allocations that escape.
```
즉 "죽지 않는다" 만 확인하고, 누수 검출은 켜져 있지 않은 sanitiser 에 위임하고 있었다.

### 2.2 추가한 케이스 (C1)

`GsvgEndurance.ThousandCycles_MemoryGrowthUnderOneMB` — B-22 의 4개 모듈과 동일한 형태
(`get_working_set_bytes()` + `ENDURANCE_WARMUP 100` / `ENDURANCE_CYCLES 1000` / `ENDURANCE_ONE_MB`).
사이클: `xpe_gsvg_init` → `xpe_gsvg_process` → `xpe_gsvg_shutdown` (핸들 수명주기).
`gsvg_tests` 타깃에 `psapi` 링크를 `if(WIN32)` 블록으로 추가했다.

### 2.3 사이클이 실제로 일을 하는지 (C2) — 이 카드의 실질

첫 구현은 카드 문구대로 `xpe_gsvg_init(&handle, nullptr)` + `gainMap = nullptr` 이었다.
실행 시간이 **0.02초**로 나왔다 — 1100 사이클 × 512×512 치고 불가능한 수치라 소스를 다시 읽었다:

```cpp
// gsvg.cpp:190-191
h->vignette_enabled = json_get_bool(configJsonOrNull, "vignette_correction", false);
h->grid_enabled     = json_get_bool(configJsonOrNull, "grid_suppression",    false);
// gsvg.cpp:218-228
if (h->vignette_enabled && gainMap != nullptr) { apply_vignette_scalar(...); }
else if (src != dst)                          { std::memcpy(...); }
if (h->grid_enabled)                          { suppress_grid_row_mean(...); }
```

**config 가 NULL 이면 두 처리 단계가 모두 꺼진다.** 그 상태의 `xpe_gsvg_process` 는 memcpy 한 번이다.
그대로 뒀다면 게이트는 핸들 수명주기만 덮고 `apply_vignette_scalar` / `suppress_grid_row_mean` 의
보유 결함은 통과시켰을 것이다. 두 단계를 켜고 실제 gain map 을 넘기도록 고쳤다:

```cpp
const char* kConfig = R"({"vignette_correction": true, "grid_suppression": true})";
const std::vector<float> gain(kN, 1.05f);
...
ASSERT_EQ(xpe_gsvg_init(&handle, kConfig), XPE_OK);
ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kW, kH, gain.data()), XPE_OK);
```

**실행 시간 0.02초 → 0.43초 (약 20배).** 이 차이가 처리 경로가 실제로 도는 증거다.

### 2.4 GREEN 5회 (`_green.log`)

```
===BUILD=0===
1/1 Test #441: GsvgEndurance.ThousandCycles_MemoryGrowthUnderOneMB ...   Passed    0.44 sec   ===RUN1=0===
1/1 Test #441: ... Passed 0.43 sec   ===RUN2=0===
1/1 Test #441: ... Passed 0.43 sec   ===RUN3=0===
1/1 Test #441: ... Passed 0.43 sec   ===RUN4=0===
1/1 Test #441: ... Passed 0.43 sec   ===RUN5=0===
```

### 2.5 민감도 프로브 (C3) — `_probe_red.log`

사이클 첫 줄에 4096 B 누수를 주입(1000회 = 4 MB, 임계값의 4배). `volatile` 로 소비해
최적화 제거를 막았다.

```
Working set grew by 3980 KB over 1000 gsvg init/process/shutdown cycles   ===RUN1=8===
Working set grew by 3980 KB ...                                          ===RUN2=8===
Working set grew by 3976 KB ...                                          ===RUN3=8===
Working set grew by 3980 KB ...                                          ===RUN4=8===
Working set grew by 3976 KB ...                                          ===RUN5=8===
```
`Working set grew by` 줄 5건, `===RUN1..5=8===` 5건 — **5회 모두 이름 붙은 실패와 수치로 확인**했다
(B-22 판정문의 "로그가 뒷받침하는 만큼만" 을 이 카드에서는 처음부터 지켰다).
주입량 4096 KB 대비 3976~3980 KB — 계측이 실제 힙을 보고 있다.

### 2.6 원복 (C4) — `_probe_green.log`

```
===RUN1=0===  ===RUN2=0===  ===RUN3=0===  ===RUN4=0===  ===RUN5=0===
100% tests passed, 0 tests failed out of 1   (×5)
```
잔재 검사:
```
$ grep -rn "QA-B-23-PROBE" modules/ tests/
(출력 없음)
```

### 2.7 재실측 (C5) — `_verify.log`

```
===CI_POST===    100% tests passed, 0 tests failed out of 444   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 129   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 48    ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | QA-B-22 실측 | QA-B-23 실측 | 차 |
|---|---|---|---|
| ci-post | 443 | 444 | +1 (`GsvgEndurance.ThousandCycles_MemoryGrowthUnderOneMB`) |
| ci-ai | 129 | 129 | 0 |
| ci-dicom | 48 | 48 | 0 |

B-22 수치는 `.moai/reports/lane-post/QA-B-22/_verify.log`, 이번 수치는 §2.7 의 이번 실행 출력이다.
빌드 디렉터리 동일(`build/ci-post`, `build/ci-ai-b20`, `build/ci-dicom`).

### 3.1 크기 축소 (C6)

카드가 허용한 대로 3072×3072 → **512×512**. 사유는 소스 주석에도 남겼다:
게이트가 재는 것은 **사이클당 보유량**이고 이는 프레임 크기에 비례하지 않는다. 반면
1100 사이블 × 3072² 는 스위트 전체 실행 시간을 지배한다. 512×512 로 0.43초다.

---

## 4. 미검증 (Gaps)

- **통과 경로의 실제 증가량 수치는 없다.** 테스트는 임계값 초과 시에만 `Working set grew by ...`
  를 출력한다. 통과 = "1 MB 미만" 까지가 관측 범위이며 실제 델타는 모른다. B-22 와 같은 한계다.
- **REQ-GSVG-019 의 3072×3072 경로에서는 측정하지 않았다.** 축소 근거는 §3.1 의 논증이며,
  두 크기에서 증가량이 같다는 것을 **실측으로 확인하지는 않았다.**
- **기존 32회 케이스(`RepeatedLifecycleDoesNotLeakOrCrash`)는 그대로 뒀다.** 단언을 바꾸지 않았다.
- **`suppress_grid_row_mean` / `apply_vignette_scalar` 의 SIMD 경로는 확인하지 않았다.**
  이 빌드에서 스칼라판이 도는지 SIMD 판이 도는지 구분하지 않았고, 사이클이 어느 쪽을
  지나는지는 미확인이다.
- **Linux 경로 미검증** — `#ifndef _WIN32` → `GTEST_SKIP`. 계측은 Windows 전용이다.
- **수백 KB 급 누수의 검출력은 측정하지 않았다.** 4 MB 주입은 확실히 잡힌다는 것까지만 안다.

---

## 5. 잔여 위험 (Residual-risk)

- **워킹셋은 힙의 대리 지표다.** OS 페이지 회수나 다른 스레드/DLL 활동이 수치를 양방향으로
  흔들 수 있다. warm-up 100회가 기동 비용만 걷어낸다.
- **config 를 켠 것은 커버리지를 위한 것이지 제품 기본값 주장이 아니다.** 제품 기본값은
  두 단계 모두 off 이며(`gsvg.cpp:190-191`), 이 테스트는 켜진 경로를 재고 있을 뿐이다.
- **gain map 은 균일값 1.05f 다.** 실제 비네팅 맵의 분포와 다르므로 데이터 의존 분기가
  있다면 그 경로는 지나지 않는다.
- 커밋은 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_gsvg.bat` | ci-post 빌드 + `GsvgEndurance` 5회 반복 |
| `_green.log` | 도입 후 5회 통과 (0.43~0.44초) |
| `_probe_red.log` | 4096 B/사이클 주입 → 5회 FAILED, 3976~3980 KB |
| `_probe_green.log` | 원복 후 5회 통과 |
| `_verify.log` | 재실측 444 / 129 / 48 |
