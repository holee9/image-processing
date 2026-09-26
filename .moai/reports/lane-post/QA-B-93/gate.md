# QA-B-93 (#180, #181) — 가상 그리드 상한·범위 처리, post 소유 누수 시험

커밋 (미푸시, `dev/postprocess`):
- `e675ca7` — 가상 그리드. 파일 5개(`virtual_grid.{h,cpp}`, `gsvg.cpp`, `gsvg_api.h`, `test_virtual_grid.cpp`)
- `1a4a10f` — #181. 시험 파일 6개와 `heap_growth.h` 5곳

## 1. 주장

1. **(1) 상한**: `[spr_cap]` 이 없으면 상한은 선택된 커널 행(gauss4 우선)의 Σa_i 이고, 커널과 같은 방식으로 혼합한다. 구획이 있으면 그 값을 쓴다.
2. **(2) 두께 범위**: 표 최대 두께를 넘는 영역은 최대값으로 제한하고 영상을 처리한다. 비율은 경고 alert 로 보고한다. 제한을 빼면 이전처럼 거부된다.
3. **최소 두께 미만**은 카드 문구("최소값에 제한")와 다르게 처리했다. 원래도 거부하지 않았고, t=0 에서 산란 0 으로 줄어드는 커널을 쓴다. 비율만 보고한다 — 2.3 참조. **리더 확인이 필요하다.**
4. **(3) 원해상도 음수 경로**: 상한을 끄면 도달한다(658 화소). 상한이 켜져 있으면 수식상 도달하지 않는다.
5. **새로 확인된 사실**: 자기 커널로 만든 데이터에서도 Σa_i 상한이 축소 격자의 2.96 % 에서 걸린다(두께 경계). 최대 오차가 0.35 → 0.80 으로 커진다 — 2.2 참조.
6. **#181**: post 소유 6개 파일의 7개 시험은 모두 작업 집합으로 **누수**를 판정하고 있었다. 7건 모두 힙 측정으로 바꾸고 대조군 6건을 붙였다. 제품 경로 힙 증가는 250/1000/4000 주기와 10회 반복에서 모두 0 이다.

## 2. 증거 — 가상 그리드 (`_vg3.log`, BUILD=0, 25건 통과)

### 2.1 (1) 상한 = Σa_i

`GsvgVirtualGridCap.KernelSumIsTheDefaultCap`

| 조건 | SprCapAt | 기대 |
|---|---|---|
| 10 cm, 80 kVp (노드) | 1.12 | 0.32 + 0.80 |
| 20 cm, 100 kVp | 2.60 | 0.45 + 2.15 |
| 15 cm, 70 kVp (네 노드 가운데) | 네 노드 합의 평균 | 같음 |
| 2.5 cm, 80 kVp | 0.25 | 첫 노드 × 0.5 |
| 45 cm (표 위) | 3.90 | 30 cm 값 |
| 120 kVp (표 밖) | < 0 | 거부 |
| `[spr_cap]` 있는 표, 10 cm/80 kVp | 1.7 | 구획 값 |
| gauss2 행에 a=9 를 넣고 gauss4 행과 함께 | 1.0 | gauss4 행의 합 |

### 2.2 상한이 과보정에서 걸리는지

`GsvgVirtualGridCap.KernelSumCapBindsOnOvercorrectionOnly`, 계단 장면, 반복 5

```
VGMEASURE kernel-sum cap: true data capped=0.0296; x3 kernels capped=0.9978 negative=0
VGMEASURE step.after5.kernelSumCap median=0.01423 p95=0.09205 max=0.79623
VGMEASURE step.after5.sectionCap median=0.01267 p95=0.08672 max=0.34983
```

- 커널 ×3 표(상한은 원래 커널의 Σa): 축소 격자의 99.78 % 에서 상한이 걸리고, 음수 화소는 0 이다.
- **커널 ×3 을 그대로 두면 Σa 도 3배가 되어 상한이 과보정을 막지 못한다.** 시험은 상한을 원래 표에서 가져와 이 점을 분리했다. 커널이 틀린 경우에 대한 방어가 아니라는 뜻이다.
- **원래 커널로 만든 데이터에서도 2.96 % 에서 상한이 걸렸다.** 원인: 계단의 얇은 쪽(8 cm) 화소는 두꺼운 쪽(24 cm)의 넓은 산란을 받는다. 그래서 그 화소의 SPR 이 자기 두께의 Σa 를 넘는다. 두께가 고르지 않은 물체에서 국소 Σa 는 국소 SPR 의 상한이 아니다. 여유 있는 합성 `[spr_cap]` 과 비교하면 중앙값 0.0127 → 0.0142, p95 0.0867 → 0.0921, 최대 0.350 → 0.796.
- 시험은 이 비율이 10 % 미만이라고만 단언한다(측정값 2.96 %).

