# QA-B-92 (#180) — GsvgEndurance 실패 판정

커밋 `a425d8e`, `e18ac86` (미푸시, `dev/postprocess`). 변경 파일 1개: `modules/gsvg/tests/test_gsvg_abi_smoke.cpp`.

## 1. 주장

1. main `63af26c` 의 `GsvgEndurance.ThousandCycles_MemoryGrowthUnderOneMB` 실패(2,588,672 B)는 **작업 집합 잡음**이며 누수가 아니다 — 판정 (나).
2. 시험을 CRT 힙 워크(`_heapwalk`) 측정으로 바꿨다. 같은 측정이 가짜 누수 2종을 잡고, DLL 이 만든 핸들도 본다(대조군 3개).
3. 새 시험은 로컬 20회 반복에서 안정적이다(BUILD=0, REPEAT_EXIT=0).
4. 문턱만 올린 것이 아니다 — 측정량 자체를 바꿨다.

## 2. 증거

### 2.1 주기 수 비례 (`_probe.log`, 임시 프로브 `GsvgLeakProbe.Report`, 커밋 전 삭제)

CRT 힙 증가 (사용 중 블록 수 / 바이트):

| 입력 | 250 주기 | 1000 주기 | 4000 주기 |
|---|---|---|---|
| 평탄(B-90 이전 입력과 같은 형태) | 0 / 0 | 0 / 0 | 0 / 0 |
| 그리드(행 교대 12000/12100) | 0 / 0 | 0 / 0 (추가 5회도 0) | 0 / 0 |
| 가짜 누수 64 B/주기 | 250 / 16,000 | 994 / 60,856 | 4000 / 256,000 |

같은 실행의 작업 집합 증가 (바이트):

| 입력 | 250 | 1000 | 4000 |
|---|---|---|---|
| 평탄 | 0 | 135,168 | 0 |
| 그리드 | 86,016 | 0 | 8,192 |
| 가짜 누수 | 2,330,624 | −40,960 | 2,473,984 |

가짜 누수는 힙에서 주기에 비례하고, 작업 집합에서는 비례하지 않는다. 제품 경로는 힙에서 0 이다.

### 2.2 옛 시험 재현 (`_probe.log` 후반, 새 바이너리)

옛 작업 집합 시험 20회 중 2회 실패: 2,527,232 B, 2,490,368 B. CI 값(2,588,672 B)과 같은 크기.

### 2.3 새 시험 20회 (`_run2.log`, `_b92.bat`: `GsvgEndurance.* --gtest_repeat=20`)

```
===BUILD=0===
===REPEAT_EXIT=0===
FAILED 줄 수: 0
     19 heap growth 0 bytes / 0 blocks, working set 0 bytes (not asserted)
      1 heap growth 0 bytes / 0 blocks, working set 2981888 bytes (not asserted)
     20 control 64 B/cycle: heap 64000 bytes / 1000 blocks
     18 control 1 MB/100 cycles: heap 10485760 bytes / 10 blocks
      1 control 1 MB/100 cycles: heap 10482616 bytes / 4 blocks
      1 control 1 MB/100 cycles: heap 10496656 bytes / 34 blocks
```

이 20회 안에서도 작업 집합이 한 번 2,981,888 B 를 찍었다 — 옛 시험이었다면 실패, 힙은 0.

### 2.3b DLL 할당 가시성 대조 (`_run3.log`, `e18ac86` 추가 후 20회)

```
===BUILD=0===
===REPEAT_EXIT=0===
FAILED 줄 수: 0
     20 live handle: +1 blocks / +2 bytes; after shutdown: 0 / 0
     20 heap growth 0 bytes / 0 blocks
```

`xpe_gsvg_init` 이 `gsvg.cpp:204` 에서 `new` 로 만든 핸들이 워크에 보인다. 제품 경로의 0 은 눈먼 0 이 아니다.

### 2.4 전체 검증 (`_verify2.log`, `_verify.bat`, `e18ac86` 기준)

```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 573   ===POST_EXIT=0===
===AI_BUILD=0===     100% tests passed, 0 tests failed out of 224   ===AI_EXIT=0===
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 192   ===DICOM_EXIT=0===
E2E                  100% tests passed, 0 tests failed out of 20    ===E2E_EXIT=0===
```

