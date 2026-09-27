# QA-A-100 — pre 소유 누수 시험 2건을 힙 측정으로 교체 (#181)

커밋 `4213cff` (dev/preprocess, 미푸시, 기준 main `3cfe6e9`)

## 1. 주장

1. 두 시험은 바꾸기 전에 힙을 재지 않았다.
   - `test_xpe_calib_endurance.cpp` 의 옛 `ThousandCycles_MemoryGrowthUnderOneMB`: `GetProcessMemoryInfo` 의 `WorkingSetSize` 증가량이 1 MB 미만인지 봤다(옛 32–38행, 118–133행).
   - `test_xpe_preprocess_memleak.cpp` 의 옛 `NoMemoryLeakAfter1000Frames`: `PROCESS_MEMORY_COUNTERS_EX::PrivateUsage` 증가량이 max(기준의 5%, 2 MB) 이하인지 봤다(옛 60–61행, 190–209행).
2. 두 시험을 CRT 힙 워크(`_heapwalk`, `heap_growth.h`, post 레인 QA-B-93 과 같은 파일)로 바꾸고, 64 B/주기 대조 시험을 하나씩 붙였다.
   - `EnduranceTest.LoadCycles_DoNotGrowCrtHeap` / `LoadCycles_ControlLeakIsCaught`
   - `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` / `ControlLeakIsCaught`
3. 모듈에 누수를 심은 빌드에서는 본 시험 2건이 빨강이 되고, 되돌린 빌드에서는 초록이 된다.
4. 증가량은 반복 수에 비례한다(250/1000/4000).

## 2. 증거

| 빌드 | BUILD_EXIT | 반복 | calib 본 시험 | memleak 본 시험 | 대조 2건 |
|---|---|---|---|---|---|
| 변경 후 | 0 (warning C 0) | 1000 | 0 B / 0 블록, OK | 0 B / 0 블록, OK | 64000 B / 1000 블록, OK |
| 누수 심음 | 0 (warning C 0) | 250 | 16000 / 250, FAILED | 16000 / 250, FAILED | 32000 / 500 |
| 누수 심음 | | 1000 | 64000 / 1000, FAILED | 64000 / 1000, FAILED | 128000 / 2000 |
| 누수 심음 | | 4000 | 256000 / 4000, FAILED | 256000 / 4000, FAILED | 512000 / 8000 |
| 되돌림 | 0 | 250 | 0 / 0, OK | 0 / 0, OK | 16000 / 250 |
| 되돌림 | | 1000 | 0 / 0, OK | 0 / 0, OK | 64000 / 1000 |
| 되돌림 | | 4000 | 0 / 0, OK | 0 / 0, OK | 256000 / 4000 |

- 심은 누수: `xpe_preprocess_init` 와 `xpe_calib_load_offset` 의 `try {` 바로 뒤에 `(void)new char[64];` 한 줄씩. 원본은 scratchpad 보관본에서 복사해 되돌렸고, `git status` 에서 `modules/preprocess/src` 변경이 사라진 것을 확인했다.
- 누수 심음·1000 주기에서 calib 시험의 작업 집합은 −245760 B였다(누수가 있는데 작업 집합은 줄었다).
- 전체: `ctest --preset ci-preprocess` → `100% tests passed, 0 tests failed out of 653`, 종료 0.
- 로그: `a100-run-{base,leak,restored}.log`, `a100-run-{leak,clean}-{250,4000}.log`, `a100-build-*.log`, `a100-ctest.log`.

## 3. 기준 귀속

- 새 문턱은 `heap_growth.h` 의 값(블록 < 주기/10, 바이트 < 16 KB)이다. post 레인 QA-B-92/93 에서 정한 값을 그대로 썼다.
- 이 레인의 측정값: 누수 없는 두 시험은 250/1000/4000 주기에서 모두 0 B / 0 블록이었다. 문턱과의 여유는 전부 남아 있다.
- 옛 문턱(작업 집합 1 MB, PrivateUsage max(5%, 2 MB))은 삭제했다.

## 4. 미검증

- CI 에서는 아직 돌리지 않았다(푸시 금지). CI 결과는 병합 뒤 리더가 확인해야 한다.
- 스레드 3건(`FourThreads*`)은 누수를 재지 않으며 바꾸지 않았다.
- 심은 누수는 64 B 한 가지 크기와 두 진입점에서만 시험했다. `gain`/`defect` 적재 경로에만 생긴 누수는 따로 시험하지 않았다. 다만 calib 시험은 세 경로를 한 주기에 모두 부른다.
- Windows 밖에서는 두 시험이 건너뛰어진다(기존과 같다).

## 5. 잔여 위험

- UCRT(/MD)가 아닌 다른 할당자(예: 모듈 내부 `HeapAlloc`, `VirtualAlloc`)의 누수는 `_heapwalk` 에 보이지 않는다.
- 다른 스레드가 측정 중에 할당하면 블록 수가 흔들릴 수 있다. 이 시험들은 단일 스레드로 돈다.
