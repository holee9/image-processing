# QA-A-221b — R2 수정(`xpe_preprocess_init` OOM → -2)과 R1 의 계약 문서·시험 (Refs #233)

기준: dev/preprocess `b9006248`(QA-A-221) 위. 코드: `preprocess.cpp`(한 곳), 헤더 `preprocess_api.h`(문서), 시험 `test_oom_injection.cpp`(신규 3건). 증거: `evidence/`. 푸시하지 않음.

## 결론

1. **R2**: `xpe_preprocess_init` 의 `catch (const std::bad_alloc&)` → `XPE_ERR_OUT_OF_MEMORY`(-2). 수정 전 같은 시험이 `allocation #1 failed (rc -3)` 으로 빨강이었고(`evidence/10_before_fix.txt`), 수정 후 20개 할당 지점 전부 통과한다. 헤더는 처음부터 OUT_OF_MEMORY 를 약속하고 있었다(`@return XPE_ERR_OUT_OF_MEMORY on allocation failure`) — 코드가 헤더를 어기고 있던 것이다.
2. **R1 (유지 + 문서 + 빈 저장소 시험)**: 헤더 문서를 한 문장으로 통일하고, 빈 저장소에서 시작하는 OOM 시험 2건을 더했다. **221 보고서에서 내가 놓친 것**: 헤더에는 이미 `xpe_preprocess_pipeline`("Once the set has loaded it stays loaded even if processing the frame then fails")과 캐시 적재 3종("On a miss the module-global store may already hold the file's map…")에 같은 사실이 적혀 있었다. 이번에 **없던 곳은 `xpe_preprocess_pipeline_batch` 하나**였고, 나머지는 "온전·검증된 맵이며 반쯤 바뀐 세트는 없다"는 보장을 문장에 더한 것이다.

## 변경

- `src/preprocess.cpp`: `xpe_preprocess_init` 의 `catch (...)` 앞에 `catch (const std::bad_alloc&)` 한 갈래(+ `#include <new>`). 초기화 플래그는 맨 끝에서만 켜지므로 실패한 init 뒤 모듈은 미초기화 그대로다(시험이 단언).
- `include/xpe/preprocess_api.h`:
  - `xpe_preprocess_pipeline`: "…stays loaded even if processing the frame then fails (QA-A-221b): the maps left in the store are whole and verified -- never a half-replaced set; the replacement of the set is atomic -- and the next call loads them again."
  - `xpe_preprocess_pipeline_batch`: 같은 규칙을 한 문장으로 신설("A call that fails after the calibration set was loaded leaves that set in the store … never half replaced").
  - `xpe_calib_load_{offset,gain,defect}_cached` 3곳: "…may already hold the file's map when the entry could not be cached -- a whole, verified map, never a half-loaded one (QA-A-221b); on a hit that fails, the store is unchanged."
  - (`xpe_preprocess_pipeline_ex` 는 교정 경로를 적재하지 않으므로 해당 없음.)

## 시험 (`test_oom_injection.cpp`, 신규 3건)

| 시험 | 단언 |
|---|---|
| `OomInjection.APreprocessInitThatRunsOutOfMemoryReportsItAsSuchAndLeavesTheModuleUninitialized` | 긴 설정으로 `xpe_preprocess_init` 의 모든 할당 지점(20)을 스윕: 성공이면 초기화됨, 실패면 **반드시 -2** 이고 모듈은 **미초기화** |
| `OomPipeline.AFailedPipelineThatStartedOnAnEmptyStoreLeavesNothingOrTheWholeVerifiedSet` | **빈 저장소에서 시작**해 `xpe_preprocess_pipeline`(교정 경로) 의 73개 지점을 스윕: 실패 뒤 저장소 디지스트가 "빈 저장소"(`none`) 또는 "디렉터리의 세 맵 전부"(`whole`) 중 하나, 그 사이는 실패. 성공이면 `whole`. 두 기준값은 시험 안에서 실제로 계산하고 서로 다름을 대조로 단언 |
| `OomPipeline.AFailedCachedOffsetLoadThatStartedOnAnEmptyStoreLeavesNothingOrTheWholeMap` | 같은 방식으로 `xpe_calib_load_offset_cached` 의 20개 지점(캐시 미스): 저장소가 비었거나 파일의 맵 전체 |

기존 시험이 못 본 이유(221 에서 적은 대로): `PipelineThatRunsOutOfMemoryLeavesEverythingAsItWas` 등은 같은 파일이 이미 적재된 채 시작해, 실패 뒤 "적재 전"과 "적재 후"가 같은 디지스트였다. 새 시험은 빈 저장소에서 시작해 둘을 구별한다. 측정 범위 주의(QA-A-221c 로 정정): 이 문장은 처음에 "디지스트는 맵 세 개와 치수를 본다"고 적었으나 사실은 **세 너비와 맵 바이트만** 봤고 높이와 맵의 존재 여부는 보지 않았다(Codex #81). 221c 가 맵마다 존재 여부·너비·높이·바이트를 넣었다. 게인 다항식·비선형 LUT·품질 메타데이터·타임스탬프·만료값은 지금도 포함하지 않는다.

### 반증 (한 번에 하나, 전체 빌드, 전체 OOM 시험 실행, `evidence/30_arm_q*.txt`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| q1 `init` 이 다시 `PROCESSING_FAILED` | 신규 init 시험 **1건만**(`rc -3`) |
| q2 파이프라인이 **오프셋만 먼저 커밋**하고 나머지는 나중에(반쯤 바뀐 세트) | 신규 빈 저장소 파이프라인 시험(`allocation #35 failed … neither nothing nor the whole verified set (a half-replaced set)`) + 기존 원자적 교체 시험 2건 |
| (캐시 적재 시험) | **반증을 만들지 못했다** — 이 시험의 "반쯤" 은 한 맵 안의 상태인데, 저장소 설치가 한 잠금에서 맵·치수를 함께 바꾸는 구조라 자연스러운 손상 지점이 없다. 이 시험은 "저장소가 비거나 맵 전체" 를 고정하는 **특성화 시험**이며 판별력은 파이프라인 시험보다 약하다 |

## 검증

`evidence/verify/`(반증 뒤 다시 빌드): 빌드 `BUILD_EXIT=0`, preprocess 902 중 894 통과·8 건너뜀(기존), 셔플 894 동일, 할당 실패(`xpe_preprocess_oom_tests`) **63**(60 + 신규 3), common 69·12, ctest 총 1087, 공개 export 변화 0(common 16), 헤더 문서 0건, 프리셋 일치 12/12.

## 미검증 (Gaps)

- 캐시 적재 시험의 판별력(위). 파이프라인 외의 "앞 단계 성공 뒤 실패" 경로(`xpe_calib_load_gain_cached`·`_defect_cached`)는 같은 코드 구조라고 읽었을 뿐 이 카드에서 빈 저장소 시험을 따로 만들지 않았다.
- 품질 메타데이터(게인 파일의 품질 기록)가 실패 뒤 "없음/새 것" 중 하나인지는 이 디지스트로 보지 않았다(별도 기존 시험 `TheQualityMetadataIsCurrentTheMomentTheMapsAre` 등이 다룸).
- 8×8 입력 한계(221 과 같음).

## 잔여 위험

- 헤더 문장은 계약이므로 앞으로 "실패하면 저장소는 적재 전 그대로"를 가정하는 호출자가 생기면 이 문서와 어긋난다.