ci-post 573 = B-90 의 570 + 대조군 3. CI 선택식(`-E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"`)에 `GsvgEndurance` 4건이 모두 포함된다(`SELECT_CI` 목록 #495~#498). `a425d8e` 단독 검증은 `_verify.log`(572, 전부 통과).

## 3. 기준 귀속

- 판정 기준: 같은 도구·같은 실행에서 (a) 제품 경로의 힙 증가, (b) 알려진 누수의 힙 증가. (b)가 주기에 비례하고 (a)가 0 이면 측정이 누수를 보면서 제품에서는 못 본 것이다.
- 문턱: 1000 주기에 블록 < 100, 바이트 < 16 KB. 제품 측정값 0/0, 소형 대조군 1000 블록/64 KB.
- 대형 대조군 하한은 `9 MiB` (보유 10 MiB). 1차 20회(`_run1.log`)에서 한 번 10,482,616 B / 4 블록이 나와 `10 MiB` 하한에서 실패했다. 같은 구간에 다른 블록이 해제되면 순증가가 몇 KB 모자랄 수 있다. 2차 20회에서도 같은 값이 한 번 나왔고 `9 MiB` 하한은 통과했다. 이 조정은 대조군(누수를 잡는 쪽)의 하한이며 제품 문턱은 바꾸지 않았다.

## 4. 미검증

- **CI 러너에서의 새 시험 결과** — 푸시 금지라 돌리지 않았다.
- 대형 대조군에서 블록 수가 4·34 로 나온 이유(힙 내부의 병합·분할로 추정) — 원인을 확인하지 않았다. 바이트 합은 보유량과 3 KB 이내로 맞는다.
- 비 Windows 경로: `GTEST_SKIP` 스텁만 있다. 대조군 3개는 Windows 전용으로 정의돼 있다.
- `_heapwalk` 는 CRT 힙만 본다. `VirtualAlloc`·`HeapAlloc` 직접 할당 누수는 보지 못한다. 현재 `modules/gsvg/src`·`include` 에서 두 이름은 0건(같은 grep 으로 `std::vector` 는 `grid_dwt.cpp`·`grid_dwt.h` 에서 검출 — 대조).
- `/MT` 로 빌드하면 DLL 과 시험이 힙을 공유하지 않아 제품 할당이 안 보인다. ci-post `build.ninja` 에서 `gsvg.cpp.obj`·`test_gsvg_abi_smoke.cpp.obj` 모두 `-MD` 1건씩(`-MT` 0건). DLL 할당이 보이는 것은 2.3b 로 확인했다. 다만 `/MT` 로 실제 빌드해 그 대조군이 실패하는지는 돌려 보지 않았다 — "/MT 면 실패한다"는 추론이다.

## 5. 잔여 위험

- 힙 워크 결과는 해제 순서에 따라 수 KB 흔들린다(2.3 의 대형 대조군). 제품 문턱 16 KB 는 측정값 0 에서 여유가 있지만, 제품이 소량의 캐시를 첫 사용 뒤 유지하도록 바뀌면 워밍업 100 주기 이후에 잡히는지 다시 봐야 한다.
- `working set` 은 기록만 한다. 다른 모듈(enhance_basic, preprocess T-010)의 같은 형태 시험은 여전히 작업 집합으로 판정한다 — 이번 카드 범위 밖이며 같은 잡음에 노출돼 있다.
- 작업 파일 `modules/gsvg/CMakeLists.txt` 가 `git status` 에 M 으로 남아 있다. 내용은 HEAD 와 바이트 동일(`cmp` 확인)이며, 임시 프로브 줄을 `sed -i` 로 넣고 뺀 뒤의 파일 시각 표시다.

## Card Cross-Check

| 요구 | 결과 |
|---|---|
| 20회 로컬 재현 | 2.2 (옛 시험 2/20 실패) |
| 작업 집합 아닌 방법으로 판정 | 2.1 (힙 워크, 250/1000/4000) |
| 가짜 누수 대조 | 2.1, 2.3 (64 B·1 MB) |
| B-90 이전 평탄 입력 대조 | 2.1 (평탄 0/0/0) |
| 문턱만 올리지 않음 | 1.4, 3 |
| 측정이 제품 할당을 보는가 | 2.3b |
| 새 시험 20회 안정 | 2.3 |
| BUILD_EXIT 보고 | 2.3, 2.4 |
