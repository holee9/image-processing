[2026-10-14 12:58 KST] [xpe-gui] GUI-C-236 보고 — 마일스톤 3 U2(빠른 시작 문서를 그대로 따르는 E2E) + U8(앱이 직접 남긴 프레임 처리 시간) (Refs #251)

푸시하지 않았다. 기준 `8feca3d6`. 바뀐 것은 시험 한 파일(`clients/ImageProcTest.E2ETests/Scenarios/Operator/QuickStartWalkthroughScenarios.cs`)뿐이며 제품 코드 변경은 없다. 로컬 가용 메모리 6042 MB 로 시작해 6376 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`). 데이터는 `xpe-data/calib-real-v2`(.xcal 3개)와 저장소 안 손목 raw(`gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw`), DLL 은 CI 에서 받은 `m233/main386/onedll` 세트.

## U2 — W14: 문서 단계 그대로 실데이터 불러오기

`W14_TheQuickStartPage_FollowedStepByStep_OnRealData` 는 문서(`quick-start.html`, 패키지된 도움말)에서 읽은 것만으로 앱을 따라 한다. 시험 코드에 문서 문장을 복사해 두지 않았다.
1. **실행**: 문서의 명령줄 코드에서 스위치 이름·백엔드(`--automation-backend Native`)·영상 크기(3072x3072)를 읽어 그대로 쓴다. 문서가 말하는 크기와 건네 준 데이터 크기가 다르면 실패. 아래 줄이 문서의 `mode=Native` 를 포함하고 제목에 `[MOCK]` 이 없는지 확인.
2. **보정 패널**: 문서의 굵은 글씨 경로 `View > Calibration Paths Panel` 을 이름으로 찾아 실행 → 문서가 말한 제목 `Calibration paths` 의 패널과 세 행(Offset/Gain/Defect)이 폴더를 보여 줌.
3. **영상 불러오기**: 문서 경로 `File > Open Raw...` → 문서 코드 `RAW 3072x3072, min=…, max=…, bytes=18874368` 의 앞부분과 `bytes=18874368` 이 화면에 나타남.
4. **처리**: 문서 경로 `Pipeline > Run Preprocessing (Phase 1a)` → 화면 아래 줄이 문서 코드의 `chain: preprocess=Applied, enhance_basic=Applied, gsvg=NotRequested, ai_bone_suppress=NotRequested` 로 시작하고 `display input=chain`·`times:`·노출 지수를 포함.

통과 3/3 (`w14_run1.txt`, `w14_run2.txt`, `w14_run3.txt`, 회당 약 11 s). 화면에서 읽은 아래 줄: `chain: preprocess=Applied, enhance_basic=Applied, gsvg=NotRequested, ai_bone_suppress=NotRequested; times: preprocess=267 ms, enhance_basic=267 ms, gsvg=0 ms, ai_bone_suppress=0 ms; display input=chain`.

**반증 (문서 쪽을 바꿔 보기, `falsify_page_vs_app.txt`)** — 패키지된 문서 사본만 바꾸고 끝나면 원본으로 복원(복원 확인 True):

| 팔 | 바꾼 것 | 결과 |
|---|---|---|
| A | `File > Open Raw...` → `File > Open Raw Image...` | 빨강: `menu item 'Open Raw Image...' ... was not found: the page and the app disagree.` |
| B | `bytes=18874368` → `bytes=18874369` | 빨강: `the summary says bytes=18874369, as the page does` |
| C | 패널 제목 `Calibration paths` → `Calibration folders` | 빨강: 15 s 안에 그 제목의 패널이 안 나타남 |

문서와 앱의 문자열이 어긋나면 이 시험이 실패한다(U9 인용 대조의 한 축). 입력은 UIA 패턴만(메뉴 ExpandCollapse/Invoke, 대화상자는 패턴 호출 + 대화상자 자신의 단추에 BM_CLICK) — 전역 키보드·마우스 없음. 반증은 문서 쪽만 바꿨다 — 앱 쪽 문자열을 바꾸는 반증은 하지 않았다(어긋남은 양쪽에서 같은 단언으로 잡힌다는 것은 추론이며 관측하지 않음).

## U8 — W15: 앱이 직접 남긴 시간, 5회

`W15_OneFrame_TheAppsOwnBaselineTimes_FiveCommands` 는 같은 실데이터를 열고 `Pipeline > Run Deterministic Baseline` 을 5번 실행한다. 이 명령은 실행마다 `baseline.json` 을 남기며(`runTotalsMs`: 두 번의 체인 실행 각각의 시간, `stageTimes`: 단계별), 시험은 새로 생긴 파일을 읽는다. 별도 기록 기능을 새로 만들지 않았다. 표본은 각 명령의 첫 실행이다.

| 회차 | 첫 실행 총 | 전처리 | 향상(enhance_basic) | 표시 합성(총−앞 둘) | 둘째 실행 총 |
|---|---|---|---|---|---|
| 1 | 658 ms | 256 | 244 | 158 | 668 |
| 2 | 643 ms | 241 | 250 | 152 | 637 |
| 3 | 636 ms | 240 | 244 | 152 | 636 |
| 4 | 577 ms | 218 | 227 | 132 | 643 |
| 5 | 632 ms | 241 | 253 | 138 | 646 |
| **중앙값** | **636 ms** | 241 | 244 | 152 | 643 |

한계 3000 ms (`PERF-P1-001`) 대비 중앙값 636 ms. 시험은 중앙값 ≤ 3000 을 단언한다(`w15_run1.txt`, 통과). **로컬값은 이 기계의 "상한 추정"이다**: CI 러너 값이 아니며(앱의 예산 상수도 "측정하되 단언하지 않음, CI 보정 전"으로 적혀 있다), 아래 CI 제안 참조. "표시 합성" 열은 총 시간에서 기록된 두 단계를 뺀 값이라 앱이 직접 기록한 숫자가 아니고 계산값이다.

**무엇을 재는가(범위)**: 한 프레임의 전처리 + 기본 향상 + 표시 합성(`RunBaselineOnce`)까지. **포함하지 않는 것**: 파일 읽기·디코딩(불러오기 구간)과 WPF 가 화면에 그리는 시간. 불러오기 완료→표시 완료 전체를 결과 JSON 에 남기는 기록은 앱에 없고 이 카드에서 만들지 않았다 — 필요하면 별도 카드.

## 데이터가 없는 러너: W16 (명시적 건너뜀)

W1·W7·W12~W15 는 데이터가 없으면 `EnvGatedFact` 때문에 **발견조차 안 된다**(조용한 부재). 리더 결정(a)에 따라 `W16_TheRealDataWalkthroughs_HaveTheirData` 를 더했다:
- 환경변수 4개(`XPE_C231_NATIVE_DIR/CALIB_DIR/RAW/RAW_SIZE`) 중 빠진 것이 있으면 **건너뜀**, 이유 문장에 무엇이 없는지와 "발견되지 않아 아무것도 안 돌았다"를 적는다(`no_data_run.txt`: W0 통과 + W16 건너뜀 1건).
- 데이터가 있으면 진짜 점검: DLL, .xcal 3개, raw 크기 = 가로×세로×2 (`w16_with_data.txt` 통과).
- **CI 허용 목록 토큰 자리는 비워 두었다**: 이슈 번호가 정해지면 이유 문장 끝에 `XPE-SKIP-ALLOWED:<번호>` 를 붙이면 된다(코드 주석에 표시). 그 전에는 CI 의 "설명되지 않은 건너뜀" 게이트가 이 건너뜀을 빨갛게 읽는다 — 푸시 전에 토큰을 정해야 한다.

## CI 측정 경로 제안
- W15 는 `XPE_C231_*` 4개가 있어야 돈다. 러너에는 .xcal(85 MB, 저장소 밖)이 없다. 영상과 보정 입력(`tests/test_data/CalData_6`)은 저장소 안에 있고, 세트는 lane-pre 하니스(`A241Measure.DISABLED_A245_ExportAndAppPath`)가 만든다 — 리더가 말한 (b) 카드(U10 닫힌 뒤 pre 와 따로)가 이 경로다.
- 그 카드가 열리면 Native E2E 잡 안에서 보정 세트를 만든 뒤 `XPE_C231_*` 를 지정해 W14·W15 를 돌린다. CI 중앙값 기준은 그때 W15 출력의 첫 실행 중앙값.

## 관찰(고치지 않음)
- 빠른 시작 3단계 문서 문장 "아래 줄에 여전히 `chain: not run`" 은 GUI-C-234b 이후(기본 체인이 열자마자 처리, W13a) 앱의 동작과 다르다고 읽힌다 — 이 문장을 직접 단언해 확인하지는 않았고, W14 는 이 문장을 단언하지 않아 통과한다. U9(문서 인용 대조)에서 확인·수정할 항목이다.

## 한계
- 전체 Native E2E 는 돌리지 않았다(리더가 PR CI 로). 이 카드의 실데이터 시험은 로컬 전용이다.
- 안정성은 W14 3회·W15 1회(5명령)로만 보았다.
- 보고서 시각: 리더에게 보낸 착수 메시지의 시각 `13:07` 은 `date` 로 읽지 않은 값이었다(규칙 위반). 이 보고서의 시각은 `date` 로 읽었다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| U2 실데이터 불러오기 E2E (문서 단계 그대로) | GUI-C-236 (W14) |
| U8 앱 안 프레임 시간 5회 중앙값 | GUI-C-236 (W15) |
| 데이터 없음의 명시적 건너뜀 | GUI-C-236 (W16) |