### 2.3 (2) 두께 범위

`GsvgVirtualGridRange.ThickRegionIsLimitedAndReported` — 계단 장면에 36×36 화소(면적 1.98 %) 물 40 cm 영역

```
VGMEASURE patch.none.outside median=0.01410 p95=0.09043 max=0.34983
VGMEASURE patch.40cm.outside median=0.01507 p95=0.08771 max=0.32587
VGMEASURE patch area=0.0198 aboveFullRes=0.0162 clampedHigh(reduced)=0.0052 below=0.0000
VGMEASURE no patch: aboveFullRes=0.0000 clampedHigh=0.0000
```

- 처리된다(오류 없음).
- 영역 밖(3 cm 여백 제외) 오차: 중앙값 1.07배, p95 0.97배. 단언은 1.5배 미만.
- **비율이 두 가지다.**
  - 원해상도 비율(최종 1차 추정이 표 최대 두께보다 어두운 화소, I>0 기준): 1.62 %. 면적 ±25 % 로 단언.
  - 축소 격자에서 실제로 제한된 비율: 0.52 %.
  - 차이의 원인: 축소 격자 블록이 영역 가장자리에서 밝은 주변과 평균돼 얇게 보인다. 임시 디버그 출력(`_dbg3.log`, 커밋하지 않음)에서 영역을 지나는 한 줄의 7개 열 중 가운데 4개만 T = 30.00 이었고, 가장자리 열 3개는 29.02·29.96·29.32 였다.
  - 경고 alert 에는 두 값을 모두 적는다: `thickness above the table in X% of the pixels (limited to the table maximum in Y% of the reduced grid)`.
- 반증: `clampThickness=false` 면 오류를 내고 영상은 바뀌지 않는다.
- 공개 API: 5 DN 균일 영상(전 화소 범위 초과)은 이제 `XPE_OK` 를 반환하고 `100.00%` 가 담긴 경고 alert 를 낸다(`GsvgVirtualGridApi.ProcessesAndKeepsTheOriginalOnFailure`). 비네팅 + dst=src 에서의 원본 복원은 kVp 140(설정 오류) 경로로 옮겨 확인한다.

**최소 두께 미만 — 카드와 다르게 둔 이유**

- 현재 동작(B-91부터): 0 < t < 5 cm 는 첫 노드 커널 × t/5 를 쓴다. t = 0(공기)은 산란 0 이다. 거부한 적이 없다.
- 카드처럼 5 cm 로 제한하면 공기·얇은 부위가 5 cm 물만큼 산란을 만든다고 계산된다. 조사야 안의 공기 영역에서 산란을 과대 추정하게 된다.
- 그래서 제한하지 않고 `belowTableFraction` 으로 비율만 보고한다. `ThinRegionIsCountedNotRefused`(4 | 10 cm 계단): 비율 0.4745, 처리됨, 중앙 오차가 보정 전의 0.1배 미만.

### 2.4 (3) 원해상도 음수 경로

`GsvgVirtualGridFalsify.SprCapPreventsOvercorrection` — 얇은 계단, 커널 ×3 표

```
VGMEASURE overcorrect capped:   err='' negative=0   capped=0.9419 high=0.0000 meanSpr=2.366
VGMEASURE overcorrect uncapped: err='' negative=658 capped=0.0000 high=0.0063 meanSpr=5.691
```

- B-91 에서는 상한을 끄면 두께가 표 밖으로 나가 영상이 거부됐다. 그래서 이 경로에 도달하지 못했다. 이제 제한하므로 끝까지 처리되고, 658 화소에서 `I − S < 0` 이 나온다.
- 상한이 켜져 있으면 `S ≤ I·cap/(1+cap)` 이므로 `I − S ≥ I/(1+cap) ≥ 0` 이다. 도달할 수 없다.
- 계수 조건을 `≤ 0` 에서 `< 0` 으로 바꿨다. I = 0 인 화소는 S = 0 으로 잘려 0 이 되는데, 이것은 음수가 아니다.

