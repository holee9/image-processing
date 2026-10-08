# GUI-C-234 보고 — 기본 체인에 필수 강조 (5)~(8)과 EI (3단계 U3·U4·U5·U8, Refs #251)

브랜치 `wip/gui-c-234` (dev/gui 233c 위로 병합, **dev/gui 에는 아직 없음**). 주요 커밋: `c0067a89`(normFactor·float 인계), `b293dcb2`(EI·uint16 훅·시간·문서), 그 앞 `6f7e8713`(계획 변경).

## U3 — 기본 체인
- `ProcessingChainPlan.BuildStages`: 전처리 → **EnhanceBasic**(끄는 설정 없음) → GSVG → AI. 순서 근거: pipeline-spec §6.1 (EI-0)·(5)~(8) 필수, 파이프라인 표의 단계 번호 순서. 계획 고정 시험 갱신(AI 는 네 번째). Mock 은 같은 단계 목록을 보고하고 각 단계가 네이티브를 요구한다는 같은 사유를 냄(SelfCheck 12).
- 매개변수 대조: 앱 `BaselineParameters` = bilateral 3.0/50.0, CLAHE 3.0 8×8, USM 0.5/2.0/10.0 (모듈 헤더 기본값). post 하니스(QA-B-213 보고서 §M2): "각 함수의 헤더 기본값", **로그 normFactor = 1000**. 값이 갈리는 곳은 로그 normFactor 하나였음 (앱 13606.41 ← 65535/log10(65536)).
- **normFactor 를 1000 으로 바꿈**(리더 결정; 근거는 `BaselineParameters` 주석·README·pin 시험). 바뀌기 전후 Baseline 지문(실데이터, `BaselineFingerprintProbe`, 같은 입력·같은 모듈):

| | 이전 (13606.41) | 이후 (1000) |
|---|---|---|
| 입력 sha256 | c823233f… | c823233f… |
| preprocess 출력 | 2ab3c65c… | 2ab3c65c… (동일) |
| enhance 출력 | 703a9751… | c5b673da… |
| display 출력 (고정 창 32768/65535, 반전) | 557718e1… (0..22934) | f360938d… (60730..62332) |

  결과: 결정론 검사는 영향 없음, 단 고정 창이 로그 영역(약 0..4800)의 일부만 덮어 Baseline 의 표시 그림은 거의 평평해짐(Baseline 은 결정성을 보며 모양을 보지 않음 — 주석에 명시). 저장소에 이 해시를 기대값으로 가진 시험은 없음(전체 grep): 갱신할 골든은 없었고 "normFactor 고정" 시험만 1000 으로 바꿈.

## U4 — 앱이 그린 영상 = 모듈 체인 출력 (바이트 동일)
방법: 실데이터(`calib-real-v2` + wrist 3072×3072)로 UIA 만으로 앱을 돌려, 시험 훅 `XPE_GUI_AUTOMATION_EXPORT_PROCESSED16`(표시 단계의 최종 uint16 을 파일로)으로 받은 바이트를 `m2-post-v2/05_FINAL_default_inverted_auto_window.png` 의 uint16 과 비교.
- **모듈을 post 빌드 한 벌(common, display, enhance_basic, spdlog, fmt + CI 의 preprocess)로 맞추면: 9437184 / 9437184 화소 동일, 최대 차 0 — BYTE-IDENTICAL** (`compare_u4_uint16_all_post_modules.txt`, 같은 구성 2회 반복 모두).
- 인계가 필요했음: 앱의 정상 체인이 전처리 float → enhance → 표시 float 으로 넘어가야 같아짐(체인 러너가 단계 사이 배열을 복제하므로 표시 입력이 enhance float 의 반올림과 같을 때만 그 float 을 씀; 아니면 16비트 경로). 이 인계 없이 normFactor 만 1000 이면 8비트 렌더 기준 98.24%.
- **모듈 구성에 민감함(미해결)**: CI 빌드 DLL 구성(ci-dlls-39f9727a + post 의 display)은 8비트 렌더 기준 99.35%/최대 차 2. all-post 에서 **enhance_basic 만** CI 빌드로 되돌리면 uint16 99.93%/최대 차 384(첫 차이 y=0 x=1436, `swap_one_module_back_to_ci_build.txt`) → 첫 갈라지는 단계는 **enhance_basic 의 모듈 빌드 차이**(앱 코드가 아님). common 만 CI 로 되돌리면 100% 동일. display 를 CI 빌드로 되돌리면 CI 의 display 가 214 이전이라 앱이 시작을 거부(시험 불가). 다만 "post 의 enhance + post 의 display + CI 의 common/spdlog/fmt" 구성은 uint16 97.22%/최대 차 65 로 all-post(100%)와 달랐는데, 둘의 차이는 common·spdlog·fmt 뿐이고 common 단독은 영향이 없었음 → spdlog/fmt 가 수치에 영향을 줄 이유는 모르겠고 **원인을 밝히지 못했습니다**. 구성별 결과는 실행 간 안정적(같은 구성 2회 동일).
- 결론: 앱 코드는 05 와 같은 처리를 함(같은 모듈 한 벌이면 바이트 동일). CI 가 만든 결합 DLL 한 벌로 같은 측정을 해야 U4 의 "같은 DLL" 조건이 닫힙니다 — 그 DLL 이 나오면 `XPE_C234_KEEP_PROCESSED16` 와 `compare234_u16.py` 방식 그대로 다시.

