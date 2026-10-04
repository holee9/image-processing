# QA-A-238 — ASan 잡에서 제외한 시험이 정말 ASan 환경 탓인지 (#256)

기준 커밋: cb983b7d (dev/preprocess). 측정일 2026-10-04, 로컬 Windows 11, MSVC 14.44.35207, AddressSanitizer(`/fsanitize=address`), CPU i7-12700.

## 1. 결론

제외된 시험 다섯 묶음(`OomPipeline.*` 30개, `ControlLeakIsCaught` 2개, `Frame3072SquaredWithinMachineRatio`, `PipelinePerformance3072...Perf001`) 중 **진짜 메모리 결함(b)은 없었다.** 전부 ASan 환경 고유(a)로 분류된다. 시험 쪽을 고쳐서 ASan일 때 사유를 적고 건너뛰거나 ASan과 충돌하는 부분을 비켜 가게 했고, 그 결과 ASan에서 **이름 제외 없이** 전체가 통과한다. 따라서 CI `asan-tests` 잡의 `ctest -E` 제외 정규식은 지워도 된다 (로컬 검증은 §4).

| 시험 | 로컬 ASan 결과 | 분류 | 조치 |
|---|---|---|---|
| `OomPipeline.*` (30개) | 두 곳에서 `unknown-crash` (§2) | (a) 시험 자체의 할당기와 ASan의 충돌 | 시험 두 곳 수정, 수정 후 30개 모두 통과 |
| `XpePreprocessEndurance.ControlLeakIsCaught` | 힙 증가 0바이트 / 0블록 (기대 900블록 이상) | (a) 측정 장치가 ASan 할당기를 못 봄 | ASan일 때 `GTEST_SKIP` + 사유 |
| `EnduranceTest.LoadCycles_ControlLeakIsCaught` | 같은 실패 (0 대 900) | (a) 같은 원인 | 같음 |
| `RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio` | 비율 13.8 (한계 1.45), 시험 52 s | (a) ASan 계측이 제품 커널만 느리게 함 | 같음 |
| `PipelinePerformance3072.TheWholeFrameWithCalibrationLoadedFitsSrsPerf001` | 단독 451 ms 통과, 전체 실행 중 522.5 ms 실패 (한계 500) | (a) 시간 단언이 계측 아래에서 실행마다 뒤집힘 | 같음 |

## 2. `OomPipeline.*` 의 두 중단 지점 (증거 `21_`, `31_`, `32_`)

CI 에서 7/30 이 멈춘다고 한 것은 로컬에서 그대로 재현됐다. 첫 시험 묶음을 그대로 돌리면 두 군데에서 `unknown-crash` 로 프로세스가 중단된다. 둘 다 시험의 `operator new` 대체(`tests/test_oom_injection.cpp`)가 만든 구조와 ASan 이 부딪힌 것이고, 제품 코드의 접근이 틀린 것이 아니다.

### 2.1 `guard::check` (test_oom_injection.cpp:89, 시험 `TheFrameCopiedIsTheOneTheDimensions...`)

- ASan 보고: `READ of size 1 at 0x…ea41`, **"located 1 bytes inside of 257-byte region [0x…ea40, 0x…eb41)"**, 할당 스택은 시험의 `operator new` (`test_oom_injection.cpp:190`) 안의 `malloc`. 즉 읽은 주소는 시험이 직접 확보한 블록 안이다: 1바이트 요청 + 카나리 256바이트 (`std::vector<char>` 의 1바이트 버퍼, `nlohmann` 의 JSON 렉서가 만든 것).
- 폭 쓰기·읽기 바깥이 아니다. 섀도 바이트가 문제의 8바이트 칸에서 `01`(앞 1바이트만 접근 가능)인데, 이 블록은 257바이트라 앞 칸이 `00` 이어야 한다: 누군가(MSVC STL 이 `std::vector` 버퍼에 다는 ASan 표시로 추정, 확정 못 함) 요청 크기 이후 바이트를 접근 불가로 표시했고, 시험의 카나리 검사는 의도적으로 요청 크기 뒤의 카나리 바이트를 읽는다.
- `ASAN_OPTIONS=detect_container_overflow=0` 으로 컨테이너 검사를 꺼도 같은 지점에서 같은 보고가 나왔다(`30_`). 그래서 "컨테이너 표시"는 가설이고, 확정된 것은 위의 읽은 바이트가 블록 안이라는 것과 시험 쪽 읽기를 허용하면 사라진다는 것이다.
- 조치: 카나리 검사가 읽기 직전에 그 구간을 ASan 에서 접근 가능으로 되돌린다 (`__asan_unpoison_memory_region`, `__SANITIZE_ADDRESS__` 일 때만). 블록은 바로 뒤에 해제된다.

