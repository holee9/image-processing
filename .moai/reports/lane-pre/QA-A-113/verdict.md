# QA-A-113 (#176) — 픽스처가 자신이 올린 것만 내리게, 전 범위로

## 1. 주장

1. **기계적 감지기를 넣었다.** `test_global_state_hygiene.cpp` 가 **모든 시험이 끝날 때마다** 전역 상태를 확인하고, 바꿔 놓고 되돌리지 않은 시험의 이름을 댄다. 텍스트 개수 세기가 아니라 실행이다(카드 2항).
2. **감지기가 실제로 잡는다.** 반증: A-111 픽스처의 정리를 다시 빼면 감지기가 `NonlinApplyTest.CorrectedPixelsFollowTheIdealLinearResponse -- module left initialized` 로 **원인 시험을 지목**하고 실행이 빨강이 된다.
3. **현재 실패 항목은 0건이다.** 모듈 초기화 상태와 교정 모드를 남기는 시험은 없다.
4. **품질 메타는 30건이 바꾸지만 되돌릴 공개 수단이 없다.** 이것은 시험의 잘못이 아니라 **빠진 기능**이라, 실패시키지 않고 목록만 출력한다. #176 2항의 판단 재료다 — 결정은 리더.
5. **공통 기반 픽스처**(`preprocess_state_fixture.h`)를 만들고 `NonlinApplyTest` 를 옮겼다. 규약: 자신이 올린 것만 내린다.
6. **#176 이 지목한 3건은 모두 현재 통과한다.** 그 중 1건은 이미 `EXPECT_EXIT`(자식 프로세스)로 고쳐져 있었다.
7. seed 22종에서 `RUN_EXIT=0`, 기본 순서 645건 통과, ctest 716건 통과, 헤더 검사 0건.

## 2. 증거

### 2.1 감지기 설계

시험 하나가 끝날 때마다 세 축을 본다. 전부 **부작용 없는 조회**다.

| 축 | 조회 | 판정 |
|---|---|---|
| 모듈 초기화 상태 | `xpe_preprocess_is_initialized()` | **실패** |
| 교정 모드 | `xpe_calib_get_mode()` | **실패** |
| 품질 메타 | `xpe_calib_get_quality_meta()` | 보고만 (§2.4) |

**기준선은 시험마다 잡는다.** 처음에는 프로그램 시작 시점과 비교했는데, 한 번 더러워지면 뒤따르는 시험이 전부 딸려 들어가 **원인 1건에 158건이 보고됐다.** 시험이 물려받은 상태와 비교하도록 바꾸니 실제로 바꾼 시험만 남았다(30건).

**교정 맵은 보지 않는다.** 읽기 전용 조회가 없고, 지우는 유일한 수단인 `shutdown` 은 감지기가 부를 것이 아니다 — `SetUpTestSuite` 에서 한 번 적재해 자기 시험들 사이에서 정당하게 들고 있는 스위트가 있다(`test_xpe_calib_endurance.cpp`).

**부작용 없는 초기화 조회가 없어 하나를 내보냈다.** `xpe_preprocess_is_initialized()` 는 내부 심볼이었다. 그전의 유일한 판별법은 `xpe_preprocess_init()` 을 불러 오류 코드를 읽는 것인데, 그것은 **관측하려는 상태를 바꾸고** 되돌리는 `shutdown` 이 교정 저장소까지 지운다. 읽기 전용 질의라 공개해도 위험이 없다고 판단했다(공개 API 하나 증가 — 보고 대상).

### 2.2 반증

| 상태 | 결과 |
|---|---|
| 정상 | `RUN_EXIT=0`, 실패 항목 0건 |
| A-111 픽스처의 정리 제거 | `RUN_EXIT=1`, `NonlinApplyTest.CorrectedPixelsFollowTheIdealLinearResponse -- module left initialized` + 뒤따른 `GainCorrectTest.AppliesGainCorrection -- module left shut down (it was up on entry)` |
| 되돌림 | `RUN_EXIT=0`, 0건 |

