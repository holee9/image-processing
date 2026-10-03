# QA-A-230 M3 — SRS-CALIB-PERF-001 을 실제 경로에서 재는 시험 (#245)

## 1. 결과

`PipelinePerformance3072.TheWholeFrameWithCalibrationLoadedFitsSrsPerf001`(`tests/test_pipeline_performance_3072.cpp`, 신규)이 꺼진 `Integration.DISABLED_PipelinePerformance3072x3072` 를 대체한다. 3072×3072 에 offset·gain·defect 맵을 실제로 적재하고, 보정이 되는 ghost 핸들(`withStableLag()`)을 만들어 한 프레임의 모든 단계를 부른다. 단계마다 반환 코드가 OK 이고 출력이 독립 계산한 기대값과 같음을 단언한 뒤, 전체 시간이 SRS 의 500 ms 이하임을 단언한다. 판정선은 SRS 그대로다(새로 정하지 않았다). 꺼진 시험은 지웠다.

로컬(i7-12700) 측정: 첫 프레임 83.8 ms, 두 번째 81.3 ms(기본 순서). 셔플 실행에서는 132~136 ms(같은 기계에서 다른 시험·빌드가 겹쳤을 수 있어 **상한 추정**). 두 값 모두 500 ms 의 약 1/4~1/6 이다.

전체 `xpe_preprocess_tests` 990 통과·8 건너뜀·종료 0(기본 순서와 셔플 시드 1·2·9 모두), `xpe_preprocess_oom_tests` 72 통과. 제품 소스 변경 0. 반증 6개 전부 발화(§4).

## 2. 시험이 하는 일

- **적재(프레임 시간 밖)**: `MakeOffsetXCal`(값 200)·`MakeGainXCal`(값 1.25)·defect 맵(1 = 결함, 고립된 16화소) 3072² 파일을 쓰고 `xpe_calib_load_*` 로 적재. 적재 시간은 PERF-003(≤200 ms)과 나란히 **출력만** 한다(로컬 124.6 ms).
- **프레임**: readout 검증 → 온도 보상 → 비선형(LUT 없음) → offset(UINT16) → gain(UINT16→FLOAT32) → defect(제자리) → ghost → 비닝 1. 두 프레임(둘째는 더 밝은 노출이라 ghost 이력이 보정할 것이 생김).
- **단언**:
  - 모든 단계 `XPE_OK`, readout 은 dropped/nonuniform 모두 거짓.
  - offset: 전체 화소가 `max(in-200, 0)` 과 같다(독립 계산).
  - gain: 결함 화소를 뺀 전체 화소가 `offset 출력/1.25` 와 같다(상대 허용 1e-3).
  - defect: 16개 결함 화소가 (60000-200)/1.25 = 47840 에서 이웃 범위 안으로 되돌려진다.
  - ghost: 핸들 생성에 `XPE_WARN_GHOST_NOT_CALIBRATED` 경고가 없다(보정되는 핸들), 둘째 프레임에서 변한 화소가 1개 이상(실측: 9,437,184 전부).
  - 시간: 두 프레임 각각 총합 ≤ 500 ms. 총합은 단계 시간의 합이다(점검용 복사 시간은 제외).
- **출력만**(단언 안 함): 단계별 시간과 SRS 예산(offset 55, 비선형 20, gain 55, 비닝 10, defect 95, ghost 140 ms). 기계 의존이고 런타임 검출기가 이미 기계 비율 게이트를 갖고 있다. 로컬 둘째 프레임: readout 4.2, temp 7.0, nonlinearity 0.0, offset 3.0, gain 27.6, defect 7.0, ghost 32.6, binning 0.0 ms. 비선형과 비닝 1 은 LUT 없음·모드 1 이라 사실상 아무것도 하지 않는다(시간 0.0 ms, 반환 OK 만 확인).

## 3. 어느 CI 잡이 이 시험을 고르는가

- `ci.yml` 잡 `preprocess-tests`: `ctest --test-dir build/ci-preprocess … --output-on-failure`(`ci.yml:185`, 제외 패턴 없음)가 `gtest_discover_tests` 로 등록된 모든 시험을 돌리고, 이어서 "Run preprocess test binary as one process (#176)" 단계가 기본 순서 + 셔플 시드 1·2·9 로 같은 실행 파일 전체를 돌린다. 이 시험은 두 곳 모두에 들어간다(로컬에서 같은 4가지로 확인).
- 그 단계는 출력에서 `^\[perf-gate|^\[hygiene\]` 로 시작하는 줄만 잡 로그에 되풀이한다(`ci.yml:227-231`). 이 시험의 출력은 모두 `[perf-gate-pipeline]` 로 시작해 그 정규식에 걸린다(로컬에서 같은 정규식으로 걸러 `evidence/ci_regex_lines_local.txt` 에 5줄). **CI 로그에서의 관측은 아직 못 했다** — 이 커밋은 푸시 전이다. 병합 후 `preprocess-tests` 잡의 해당 단계에서 `[perf-gate-pipeline] frame 0 (cold) total …` 줄을 확인해야 한다.
- 커버리지(`XpeCoverage.cmake:29`)의 제외 패턴 `Performance|Within[0-9]+ms|…` 에 시험 이름 `PipelinePerformance3072` 가 걸려 커버리지 실행에서는 빠진다(계측 기계에서 500 ms 단언이 의미 없으므로 의도에 맞음). `ci-post` 의 ctest 제외도 같은 이름 규칙이지만 `ci-post` 는 전처리를 빌드하지 않는다.