## U5 — EI/DI
- 정상 체인도 EI 를 잼(러너에 항상 측정 지시; `measureExposureIndex` 는 이제 "Baseline 실행" 의미로만). 상태 줄 끝과 결과 JSON `exposureIndex` 에 `Exposure index: uncalibrated EI (보정 안 된 EI) = 219.82, DI = -0.56 (measured, not a pass criterion; …)` (모듈 문장 그대로, 라벨의 한국어 괄호도 그대로); 못 잰 경우 `not measured (preprocessing did not run for this image)` / `(the module gave no value for this image)`.
- 관측: W11 (UIA)가 상태 줄의 `Exposure index: uncalibrated EI` 와 `enhance_basic=Applied` 를 단언해 통과. Mock 은 SelfCheck 12 가 "preprocessing did not run" 사유를 단언.
- 한계: EI 가 나오지 않는 조건(해부 영역 없음)을 만드는 입력으로 실제 모듈을 돌려 보지는 않음 — 문구는 코드 경로 기준.

## U8 — 시간
W12 (UIA, 이 PC, 실데이터): Run Preprocessing → 그림까지 벽시계 5회 **1813, 1811, 1531, 1513, 1519 ms, 중앙값 1531 ms** (한도 3000 ms). UIA 폴링 포함, 앱 안 단계별 시간은 체인 줄(preprocess ≈258 ms, enhance_basic ≈254 ms). 파일 열기 → 요약 표시는 3542 ms(대화상자 구동 포함). **이 PC 한 대의 수치**이며 마일스톤이 요구하는 CI 중앙값이 아닙니다.

## 반증·시험
- 강조 단계를 계획에서 빼면 빨강: `falsification_enhance_removed_from_plan.txt` (주의: 제 `sed` 가 일반 계획과 Baseline 계획의 같은 줄을 둘 다 지워서 Baseline 쪽 시험도 같이 빨개짐 — 일반 계획 전용 시험(ProcessingChainRunnerTests/AiBoneSuppressionStageTests/EnhanceBasicStageTests)도 빨강 목록에 있음).
- 통합 945 통과/0 실패/3 건너뜀(임시 DLL 폴더). SelfCheck 시나리오 12. 빠른 시작(영·한) 인용 대조 45/43 missing 0.

## 미검증·결정 요청
1. U4 를 CI 결합 DLL 로 다시(위). 2. 앱 기준 체인 normFactor 변경은 Baseline 지문을 바꿈(표). 3. EI 의 "해부 영역 없음" 실입력 미관측. 4. U8 은 CI 중앙값 미측정. 5. `wip/gui-c-234` 를 dev/gui 로 병합할 시점은 리더가 정함 (233c 까지 포함해 병합 완료 상태).