두 번째 행이 중요하다. 감지기가 **원인을 먼저** 대고, 그 여파(남은 상태를 내려 버린 뒤 시험)도 함께 댄다. 두 문장 모두 사실이다.

### 2.3 규약과 공통 기반

`preprocess_state_fixture.h` 의 `XpePreprocessStateFixture`:
- `SetUp` — 모드를 기록하고 `init` 을 시도해 **자기가 올렸는지**를 기록한다.
- `TearDown` — 모드가 바뀌었으면 되돌리고, **자기가 올렸을 때만** 내린다.

"자기가 올렸을 때만" 이 놓치기 쉬운 부분이다. `TearDown` 에서 무조건 `shutdown` 하는 픽스처는 **이미 올라와 있던 모듈을 내린다** — 남기는 것과 똑같이 전역을 바꾸는 일이고, 감지기가 그것도 보고한다(§2.2 두 번째 행의 둘째 줄).

`NonlinApplyTest` 를 이 기반으로 옮겼다. **나머지 45개 파일은 옮기지 않았다** — 감지기가 실패 항목 0건을 보고하므로 옮길 근거가 없고, 근거 없는 대량 수정은 검토 비용만 늘린다. 새로 쓰는 시험이 규약을 따르기 쉽도록 기반을 제공하고, 어기면 감지기가 잡는다.

### 2.4 품질 메타 30건 — 시험의 잘못이 아니다 (#176 2항)

품질 메타를 채우는 것은 생성·적재 호출의 **문서화된 효과**다(`xpe_calib_get_quality_meta` 가 "현재 사용 중인 교정을 기술한다"). 그리고 그것을 되돌리는 공개 호출이 **없다** — `xpe_preprocess_shutdown` 은 명시적으로 지우지 않는다고 헤더가 적고 있다.

따라서 교정을 생성하는 시험은 아무리 잘 써도 이 축을 복원할 수 없다. 실패시키면 **없는 기능을 시험 탓으로 돌리는 것**이므로, 목록만 출력한다:

```
[hygiene] 30 tests changed the calibration quality metadata and no public call
          can restore it (QA-A-113, #176 item 2):
```

30건의 분포: `CalibQualityMetaTest` 13, `CalibModeEnforcementTest` 6, `GenerateGainTest` 4, `GainPolyLoadTest` 5, `CalibOverwriteTest` 2.