## 4. 반증 (제품 한 줄 손상, 모두 `evidence/arm_h*.txt`)

| 손상 | 걸린 단언 |
|---|---|
| h1 offset 이 600 ms 바쁜 대기 | 시간: 첫 프레임 683.6 ms > 500, 둘째 683.8 ms > 500 |
| h2 gain 이 즉시 `NOT_INITIALIZED` 거부 | 반환 코드(-6) + gain 출력 불일치 |
| h3 gain 이 입력을 float 로 복사하고 OK | gain 출력 불일치(9,437,168 화소) — 반환 코드는 OK 라 코드 단언만으로는 못 잡는 경우 |
| h4 offset 이 입력을 복사하고 OK | offset 출력 불일치(9,437,184 화소) |
| h5 defect 가 아무것도 안 하고 OK | 결함 화소 16개가 47840 그대로 |
| h6 ghost 핸들이 보정 안 됨(`calibrated=false`) | `XPE_WARN_GHOST_NOT_CALIBRATED` 경고 |

h3·h4 는 처음 빌드가 `/WX`(미사용 변수·함수)로 실패했고 그 사이에 돌린 시험은 앞 반증의 낡은 exe 였다 — 그 결과는 버리고 미사용 참조를 달아 다시 돌렸다(위 표는 재실행 값). 반증 뒤 원본 복원(`git status` 에 시험·CMake 만 수정).

h1 은 이 시험이 "시계가 켜져 있다"는 것을 보인다. 반대로 단계가 거부·통과만 하면 시계가 빠르게 나오는데(꺼진 시험이 정확히 그랬다), h2~h5 가 그것을 코드와 출력 단언이 잡는 것을 보인 것이다.

## 5. 지운 이름을 인용하는 곳

`PipelinePerformance3072x3072` 를 인용하는 곳을 저장소 전체에서 찾았다(`build/`·`.git/` 제외): **330개 파일**. 이 중 소스 3곳을 뺀 전부가 `.moai/reports/lane-pre/QA-A-NN/` 아래의 과거 실행 증거(ctest 목록·전체 실행 로그 약 320개)와 과거 판정서(`QA-A-01/gate.md`, `QA-A-10·102·137·144·147·148·151·175/verdict.md`, `QA-A-228/report.md`·`evidence_greps.txt`·`spec_table_draft.txt`)다. 증거 로그는 그 시점의 사실이므로 고치지 않는다. `docs/`, `.moai/specs/`, `clients/`, `gui/`, `tools/`, `.github/` 에서는 0건(grep).

소스 3곳: `test_integration.cpp`(삭제한 자리에 대체 사유 주석을 남김), `test_pipeline_performance_3072.cpp`(대체 사유), `test_runtime_detection_performance_gate.cpp:578`("Integration.PipelinePerformance3072x3072 is DISABLED and does not call the detector")는 QA-A-141 때까지의 **기록 블록**이라 그대로 두었다 — 그 문장은 그 시점에는 사실이었고 지금 "꺼져 있다"는 부분만 낡았다. 손대지 않고 여기에만 적는다(시험 이름이 사라졌으므로 그 주석의 참조가 끊김).

228 보고서(`QA-A-228/report.md`, 리더가 반영 중인 SRS·RTM 초안 포함)는 이 시험을 "꺼진 성능 시험"으로 인용한다. 그 문장은 228 시점에는 맞았다. RTM·SRS 수정안을 반영할 때 이 커밋 이후 상태("대체됨, `PipelinePerformance3072`")로 갱신해야 한다.

## 6. 미검증 (Gaps) · 잔여 위험

- **CI 에서의 관측 없음**: §3. 병합 후 `preprocess-tests` 잡 로그에서 `[perf-gate-pipeline]` 줄과 시험 통과를 확인해야 한다. 로컬과 CI 기계는 다르다(로컬 i7-12700, CI AMD EPYC 9V45 4 vCPU). 로컬 값은 상한 추정이며 CI 의 값은 모른다. 500 ms 와의 여유가 로컬에서 약 4~6배이므로 CI 에서 2~3배 느려져도 통과하리라는 예상은 **추정**이다.
- 측정은 한 프레임씩 두 번이다. 반복 최소값이 아니라 그 한 번의 값을 단언한다(기계가 일시적으로 바쁘면 흔들릴 수 있다 — 셔플 실행에서 132~136 ms 로 이미 1.6배 차이를 봤다).
- 다음은 시험하지 않는다: ghost 티어 2·3, 비닝 2×2, 비선형 LUT 가 적재된 경우, 폴리노미얼 gain, 3072² 이외 크기. SRS 의 "Ghost Tiers 2-3 ≤ +130 ms" 는 여전히 단언이 없다.
- 시험은 단계별 함수를 직접 부른다. `xpe_preprocess_pipeline`/`_ex`(파일 경로 또는 상태 적재 후)의 한 호출 전체 경로는 재지 않았다. SRS 가 말하는 "total preprocessing pipeline execution time" 과 단계 합이 같은 것이라고 가정했다.
- 단계 합 대비 실제 벽시계: 단계 사이 호출 오버헤드는 포함하지 않는다(무시할 수 있다고 보지만 측정하지 않았다).
- 시험이 쓰는 메모리는 약 400 MB(맵 3개 + 프레임 버퍼 + ghost 이력 189 MB 포함)이다. 메모리가 작은 러너에서는 실패할 수 있다(PERF-002 의 200 MB 는 이 시험이 보증하지 않는다).
- 단일 구성(`ci-preprocess`)에서만 돌렸다.