### 2.2 `std::_Zero_range<unsigned short *>` (xmemory:2072, 시험 `AGainBypassFollowedByBinningDoesNotReadPastTheStageBuffer`)

- ASan 보고: `WRITE of size 32 at 0x…0ffc`, "wild pointer inside of access range", 호출 스택은 `pipeline.cpp:252` 의 `stage2Data.resize(pixelCount)` (`std::vector<unsigned short>`). 섀도는 `[04]` (앞 4바이트만 접근 가능).
- 이 시험은 `pageguard` 를 켠다: 시험의 `operator new` 가 블록을 `VirtualAlloc` 으로 만들어 블록 끝이 접근 금지 페이지 앞에 오게 한다. ASan 은 `VirtualAlloc` 영역을 추적하지 않아서 **같은 주소에 있던 이전 힙 블록의 섀도가 남아** 유효한 접근을 막는 것으로 보인다 (추정: 이 주소가 시험의 페이지 가드 영역인지 직접 확인하지는 못했다).
- 가르는 실험: ASan 일 때 페이지 가드 할당을 건너뛰게 하고(그 접근은 일반 `malloc` 블록이 되어 ASan 이 정확한 경계를 검사) 같은 30개를 다시 돌렸다. 30개 모두 통과했고 제품 프레임의 ASan 보고는 없었다(`32_`). 만약 제품에 실제 과읽기·과쓰기가 있었다면 이 실행에서 `heap-buffer-overflow` 가 나왔을 것이다.
- 페이지 가드 시험(`AGainBypassFollowedByBinning...`, `EveryConfigurationReadsOnly...` 등)은 `faulted == false` 만 단언한다(`faulted` 가 참이어야 하는 시험은 없음). ASan 에서는 접근 위반이 시험의 SEH 대신 ASan 중단으로 나타나므로 같은 질문에 더 엄격하게 답한다.

두 조치 뒤 `OomPipeline.*` 30개와 `xpe_preprocess_oom_tests` 전체 88개가 ASan 에서 통과한다 (`32_`, `40_`).

## 3. 나머지 시험의 근거

- **`ControlLeakIsCaught` 두 개** (`22_`, `43_`): 측정 장치가 CRT 힙을 걷는다(`test_xpe_preprocess_memleak.cpp`, `test_xpe_calib_endurance.cpp`). ASan 에서는 `malloc` 이 ASan 의 할당기라서 걷는 힙에 블록이 없다: "control 64 B/cycle: heap growth 0 bytes / 0 blocks over 1000 cycles". 대조 시험이 "측정 장치가 누수를 볼 수 있다"를 증명하는 것인데 ASan 에서는 장치가 눈이 멀었다. 누수가 새서 못 잡은 것이 아니다.
- **`Frame3072SquaredWithinMachineRatio`** (`23_`): `ratio=13.805 limit=1.450`, `ratio=14.025`. 분자는 제품 커널(ASan 계측), 분모는 시험 안의 기준 커널이라 ASan 은 한쪽만 느리게 한다.
- **`Perf001`** (`20_`, `50_`): 같은 빌드, 같은 기계에서 단독 실행은 첫 프레임 451.0 ms(통과), 전체 스위트 중에는 522.5 ms(실패). 한계 500 ms 를 오가므로 로컬 통과는 우연이다. CI 러너에서의 실패(`d13b30d3`)는 확인하지 않았고 리더의 기록을 따른다.

