# QA-B-22 게이트 보고서 — 누수 게이트(G3) 나머지 4개 모듈

**카드**: QA-B-22 (#105)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-22/`
**선행 병합**: `git merge main` → fast-forward, HEAD `45e1c36` (QA-B-21 통합본)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 현황 조사 결과 G3 미충족 모듈은 enhance_advanced · display · ai · dicom 4개다 (gsvg 는 별건, §2.1) | PASS |
| C2 | 4개 모듈 전부에 warm-up 100 → 기준선 → 1000회 → 증가량 < 1 MB 케이스를 넣었다 | PASS |
| C3 | 구현은 enhance_basic(92bcf17)을 그대로 맞췄다 — 임계값·사이클 수·warm-up 수 동일 | PASS |
| C4 | 민감도 프로브(사이클당 4096 B 누수 주입)로 4개 모듈 전부 FAILED 관측, 5회 재현 | PASS |
| C5 | 프로브 원복 후 5회 전부 통과, 소스 잔재 0건 | PASS |
| C6 | 재실측 ci-post 443 / ci-ai 129 / ci-dicom 48 전부 통과 | PASS |
| C7 | 시간 예산 단언은 넣지 않았다 (B-19 원칙) | PASS |

---

## 2. 증거 (Evidence)

### 2.1 현황 표 (작업 전)

| 모듈 | 지구력 루프 | 힙 계측 | warm-up | 판정 |
|---|---|---|---|---|
| `preprocess` | 1000회 (`test_xpe_calib_endurance.cpp` T-010) | 있음 | 있음 (A-16) | 기존 충족 — Lane A 소유, 손대지 않음 |
| `enhance_basic` | 1000회 ×2 (`test_enhance_integration.cpp`) | 있음 | 있음 (`ENDURANCE_WARMUP`, 92bcf17) | 기존 충족 — QA-B-05 |
| `enhance_advanced` | 1000회 (`test_integration.cpp` T605) | **없음** | 없음 | **대상** |
| `display` | **없음** | 없음 | 없음 | **대상** |
| `ai` | **없음** | 없음 | 없음 | **대상** |
| `dicom` | **없음** | 없음 | 없음 | **대상** |
| `gsvg` | 32회 (`test_gsvg_abi_smoke.cpp` `RepeatedLifecycleDoesNotLeakOrCrash`) | 없음 | 없음 | §4 참조 — 이 카드 범위 밖 |

`enhance_advanced` T605 는 루프만 있고 계측이 없다. 소스 주석이 스스로 그렇게 적고 있다:
```
// Note: Actual memory leak detection requires platform-specific tools:
...
        // Optional: Check memory every 100 cycles
        if ((cycle + 1) % 100 == 0) {
            // Log checkpoint (no actual memory measurement here)
        }
```
즉 "1000회 돌고 안 죽었다" 만 확인한다 — 보유 결함은 조용히 통과한다.

### 2.2 추가한 케이스 (C2·C3)

각 파일에 `get_working_set_bytes()` + `ENDURANCE_CYCLES/WARMUP/ONE_MB` 를 enhance_basic 과
같은 형태로 복제했다(`xpe_common` export 는 REQ-P0-008 로 16개 고정이라 공용화하지 않는다).
`psapi.lib` 링크를 4개 테스트 타깃에 추가했다(enhance_basic CMake 선례와 동일한 `if(WIN32)` 블록).

| 모듈 | 케이스 | 사이클 내용 |
|---|---|---|
| `enhance_advanced` | `IntegrationTest.T605b_MemoryGrowthUnderOneMB` | 128×128 FLOAT32 alloc → multiscale / fractional / collimation / exposure_index → free |
| `display` | `DisplayEndurance.ThousandCycles_MemoryGrowthUnderOneMB` | 64×64 alloc → modality_lut → voi_lut → 정규화 → presentation_lut(버퍼 교체) → free |
| `ai` | `AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB` | `xpe_ai_init` → bodypart_recognize / dl_denoise (stub, `PROCESSING_FAILED`) → `xpe_ai_shutdown` |
| `dicom` | `DicomWriterTest.ThousandCycles_MemoryGrowthUnderOneMB` | 64×64 UINT16 `xpe_alloc_image` → `xpe_dicom_write`(같은 경로 덮어쓰기) → `xpe_free_image` |

T605 는 그대로 두고 계측판을 T605b 로 따로 넣었다(기존 케이스의 단언을 바꾸지 않는다).
비용은 실측 +2.23초다(§3).

### 2.3 GREEN (`_green.log`)

```
1/5 Test #130: EnhanceIntegration.NoHeapLeak_1000Iterations ...................   Passed    0.01 sec
2/5 Test #131: EnhanceIntegration.NoHeapLeak_1000Iterations_AllocatingPaths ...   Passed    0.10 sec
3/5 Test #216: IntegrationTest.T605_MemoryLeakEndurance .......................   Passed    2.06 sec
4/5 Test #222: IntegrationTest.T605b_MemoryGrowthUnderOneMB ...................   Passed    2.23 sec
5/5 Test #413: DisplayEndurance.ThousandCycles_MemoryGrowthUnderOneMB .........   Passed    0.02 sec
100% tests passed, 0 tests failed out of 5
===POST_CTEST=0===
[       OK ] AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB (4 ms)
===AI_RUN=0===
2/2 Test #93: DicomWriterTest.ThousandCycles_MemoryGrowthUnderOneMB ...   Passed    1.30 sec
===DICOM_CTEST=0===
```

### 2.4 민감도 프로브 (C4) — 게이트가 눈뜨고 있는가

각 모듈의 측정 사이클 첫 줄에 사이클당 4096 B 누수를 주입했다
(1000회 = 4 MB, 임계값 1 MB 의 4배). 주입 코드는 최적화로 사라지지 않도록 `volatile` 로 소비한다.

**주입 후 실측 증가량** (`_probe_detail.log`):
```
Working set grew by 4496 KB over 1000 enhance_advanced alloc-process-free cycles
Working set grew by 4024 KB over 1000 display modality/voi/presentation cycles
Working set grew by 3988 KB over 1000 ai init/process/shutdown cycles
Working set grew by 4076 KB over 1000 dicom alloc/write/free cycles
```
네 모듈 모두 주입량(4096 KB)과 일치하는 값을 관측했다 — 계측이 실제로 힙을 보고 있다.

**5회 재현** (`_probe_red.log`):
```
### RUN 1   0% tests passed, 2 tests failed out of 2   ===POST_RUN1=8===   ===AI_RUN1=1===   0% tests passed, 1 tests failed out of 1   ===DICOM_RUN1=8===
### RUN 2   0% ... ===POST_RUN2=8===   ===AI_RUN2=1===   0% ... ===DICOM_RUN2=8===
### RUN 3   0% ... ===POST_RUN3=8===   ===AI_RUN3=1===   0% ... ===DICOM_RUN3=8===
### RUN 4   0% ... ===POST_RUN4=8===   ===AI_RUN4=1===   0% ... ===DICOM_RUN4=8===
### RUN 5   0% ... ===POST_RUN5=8===   ===AI_RUN5=1===   0% ... ===DICOM_RUN5=8===
```
5회 × 4모듈 = 20건 전부 FAILED. 산발적 통과 없음.

**증거 등급 정정 (leader 지적 접수, 2026-09-10).** 위 로그에서 ai 의 5회는 `===AI_RUN1..5=1===`
종료 코드만 남았다 — `_probe.bat` 이 ai 실행 출력을 `> nul` 로 버렸기 때문이다. 나머지 3모듈은
`0% tests passed` 줄이 있는데 ai 만 없었으니, 같은 "20건" 으로 세면 로그가 뒷받침하는 것보다
강하게 적는 것이 된다. ai 만 출력을 살려 다시 찍었다 (`_probe_ai.bat` → `_probe_red_ai.log`):

```
Working set grew by 3988 KB over 1000 ai init/process/shutdown cycles
[  FAILED  ] AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB (5 ms)
===AI_RUN1=1===      ... (RUN2~RUN5 동일, 5회 모두 3988 KB)
===AI_RUN5=1===
```
`FAILED  ] AiEndurance` 줄 10건(회당 gtest 요약 2줄 × 5회) 관측.
이제 4모듈 모두 이름 붙은 실패 줄로 5회씩 확인된다.

### 2.5 원복 (C5) — `_probe_green.log`

```
===POST_RUN1=0===  ===AI_RUN1=0===  ===DICOM_RUN1=0===
===POST_RUN2=0===  ===AI_RUN2=0===  ===DICOM_RUN2=0===
===POST_RUN3=0===  ===AI_RUN3=0===  ===DICOM_RUN3=0===
===POST_RUN4=0===  ===AI_RUN4=0===  ===DICOM_RUN4=0===
===POST_RUN5=0===  ===AI_RUN5=0===  ===DICOM_RUN5=0===
```
5회 전부 통과. ai 도 출력을 살려 5회 재확인했다 (`_probe_green_ai.log`):
```
[       OK ] AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB (12 ms)   ===AI_RUN1=0===
[       OK ] ... (4 ms)   ===AI_RUN2=0===  ===AI_RUN3=0===  ===AI_RUN4=0===  ===AI_RUN5=0===
```
잔재 검사:
```
$ grep -rn "QA-B-22-PROBE" modules/ tests/
(출력 없음)
```

### 2.6 재실측 (C6) — `_verify.log`

```
===CI_POST===    100% tests passed, 0 tests failed out of 443   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 129   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 48    ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | QA-B-21 실측 | QA-B-22 실측 | 차 |
|---|---|---|---|
| ci-post | 439 | 443 | +4 |
| ci-ai | 128 | 129 | +1 |
| ci-dicom | 47 | 48 | +1 |

**+4 중 내 몫은 3이다.** 두 로그의 테스트 이름 집합을 직접 비교했다:

```
$ comm -13 b21.txt b22.txt
AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB          ← 이 카드
DicomWriterTest.ThousandCycles_MemoryGrowthUnderOneMB      ← 이 카드
DisplayEndurance.ThousandCycles_MemoryGrowthUnderOneMB     ← 이 카드
IntegrationTest.T605b_MemoryGrowthUnderOneMB               ← 이 카드
XpeCommonTest.LogFlushAfterShutdownDoesNotCrash            ← Lane A (#116), 이 카드 아님
XpeCommonTest.ShutdownReleasesLogFileSink                  ← Lane A (#116), 이 카드 아님
```
사라진 테스트는 0건. `XpeCommonTest` 2건은 `modules/common` (Lane A 소유) 것으로,
이번 빌드에서 `gtest_discover_tests` 가 새로 잡은 것이다 — 내 변경이 아니다.
**숫자 차이를 전부 내 공로로 적으면 안 되므로 분리해 기록한다.**

시간 비용 실측: T605b 2.23초, dicom 1.30초, display 0.02초, ai 0.004초 추가.

---

## 4. 미검증 (Gaps)

- **통과 경로의 실제 증가량 수치는 관측하지 못했다.** 테스트는 임계값 초과 시에만
  `Working set grew by ...` 를 출력한다. 통과 = "1 MB 미만" 만 알 수 있고 실제 델타(예: 0 KB 인지
  900 KB 인지)는 로그에 없다. 임계값을 바꿔 재측정하는 것은 카드가 금지했으므로 하지 않았다.
- **DCMTK 내부 캐시 증가는 관측되지 않았다** — dicom 케이스가 5회 모두 통과했다. 카드 4번이
  "잡히면 수치와 함께 보고" 라고 했는데 잡히지 않았다. 다만 위 항목 때문에 "증가가 0" 이라고는
  말할 수 없다. **"1 MB 미만" 까지가 관측 범위다.**
- **ai 의 추론 경로 누수는 측정 대상이 아니다.** stub 빌드라 `PROCESSING_FAILED` 로 즉시 반환한다.
  이 케이스가 덮는 것은 `xpe_ai_init`/`xpe_ai_shutdown` 수명주기와 검증 경로뿐이다.
  **ONNX 실경로는 여전히 미검증이며, 이 GREEN 을 "AI 누수 없음"으로 읽으면 안 된다.**
- **`gsvg` 는 이 카드에서 다루지 않았다.** 현황은 32회 수명주기 루프에 힙 계측·warm-up 모두 없음
  (`test_gsvg_abi_smoke.cpp:224`). 카드가 지정한 4개 모듈에 없어 손대지 않았다 — 별도 카드 판단은 leader 몫.
- **Linux 경로 미검증.** 네 케이스 모두 `#ifndef _WIN32` → `GTEST_SKIP`. 계측은 Windows 전용이다.
- **`preprocess` 는 확인만 했고 실행하지 않았다** — Lane A 소유. 표의 "기존 충족" 은 소스 판독 근거다.

---

## 5. 잔여 위험 (Residual-risk)

- **워킹셋은 힙의 대리 지표다.** OS 가 페이지를 회수하면 실제 누수가 있어도 증가가 1 MB 아래로
  보일 수 있고, 반대로 다른 스레드/DLL 활동이 증가로 잡힐 수도 있다. warm-up 100회가 그중
  기동 비용만 걷어낸다. 4 MB 주입은 확실히 잡히지만 **수백 KB 급 누수의 검출력은 측정하지 않았다.**
- **`display` 사이클은 `presentation_lut` 앞에 정규화 한 줄을 넣었다.** voi_lut 출력이
  `[minOut,maxOut]` 이고 presentation 은 `[0,1]` 을 기대해서다. 이 줄은 테스트 편의이지
  제품 파이프라인의 순서를 주장하는 것이 아니다.
- **`dicom` 사이클은 같은 경로에 1000번 덮어쓴다.** 파일시스템 캐시가 워킹셋에 잡힐 수
  있는 구조이며, 이번엔 임계값 아래였다. CI 의 디스크가 다르면 결과가 달라질 수 있다.
- **`ci-dicom` 은 vcpkg DLL 경로가 PATH 에 있어야 빌드·실행된다** (없으면 gtest 탐색이 `0xc0000135`).
  `_end.bat` / `_verify.bat` 에 경로가 박혀 있다.
- 커밋은 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_end.bat` | 3개 config 빌드 + 지구력 케이스 실행 |
| `_green.log` | 도입 직후 GREEN (5 + 1 + 2) |
| `_probe.bat` | 프로브 5회 반복 실행기 |
| `_probe_red.log` | 4096 B/사이클 주입 → 5회 × 4모듈 전부 FAILED |
| `_probe_detail.log` | 주입 시 실측 증가량 4496 / 4024 / 3988 / 4076 KB |
| `_probe_green.log` | 원복 후 5회 전부 통과 (ai 는 종료 코드만) |
| `_probe_ai.bat` | ai 전용 5회 반복 실행기 (출력 보존) |
| `_probe_red_ai.log` | ai 주입 5회 — 이름 붙은 FAILED + 3988 KB × 5 |
| `_probe_green_ai.log` | ai 원복 5회 — 이름 붙은 OK × 5 |
| `_verify.log` | 재실측 443 / 129 / 48 |