**`shutdown` 이 모든 전역을 지워야 하는가**(#176 2항)에 대한 관측:
- 현재 문서(`preprocess_api.h`)는 지우지 않는다고 적고 있지만, 그 문장은 QA-A-90 커밋 `8eca1c4` 에서 **"헤더를 실제 동작에 맞추고"** 로 들어왔다 — 설계 결정을 적은 것이 아니라 현상을 기술한 것이다.
- 모드·품질 메타가 `shutdown` 을 넘어 살아남아야 한다고 단언하는 시험은 **없다**(`MetadataSurvivesSaveAndLoad` 는 파일 왕복이지 수명주기가 아니다).
- 즉 `shutdown` 이 전부 지우도록 바꾸는 데 걸리는 시험은 현재 없어 보인다. 다만 이것은 **운영 의미 변경**이고 호스트가 모드 유지를 기대할 수 있으므로 **혼자 바꾸지 않았다.** 리더 결정 사항이다.

### 2.5 #176 이 지목한 3건의 현재 상태 (카드 4항)

| 시험 | 현재 | 확인 방법 |
|---|---|---|
| `CalibModeTest.QualityMeta_InitialState` | **통과** — `EXPECT_EXIT` 로 자식 프로세스에서 확인하도록 이미 고쳐져 있다. 부모 프로세스의 오염과 무관해졌다 | 코드 확인 + 기본 순서 실행 |
| `PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded` | **통과** | 기본 순서 + seed 22종 |
| `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` | **통과** | 같음 (#176 이 지목한 seed 9·12 포함) |

셋 다 지금은 통과하지만, **감지기의 축이 이 셋의 오염원(교정 맵)을 보지 못한다**는 점은 §2.1 대로 한계다. 맵 누수는 이번 감지기로 잡히지 않는다.

### 2.6 실행 결과

| 확인 | 결과 |
|---|---|
| 바이너리 기본 순서 | `RUN_EXIT=0`, 645건 통과 (639 PASSED + 8 SKIPPED) |
| seed 1–20, 42, 137 (22종) | 전부 `RUN_EXIT=0` |
| ctest 전체 | `CTEST_EXIT=0`, 716건 통과 |
| `BUILD_EXIT` | 0 |
| 헤더 `@param` 검사 | 0 findings |

**표본이지 증명이 아니다.** seed 22종은 가능한 순열의 극히 일부다. 감지기는 그 한계를 좁힌다 — 어떤 순서에서 돌든, 상태를 남긴 시험이 있으면 그 실행에서 이름이 나온다.

## 3. 기준 귀속
- 모든 결과는 이 워크트리 `build/ci-preprocess` RelWithDebInfo 빌드, i7-12700 에서 위 로그 원문으로 관측했다.
- 반증은 같은 빌드에서 코드를 되돌렸다 다시 적용해 두 방향을 모두 관측했다.

## 4. 미검증 · 한계
- **교정 맵 누수는 이 감지기가 보지 못한다**(읽기 전용 조회 없음). #176 2행의 오염원이 그것이다.
- 알림 큐, 캐시 크기 등 다른 전역은 축에 없다. `xpe_clear_alerts` 는 있지만 "몇 건이 쌓였는가"를 기준선과 비교하는 것이 유용한지 판단하지 못했다.
- `ci-common`·`ci-post` 바이너리는 보지 않았다(다른 레인 소유).
- seed 22종 밖의 순서.
- `shutdown` 이 전부 지우도록 바꿨을 때 무엇이 깨지는지는 **실행으로 확인하지 않았다** — 바꾸지 않았기 때문이다.

## 5. 리더 결정이 필요한 것

1. **`xpe_preprocess_shutdown` 이 모드·품질 메타도 지워야 하는가**(#176 2항). 지운다면 품질 메타 30건이 자동으로 해소되고 감지기의 세 번째 축을 실패로 올릴 수 있다. 운영 의미 변경이라 혼자 하지 않았다.
2. **CI 에 바이너리 직접 실행 추가**(#176 3항) — `ci.yml` 은 리더 소유다. 필요한 것은 두 줄이다: 기본 순서 1회, 고정 시드 셔플 1회(예: seed 9). ctest 의 프로세스 분리가 숨기는 것을 드러내는 유일한 방법이고, 감지기는 그 실행에서만 말을 한다.
3. **공개 API 하나 증가**: `xpe_preprocess_is_initialized()`. 읽기 전용 질의다.

## 6. 발견 — 헤더 검사기가 `extern "C" XPE_API` 선언의 주석을 못 찾는다

`tools/docs/check_header_docs.py` 는 `XPE_API` 부터 매칭을 시작하고, 주석 블록과 매칭 시작점 사이에 공백 외의 것이 있으면 "no doc block" 으로 본다. 그래서 다음은 **주석이 있는데도 findings 로 잡힌다**:

```cpp
/** ... */
extern "C" XPE_API bool f(void);
```

이번에는 선언을 `extern "C" { ... }` 블록 안으로 옮겨(주석이 `XPE_API` 바로 위에 오도록) 통과시켰다. 검사기를 고칠지는 리더 판단이다 — 고치지 않으면 앞으로도 같은 형태가 거짓 양성으로 잡히고, 사람이 주석을 중복해 붙이게 될 수 있다.
