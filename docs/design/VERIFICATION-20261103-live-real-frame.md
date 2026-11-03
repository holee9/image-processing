# 운영자 앱(ImageProcTest GUI) 실영상 수동 점검 — 2026-11-03

리더가 실제 화면에서, 실제 영상으로, 설계 문서([README.md](README.md))가 약속한 세 가지 핵심 아이디어가 지금 빌드에서 어떻게 동작하는지 확인한 기록이다. 이 문서는 판정이 아니라 관측 기록이며, 조치 결정은 연결된 이슈에서 한다.

## 조건

| 항목 | 값 |
|---|---|
| 앱 | `ImageProcTest.exe` (GUI-S0, `gui/ImageProcTest`), Native 백엔드, `xpe_display 1.0.0`, `common 0.1.0` |
| 영상 | `fixtures/gui-s0/raw/wrist_lat_3072x3072.raw` (UINT16LE 3072×3072, 화소값 2481~15451) |
| 보정 | `offset.xcal`, `gain.xcal`, `defect.xcal` 한 폴더 (`calib-real-v2`) |
| 조작 | UI Automation 패턴(Expand/Invoke/Select/Value)과 파일 대화상자 자식 창 메시지만 사용. 전역 마우스·키보드 입력 없음 |
| 증거 | 단계 로그 6개, 화면 9장 — 리더 로컬 `.moai/reports/lead/gui-live-real-20261103/verify/` (저장소 무시 경로라 미추적) |

## 설계 약속 대 실제

| 설계 문서의 약속 | 실제 (2026-11-03) | 이슈 |
|---|---|---|
| 핵심 아이디어 1 — 레인 A(기준)·B(후보)에 알고리즘을 골라 같은 영상에서 비교 | 선택칸 항목 4개(No grid correction / Grid suppression / Virtual grid / Preprocess + virtual grid). 레인 B 에서 Virtual grid 를 고르고 실행하면 레인 태그는 바뀌지만 체인은 `gsvg=NotRequested`. gsvg 를 실제로 요청하는 제어는 Parameters 탭의 Grid correction 라디오. 비교 모드 버튼(Swipe/Difference/Source/Processed)은 뷰포트에 반영됨 | #281 |
| 핵심 아이디어 2 — 스터디 큐 + 합격·보류·불합격 판정 + Run-set 집계 | 사용자가 연 영상은 큐에 들어가지 않음(큐 "+" 도 같은 열기 대화상자, 선택 후에도 0건). ✓ Pass → Save & Next 뒤에도 0 pass, Run-set Total 0, 상태줄 "Settings saved." | #280 |
| 핵심 아이디어 3 — 분석 패널(Metrics / Parameters / Run-set / Log) | Parameters·Run-set·Log 탭은 채워짐. **Metrics 탭은 자동 실행 뒤에도, 명시적 Run Preprocessing 뒤에도 "Metrics populated after pipeline run." 자리표시 그대로** | #282 |
| 보정 설정 대화상자(C.2) | 모달이 아니라 오른쪽 "Calibration paths" 패널(View > Calibration Paths Panel, 기본 숨김; Tools > Calibration Settings 로도 열림). 저장하면 다음 시작부터 열린 채 시작 | — |

## 처리 순서 (전처리 → 기본 강조 → 표시)

- 보정 경로가 저장돼 있으면 **영상을 열기만 해도** `preprocess=Applied → enhance_basic=Applied → Display` 순으로 한 번에 실행된다(전처리 182 ms, 기본 강조 182 ms, 열기→표시 711 ms). 자동 창은 C 2580 / W 1198.
- 보정이 Off 이거나 없으면 `preprocess=Skipped, enhance_basic=Applied`, 자동 창 C 3659 / W 460, 빨간 "NOT CORRECTED …" 배너. 앱 내장 빠른 시작은 "기본 강조는 항상 켜져 있고 끄는 설정이 없다"고 적고 있다(의도된 동작, #274 에서 정책 결정 대기). 이 배너는 좌상단 상태 오버레이 뒤에 깔려 앞부분이 가려진다(#282).
- Pipeline 메뉴 순서는 Apply Display Pipeline / Use Automatic Window / Run Preprocessing / Run Deterministic Baseline / Run AI Bone Suppression / Stop / Stage Timing / Diagnostics — 처리 순서와 다르다(#282 참고 항목).
- Tools 첫 항목 "PRE 캘리브레이션 입력·생성·재로드·평가" 창은 1→6 번호 순서(입력 준비 → 입력 검사 → 생성 → 재로드 → 이 GUI 경로로 적용 → 독립 평가·보고서)로 짜여 있다. 전처리 보정 세트에 한해서는 절차 순서대로 된 전용 화면이 있다.

## 상호작용

- 레인 B 창 폭 입력은 즉시 반영이 아니라 "오래됨" 표시 + "Candidate: settings changed – apply again" 배너가 뜨는 변경→재적용 2단계다.
- 하단 상태줄 3칸이 모두 장문이라 잘리고 1·2번째 칸은 같은 chain 줄을 반복한다. UI 안에 한글(미적용, 보정 안 된 EI, 오래됨, 기본 3단계 연결, 연결 대기, Tools 한글 항목 2개)과 영어가 섞여 있다(#282).

## 이번에 확인하지 않은 것

- AI 골 억제(모델 파일 없음), 고급 강조 단독 실행(메뉴 항목 없음), 가상 그리드 실제 적용(파라미터 표·`gsvg.dll` 없음)
- Export bundle / Run on all queued(큐 0건이라 의미 없음), fixture manifest 로 큐를 채운 뒤의 판정 흐름

## 이전 판단의 정정

같은 날 19:54 에 캡처 3장으로 내린 판단 중 "레인 선택칸에 Grid suppression 만 있다", "처리 절차를 따라가는 화면이 없다"는 틀렸고, "보정 없이 강조가 먼저 적용된다"는 보정 미설정 상태에서만 맞는 조건부 관측이었다. 이 문서가 그 정정판이다.
