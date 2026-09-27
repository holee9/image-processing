QA-A-120 (#176) — Lane A, pre. 커밋 `28c8db9`.

## (a) 빨강이 나지 않습니다 — 카드 지시대로 멈추고 보고합니다

이 트리(main 병합 후, `0 0` ahead/behind)에서 **지금 상태 그대로** 돌린 출력입니다. 이슈 본문 표를 옮겨 적지 않았습니다.

| 실행 | 결과 |
|---|---|
| 기본 순서 | `RUN_EXIT=0` — 648건 중 640 통과, 8 건너뜀, **실패 0** |
| `--gtest_shuffle --gtest_random_seed=1` | `RUN_EXIT=0` |
| 같은 방식 seed 2 | `RUN_EXIT=0` |
| 같은 방식 seed 9 | `RUN_EXIT=0` |
| 같은 방식 seed 17 | `RUN_EXIT=0` |

이슈가 적은 "기본 순서에서 이미 1건 실패, 셔플에서 3건" 은 **QA-A-88 시점 상태**이고, 그 뒤 고쳐졌습니다. 세 행을 하나씩 확인했습니다:

| #176 의 행 | 지금 상태 | 무엇이 고쳤나 |
|---|---|---|
| `CalibModeTest.QualityMeta_InitialState` | 통과 | `EXPECT_EXIT` 으로 **자식 프로세스**에서 확인한다 — 부모의 오염과 무관해졌다 |
| `PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded` | 통과 | **QA-A-89 (#176)**: 픽스처 `SetUp` 이 `init → shutdown → init` 으로 **맵 없음을 스스로 만든다**(`test_pipeline_stages.cpp:44-60`) |
| `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` | 통과 | 같은 방식(`test_xpe_preprocess_memleak.cpp:120-129`) |

**(c) 가 지목한 둘째 행에 답합니다**: 이 시험은 이제 **앞 시험이 안 남겨서 우연히 맞는 것이 아닙니다.** `SetUp` 이 직접 맵 없는 상태를 만들고, 그 주석이 이유까지 적고 있습니다 — "init → shutdown → init: shutdown is called on an INITIALIZED module so the clear does not depend on what shutdown does when uninitialized".

**CI 단계도 이미 있습니다**: `.github/workflows/ci.yml:184` 의 `Run preprocess test binary as one process (#176)` 가 **기본 순서 + 고정 시드 1/2/9** 로 돕니다(같은 파일 165–175행에 시드를 고정한 이유가 적혀 있습니다). 카드 ②(시드를 소스에 박기)는 이미 충족돼 있습니다.

그리고 **QA-A-113 이 기계적 감지기를 넣었습니다**(`test_global_state_hygiene.cpp`) — 시험마다 전역 상태를 확인해 되돌리지 않은 시험의 이름을 댑니다. 순서 의존이 "더는 안 나는" 것이 아니라 **나면 즉시 이름이 나오는** 상태입니다.

## (b) 결정 ① 적용 — shutdown 이 모든 전역을 지웁니다

빨강이 없어 "고쳐서 초록으로" 는 성립하지 않지만, 결정 ① 은 그 자체로 유효해 적용했습니다.

- `g_calib_mode`·`g_quality_meta` 는 `xpe_calib_mode.cpp` 의 익명 이름공간에 있어 수명주기 코드가 직접 닿지 못합니다. `xpe_calib_mode_reset_globals()` 를 통해 되돌립니다.
- 헤더의 "지우지 않는다" 문장을 **정정**했습니다. 그 문장은 QA-A-90 이 **동작에 맞춰 넣은 것**이라 계약이 아니라 누락의 기술이었습니다.
- **깨진 것은 없습니다**: 바이너리 648건, ctest 717건, seed 1·2·9·17 모두 통과. 즉 shutdown 뒤에 남은 상태에 기대는 호출자는 이 저장소 안에 없습니다.

**부수 효과가 큽니다.** 위생 감지기가 보고하던 품질 메타 변경이 **30건 → 0건**이 됐습니다. 자기 init/shutdown 을 짝지은 픽스처가 이 축을 공짜로 복원하기 때문입니다. 그래서 그 축을 **보고에서 실패로 올렸습니다** — 보류 사유("되돌릴 공개 호출이 없다")가 사라졌으므로 #176 item 2 가 닫힙니다.

**반증**: `shutdown` 의 reset 호출을 빼면 `RUN_EXIT=1` 이 되고 30건이 다시 이름을 올립니다(`GenerateGainTest.GenerateGain_BasicFlatField -- quality metadata changed;` 외 29건). 되돌리면 초록.

## 모드 누수

`CalibModeTest` 4건이 AUTO 로 끝나던 건은 **이번 변경으로 함께 해소**됐습니다 — 그 픽스처가 `TearDown` 에서 `shutdown` 을 부르면 모드도 기본값으로 돌아갑니다. 감지기의 모드 축이 0건인 것이 그 증거입니다.

## 미검증

- **`g_calib` 맵 자체는 감지기가 보지 않습니다**(읽기 전용 조회가 없습니다). 맵 누수는 여전히 이 감지기로 잡히지 않습니다.
- seed 는 5종만 돌렸습니다(1·2·9·17 + 기본). 표본이지 증명이 아닙니다.
- `clients`·`gui` 가 shutdown 뒤 모드·품질 메타에 기대는지는 **보지 않았습니다**(다른 레인 소유). 이 저장소의 C++ 시험에서는 깨진 것이 없습니다.
- 알림 큐·캐시 크기 등 다른 전역은 축에 없습니다.