## 4. 검증 (증거: `evidence/`)

| 항목 | 결과 | 증거 |
|---|---|---|
| ASan 구성·빌드 (main `ci-asan` 프리셋과 같은 플래그를 명령줄로) | 성공 (병렬 2) | `10_`, `11_` |
| 제외된 시험 따로 실행 | §1 표 | `20_`~`23_` |
| 시험 수정 후 `OomPipeline.*` | 30 통과, ASan 보고 0 | `31_` (카나리만 고친 상태: 두 번째 지점에서 중단), `32_` (둘 다 고친 상태: 통과) |
| ASan, 수정 전 전체 (제외 3개만 뺌) | oom 88, common oom 12, common 69, preprocess 1018 통과 + 알려진 대조 1개 실패. ASan 보고 0건 | `40_`~`43_` |
| ASan, 수정 후 `xpe_preprocess_tests` 전체, **이름 제외 없음** | 1017 통과, 실패 0, 건너뜀 표시 줄 25개 (시험마다 진행 중 한 줄과 요약 한 줄이라 시험 수로는 그 절반 안팎이고, 이 수정이 넣은 4개와 기존 건너뜀을 구분해 세지는 않았다) | `51_asan_full_preprocess_final.txt` |
| 일반(비-ASan) 빌드 | oom 88 통과, 영향받은 시험 11개 통과 (대조 시험은 건너뛰지 않고 실행) | `60_`, `61_` |

`50_` 은 `Perf001` 을 건너뛰기 전의 전체 실행(그 시험만 실패)이고, `51_` 이 최종이다.

## 5. 변경한 시험 코드

| 파일 | 변경 |
|---|---|
| `test_oom_injection.cpp` | ASan 일 때만: 카나리 읽기 직전 `__asan_unpoison_memory_region`, 페이지 가드 할당 건너뛰기 (전처리기로 감쌈, `/WX` 의 C4127 때문에 `constexpr` 불가) |
| `test_xpe_preprocess_memleak.cpp`, `test_xpe_calib_endurance.cpp` | `ControlLeakIsCaught` 에 `GTEST_SKIP` + 사유 (ASan 일 때만) |
| `test_runtime_detection_performance_gate.cpp`, `test_pipeline_performance_3072.cpp` | 시간 단언 두 시험에 `GTEST_SKIP` + 사유 (ASan 일 때만) |

일반 빌드에서는 어느 것도 동작이 바뀌지 않는다.

## 6. 리더가 할 일 / 미검증 · 잔여 위험

- CI `ci.yml` `asan-tests` 잡의 `$exclude` 정규식과 그 위의 이유 주석을 지울 수 있다 (`ctest -E` 없이). 이 보고서의 근거는 로컬 gtest 실행이고, **CI 러너에서 `ctest` 로 같은 결과가 나오는지는 확인하지 못했다.** CI 에서 한 번 돌려 보고 지우는 순서를 권한다.
- 첫 번째 지점(§2.1)의 근본 원인(MSVC STL 의 컨테이너 표시가 요청 크기 이후 바이트를 접근 불가로 만드는지)은 확정하지 못했다. 시험이 자기 블록 안을 읽는 것임은 확정했다. 두 번째 지점(§2.2)의 "남은 섀도" 설명도 추정이다. 둘 다 ASan 보고서의 주소·섀도 바이트와 "그 조치로 사라짐"이 근거이고, 어느 쪽이든 제품 프레임이 잘못 접근한 것은 아니었다.
- ASan 은 로컬 한 대에서 한 번씩 돌렸다. 실행마다 달라질 수 있는 시험(시간 단언)은 §3 에서 건너뛰게 했지만, 그 밖에 CI 에서만 보이는 시험은 못 봤다.
- 다음에 `OomPipeline` 에 시험을 더하면 같은 두 조치가 이미 적용돼 있다.

## Card Cross-Check

| milestone | card |
|---|---|
| ASan 로 제외 시험 분류, 시험 쪽 건너뛰기 제안·적용 | QA-A-238 (이 보고서) |