### 2.5 시간 (로컬, 판정 외)

`_vg3.log`: 3072², 합성 표, med 650,541 µs. B-91 의 562,593 µs 보다 길다. 원해상도 비율 계산과 상한 호출(`SprCapAt`)이 늘었다. 같은 기계의 부하 차이와 나눠서 보지 않았다.

## 3. 증거 — #181

### 3.1 파일별 판독

검색 범위: 아래 6개 파일 전체. 검색어: `working.?set|GetProcessMemoryInfo|PROCESS_MEMORY_COUNTERS`, 그리고 각 시험의 단언문을 읽었다.

| 파일 | 시험 | 판정 대상 | 결론 |
|---|---|---|---|
| `ai/tests/test_ai_fallback.cpp` | `AiEndurance.ThousandCycles_MemoryGrowthUnderOneMB` | 1000 주기 뒤 작업 집합 증가 < 1 MB | 누수 판정 → 교체 |
| `dicom/tests/test_dicom_reader.cpp` | `DicomJ2kFailureTest.FailurePathsDoNotGrowWorkingSet` | 실패 디코드 1000×2 뒤 작업 집합 증가 < 1 MB | 누수 판정 → 교체. 주석이 가리킨 "sensitivity probe below" 는 파일에 없었다(`sensitivity` 검색 결과가 그 주석 한 줄) |
| `dicom/tests/test_dicom_writer.cpp` | `DicomWriterTest.ThousandCycles_MemoryGrowthUnderOneMB` | 작업 집합 증가 < 1 MB | 누수 판정 → 교체 |
| `display/tests/test_display_integration.cpp` | `DisplayEndurance.ThousandCycles_MemoryGrowthUnderOneMB` | 작업 집합 증가 < 1 MB | 누수 판정 → 교체 |
| `enhance_advanced/tests/test_integration.cpp` | `IntegrationTest.T605b_MemoryGrowthUnderOneMB` | 작업 집합 증가 < 1 MB | 누수 판정 → 교체. T605(측정 없음)는 그대로 |
| `enhance_basic/tests/test_enhance_integration.cpp` | `NoHeapLeak_1000Iterations`, `NoHeapLeak_1000Iterations_AllocatingPaths` | 작업 집합 증가 < 1 MB | 누수 판정 → 둘 다 교체 |

최대 메모리(예산) 판정은 6개 파일에 없었다.

### 3.2 변경

- `heap_growth.h`(5곳에 같은 내용)
  - `_heapwalk` 로 사용 중 블록을 센다. 워밍업은 100 주기다.
  - 단언: 블록 < 주기/10, 바이트 < 16 KB. 작업 집합은 기록만 한다.
  - `XPE_HEAP_CYCLES` 로 주기 수를 바꿀 수 있다.
- 시험 이름: `…MemoryGrowthUnderOneMB` → `…CrtHeapDoesNotGrow`, `FailurePathsDoNotGrowWorkingSet` → `FailurePathsDoNotGrowCrtHeap`. `NoHeapLeak_*` 두 건은 이름을 유지했다.
- 대조군 6건(`*_ControlLeakIsCaught`): 같은 주기에 64 B 를 매번 남긴다. 블록·바이트가 주기의 90 % 이상이어야 한다.
- 비 Windows 에서는 7건 모두 `GTEST_SKIP` 한다. `NoHeapLeak_1000Iterations` 는 예전에 비 Windows 에서도 돌며 아무것도 단언하지 않았는데, 이제 건너뛴다.

### 3.3 주기 비례와 반복 (`_heap1.log` → `_heap_table.txt`)

| 시험 | 250 | 1000 | 4000 | 1000 × 10회 |
|---|---|---|---|---|
| 제품 7건 | 모두 0 B / 0 블록 | 모두 0 / 0 | 모두 0 / 0 | 모두 (0, 0) |
| 대조군 6건 | 16,000 B / 250 | 64,000 / 1000 | 256,000 / 4000 | 모두 (64,000, 1000) |

같은 실행의 작업 집합(바이트):
- 제품 경로: 0 – 528,384. 예: `T605b` 1000 주기 524,288, 4000 주기 528,384.
- 대조군: 8,192 – 634,880.

