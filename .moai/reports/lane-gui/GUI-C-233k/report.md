# GUI-C-233k 보고 — HUD 교정 줄은 실행 결과로, 빈 레인 표지, 간접 모듈 호출자 (Codex #179, Refs #251)

dev/gui 에서 작업, 233j(`e4b37bd6`) 위. 푸시 안 함.

## 1. HUD `Calib:` 줄 (높음)
- 결함: `CalibStageCountDisplay` 가 렌더 당시 **설정된** 교정 모드 개수를 세어 기본 설정(`PreprocessInChain=false`, 체인의 preprocess=`NotRequested`)에서도 "7/7 stages" 로 보였다. 233j 가 이 값을 설정에서 렌더 기록으로 옮기고도 "설정된 수" 라는 의미는 그대로 두었다 — 고친 것은 출처이지 의미가 아니었다.
- 고침: 렌더 기록 속 체인의 preprocess 단계 상태에서 계산한다. 기록 없음 → `—`, `NotRequested` → `not run`, `RequestedNotApplied` → `failed`, `Applied`/`AppliedNoChange` → `3 stages (offset · gain · defect)`.
- **"7" 이 두 번 틀렸다는 것을 이 카드에서 확인했다**: 앱이 부르는 `xpe_preprocess_pipeline_out` 은 offset → gain → defect 세 단계만 실행한다(러너의 결과 문구가 이미 그렇게 적는다). 설정에 있는 ghost / temperature / nonlinearity / binning 네 모드는 이 호출에 연결돼 있지 않다. 그래서 전처리가 정상적으로 돌았을 때도 "7/7" 은 틀린 말이었다. 실행되는 단계 목록을 `GuiPreprocessRunner.ExecutedStages` 한 곳에 두고, HUD 줄과 러너의 결과 문구가 같은 배열을 읽는다(결과 문구는 전과 같은 글자로 나온다).
- 보고서: `calibrationEvaluation.executed = { preprocess, hud, stages }` 를 추가했다. 같은 보고서의 `summary`/`offset`…`binning` 은 **설정된 값**임을 주석으로 명시했다(233e 가 "적용값" 이라 부른 것은 "그 렌더에 쓰인 입력" 이지 "실행된 단계" 가 아니다).
- 시험 (SelfCheck 15, 스크립트된 백엔드가 preprocess 결과를 고름): 기본 설정 / 전처리 요청+실패 / 전처리 요청+실행 세 경우에서 HUD·보고서의 `executed.hud`/`executed.preprocess`·`PreprocessRan`·기록의 체인 상태를 교차 단언하고, 설정된 교정 모드 요약이 세 경우에서 변하지 않았음도 단언(HUD 가 그것의 함수가 아님):

| 경우 | HUD `Calib:` | 보고서 preprocess | PreprocessRan |
|---|---|---|---|
| 기본 (전처리 미요청) | `not run` | NotRequested | False |
| 요청 + 실패 | `failed` | RequestedNotApplied | False |
| 요청 + 실행 | `3 stages (offset · gain · defect)` | Applied | True |

  (증거 파일의 가운뎃점은 콘솔 인코딩 때문에 깨져 보인다. 시험 단언 자체는 통과.) 반증: HUD 를 설정 개수("7/7 stages")로 되돌리면 세 경우 모두 빨강 — "default (preprocess not requested): HUD says '7/7 stages' … expected 'not run'" (`falsification_hud_from_configured_and_tag_without_lane.txt`).
- UIA(L07, Mock fixture): 레인이 그려진 동안 `HudCalibStages` = `Calib: not run`, 백엔드 교체 뒤 `Calib: —` (`e2e_two_lane_class_raw.txt`).
- 한계: Native 백엔드의 실제 `Applied` 경로로 HUD 를 읽는 UIA 는 이 카드에서 돌리지 않았다(Mock fixture 만). `RequestedNotApplied` 를 "failed" 로 보이는 것은 지시대로지만, 이 상태는 "교정 파일 없음으로 건너뜀" 같은 비실패 사유도 포함한다(사유는 체인 상태 줄·진단 패널에 있고 HUD 줄은 한 단어).

## 2. 빈 레인 표지 (중간)
- `RenderedLaneAAlgorithm`/`B` 가 대응 레인 영상(`LaneA`/`LaneB`)이 null 이면 null 을 돌려 화면은 `—` 를 보인다. 레인 작업이 실패해 영상이 없을 때 그 알고리즘 이름을 말하지 않는다. 기록의 `Inputs` 는 여전히 그 알고리즘을 담는다(SelfCheck 14 가 둘 다 확인).
- UIA 텍스트까지: `LaneATag`/`LaneBTag`/`HudCalibStages` 에 자동화 ID 를 달아 L07 이 읽는다 — 그려진 동안 `A  No correction`, 백엔드 교체 뒤 `A  —` / `B  —` (`e2e_two_lane_class_raw.txt` 의 `L-07 before/after` 줄). 반증은 위 파일: 레인이 없는 상태에서 이름을 말하게 하면 14 가 빨강 ("a lane without a picture still names an algorithm").
- Lane B 실패 경로의 UIA(Candidate 만 던지게 해서 화면 텍스트까지 읽기)는 돌리지 못했다 — Mock fixture 로 Candidate 만 실패시키는 수단이 없다. SelfCheck 14(뷰모델 수준)로만 확인.

## 3. 간접 호출자 (낮음)
- 호출 표지를 한 곳(`PreprocessModuleCallMarkers`)으로 모았다: 기존 셋에 모듈을 init 하는 도우미 `LoadCalibrated(` 와 `PrepareMapsThroughTheModule(` 을 더했다. collection 가드와 누수 스윕이 같은 목록을 쓴다. 표지만 이름으로 언급하는 파일(이 목록, 가드, 스윕)은 호출자에서 제외.
- 반증: `LoadCalibrated()` 만 부르는 새 시험 클래스를 넣으면 가드(`not in PreprocessModuleCollection`)와 스윕의 파일 목록 시험(`not in the sweep`)이 빨강 (`falsification_indirect_caller_class.txt`).
- 한계: 모듈을 `RealXpeBackend` 같은 상위 객체를 통해 **간접의 간접**으로 부르는 시험은 표지로 잡지 못한다(통합 시험 프로젝트에서 `git grep 'new RealXpeBackend'` 결과가 0 건임은 확인했지만 이것은 한 가지 경로만 본 것이고, 앞으로 생길 수 있다).

## 4. 확인 (보고만): 기본 `PreprocessInChain`
**GUI-C-234 에서도 바뀌지 않았다.** `wip/gui-c-234` 의 `AppSettings.PreprocessInChain` 은 `dev/gui` 와 같은 코드(필드 기본값 false, 주석 "Off by default")이고, 234 의 `ProcessingChainPlan.BuildStages` 도 `new StageRequest(StageIds.Preprocess, settings.PreprocessInChain)` 그대로다(234 가 바꾼 것은 EnhanceBasic 을 항상 켠 것뿐). 따라서 3단계 요구(기본 체인의 필수 (1) offset·(2) gain)는 **234 에서 바꿀 일**이고 여기서는 고치지 않았다. 234 에서 Run Preprocessing 메뉴 없이 기본 설정으로 연 영상은 전처리를 건너뛰며, 이제 HUD 가 그것을 `not run` 으로 말한다.

## 통과·범위
- SelfCheck 전 시나리오 통과 (`selfcheck_final.txt`, 15 포함). 통합 시험 연속 3회 954 통과 / 0 실패 / 2 건너뜀 (`integration_suite_runs.txt`). `TwoLaneWorkbenchScenarios`: 통과 4, 건너뜀 2(L04·L06 Native 전용).
- Native E2E 전체·결합 CI 는 돌리지 않았다. 스크린샷으로 화면을 보지는 않았다.