작업 집합은 누수 유무와 관계없이 움직였다.

### 3.4 빌드 설정

`build.ninja`: `-MD`/`-MDd` 는 ci-post 102, ci-ai-b20 37, ci-dicom 34건, `-MT` 는 세 곳 모두 0건이다.

## 4. 전체 검증

`_verify2.log`(두 커밋 뒤, `heap_growth.h` 주석 수정 포함):

```
===POST_BUILD=0===   100% tests passed, 0 tests failed out of 601   ===POST_EXIT=0===
===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
E2E                  100% tests passed, 0 tests failed out of 20    ===E2E_EXIT=0===
```

- 594 → 601: 가상 그리드 +4, 대조군 +3(display, enhance_advanced, enhance_basic)
- 224 → 225: ai 대조군
- 192 → 194: dicom 대조군 2

빌드 실패 기록:
- `_vg1.log`: BUILD=1, C2664. 시험 결과는 낡은 바이너리라 읽지 않았다.
- `_dbg1.log`: C2001, heredoc `\n`.
- `_dbg2.log`: C4996, getenv.

## 5. 기준 귀속

- 두께 범위 시험의 기준은 같은 장면에서 영역만 뺀 실행이다. 음수 경로의 기준은 같은 입력·같은 표에서 상한만 켠 실행이다.
- 누수 시험의 기준은 같은 실행파일·같은 주기에서 64 B 를 남기는 대조군이다.
- 문턱은 측정값을 보고 정했다(`_vg2`·`_vg3`, `_heap1`).

## 6. 미검증

- **각 모듈 DLL 할당이 힙 워크에 보이는지** — gsvg 에서만 확인했다(B-92 `ControlDllHandleIsVisibleToHeapWalk`). 나머지 5개 모듈은 빌드 플래그(-MD)만 확인했다. 대조군은 시험 쪽 할당이다.
  - dicom 의 DCMTK·OpenJPEG 는 vcpkg 에서 온 DLL 이다. 이 DLL 들의 런타임 설정은 확인하지 않았다. 이들이 별도 힙을 쓰면 dicom 의 0 은 그 라이브러리 내부 누수를 보지 못한다.
- CI 러너에서의 결과(푸시 금지)
- 최소 두께 미만 처리 방식에 대한 리더 확인(1.3)
- Σa 상한이 실제 영상의 두께 경계에서 주는 영향. 합성 계단 하나만 봤다.
- preprocess 파일 2개(pre 레인 소유)는 보지 않았다.

## 7. 잔여 위험

- Σa 상한은 두께 경계에서 참 SPR 보다 작을 수 있다(2.2). `[spr_cap]` 이 채워지기 전까지 이 기본값을 쓰면 경계 부근이 덜 보정된다.
- 커널이 과대 추정된 표에서는 Σa 상한도 같이 커져 과보정을 막지 못한다(2.2).
- 원해상도 비율은 "최종 1차 추정이 표 최대 두께보다 어둡다"로 센다. 그 추정 자체가 경계에서 흔들린다(1.62 % 대 면적 1.98 %).
- 힙 워크는 CRT 힙만 본다. `VirtualAlloc`, 별도 힙, GPU 메모리 누수는 보지 못한다. ai 는 stub 빌드라 ONNX 경로를 재지 않는다.

## Card Cross-Check

| 카드 요구 | 결과 |
|---|---|
| (1) `[spr_cap]` 없으면 Σa_i, 같은 행 | 2.1 |
| (1) 과보정에서 상한이 걸림 | 2.2 |
| (2) 초과 두께 제한, 비율 보고 | 2.3 |
| (2) 약 2 % 영역: 처리·비율·영역 밖 오차 | 2.3 (비율 두 가지) |
| (2) 반증: 제한 빼면 거부 | 2.3 |
| (2) 최소 두께 미만 — 현재 동작 확인 | 2.3 (제한하지 않음, 리더 확인 필요) |
| kVp·격자비는 그대로 CONFIG_INVALID | 2.3, 기존 시험 |
| (3) 음수 경로 시험 또는 도달 불가 설명 | 2.4 (둘 다) |
| #181 파일별 판독·교체 | 3.1–3.3 |
| BUILD_EXIT | 4 |
